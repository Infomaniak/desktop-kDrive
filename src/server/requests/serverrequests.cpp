/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(KD_WINDOWS)
#define _WINSOCKAPI_
#endif

#include "serverrequests.h"
#include "config.h"
#include "keychainmanager/keychainmanager.h"
#include "jobs/network/kDrive_API/getrootfilelistjob.h"
#include "jobs/network/kDrive_API/getfilelistjob.h"
#include "jobs/network/kDrive_API/getfileinfojob.h"
#include "jobs/network/kDrive_API/postfilelinkjob.h"
#include "jobs/network/kDrive_API/getfilelinkjob.h"
#include "jobs/network/infomaniak_API/getinfouserjob.h"
#include "jobs/network/kDrive_API/getinfodrivejob.h"
#include "jobs/network/kDrive_API/getthumbnailjob.h"
#include "jobs/network/getavatarjob.h"
#include "jobs/network/kDrive_API/getdriveslistjob.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/getsizejob.h"
#include "utility/jsonparserutility.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/user.h"
#include "libcommon/utility/utility.h" // fileSystemName(const QString&)
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/olddb/oldsyncdb.h"
#include "libsyncengine/requests/parameterscache.h"
#include "libsyncengine/requests/exclusiontemplatecache.h"

#include <QDir>
#include <QUuid>

#include <sstream>
#include <fstream>

namespace KDC {

ExitCode ServerRequests::getUserDbIdList(QList<int> &list) {
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }

    list.clear();
    for (const User &user: userList) {
        list << user.dbId();
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getUserDbIdList(std::vector<int> &list) {
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }

    list.clear();
    for (const User &user: userList) {
        list.push_back(user.dbId());
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getUserInfoList(QList<UserInfo> &list) {
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }

    list.clear();
    for (const User &user: userList) {
        UserInfo userInfo;
        userToUserInfo(user, userInfo);
        list << userInfo;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getUserInfoList(std::vector<UserInfo> &list) {
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }

    list.clear();
    for (const User &user: userList) {
        UserInfo userInfo;
        userToUserInfo(user, userInfo);
        list.push_back(userInfo);
    }

    return ExitCode::Ok;
}

ExitCode KDC::ServerRequests::getUserIdFromUserDbId(int userDbId, int &userId) {
    User user;
    bool found;
    if (!ParmsDb::instance()->selectUser(userDbId, user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUser");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User with id=" << userDbId << " not found");
        return ExitCode::DataError;
    }

    userId = user.userId();

    return ExitCode::Ok;
}

ExitCode ServerRequests::deleteUser(int userDbId) {
    // Delete user (and linked accounts/drives/syncs by cascade)
    bool found;
    if (!ParmsDb::instance()->deleteUser(userDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteUser");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User with id=" << userDbId << " not found");
        return ExitCode::DataError;
    }

    AbstractTokenNetworkJob::clearCacheForUser(userDbId);

    return ExitCode::Ok;
}

ExitCode ServerRequests::deleteAccount(int accountDbId) {
    // Delete account (and linked drives/syncs by cascade)
    bool found;
    if (!ParmsDb::instance()->deleteAccount(accountDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteAccount");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Account with id=" << accountDbId << " not found");
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::deleteDrive(int driveDbId) {
    // Delete drive (and linked syncs by cascade)
    bool found;
    if (!ParmsDb::instance()->deleteDrive(driveDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCode::DataError;
    }

    AbstractTokenNetworkJob::clearCacheForDrive(driveDbId);

    return ExitCode::Ok;
}

ExitCode ServerRequests::deleteSync(int syncDbId) {
    // Delete Sync in DB
    bool found;
    if (!ParmsDb::instance()->deleteSync(syncDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteSync");
        return ExitCode::DbError;
    }

    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getAccountInfoList(QList<AccountInfo> &list) {
    std::vector<Account> accountList;
    if (!ParmsDb::instance()->selectAllAccounts(accountList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllAccounts");
        return ExitCode::DbError;
    }

    list.clear();
    for (const Account &account: accountList) {
        AccountInfo accountInfo;
        accountToAccountInfo(account, accountInfo);
        list << accountInfo;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getDriveInfoList(QList<DriveInfo> &list) {
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(driveList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllDrives");
        return ExitCode::DbError;
    }

    list.clear();
    for (const Drive &drive: driveList) {
        DriveInfo driveInfo;
        driveToDriveInfo(drive, driveInfo);
        list << driveInfo;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getDriveInfo(int driveDbId, DriveInfo &driveInfo) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCode::DataError;
    }

    driveToDriveInfo(drive, driveInfo);

    return ExitCode::Ok;
}

ExitCode ServerRequests::getDriveIdFromDriveDbId(int driveDbId, int &driveId) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCode::DataError;
    }

    driveId = drive.driveId();

    return ExitCode::Ok;
}

ExitCode KDC::ServerRequests::getDriveIdFromSyncDbId(int syncDbId, int &driveId) {
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync with id=" << syncDbId << " not found");
        return ExitCode::DataError;
    }

    Drive drive;
    if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << sync.driveDbId() << " not found");
        return ExitCode::DataError;
    }

    driveId = drive.driveId();

    return ExitCode::Ok;
}

ExitCode ServerRequests::updateDrive(const DriveInfo &driveInfo) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveInfo.dbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found in table drive for dbId=" << driveInfo.dbId());
        return ExitCode::DataError;
    }

    drive.setNotifications(driveInfo.notifications());

    if (!ParmsDb::instance()->updateDrive(drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found in table drive for dbId=" << driveInfo.dbId());
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getSyncInfoList(QList<SyncInfo> &list) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return ExitCode::DbError;
    }
    list.clear();
    SyncInfo syncInfo;
    for (const Sync &sync: syncList) {
        syncToSyncInfo(sync, syncInfo);
        list << syncInfo;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getParameters(ParametersInfo &parametersInfo) {
    parametersToParametersInfo(ParametersCache::instance()->parameters(), parametersInfo);
    return ExitCode::Ok;
}

ExitCode ServerRequests::updateParameters(const ParametersInfo &parametersInfo) {
    parametersInfoToParameters(parametersInfo, ParametersCache::instance()->parameters());
    auto exitCode = ExitCode::Ok;
    ParametersCache::instance()->save(&exitCode);
    return exitCode;
}

ExitCode ServerRequests::findGoodPathForNewSync(int driveDbId, const QString &basePath, QString &path, QString &error) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(driveDbId, syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return ExitCode::DbError;
    }

    QString folder = basePath;

    // If the parent folder is a sync folder or contained in one, we can't possibly find a valid sync folder inside it.
    QString parentFolder = QFileInfo(folder).dir().canonicalPath();
    int syncDbId = 0;
    ExitCode exitCode = syncForPath(syncList, parentFolder, syncDbId);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in syncForPath: code=" << exitCode);
        return exitCode;
    }

    if (syncDbId) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"The parent folder is a sync folder or contained in one : " << Path2WStr(QStr2Path(parentFolder)));
        error = QObject::tr("The parent folder is a sync folder or contained in one");
        return ExitCode::SystemError;
    }

    int attempt = 1;
    forever {
        exitCode = checkPathValidityForNewFolder(syncList, driveDbId, folder, error);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in checkPathValidityForNewFolder: code=" << exitCode);
            return exitCode;
        }

        const bool isGood = !QFileInfo::exists(folder) && error.isEmpty();
        if (isGood) {
            break;
        }

        // Count attempts and give up eventually
        attempt++;
        if (attempt > 100) {
            LOG_WARN(Log::instance()->getLogger(), "Can't find a valid path");
            error = QObject::tr("Can't find a valid path");
            return ExitCode::SystemError;
        }

        folder = basePath + QString::number(attempt);
    }

    path = folder;
    return ExitCode::Ok;
}

ExitCode ServerRequests::requestToken(const std::string &code, const std::string &codeVerifier, UserInfo &userInfo,
                                      bool &userCreated, std::string &error, std::string &errorDescr) {
    ExitCode exitCode;

    // Generate keychainKey
    std::string keychainKey(Utility::computeMd5Hash(std::to_string(std::time(nullptr))));

    // Create Login instance and request token
    Login login(keychainKey);
    exitCode = login.requestToken(code, codeVerifier);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Login::requestToken: code=" << exitCode);
        error = login.error();
        errorDescr = login.errorDescr();
        return exitCode;
    }

    // Create or update user
    exitCode = processRequestTokenFinished(login, userInfo, userCreated);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in processRequestTokenFinished: code=" << exitCode);
        return exitCode;
    }

    return exitCode;
}

ExitCode ServerRequests::requestToken(const QString &code, const QString &codeVerifier, UserInfo &userInfo, bool &userCreated,
                                      std::string &error, std::string &errorDescr) {
    return requestToken(QStr2Str(code), QStr2Str(codeVerifier), userInfo, userCreated, error, errorDescr);
}

ExitInfo ServerRequests::getNodeInfo(int userDbId, int driveId, const QString &nodeId, NodeInfo &nodeInfo,
                                     bool withPath /*= false*/) {
    std::shared_ptr<GetFileInfoJob> job;
    try {
        job = std::make_shared<GetFileInfoJob>(userDbId, driveId, nodeId.toStdString());
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetFileInfoJob::GetFileInfoJob for userDbId="
                                                       << userDbId << " driveId=" << driveId << " nodeId=" << nodeId.toStdString()
                                                       << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    job->setWithPath(withPath);

    if (const auto exitInfo = job->runSynchronously(); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetFileInfoJob::runSynchronously for userDbId="
                                                       << userDbId << " driveId=" << driveId << " nodeId=" << nodeId.toStdString()
                                                       << " exitInfo=" << exitInfo << " status=" << job->getStatusCode());
        return ExitCode::BackError;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        LOG_WARN(Log::instance()->getLogger(), "GetFileInfoJob failed for userDbId=" << userDbId << " driveId=" << driveId
                                                                                     << " nodeId=" << nodeId.toStdString());
        ExitCause exitCause{ExitCause::Unknown};
        if (job->getStatusCode() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) exitCause = ExitCause::NotFound;

        return {ExitCode::BackError, exitCause};
    }

    Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
    if (!dataObj) {
        LOG_WARN(Log::instance()->getLogger(), "GetFileInfoJob failed for userDbId=" << userDbId << " driveId=" << driveId
                                                                                     << " nodeId=" << nodeId.toStdString());
        return ExitCode::BackError;
    }

    nodeInfo.setNodeId(nodeId);

    SyncTime modTime;
    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, modTime)) {
        return ExitCode::BackError;
    }
    nodeInfo.setModtime(modTime);

    std::string tmp;
    if (!JsonParserUtility::extractValue(dataObj, nameKey, tmp)) {
        return ExitCode::BackError;
    }
    nodeInfo.setName(QString::fromStdString(tmp));

    if (!JsonParserUtility::extractValue(dataObj, parentIdKey, tmp)) {
        return ExitCode::BackError;
    }
    nodeInfo.setParentNodeId(QString::fromStdString(tmp));

    if (withPath) {
        if (!JsonParserUtility::extractValue(dataObj, pathKey, tmp)) {
            return ExitCode::BackError;
        }
        nodeInfo.setPath(QString::fromStdString(tmp));
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getUserAvailableDrives(int userDbId, QHash<int, DriveAvailableInfo> &list) {
    std::shared_ptr<GetDrivesListJob> job = nullptr;
    try {
        job = std::make_shared<GetDrivesListJob>(userDbId);
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetDrivesListJob::GetDrivesListJob for userDbId=" << userDbId << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetDrivesListJob::runSynchronously for userDbId=" << userDbId << " code=" << exitCode);
        return exitCode;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        LOG_WARN(Log::instance()->getLogger(), "GetDrivesListJob failed for userDbId=" << userDbId);
        return ExitCode::BackError;
    }

    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(Log::instance()->getLogger(), "GetDrivesListJob failed for userDbId=" << userDbId);
        return ExitCode::BackError;
    }

    list.clear();
    for (size_t i = 0; i < dataArray->size(); i++) {
        Poco::JSON::Object::Ptr obj = dataArray->getObject(static_cast<unsigned int>(i));
        if (!obj) {
            continue;
        }

        int driveId = -1;
        if (!JsonParserUtility::extractValue(obj, driveIdKey, driveId)) {
            return ExitCode::BackError;
        }

        int userId = -1;
        if (!JsonParserUtility::extractValue(obj, idKey, userId)) {
            return ExitCode::BackError;
        }

        int accountId = -1;
        if (!JsonParserUtility::extractValue(obj, accountIdKey, accountId)) {
            return ExitCode::BackError;
        }

        std::string driveName;
        if (!JsonParserUtility::extractValue(obj, driveNameKey, driveName)) {
            return ExitCode::BackError;
        }

        std::string colorHex;
        if (Poco::JSON::Object::Ptr prefObj = obj->getObject(preferenceKey)) {
            if (!JsonParserUtility::extractValue(prefObj, colorKey, colorHex, false)) {
                return ExitCode::BackError;
            }
        }
        DriveAvailableInfo driveInfo(driveId, userId, accountId, QString::fromStdString(driveName),
                                     QString::fromStdString(colorHex));

        // Search user in DB
        User user;
        bool found = false;
        if (!ParmsDb::instance()->selectUserByUserId(userId, user, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUserByUserId");
            return ExitCode::DbError;
        }
        if (found) {
            driveInfo.setUserDbId(user.dbId());
        }

        list.insert(driveId, driveInfo);
    }

    return ExitCode::Ok;
}

ExitInfo ServerRequests::getSubFolders(const int userDbId, const int driveId, const QString &nodeId, QList<NodeInfo> &list,
                                       const bool withPath /*= false*/) {
    list.clear();
    uint64_t page = 1;
    uint64_t totalPages = 0;
    do {
        std::shared_ptr<GetRootFileListJob> job = nullptr;
        if (nodeId.isEmpty()) {
            try {
                job = std::make_shared<GetRootFileListJob>(userDbId, driveId, page, true);
            } catch (const std::exception &e) {
                LOG_WARN(Log::instance()->getLogger(), "Error in GetRootFileListJob::GetRootFileListJob for userDbId="
                                                               << userDbId << " driveId=" << driveId << " error=" << e.what());
                return AbstractTokenNetworkJob::exception2ExitCode(e);
            }
        } else {
            try {
                job = std::make_shared<GetFileListJob>(userDbId, driveId, nodeId.toStdString(), page, true);
            } catch (const std::exception &e) {
                LOG_WARN(Log::instance()->getLogger(), "Error in GetFileListJob::GetFileListJob for userDbId="
                                                               << userDbId << " driveId=" << driveId
                                                               << " nodeId=" << nodeId.toStdString() << " error=" << e.what());
                return AbstractTokenNetworkJob::exception2ExitCode(e);
            }
        }

        job->setWithPath(withPath);
        if (const auto exitInfo = job->runSynchronously(); !exitInfo) {
            LOG_WARN(Log::instance()->getLogger(), "Error in GetFileListJob::runSynchronously for userDbId="
                                                           << userDbId << " driveId=" << driveId
                                                           << " nodeId=" << nodeId.toStdString() << " error=" << exitInfo);
            return exitInfo;
        }

        Poco::JSON::Object::Ptr resObj = job->jsonRes();
        if (!resObj) {
            LOG_WARN(Log::instance()->getLogger(), "GetFileListJob failed: no valid JSON message retrieved.");
            return ExitCode::BackError;
        }

        Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
        if (!dataArray) {
            LOG_WARN(Log::instance()->getLogger(), "GetFileListJob failed: missing `data` array in retrieved JSON message.");
            return ExitCode::BackError;
        }

        for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
            Poco::JSON::Object::Ptr dirObj = it->extract<Poco::JSON::Object::Ptr>();
            SyncTime modTime;
            if (!JsonParserUtility::extractValue(dirObj, lastModifiedAtKey, modTime)) {
                return ExitCode::BackError;
            }

            NodeId nodeId2;
            if (!JsonParserUtility::extractValue(dirObj, idKey, nodeId2)) {
                return ExitCode::BackError;
            }

            SyncName tmp;
            if (!JsonParserUtility::extractValue(dirObj, nameKey, tmp)) {
                return ExitCode::BackError;
            }

            SyncName name;
            if (!Utility::normalizedSyncName(tmp, name)) {
                LOGW_DEBUG(Log::instance()->getLogger(),
                           L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(tmp));
                // Ignore the folder
                continue;
            }

            std::string parentId;
            if (!JsonParserUtility::extractValue(dirObj, parentIdKey, parentId)) {
                return ExitCode::BackError;
            }

            SyncName path;
            if (withPath) {
                if (!JsonParserUtility::extractValue(dirObj, pathKey, tmp)) {
                    return ExitCode::BackError;
                }
                if (!Utility::normalizedSyncName(tmp, path)) {
                    LOGW_DEBUG(Log::instance()->getLogger(),
                               L"Error in Utility::normalizedSyncName: " << Utility::formatSyncName(tmp));
                    // Ignore the folder
                    continue;
                }
            }

            NodeInfo nodeInfo(QString::fromStdString(nodeId2), SyncName2QStr(name),
                              -1, // Size is not set here as it can be very long to evaluate
                              parentId.c_str(), modTime, SyncName2QStr(path));
            list << nodeInfo;
        }

        page++;
        totalPages = job->totalPages();
    } while (page <= totalPages);

    return ExitCode::Ok;
}

ExitInfo ServerRequests::getSubFolders(int driveDbId, const QString &nodeId, QList<NodeInfo> &list, bool withPath /*= false*/) {
    Drive drive;
    bool found = false;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCode::DataError;
    }

    Account account;
    if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selectAccount");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Account with id=" << drive.accountDbId() << " not found");
        return ExitCode::DataError;
    }

    return getSubFolders(account.userDbId(), drive.driveId(), nodeId, list, withPath);
}

ExitCode ServerRequests::getNodeIdByPath(int userDbId, int driveId, const SyncPath &path, QString &nodeId) {
    // TODO: test
    QList<NodeInfo> list;
    ExitCode exitCode = getSubFolders(userDbId, driveId, "", list);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Requests::getSubFolders: code=" << exitCode);
        return exitCode;
    }

    std::vector<SyncName> names;
    std::filesystem::path pathTmp(path);
    while (pathTmp != pathTmp.root_path()) {
        names.push_back(pathTmp.filename().native());
        pathTmp = pathTmp.parent_path();
    }

    SyncName target = names.front();
    NodeInfo current;
    bool found = false;
    while (!found && !names.empty()) {
        for (auto node: list) {
            if (QStr2SyncName(node.name()) == names.back()) {
                current = node;
                if (QStr2SyncName(current.name()) == target) {
                    found = true;
                }
                break;
            }
        }
        if (!found) {
            names.pop_back();
            exitCode = getSubFolders(userDbId, driveId, current.nodeId(), list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(Log::instance()->getLogger(), "Error in Requests::getSubFolders: code=" << exitCode);
                return exitCode;
            }
        }
    }
    nodeId = current.nodeId();
    return ExitCode::Ok;
}

ExitInfo ServerRequests::getPathByNodeId(int userDbId, int driveId, const QString &nodeId, QString &path) {
    NodeInfo nodeInfo;

    if (auto exitInfo = getNodeInfo(userDbId, driveId, nodeId, nodeInfo, true); !exitInfo) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Requests::getNodeInfo: " << exitInfo);
        return exitInfo;
    }

    path = nodeInfo.path();

    return ExitCode::Ok;
}

ExitCode ServerRequests::migrateSelectiveSync(int syncDbId, std::pair<SyncPath, SyncName> &syncToMigrate) {
    Sync sync;
    bool found = false;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error while connecting to ParmsDb");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selecSync id = " << syncDbId << " not found");
        return ExitCode::DataError;
    }

    // contruct file path with folder path and dbName
    SyncPath dbPath(syncToMigrate.first);
    dbPath /= syncToMigrate.second;

    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(dbPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dbPath, ioError));
        return ExitCode::SystemError;
    }

    if (ioError == IoError::AccessDenied) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"DB to migrate " << Path2WStr(dbPath) << L" misses search permission.");
        return ExitCode::SystemError;
    }

    if (!exists) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"DB to migrate " << Path2WStr(dbPath) << L" does not exist.");
        return ExitCode::SystemError;
    }


    QList<QPair<QString, SyncNodeType>> list;
    ExitCode exitCode = loadOldSelectiveSyncTable(dbPath, list);
    if (exitCode != ExitCode::Ok) {
        return exitCode;
    }

    for (auto &pair: list) {
        std::filesystem::path path(QStr2Path(pair.first));
        SyncNodeType type = pair.second;
        if (!ParmsDb::instance()->insertMigrationSelectiveSync(MigrationSelectiveSync(syncDbId, path, type))) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertMigrationSelectiveSync");
            return ExitCode::DbError;
        }
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::createUser(const User &user, UserInfo &userInfo) {
    if (!ParmsDb::instance()->insertUser(user)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertUser");
        return ExitCode::DbError;
    }

    // Load User info
    User updatedUser(user);
    bool updated = false;
    if (ExitCode exitCode = loadUserInfo(updatedUser, updated); exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in loadUserInfo");
        return exitCode;
    }

    if (updated) {
        bool found = false;
        if (!ParmsDb::instance()->updateUser(updatedUser, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateUser");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "User not found for userDbId=" << updatedUser.dbId());
            return ExitCode::DataError;
        }
    }

    userToUserInfo(updatedUser, userInfo);

    return ExitCode::Ok;
}

ExitCode ServerRequests::updateUser(const User &user, UserInfo &userInfo) {
    bool found = false;
    if (!ParmsDb::instance()->updateUser(user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateUser");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User not found for userDbId=" << user.dbId());
        return ExitCode::DataError;
    }

    // Load User info
    User userUpdated(user);
    bool updated = false;
    if (const ExitCode exitCode = loadUserInfo(userUpdated, updated); exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in loadUserInfo");
        return exitCode;
    }
    userToUserInfo(userUpdated, userInfo);

    return ExitCode::Ok;
}

ExitCode ServerRequests::createAccount(const Account &account, AccountInfo &accountInfo) {
    if (!ParmsDb::instance()->insertAccount(account)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertAccount");
        return ExitCode::DbError;
    }

    accountToAccountInfo(account, accountInfo);

    return ExitCode::Ok;
}

ExitCode ServerRequests::createDrive(const Drive &drive, DriveInfo &driveInfo) {
    if (!ParmsDb::instance()->insertDrive(drive)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertDrive");
        return ExitCode::DbError;
    }

    // Load Drive info
    Drive driveUpdated(drive);
    Account account;
    bool updated;
    bool accountUpdated;
    bool quotaUpdated;
    ExitCode exitCode = loadDriveInfo(driveUpdated, account, updated, quotaUpdated, accountUpdated);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in User::loadDriveInfo");
        return exitCode;
    }

    if (updated) {
        bool found;
        if (!ParmsDb::instance()->updateDrive(driveUpdated, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateDrive");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "Drive not found for driveDbId=" << driveUpdated.dbId());
            return ExitCode::DataError;
        }
    }

    driveToDriveInfo(driveUpdated, driveInfo);

    return ExitCode::Ok;
}

ExitCode ServerRequests::createSync(const Sync &sync, SyncInfo &syncInfo) {
    if (!ParmsDb::instance()->insertSync(sync)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertSync");
        return ExitCode::DbError;
    }

    syncToSyncInfo(sync, syncInfo);

    return ExitCode::Ok;
}

ExitCode ServerRequests::fixProxyConfig() {
    ProxyConfig proxyConfig = ParametersCache::instance()->parameters().proxyConfig();
    proxyConfig.setType(ProxyType::None);
    ParametersCache::instance()->parameters().setProxyConfig(proxyConfig);
    ParametersCache::instance()->save();
    return ExitCode::Ok;
}

bool ServerRequests::isDisplayableError(const Error &error) {
    switch (error.exitCode()) {
        case ExitCode::UpdateRequired:
            return true;
        case ExitCode::NetworkError: {
            switch (error.exitCause()) {
                // case ExitCause::NetworkTimeout:
                case ExitCause::SocketsDefuncted:
                    return true;
                default:
                    return false;
            }
        }
        case ExitCode::LogicError: {
            return true;
        }
        case ExitCode::DataError: {
            switch (error.exitCause()) {
                case ExitCause::MigrationError:
                case ExitCause::MigrationProxyNotImplemented:
                case ExitCause::FileExists:
                case ExitCause::SyncDirChanged:
                    return true;
                default:
                    return false;
            }
        }
        case ExitCode::BackError: {
            switch (error.exitCause()) {
                case ExitCause::DriveMaintenance:
                case ExitCause::DriveNotRenew:
                case ExitCause::DriveAsleep:
                case ExitCause::DriveWakingUp:
                case ExitCause::DriveAccessError:
                case ExitCause::HttpErrForbidden:
                case ExitCause::ApiErr:
                case ExitCause::FileTooBig:
                case ExitCause::NotFound:
                case ExitCause::QuotaExceeded:
                case ExitCause::FileLocked:
                    return true;
                default:
                    return false;
            }
        }
        case ExitCode::SystemError: {
            switch (error.exitCause()) {
                case ExitCause::SyncDirAccessError:
                    return false;
                default:
                    return true;
            }
        }
        case ExitCode::Unknown: {
            return error.inconsistencyType() != InconsistencyType::PathLength &&
                   error.cancelType() != CancelType::AlreadyExistRemote;
        }
        default:
            return true;
    }
}

bool ServerRequests::isAutoResolvedError(const Error &error) {
    bool autoResolved = false;
    if (error.level() == ErrorLevel::Server) {
        autoResolved = false;
    } else if (error.level() == ErrorLevel::SyncPal) {
        autoResolved =
                (error.exitCode() ==
                         ExitCode::NetworkError // Sync is paused, and we try to restart it every RESTART_SYNCS_INTERVAL
                 || (error.exitCode() == ExitCode::BackError // Sync is stopped and a full sync is restarted
                     && error.exitCause() != ExitCause::DriveAccessError && error.exitCause() != ExitCause::DriveNotRenew) ||
                 error.exitCode() == ExitCode::DataError); // Sync is stopped and a full sync is restarted
    } else if (error.level() == ErrorLevel::Node) {
        autoResolved = (error.conflictType() != ConflictType::None && !isConflictsWithLocalRename(error.conflictType())) ||
                       (error.inconsistencyType() !=
                        InconsistencyType::None /*&& error.inconsistencyType() != InconsistencyType::ForbiddenChar*/) ||
                       error.cancelType() != CancelType::None;
    }

    return autoResolved;
}

ExitCode ServerRequests::getUserFromSyncDbId(int syncDbId, User &user) {
    // Get User
    bool found;
    Sync sync;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found with dbId=" << syncDbId);
        return ExitCode::DataError;
    }

    Drive drive;
    if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found with dbId=" << sync.driveDbId());
        return ExitCode::DataError;
    }

    Account acc;
    if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), acc, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAccount");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Account not found with dbId=" << drive.accountDbId());
        return ExitCode::DataError;
    }

    if (!ParmsDb::instance()->selectUser(acc.userDbId(), user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUser");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User not found with dbId=" << acc.userDbId());
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::createDir(int driveDbId, const QString &parentNodeId, const QString &dirName, QString &newNodeId) {
    // Get drive data
    std::shared_ptr<CreateDirJob> job = nullptr;
    try {
        job = std::make_shared<CreateDirJob>(nullptr, driveDbId, QStr2SyncName(dirName), parentNodeId.toStdString(),
                                             QStr2SyncName(dirName));
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in CreateDirJob::CreateDirJob for driveDbId=" << driveDbId << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in CreateDirJob::runSynchronously for driveDbId=" << driveDbId);
        return exitCode;
    }

    // Extract file ID
    if (job->jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = job->jsonRes()->getObject(dataKey);
        if (dataObj) {
            std::string tmp;
            if (!JsonParserUtility::extractValue(dataObj, idKey, tmp)) {
                return ExitCode::BackError;
            }
            newNodeId = QString::fromStdString(tmp);
        }
    }

    if (newNodeId.isEmpty()) {
        return ExitCode::BackError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getPublicLinkUrl(int driveDbId, const NodeId &nodeId, std::string &linkUrl) {
    auto logWarning = [&](const std::string &context_, const int driveDbId_, const std::string &nodeId_,
                          const std::string &error_) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in " << context_ << " for driveDbId=" << driveDbId_ << " nodeId=" << nodeId_ << " error=" << error_);
    };

    // Create link
    std::shared_ptr<AbstractTokenNetworkJob> job;
    try {
        job = std::make_shared<PostFileLinkJob>(driveDbId, nodeId);
    } catch (const std::exception &e) {
        logWarning("PostFileLinkJob", driveDbId, nodeId, e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    if (!job->runSynchronously()) {
        if (job->exitInfo().code() == ExitCode::BackError && job->exitInfo().cause() == ExitCause::ShareLinkAlreadyExists) {
            // The link already exists, get it
            job.reset();
            try {
                job = std::make_shared<GetFileLinkJob>(driveDbId, nodeId);
            } catch (const std::exception &e) {
                logWarning("GetFileLinkJob", driveDbId, nodeId, e.what());
                return AbstractTokenNetworkJob::exception2ExitCode(e);
            }

            if (!job->runSynchronously()) {
                logWarning("GetFileLinkJob", driveDbId, nodeId, toString(job->exitInfo().code()));
                return job->exitInfo();
            }
        } else {
            logWarning("PostFileLinkJob", driveDbId, nodeId, toString(job->exitInfo().code()));
            return job->exitInfo();
        }
    }

    const Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        logWarning("ServerRequests::getPublicLinkUrl", driveDbId, nodeId, "Fail to parse JSON object");
        return ExitCode::BackError;
    }

    const Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
    if (!dataObj) {
        logWarning("ServerRequests::getPublicLinkUrl", driveDbId, nodeId, "Fail to extract data object");
        return ExitCode::BackError;
    }

    if (!JsonParserUtility::extractValue(dataObj, urlKey, linkUrl)) {
        logWarning("ServerRequests::getPublicLinkUrl", driveDbId, nodeId, "Fail to extract URL");
        return ExitCode::BackError;
    }

    return ExitCode::Ok;
}

ExitInfo ServerRequests::getFolderSize(int userDbId, int driveId, const NodeId &nodeId,
                                       std::function<void(const QString &, qint64)> callback) {
    if (nodeId.empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Node ID is empty");
        return ExitCode::DataError;
    }

    // get size of folder
    std::shared_ptr<GetSizeJob> job = nullptr;
    try {
        job = std::make_shared<GetSizeJob>(userDbId, driveId, nodeId);
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetSizeJob::GetSizeJob for userDbId=" << userDbId << " driveId=" << driveId << " nodeId=" << nodeId
                                                                 << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetSizeJob::runSynchronously for userDbId=" << userDbId << " driveId=" << driveId
                                                                       << " nodeId=" << nodeId << " code=" << exitCode);
        return exitCode;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        // Level = Debug because access forbidden is a normal case
        LOG_DEBUG(Log::instance()->getLogger(),
                  "GetSizeJob failed for userDbId=" << userDbId << " driveId=" << driveId << " nodeId=" << nodeId);
        return ExitCode::BackError;
    }

    Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
    if (!dataObj) {
        LOG_WARN(Log::instance()->getLogger(),
                 "GetSizeJob failed for userDbId=" << userDbId << " driveId=" << driveId << " nodeId=" << nodeId);
        return ExitCode::BackError;
    }

    qint64 size = 0;
    if (!JsonParserUtility::extractValue(dataObj, sizeKey, size)) {
        return ExitCode::BackError;
    }

    callback(QString::fromStdString(nodeId), size);

    return ExitCode::Ok;
}

ExitCode ServerRequests::getPrivateLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCode::DataError;
    }

    linkUrl = QString(APPLICATION_PREVIEW_URL).arg(drive.driveId()).arg(fileId);

    return ExitCode::Ok;
}

ExitCode ServerRequests::getExclusionTemplateList(bool def, QList<ExclusionTemplateInfo> &list) {
    list.clear();
    for (const ExclusionTemplate &exclusionTemplate: ExclusionTemplateCache::instance()->exclusionTemplates(def)) {
        ExclusionTemplateInfo exclusionTemplateInfo;
        ServerRequests::exclusionTemplateToExclusionTemplateInfo(exclusionTemplate, exclusionTemplateInfo);
        list << exclusionTemplateInfo;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::setExclusionTemplateList(bool def, const QList<ExclusionTemplateInfo> &list) {
    std::vector<ExclusionTemplate> exclusionList;
    for (const ExclusionTemplateInfo &exclusionTemplateInfo: list) {
        ExclusionTemplate exclusionTemplate;
        ServerRequests::exclusionTemplateInfoToExclusionTemplate(exclusionTemplateInfo, exclusionTemplate);
        exclusionList.push_back(exclusionTemplate);
    }
    ExitCode exitCode = ExclusionTemplateCache::instance()->update(def, exclusionList);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ExclusionTemplateCache::save");
        return exitCode;
    }

    return ExitCode::Ok;
}

#if defined(KD_MACOS)
ExitCode ServerRequests::getExclusionAppList(bool def, QList<ExclusionAppInfo> &list) {
    std::vector<ExclusionApp> exclusionList;
    if (!ParmsDb::instance()->selectAllExclusionApps(def, exclusionList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllExclusionApps");
        return ExitCode::DbError;
    }
    list.clear();
    for (const ExclusionApp &exclusionApp: exclusionList) {
        ExclusionAppInfo exclusionAppInfo;
        ServerRequests::exclusionAppToExclusionAppInfo(exclusionApp, exclusionAppInfo);
        list << exclusionAppInfo;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::setExclusionAppList(bool def, const QList<ExclusionAppInfo> &list) {
    std::vector<ExclusionApp> exclusionList;
    for (const ExclusionAppInfo &exclusionAppInfo: list) {
        ExclusionApp exclusionApp;
        ServerRequests::exclusionAppInfoToExclusionApp(exclusionAppInfo, exclusionApp);
        exclusionList.push_back(exclusionApp);
    }
    if (!ParmsDb::instance()->updateAllExclusionApps(def, exclusionList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAllExclusionApps");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}
#endif

ExitCode ServerRequests::getErrorInfoList(ErrorLevel level, int syncDbId, int limit, QList<ErrorInfo> &list) {
    std::vector<Error> errorList;
    if (!ParmsDb::instance()->selectAllErrors(level, syncDbId, limit, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return ExitCode::DbError;
    }

    list.clear();
    for (const Error &error: errorList) {
        if (isDisplayableError(error)) {
            ErrorInfo errorInfo;
            errorToErrorInfo(error, errorInfo);
            list << errorInfo;
        }
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getConflictList(int syncDbId, const std::unordered_set<ConflictType> &filter,
                                         std::vector<Error> &errorList) {
    if (filter.empty()) {
        if (!ParmsDb::instance()->selectConflicts(syncDbId, ConflictType::None, errorList)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
            return ExitCode::DbError;
        }
    } else {
        for (auto conflictType: filter) {
            if (!ParmsDb::instance()->selectConflicts(syncDbId, conflictType, errorList)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
                return ExitCode::DbError;
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getConflictErrorInfoList(int syncDbId, const std::unordered_set<ConflictType> &filter,
                                                  QList<ErrorInfo> &errorInfoList) {
    std::vector<Error> errorList;
    ServerRequests::getConflictList(syncDbId, filter, errorList);

    for (const Error &error: errorList) {
        if (isDisplayableError(error)) {
            ErrorInfo errorInfo;
            errorToErrorInfo(error, errorInfo);
            errorInfoList << errorInfo;
        }
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::deleteErrorsServer() {
    if (!ParmsDb::instance()->deleteErrors(ErrorLevel::Server)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteErrors");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

bool keepError(const int syncDbId, const Error &error, ExitInfo &exitInfo) {
    exitInfo = ExitCode::Ok;
    if (error.conflictType() == ConflictType::CreateCreate || error.conflictType() == ConflictType::EditEdit ||
        error.cancelType() == CancelType::FileRescued) {
        // For the selected conflict types, the local item is renamed.
        Sync sync;
        bool found = false;
        if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
            exitInfo = ExitCode::DbError;
            return false;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "Sync with id=" << syncDbId << " not found");
            exitInfo = ExitCode::DataError;
            return false;
        }

        auto ioError = IoError::Success;
        const SyncPath dest = sync.localPath() / error.destinationPath();
        if (const bool success = IoHelper::checkIfPathExists(dest, found, ioError); !success) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dest, ioError));
            exitInfo = ExitCode::SystemError;
            return false;
        }

        // If the conflicted file still exists, keep the error.
        if (found) {
            return true;
        }
    }
    return false;
}

ExitCode ServerRequests::deleteErrorsForSync(const int syncDbId, const bool autoResolved) {
    std::vector<Error> errorList;
    if (!ParmsDb::instance()->selectAllErrors(ErrorLevel::SyncPal, syncDbId, INT_MAX, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return ExitCode::DbError;
    }

    if (!ParmsDb::instance()->selectAllErrors(ErrorLevel::Node, syncDbId, INT_MAX, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return ExitCode::DbError;
    }

    for (const Error &error: errorList) {
        ExitInfo exitInfo;
        if (keepError(syncDbId, error, exitInfo)) continue;
        if (!exitInfo) return exitInfo;

        if (isAutoResolvedError(error) == autoResolved) {
            bool found = false;
            if (!ParmsDb::instance()->deleteError(error.dbId(), found)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteError for dbId=" << error.dbId());
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_WARN(Log::instance()->getLogger(), "Error not found for dbId=" << error.dbId());
                return ExitCode::DataError;
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::deleteInvalidTokenErrors() {
    if (!ParmsDb::instance()->deleteAllErrorsByExitCode(ExitCode::InvalidToken)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteAllErrorsByExitCode");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

#ifdef Q_OS_MAC
ExitCode ServerRequests::deleteLiteSyncNotAllowedErrors() {
    if (!ParmsDb::instance()->deleteAllErrorsByExitCause(ExitCause::LiteSyncNotAllowed)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteAllErrorsByExitCause");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}
#endif

ExitCode ServerRequests::addSync(int userDbId, int accountId, int driveId, const QString &localFolderPath,
                                 const QString &serverFolderPath, const QString &serverFolderNodeId, bool liteSync,
                                 bool showInNavigationPane, AccountInfo &accountInfo, DriveInfo &driveInfo, SyncInfo &syncInfo) {
    LOGW_INFO(Log::instance()->getLogger(), L"Adding new sync - userDbId="
                                                    << userDbId << L" accountId=" << accountId << L" driveId=" << driveId
                                                    << L" localFolderPath=" << Path2WStr(QStr2Path(localFolderPath)).c_str()
                                                    << L" serverFolderPath=" << Path2WStr(QStr2Path(serverFolderPath)).c_str()
                                                    << L" liteSync=" << liteSync);

#ifndef Q_OS_WIN
    Q_UNUSED(showInNavigationPane)
#endif

    ExitCode exitCode;

    // Create Account in DB if needed
    int accountDbId;
    if (!ParmsDb::instance()->accountDbId(userDbId, accountId, accountDbId)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::accountDbId");
        return ExitCode::DbError;
    }

    if (!accountDbId) {
        if (!ParmsDb::instance()->getNewAccountDbId(accountDbId)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewAccountDbId");
            return ExitCode::DbError;
        }

        Account account;
        account.setDbId(accountDbId);
        account.setAccountId(accountId);
        account.setUserDbId(userDbId);
        exitCode = createAccount(account, accountInfo);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in createAccount");
            return exitCode;
        }

        LOG_INFO(Log::instance()->getLogger(), "New account created in DB - accountDbId=" << accountDbId
                                                                                          << " accountId= " << accountId
                                                                                          << " userDbId= " << userDbId);
    }

    // Create Drive in DB if needed
    int driveDbId;
    if (!ParmsDb::instance()->driveDbId(accountDbId, driveId, driveDbId)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::driveDbId");
        return ExitCode::DbError;
    }

    if (!driveDbId) {
        if (!ParmsDb::instance()->getNewDriveDbId(driveDbId)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewDriveDbId");
            return ExitCode::DbError;
        }

        Drive drive;
        drive.setDbId(driveDbId);
        drive.setDriveId(driveId);
        drive.setAccountDbId(accountDbId);
        exitCode = createDrive(drive, driveInfo);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in createDrive");
            return exitCode;
        }

        LOGW_INFO(Log::instance()->getLogger(), L"New drive created in DB - driveDbId=" << driveDbId << L" driveId=" << driveId
                                                                                        << L" accountDbId=" << accountDbId);
    }

    return addSync(driveDbId, localFolderPath, serverFolderPath, serverFolderNodeId, liteSync, showInNavigationPane, syncInfo);
}

ExitCode ServerRequests::addSync(int driveDbId, const QString &localFolderPath, const QString &serverFolderPath,
                                 const QString &serverFolderNodeId, bool liteSync, bool showInNavigationPane,
                                 SyncInfo &syncInfo) {
    LOGW_INFO(Log::instance()->getLogger(), L"Adding new sync - driveDbId="
                                                    << driveDbId << L" localFolderPath=" << Path2WStr(QStr2Path(localFolderPath))
                                                    << L" serverFolderPath=" << Path2WStr(QStr2Path(serverFolderPath))
                                                    << L" liteSync=" << liteSync);

#ifndef Q_OS_WIN
    Q_UNUSED(showInNavigationPane)
#endif

#if !defined(Q_OS_MAC) && !defined(Q_OS_WIN)
    Q_UNUSED(liteSync)
#endif

    ExitCode exitCode;

    // Create Sync in DB
    int syncDbId;
    if (!ParmsDb::instance()->getNewSyncDbId(syncDbId)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewSyncDbId");
        return ExitCode::DbError;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"New sync DB ID retrieved - syncDbId=" << syncDbId);

    QUuid navigationPaneClsid;
#ifdef Q_OS_WIN
    if (showInNavigationPane) {
        navigationPaneClsid = QUuid::createUuid();
    }
#endif

    Sync sync;
    sync.setDbId(syncDbId);
    sync.setDriveDbId(driveDbId);
    auto localPath = QStr2Path(localFolderPath);
#if defined(KD_MACOS)
    // On macOS, the special characters in file names are NFD encoded. However, we use QFileDialog::getExistingDirectory to
    // retrieve the selected sync path which return a NFC encoded path.
    (void) Utility::normalizedSyncPath(localFolderPath.toStdString(), localPath, UnicodeNormalization::NFD);
#endif
    sync.setLocalPath(localPath);
    sync.setTargetPath(QStr2Path(serverFolderPath));
    sync.setTargetNodeId(serverFolderNodeId.toStdString());
    sync.setPaused(false);

    // Check vfs support
    const bool supportVfs = CommonUtility::isNTFS(sync.localPath()) || CommonUtility::isAPFS(sync.localPath());
    sync.setSupportVfs(supportVfs);

#if defined(KD_MACOS)
    sync.setVirtualFileMode(liteSync ? VirtualFileMode::Mac : VirtualFileMode::Off);
#elif defined(KD_WINDOWS)
    sync.setVirtualFileMode(liteSync ? VirtualFileMode::Win : VirtualFileMode::Off);
#else
    sync.setVirtualFileMode(VirtualFileMode::Off);
#endif

    sync.setNotificationsDisabled(false);
    sync.setDbPath(std::filesystem::path());
    sync.setHasFullyCompleted(false);
    sync.setNavigationPaneClsid(navigationPaneClsid.toString().toStdString());
    exitCode = createSync(sync, syncInfo);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in createSync");
        return exitCode;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"New sync created in DB - syncDbId="
                                                    << syncDbId << L" driveDbId=" << driveDbId << L" localFolderPath="
                                                    << Path2WStr(sync.localPath()) << L" serverFolderPath="
                                                    << Path2WStr(sync.targetPath()) << L" dbPath=" << Path2WStr(sync.dbPath()));

    return ExitCode::Ok;
}

ExitInfo ServerRequests::loadDriveInfo(Drive &drive, Account &account, bool &updated, bool &quotaUpdated, bool &accountUpdated) {
    updated = false;
    accountUpdated = false;
    quotaUpdated = false;

    // Get drive data
    std::shared_ptr<GetInfoDriveJob> job = nullptr;
    try {
        job = std::make_shared<GetInfoDriveJob>(drive.dbId());
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetInfoDriveJob::GetInfoDriveJob for driveDbId=" << drive.dbId() << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    const auto exitInfo = job->runSynchronously();
    const bool knownMaintenanceMode = exitInfo.cause() == ExitCause::DriveNotRenew ||
                                      exitInfo.cause() == ExitCause::DriveAsleep || exitInfo.cause() == ExitCause::DriveWakingUp;
    if (!exitInfo && !knownMaintenanceMode) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetInfoDriveJob::runSynchronously for driveDbId=" << drive.dbId());
        return exitInfo;
    }

    Poco::Net::HTTPResponse::HTTPStatus httpStatus = job->getStatusCode();
    if (httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN ||
        httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to get drive info for driveDbId=" << drive.dbId());
        drive.setAccessDenied(true);
        return ExitCode::Ok;
    }
    if (httpStatus != Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Network error in GetInfoDriveJob::runSynchronously for driveDbId=" << drive.dbId());
        return ExitCode::NetworkError;
    }

    Poco::JSON::Object::Ptr dataObj = job->jsonRes()->getObject(dataKey);
    if (dataObj && dataObj->size() != 0) {
        std::string name;
        if (!JsonParserUtility::extractValue(dataObj, nameKey, name)) {
            return ExitCode::BackError;
        }
        if (drive.name() != name) {
            drive.setName(name);
            updated = true;
        }

        int64_t size = 0;
        if (!JsonParserUtility::extractValue(dataObj, sizeKey, size)) {
            return ExitCode::BackError;
        }
        if (drive.size() != size) {
            drive.setSize(size);
            updated = true;
            quotaUpdated = true;
        }

        bool admin = false;
        if (!JsonParserUtility::extractValue(dataObj, accountAdminKey, admin)) {
            return ExitCode::BackError;
        }
        if (drive.admin() != admin) {
            drive.setAdmin(admin);
            updated = true;
        }

        int accountId = 0;
        if (!JsonParserUtility::extractValue(dataObj, accountIdKey, accountId)) {
            return ExitCode::BackError;
        }
        if (account.accountId() != accountId) {
            account.setAccountId(accountId);
            accountUpdated = true;
        }

        // Non DB attributes
        bool inMaintenance = false;
        if (!JsonParserUtility::extractValue(dataObj, inMaintenanceKey, inMaintenance)) {
            return ExitCode::BackError;
        }

        int64_t maintenanceFrom = 0;
        if (drive.maintenanceInfo()._maintenance) {
            if (!JsonParserUtility::extractValue(dataObj, maintenanceAtKey, maintenanceFrom, false)) {
                return ExitCode::BackError;
            }
        }

        bool isLocked = false;
        if (!JsonParserUtility::extractValue(dataObj, isLockedKey, isLocked)) {
            return ExitCode::BackError;
        }
        drive.setLocked(isLocked);
        drive.setAccessDenied(false);
        drive.setMaintenanceInfo({._maintenance = inMaintenance,
                                  ._notRenew = job->exitInfo().cause() == ExitCause::DriveNotRenew,
                                  ._asleep = job->exitInfo().cause() == ExitCause::DriveAsleep,
                                  ._wakingUp = job->exitInfo().cause() == ExitCause::DriveWakingUp,
                                  ._maintenanceFrom = maintenanceFrom});

        int64_t usedSize = 0;
        if (!JsonParserUtility::extractValue(dataObj, usedSizeKey, usedSize)) {
            return ExitCode::BackError;
        }
        if (drive.usedSize() != usedSize) {
            drive.setUsedSize(usedSize);
            quotaUpdated = true;
        }
    } else {
        LOG_WARN(Log::instance()->getLogger(), "Unable to read drive info for driveDbId=" << drive.dbId());
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::getThumbnail(int driveDbId, const NodeId &nodeId, int width, std::string &thumbnail) {
    std::shared_ptr<GetThumbnailJob> job = nullptr;
    try {
        job = std::make_shared<GetThumbnailJob>(driveDbId, nodeId, width);
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetThumbnailJob::GetThumbnailJob for driveDbId="
                                                       << driveDbId << " and nodeId=" << nodeId << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCode::Ok) {
        return exitCode;
    }

    Poco::Net::HTTPResponse::HTTPStatus httpStatus = job->getStatusCode();
    if (httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN ||
        httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to get thumbnail for driveDbId=" << driveDbId << " and nodeId=" << nodeId);
        return ExitCode::DataError;
    } else if (httpStatus != Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Network error in GetThumbnailJob::runSynchronously for driveDbId=" << driveDbId << " and nodeId=" << nodeId);
        return ExitCode::NetworkError;
    }

    thumbnail = job->octetStreamRes();
    return ExitCode::Ok;
}

ExitCode ServerRequests::loadUserInfo(User &user, bool &updated) {
    updated = false;

    // Get user data
    std::shared_ptr<GetInfoUserJob> job = nullptr;
    try {
        job = std::make_shared<GetInfoUserJob>(user.dbId());
    } catch (const std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetInfoUserJob::GetInfoUserJob for userDbId=" << user.dbId() << " error=" << e.what());
        return AbstractTokenNetworkJob::exception2ExitCode(e);
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCode::Ok) {
        if (exitCode == ExitCode::InvalidToken) {
            user.setKeychainKey(""); // Invalid keychain key
        }
        return exitCode;
    }

    if (const Poco::Net::HTTPResponse::HTTPStatus httpStatus = job->getStatusCode();
        httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN ||
        httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to get user info for userId=" << user.userId());
        return ExitCode::DataError;
    } else if (httpStatus != Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK) {
        LOG_WARN(Log::instance()->getLogger(), "Network error in GetInfoUserJob::runSynchronously for userId=" << user.userId());
        return ExitCode::NetworkError;
    }

    if (user.name() != job->name()) {
        user.setName(job->name());
        updated = true;
    }

    if (user.email() != job->email()) {
        user.setEmail(job->email());
        updated = true;
    }

    if (user.avatarUrl() != job->avatarUrl()) {
        if (job->avatarUrl().empty()) {
            user.setAvatar(nullptr);
            user.setAvatarUrl(std::string());
        } else if (user.avatarUrl() != job->avatarUrl()) {
            // get avatarData
            user.setAvatarUrl(job->avatarUrl());
            exitCode = loadUserAvatar(user);
        }
        updated = true;
    }

    if (user.isStaff() != job->isStaff()) {
        user.setIsStaff(job->isStaff());
        updated = true;
    }

    return exitCode;
}

ExitCode ServerRequests::loadUserAvatar(User &user) {
    try {
        GetAvatarJob getAvatarJob = GetAvatarJob(user.avatarUrl());
        const ExitCode exitCode = getAvatarJob.runSynchronously();
        if (exitCode != ExitCode::Ok) {
            return exitCode;
        }

        user.setAvatar(getAvatarJob.avatar());
    } catch (const std::runtime_error &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetAvatarJob::GetAvatarJob: error=" << e.what());
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::processRequestTokenFinished(const Login &login, UserInfo &userInfo, bool &userCreated) {
    // Get user
    User user;
    bool found;
    if (!ParmsDb::instance()->selectUserByUserId(login.apiToken().userId(), user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUserByUserId");
        return ExitCode::DbError;
    }
    if (found) {
        // Update User in DB
        user.setKeychainKey(login.keychainKey());

        AbstractTokenNetworkJob::updateLoginByUserDbId(login, user.dbId());

        ExitCode exitCode = updateUser(user, userInfo);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in updateUser");
            return exitCode;
        }

        userCreated = false;
    } else {
        // Create User in DB
        int dbId;
        if (!ParmsDb::instance()->getNewUserDbId(dbId)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewUserDbId");
            return ExitCode::DbError;
        }

        user.setDbId(dbId);
        user.setUserId(login.apiToken().userId());
        user.setKeychainKey(login.keychainKey());
        ExitCode exitCode = createUser(user, userInfo);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in createUser");
            return exitCode;
        }

        userCreated = true;
    }

    return ExitCode::Ok;
}

QString ServerRequests::canonicalPath(const QString &path) {
    QFileInfo selFile(path);
    if (!selFile.exists()) {
        const auto parentPath = selFile.dir().path();

        // It's possible for the parentPath to match the path
        // (possibly we've arrived at a non-existant drive root on Windows)
        // and recursing would be fatal.
        if (parentPath == path) {
            return path;
        }

        return canonicalPath(parentPath) + '/' + selFile.fileName();
    }
    return selFile.canonicalFilePath();
}

ExitCode ServerRequests::checkPathValidityRecursive(const QString &path, QString &error) {
    if (path.isEmpty()) {
        error = QObject::tr("No valid folder selected!");
        return ExitCode::SystemError;
    }

    QFileInfo selFile(path);

    if (!selFile.exists()) {
        QString parentPath = selFile.dir().path();
        if (parentPath != path) {
            return checkPathValidityRecursive(parentPath, error);
        }
        error = QObject::tr("The selected path does not exist!");
        return ExitCode::SystemError;
    }

    if (!selFile.isDir()) {
        error = QObject::tr("The selected path is not a folder!");
        return ExitCode::SystemError;
    }

    if (!selFile.isWritable()) {
        error = QObject::tr("You have no permission to write to the selected folder!");
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::checkPathValidityForNewFolder(const std::vector<Sync> &syncList, int driveDbId, const QString &path,
                                                       QString &error) {
    ExitCode exitCode = checkPathValidityRecursive(path, error);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in checkPathValidityRecursive: code=" << exitCode);
        return exitCode;
    }

    // check if the local directory isn't used yet in another sync
    Qt::CaseSensitivity cs = Qt::CaseSensitive;

    const QString userDir = QDir::cleanPath(canonicalPath(path)) + '/';

    QList<QPair<std::filesystem::path, int>> folderByDriveList;
    for (const Sync &sync: syncList) {
        folderByDriveList << qMakePair(sync.localPath(), sync.driveDbId());
    }

    for (QPair<std::filesystem::path, int> folderByDrive: folderByDriveList) {
        QString folderDir = QDir::cleanPath(canonicalPath(SyncName2QStr(folderByDrive.first.native()))) + '/';

        bool differentPaths = QString::compare(folderDir, userDir, cs) != 0;
        if (differentPaths && folderDir.startsWith(userDir, cs)) {
            error = QObject::tr(
                            "The local folder %1 contains a folder already synced. "
                            "Please pick another one!")
                            .arg(QDir::toNativeSeparators(path));
            return ExitCode::SystemError;
        }

        if (differentPaths && userDir.startsWith(folderDir, cs)) {
            error = QObject::tr(
                            "The local folder %1 is contained in a folder already synced. "
                            "Please pick another one!")
                            .arg(QDir::toNativeSeparators(path));
            return ExitCode::SystemError;
        }

        // If both pathes are equal, the drive needs to be different
        if (!differentPaths) {
            if (driveDbId == folderByDrive.second) {
                error = QObject::tr(
                                "The local folder %1 is already synced on the same drive. "
                                "Please pick another one!")
                                .arg(QDir::toNativeSeparators(path));
                return ExitCode::SystemError;
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode ServerRequests::syncForPath(const std::vector<Sync> &syncList, const QString &path, int &syncDbId) {
    QString absolutePath = QDir::cleanPath(path) + QLatin1Char('/');

    for (const Sync &sync: syncList) {
        const QString localPath = SyncName2QStr(sync.localPath().native()) + QLatin1Char('/');

        if (absolutePath.startsWith(localPath, (CommonUtility::isWindows() || CommonUtility::isMac()) ? Qt::CaseInsensitive
                                                                                                      : Qt::CaseSensitive)) {
            syncDbId = sync.dbId();
            break;
        }
    }

    return ExitCode::Ok;
}

void ServerRequests::userToUserInfo(const User &user, UserInfo &userInfo) {
    userInfo.setDbId(user.dbId());
    userInfo.setUserId(user.userId());
    userInfo.setName(QString::fromStdString(user.name()));
    userInfo.setEmail(QString::fromStdString(user.email()));
    if (user.avatar()) {
        QByteArray avatarArr;
        std::copy(user.avatar()->begin(), user.avatar()->end(), std::back_inserter(avatarArr));
        QImage avatarImg;
        avatarImg.loadFromData(avatarArr);
        userInfo.setAvatar(avatarImg);
    }
    userInfo.setConnected(!user.keychainKey().empty());
    userInfo.setCredentialsAsked(false);
    userInfo.setIsStaff(user.isStaff());
}

void ServerRequests::accountToAccountInfo(const Account &account, AccountInfo &accountInfo) {
    accountInfo.setDbId(account.dbId());
    accountInfo.setUserDbId(account.userDbId());
}

void ServerRequests::driveToDriveInfo(const Drive &drive, DriveInfo &driveInfo) {
    driveInfo.setDbId(drive.dbId());
    driveInfo.setId(drive.driveId());
    driveInfo.setAccountDbId(drive.accountDbId());
    driveInfo.setName(QString::fromStdString(drive.name()));
    driveInfo.setColor(QColor(QString::fromStdString(drive.color())));
    driveInfo.setNotifications(drive.notifications());
    driveInfo.setAdmin(drive.admin());
    driveInfo.setMaintenance(drive.maintenanceInfo()._maintenance);
    driveInfo.setLocked(drive.locked());
    driveInfo.setAccessDenied(drive.accessDenied());
}

void ServerRequests::syncToSyncInfo(const Sync &sync, SyncInfo &syncInfo) {
    syncInfo.setDbId(sync.dbId());
    syncInfo.setDriveDbId(sync.driveDbId());
    syncInfo.setLocalPath(SyncName2QStr(sync.localPath().native()));
    syncInfo.setTargetPath(SyncName2QStr(sync.targetPath().native()));
    syncInfo.setTargetNodeId(QString::fromStdString(sync.targetNodeId()));
    syncInfo.setSupportVfs(sync.supportVfs());
    syncInfo.setVirtualFileMode(sync.virtualFileMode());
    syncInfo.setNavigationPaneClsid(QString::fromStdString(sync.navigationPaneClsid()));
}

void ServerRequests::syncInfoToSync(const SyncInfo &syncInfo, Sync &sync) {
    sync.setDbId(syncInfo.dbId());
    sync.setDriveDbId(syncInfo.driveDbId());
    sync.setLocalPath(QStr2Path(syncInfo.localPath()));
    sync.setTargetPath(QStr2Path(syncInfo.targetPath()));
    sync.setTargetNodeId(syncInfo.targetNodeId().toStdString());
    sync.setSupportVfs(syncInfo.supportVfs());
    sync.setVirtualFileMode(syncInfo.virtualFileMode());
    sync.setNavigationPaneClsid(syncInfo.navigationPaneClsid().toStdString());
}

void ServerRequests::errorToErrorInfo(const Error &error, ErrorInfo &errorInfo) {
    errorInfo.setDbId(error.dbId());
    errorInfo.setTime(error.time());
    errorInfo.setLevel(error.level());
    errorInfo.setFunctionName(QString::fromStdString(error.functionName()));
    errorInfo.setSyncDbId(error.syncDbId());
    errorInfo.setWorkerName(QString::fromStdString(error.workerName()));
    errorInfo.setExitCode(error.exitCode());
    errorInfo.setExitCause(error.exitCause());
    errorInfo.setLocalNodeId(QString::fromStdString(error.localNodeId()));
    errorInfo.setRemoteNodeId(QString::fromStdString(error.remoteNodeId()));
    errorInfo.setNodeType(error.nodeType());
    errorInfo.setPath(SyncName2QStr(error.path().native()));
    errorInfo.setConflictType(error.conflictType());
    errorInfo.setInconsistencyType(error.inconsistencyType());
    errorInfo.setCancelType(error.cancelType());
    errorInfo.setDestinationPath(SyncName2QStr(error.destinationPath().native()));
    errorInfo.setAutoResolved(isAutoResolvedError(error));
}

void ServerRequests::syncFileItemToSyncFileItemInfo(const SyncFileItem &item, SyncFileItemInfo &itemInfo) {
    itemInfo.setType(item.type());
    itemInfo.setPath(SyncName2QStr(item.path().native()));
    itemInfo.setNewPath(item.newPath().has_value() ? SyncName2QStr(item.newPath().value().native()) : "");
    itemInfo.setLocalNodeId(item.localNodeId().has_value() ? QString::fromStdString(item.localNodeId().value()) : QString());
    itemInfo.setRemoteNodeId(item.remoteNodeId().has_value() ? QString::fromStdString(item.remoteNodeId().value()) : QString());
    itemInfo.setDirection(item.direction());
    itemInfo.setInstruction(item.instruction());
    itemInfo.setStatus(item.status());
    itemInfo.setConflict(item.conflict());
    itemInfo.setInconsistency(item.inconsistency());
    itemInfo.setCancelType(item.cancelType());
    itemInfo.setError(QString::fromStdString(item.error()));
}

void ServerRequests::parametersToParametersInfo(const Parameters &parameters, ParametersInfo &parametersInfo) {
    parametersInfo.setLanguage(parameters.language());
    parametersInfo.setAutoStart(parameters.autoStart());
    parametersInfo.setMonoIcons(parameters.monoIcons());
    parametersInfo.setMoveToTrash(parameters.moveToTrash());
    parametersInfo.setNotificationsDisabled(parameters.notificationsDisabled());
    parametersInfo.setUseLog(parameters.useLog());
    parametersInfo.setLogLevel(parameters.logLevel());
    parametersInfo.setExtendedLog(parameters.extendedLog());
    parametersInfo.setPurgeOldLogs(parameters.purgeOldLogs());

    ProxyConfigInfo proxyConfigInfo;
    proxyConfigToProxyConfigInfo(parameters.proxyConfig(), proxyConfigInfo);
    parametersInfo.setProxyConfigInfo(proxyConfigInfo);

    parametersInfo.setUseBigFolderSizeLimit(parameters.useBigFolderSizeLimit());
    parametersInfo.setBigFolderSizeLimit(parameters.bigFolderSizeLimit());
    parametersInfo.setDarkTheme(parameters.darkTheme());
    parametersInfo.setShowShortcuts(parameters.showShortcuts());

    if (parameters.dialogGeometry()) {
        QByteArray dialogGeometryArr;
        std::copy(parameters.dialogGeometry()->begin(), parameters.dialogGeometry()->end(),
                  std::back_inserter(dialogGeometryArr));
        QList<QByteArray> dialogGeometryLines = dialogGeometryArr.split('\n');
        for (const QByteArray &dialogGeometryLine: dialogGeometryLines) {
            QList<QByteArray> dialogGeometryElts = dialogGeometryLine.split(';');
            if (dialogGeometryElts.size() == 2) {
                parametersInfo.setDialogGeometry(QString(dialogGeometryElts[0]), dialogGeometryElts[1]);
            }
        }
    }
    parametersInfo.setMaxAllowedCpu(parameters.maxAllowedCpu());
    parametersInfo.setDistributionChannel(parameters.distributionChannel());
}

void ServerRequests::parametersInfoToParameters(const ParametersInfo &parametersInfo, Parameters &parameters) {
    parameters.setLanguage(parametersInfo.language());
    parameters.setMonoIcons(parametersInfo.monoIcons());
    parameters.setAutoStart(parametersInfo.autoStart());
    parameters.setNotificationsDisabled(parametersInfo.notificationsDisabled());
    parameters.setUseLog(parametersInfo.useLog());
    parameters.setLogLevel(parametersInfo.logLevel());
    parameters.setExtendedLog(parametersInfo.extendedLog());
    parameters.setPurgeOldLogs(parametersInfo.purgeOldLogs());

    ProxyConfig proxyConfig;
    proxyConfigInfoToProxyConfig(parametersInfo.proxyConfigInfo(), proxyConfig);
    parameters.setProxyConfig(proxyConfig);

    parameters.setUseBigFolderSizeLimit(parametersInfo.useBigFolderSizeLimit());
    parameters.setBigFolderSizeLimit(parametersInfo.bigFolderSizeLimit());
    parameters.setDarkTheme(parametersInfo.darkTheme());
    parameters.setShowShortcuts(parametersInfo.showShortcuts());
    parameters.setMoveToTrash(parametersInfo.moveToTrash());

    if (parametersInfo.dialogGeometry().size()) {
        QByteArray dialogGeometryArr;
        for (const QString &objectName: parametersInfo.dialogGeometry().keys()) {
            dialogGeometryArr += objectName.toUtf8();
            dialogGeometryArr += ";";
            dialogGeometryArr += parametersInfo.dialogGeometry().value(objectName);
            dialogGeometryArr += "\n";
        }
        parameters.setDialogGeometry(
                std::shared_ptr<std::vector<char>>(new std::vector<char>(dialogGeometryArr.begin(), dialogGeometryArr.end())));
    }
    parameters.setMaxAllowedCpu(parametersInfo.maxAllowedCpu());
    parameters.setDistributionChannel(parametersInfo.distributionChannel());
}

void ServerRequests::proxyConfigToProxyConfigInfo(const ProxyConfig &proxyConfig, ProxyConfigInfo &proxyConfigInfo) {
    proxyConfigInfo.setType(proxyConfig.type());
    proxyConfigInfo.setHostName(QString::fromStdString(proxyConfig.hostName()));
    proxyConfigInfo.setPort(proxyConfig.port());
    proxyConfigInfo.setNeedsAuth(proxyConfig.needsAuth());

    if (proxyConfig.needsAuth()) {
        proxyConfigInfo.setUser(QString::fromStdString(proxyConfig.user()));

        // Read pwd from keystore
        std::string pwd;
        bool found;
        if (!KeyChainManager::instance()->readDataFromKeystore(proxyConfig.token(), pwd, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to read proxy pwd from keychain");
            proxyConfigInfo.setPwd(QString());
            return;
        }
        if (!found) {
            LOG_DEBUG(Log::instance()->getLogger(), "Proxy pwd not found for keychainKey=" << proxyConfig.token());
            proxyConfigInfo.setPwd(QString());
            return;
        }
        proxyConfigInfo.setPwd(QString::fromStdString(pwd));
    }
}

void ServerRequests::proxyConfigInfoToProxyConfig(const ProxyConfigInfo &proxyConfigInfo, ProxyConfig &proxyConfig) {
    proxyConfig.setType(proxyConfigInfo.type());
    proxyConfig.setHostName(proxyConfigInfo.hostName().toStdString());
    proxyConfig.setPort(proxyConfigInfo.port());
    proxyConfig.setNeedsAuth(proxyConfigInfo.needsAuth());

    if (proxyConfig.needsAuth()) {
        proxyConfig.setUser(proxyConfigInfo.user().toStdString());

        // Write pwd to keystore
        if (proxyConfig.token().empty()) {
            // Generate token
            std::string keychainKeyProxyPass(Utility::computeMd5Hash(std::to_string(std::time(nullptr))));
            proxyConfig.setToken(keychainKeyProxyPass);
        }
        if (!KeyChainManager::instance()->writeToken(proxyConfig.token(), proxyConfigInfo.pwd().toStdString())) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to write proxy token into keychain");
            return;
        }
    }
}

void ServerRequests::exclusionTemplateToExclusionTemplateInfo(const ExclusionTemplate &exclusionTemplate,
                                                              ExclusionTemplateInfo &exclusionTemplateInfo) {
    exclusionTemplateInfo.setTempl(QString::fromStdString(exclusionTemplate.templ()));
    exclusionTemplateInfo.setWarning(exclusionTemplate.warning());
    exclusionTemplateInfo.setDef(exclusionTemplate.def());
    exclusionTemplateInfo.setDeleted(exclusionTemplate.deleted());
}

void ServerRequests::exclusionTemplateInfoToExclusionTemplate(const ExclusionTemplateInfo &exclusionTemplateInfo,
                                                              ExclusionTemplate &exclusionTemplate) {
    exclusionTemplate.setTempl(exclusionTemplateInfo.templ().toStdString());
    exclusionTemplate.setWarning(exclusionTemplateInfo.warning());
    exclusionTemplate.setDef(exclusionTemplateInfo.def());
    exclusionTemplate.setDeleted(exclusionTemplateInfo.deleted());
}

void ServerRequests::exclusionAppToExclusionAppInfo(const ExclusionApp &exclusionApp, ExclusionAppInfo &exclusionAppInfo) {
    exclusionAppInfo.setAppId(QString::fromStdString(exclusionApp.appId()));
    exclusionAppInfo.setDescription(QString::fromStdString(exclusionApp.description()));
    exclusionAppInfo.setDef(exclusionApp.def());
}

void ServerRequests::exclusionAppInfoToExclusionApp(const ExclusionAppInfo &exclusionAppInfo, ExclusionApp &exclusionApp) {
    exclusionApp.setAppId(exclusionAppInfo.appId().toStdString());
    exclusionApp.setDescription(exclusionAppInfo.description().toStdString());
    exclusionApp.setDef(exclusionAppInfo.def());
}

ExitCode ServerRequests::loadOldSelectiveSyncTable(const SyncPath &syncDbPath, QList<QPair<QString, SyncNodeType>> &list) {
    try {
        OldSyncDb oldSyncDb(syncDbPath);

        if (!oldSyncDb.exists()) {
            LOG_WARN(Log::instance()->getLogger(), "Cannot open DB!");
            return ExitCode::SystemError;
        }

        std::list<std::pair<std::string, SyncNodeType>> selectiveSyncList;
        if (!oldSyncDb.selectAllSelectiveSync(selectiveSyncList)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in OldSyncDb::selectAllSelectiveSync");
            return ExitCode::DbError;
        }

        list.clear();
        for (const auto &selectiveSyncElt: selectiveSyncList) {
            list << qMakePair(QString::fromStdString(selectiveSyncElt.first), selectiveSyncElt.second);
        }

        oldSyncDb.close();
    } catch (std::runtime_error &err) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error getting old selective sync list, oldSyncDb may not exist anymore or is corrupted. error=" << err.what());
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
