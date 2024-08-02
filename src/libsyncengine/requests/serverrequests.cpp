/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#ifdef _WIN32
#define _WINSOCKAPI_
#endif

#include "serverrequests.h"
#include "common/utility.h"
#include "config.h"
#include "parameterscache.h"
#include "exclusiontemplatecache.h"
#include "keychainmanager/keychainmanager.h"
#include "libparms/db/parmsdb.h"
#include "libparms/db/user.h"
#include "libcommon/utility/utility.h"  // fileSystemName(const QString&)
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/jobs/network/getrootfilelistjob.h"
#include "libsyncengine/jobs/network/getfilelistjob.h"
#include "libsyncengine/jobs/network/getfileinfojob.h"
#include "libsyncengine/jobs/network/postfilelinkjob.h"
#include "libsyncengine/jobs/network/getfilelinkjob.h"
#include "libsyncengine/jobs/network/getinfouserjob.h"
#include "libsyncengine/jobs/network/getinfodrivejob.h"
#include "libsyncengine/jobs/network/getthumbnailjob.h"
#include "libsyncengine/jobs/network/getavatarjob.h"
#include "libsyncengine/jobs/network/getdriveslistjob.h"
#include "libsyncengine/jobs/network/createdirjob.h"
#include "libsyncengine/jobs/network/getsizejob.h"
#include "libsyncengine/olddb/oldsyncdb.h"
#include "utility/jsonparserutility.h"
#include "server/logarchiver.h"
#include "libsyncengine/jobs/jobmanager.h"
#include "libsyncengine/jobs/network/upload_session/loguploadsession.h"

#include <QDir>
#include <QUuid>

#include <sstream>
#include <fstream>

namespace KDC {

ExitCode ServerRequests::getUserDbIdList(QList<int> &list) {
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        return ExitCodeDbError;
    }

    list.clear();
    for (const User &user : userList) {
        list << user.dbId();
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getUserInfoList(QList<UserInfo> &list) {
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
        return ExitCodeDbError;
    }

    list.clear();
    for (const User &user : userList) {
        UserInfo userInfo;
        userToUserInfo(user, userInfo);
        list << userInfo;
    }

    return ExitCodeOk;
}

ExitCode KDC::ServerRequests::getUserIdFromUserDbId(int userDbId, int &userId) {
    User user;
    bool found;
    if (!ParmsDb::instance()->selectUser(userDbId, user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUser");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User with id=" << userDbId << " not found");
        return ExitCodeDataError;
    }

    userId = user.userId();

    return ExitCodeOk;
}

ExitCode ServerRequests::deleteUser(int userDbId) {
    // Delete user (and linked accounts/drives/syncs by cascade)
    bool found;
    if (!ParmsDb::instance()->deleteUser(userDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteUser");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User with id=" << userDbId << " not found");
        return ExitCodeDataError;
    }

    AbstractTokenNetworkJob::clearCacheForUser(userDbId);

    return ExitCodeOk;
}

ExitCode ServerRequests::deleteAccount(int accountDbId) {
    // Delete account (and linked drives/syncs by cascade)
    bool found;
    if (!ParmsDb::instance()->deleteAccount(accountDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteAccount");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Account with id=" << accountDbId << " not found");
        return ExitCodeDataError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::deleteDrive(int driveDbId) {
    // Delete drive (and linked syncs by cascade)
    bool found;
    if (!ParmsDb::instance()->deleteDrive(driveDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCodeDataError;
    }

    AbstractTokenNetworkJob::clearCacheForDrive(driveDbId);

    return ExitCodeOk;
}

ExitCode ServerRequests::deleteSync(int syncDbId) {
    // Delete Sync in DB
    bool found;
    if (!ParmsDb::instance()->deleteSync(syncDbId, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteSync");
        return ExitCodeDbError;
    }

    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found for syncDbId=" << syncDbId);
        return ExitCodeDataError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getAccountInfoList(QList<AccountInfo> &list) {
    std::vector<Account> accountList;
    if (!ParmsDb::instance()->selectAllAccounts(accountList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllAccounts");
        return ExitCodeDbError;
    }

    list.clear();
    for (const Account &account : accountList) {
        AccountInfo accountInfo;
        accountToAccountInfo(account, accountInfo);
        list << accountInfo;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getDriveInfoList(QList<DriveInfo> &list) {
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(driveList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllDrives");
        return ExitCodeDbError;
    }

    list.clear();
    for (const Drive &drive : driveList) {
        DriveInfo driveInfo;
        driveToDriveInfo(drive, driveInfo);
        list << driveInfo;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getDriveInfo(int driveDbId, DriveInfo &driveInfo) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCodeDataError;
    }

    driveToDriveInfo(drive, driveInfo);

    return ExitCodeOk;
}

ExitCode ServerRequests::getDriveIdFromDriveDbId(int driveDbId, int &driveId) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCodeDataError;
    }

    driveId = drive.driveId();

    return ExitCodeOk;
}

ExitCode KDC::ServerRequests::getDriveIdFromSyncDbId(int syncDbId, int &driveId) {
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync with id=" << syncDbId << " not found");
        return ExitCodeDataError;
    }

    Drive drive;
    if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << sync.driveDbId() << " not found");
        return ExitCodeDataError;
    }

    driveId = drive.driveId();

    return ExitCodeOk;
}

ExitCode ServerRequests::updateDrive(const DriveInfo &driveInfo) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveInfo.dbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found in table drive for dbId=" << driveInfo.dbId());
        return ExitCodeDataError;
    }

    drive.setNotifications(driveInfo.notifications());

    if (!ParmsDb::instance()->updateDrive(drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found in table drive for dbId=" << driveInfo.dbId());
        return ExitCodeDataError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getSyncInfoList(QList<SyncInfo> &list) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return ExitCodeDbError;
    }
    list.clear();
    SyncInfo syncInfo;
    for (const Sync &sync : syncList) {
        syncToSyncInfo(sync, syncInfo);
        list << syncInfo;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getParameters(ParametersInfo &parametersInfo) {
    parametersToParametersInfo(ParametersCache::instance()->parameters(), parametersInfo);
    return ExitCodeOk;
}

ExitCode ServerRequests::updateParameters(const ParametersInfo &parametersInfo) {
    parametersInfoToParameters(parametersInfo, ParametersCache::instance()->parameters());
    return ParametersCache::instance()->save();
}

ExitCode ServerRequests::findGoodPathForNewSync(int driveDbId, const QString &basePath, QString &path, QString &error) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(driveDbId, syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        return ExitCodeDbError;
    }

    QString folder = basePath;

    // If the parent folder is a sync folder or contained in one, we can't possibly find a valid sync folder inside it.
    QString parentFolder = QFileInfo(folder).dir().canonicalPath();
    int syncDbId = 0;
    ExitCode exitCode = syncForPath(syncList, parentFolder, syncDbId);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in syncForPath : " << exitCode);
        return exitCode;
    }

    if (syncDbId) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"The parent folder is a sync folder or contained in one : " << Path2WStr(QStr2Path(parentFolder)).c_str());
        error = QObject::tr("The parent folder is a sync folder or contained in one");
        return ExitCodeSystemError;
    }

    int attempt = 1;
    forever {
        exitCode = checkPathValidityForNewFolder(syncList, driveDbId, folder, error);
        if (exitCode != ExitCodeOk) {
            LOG_WARN(Log::instance()->getLogger(), "Error in checkPathValidityForNewFolder : " << exitCode);
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
            return ExitCodeSystemError;
        }

        folder = basePath + QString::number(attempt);
    }

    path = folder;
    return ExitCodeOk;
}

ExitCode ServerRequests::requestToken(QString code, QString codeVerifier, UserInfo &userInfo, bool &userCreated,
                                      std::string &error, std::string &errorDescr) {
    ExitCode exitCode;

    // Generate keychainKey
    std::string keychainKey(Utility::computeMd5Hash(std::to_string(std::time(0)).c_str()));

    // Create Login instance and request token
    Login login(keychainKey);
    exitCode = login.requestToken(code.toStdString(), codeVerifier.toStdString());
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Login::requestToken : " << exitCode);
        error = login.error();
        errorDescr = login.errorDescr();
        return exitCode;
    }

    // Create or update user
    exitCode = processRequestTokenFinished(login, userInfo, userCreated);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in processRequestTokenFinished : " << exitCode);
        return exitCode;
    }

    return exitCode;
}

ExitCode ServerRequests::getNodeInfo(int userDbId, int driveId, const QString &nodeId, NodeInfo &nodeInfo,
                                     bool withPath /*= false*/) {
    std::shared_ptr<GetFileInfoJob> job;
    try {
        job = std::make_shared<GetFileInfoJob>(userDbId, driveId, nodeId.toStdString());
    } catch (std::exception const &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetFileInfoJob::GetFileInfoJob for userDbId="
                                                   << userDbId << " driveId=" << driveId
                                                   << " nodeId=" << nodeId.toStdString().c_str() << " : " << e.what());
        return ExitCodeDataError;
    }

    job->setWithPath(withPath);
    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetFileInfoJob::runSynchronously for userDbId="
                                                   << userDbId << " driveId=" << driveId
                                                   << " nodeId=" << nodeId.toStdString().c_str() << " : " << exitCode);
        return exitCode;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        LOG_WARN(Log::instance()->getLogger(),
                 "GetFileInfoJob failed for userDbId=" << userDbId << " driveId=" << driveId
                                                       << " nodeId=" << nodeId.toStdString().c_str());
        return ExitCodeBackError;
    }

    Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
    if (!dataObj) {
        LOG_WARN(Log::instance()->getLogger(),
                 "GetFileInfoJob failed for userDbId=" << userDbId << " driveId=" << driveId
                                                       << " nodeId=" << nodeId.toStdString().c_str());
        return ExitCodeBackError;
    }

    nodeInfo.setNodeId(nodeId);

    SyncTime modTime;
    if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, modTime)) {
        return ExitCodeBackError;
    }
    nodeInfo.setModtime(modTime);

    std::string tmp;
    if (!JsonParserUtility::extractValue(dataObj, nameKey, tmp)) {
        return ExitCodeBackError;
    }
    nodeInfo.setName(QString::fromStdString(tmp));

    if (!JsonParserUtility::extractValue(dataObj, parentIdKey, tmp)) {
        return ExitCodeBackError;
    }
    nodeInfo.setParentNodeId(QString::fromStdString(tmp));

    if (withPath) {
        if (!JsonParserUtility::extractValue(dataObj, pathKey, tmp)) {
            return ExitCodeBackError;
        }
        nodeInfo.setPath(QString::fromStdString(tmp));
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getUserAvailableDrives(int userDbId, QHash<int, DriveAvailableInfo> &list) {
    std::shared_ptr<GetDrivesListJob> job = nullptr;
    try {
        job = std::make_shared<GetDrivesListJob>(userDbId);
    } catch (std::exception const &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetDrivesListJob::GetDrivesListJob for userDbId=" << userDbId << " : " << e.what());
        return ExitCodeDataError;
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetDrivesListJob::runSynchronously for userDbId=" << userDbId << " : " << exitCode);
        return exitCode;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        LOG_WARN(Log::instance()->getLogger(), "GetDrivesListJob failed for userDbId=" << userDbId);
        return ExitCodeBackError;
    }

    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(Log::instance()->getLogger(), "GetDrivesListJob failed for userDbId=" << userDbId);
        return ExitCodeBackError;
    }

    list.clear();
    for (size_t i = 0; i < dataArray->size(); i++) {
        Poco::JSON::Object::Ptr obj = dataArray->getObject(static_cast<unsigned int>(i));
        if (!obj) {
            continue;
        }

        int driveId;
        if (!JsonParserUtility::extractValue(obj, driveIdKey, driveId)) {
            return ExitCodeBackError;
        }

        int userId;
        if (!JsonParserUtility::extractValue(obj, idKey, userId)) {
            return ExitCodeBackError;
        }

        int accountId;
        if (!JsonParserUtility::extractValue(obj, accountIdKey, accountId)) {
            return ExitCodeBackError;
        }

        std::string driveName;
        if (!JsonParserUtility::extractValue(obj, driveNameKey, driveName)) {
            return ExitCodeBackError;
        }

        std::string colorHex;
        Poco::JSON::Object::Ptr prefObj = obj->getObject(preferenceKey);
        if (prefObj) {
            if (!JsonParserUtility::extractValue(prefObj, colorKey, colorHex, false)) {
                return ExitCodeBackError;
            }
        }
        DriveAvailableInfo driveInfo(driveId, userId, accountId, QString::fromStdString(driveName),
                                     QString::fromStdString(colorHex));

        // Search user in DB
        User user;
        bool found;
        if (!ParmsDb::instance()->selectUserByUserId(userId, user, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUserByUserId");
            return ExitCodeDbError;
        }
        if (found) {
            driveInfo.setUserDbId(user.dbId());
        }

        list.insert(driveId, driveInfo);
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getSubFolders(int userDbId, int driveId, const QString &nodeId, QList<NodeInfo> &list,
                                       bool withPath /*= false*/) {
    std::shared_ptr<GetRootFileListJob> job = nullptr;
    if (nodeId.isEmpty()) {
        try {
            job = std::make_shared<GetRootFileListJob>(userDbId, driveId, 1, true);
        } catch (std::exception const &e) {
            LOG_WARN(Log::instance()->getLogger(), "Error in GetRootFileListJob::GetRootFileListJob for userDbId="
                                                       << userDbId << " driveId=" << driveId << " : " << e.what());
            return ExitCodeDataError;
        }
    } else {
        try {
            job = (std::shared_ptr<GetFileListJob>)std::make_shared<GetFileListJob>(userDbId, driveId, nodeId.toStdString(), 1,
                                                                                    true);
        } catch (std::exception const &e) {
            LOG_WARN(Log::instance()->getLogger(), "Error in GetFileListJob::GetFileListJob for userDbId="
                                                       << userDbId << " driveId=" << driveId
                                                       << " nodeId=" << nodeId.toStdString().c_str() << " : " << e.what());
            return ExitCodeDataError;
        }
    }

    job->setWithPath(withPath);
    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetFileListJob::runSynchronously for userDbId="
                                                   << userDbId << " driveId=" << driveId
                                                   << " nodeId=" << nodeId.toStdString().c_str() << " : " << exitCode);
        return exitCode;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        LOG_WARN(Log::instance()->getLogger(),
                 "GetFileListJob failed for userDbId=" << userDbId << " driveId=" << driveId
                                                       << " nodeId=" << nodeId.toStdString().c_str());
        return ExitCodeBackError;
    }

    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    if (!dataArray) {
        LOG_WARN(Log::instance()->getLogger(),
                 "GetFileListJob failed for userDbId=" << userDbId << " driveId=" << driveId
                                                       << " nodeId=" << nodeId.toStdString().c_str());
        return ExitCodeBackError;
    }

    list.clear();
    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        Poco::JSON::Object::Ptr dirObj = it->extract<Poco::JSON::Object::Ptr>();
        SyncTime modTime;
        if (!JsonParserUtility::extractValue(dirObj, lastModifiedAtKey, modTime)) {
            return ExitCodeBackError;
        }

        NodeId nodeId;
        if (!JsonParserUtility::extractValue(dirObj, idKey, nodeId)) {
            return ExitCodeBackError;
        }

        SyncName tmp;
        if (!JsonParserUtility::extractValue(dirObj, nameKey, tmp)) {
            return ExitCodeBackError;
        }
        SyncName name = Utility::normalizedSyncName(tmp);

        std::string parentId;
        if (!JsonParserUtility::extractValue(dirObj, parentIdKey, parentId)) {
            return ExitCodeBackError;
        }

        SyncName path;
        if (withPath) {
            if (!JsonParserUtility::extractValue(dirObj, pathKey, tmp)) {
                return ExitCodeBackError;
            }
            path = Utility::normalizedSyncName(tmp);
        }

        NodeInfo nodeInfo(QString::fromStdString(nodeId), SyncName2QStr(name),
                          -1,  // Size is not set here as it can be very long to evaluate
                          parentId.c_str(), modTime, SyncName2QStr(path));
        list << nodeInfo;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getSubFolders(int driveDbId, const QString &nodeId, QList<NodeInfo> &list, bool withPath /*= false*/) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selectDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCodeDataError;
    }

    Account account;
    if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selectAccount");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Account with id=" << drive.accountDbId() << " not found");
        return ExitCodeDataError;
    }

    return getSubFolders(account.userDbId(), drive.driveId(), nodeId, list, withPath);
}

ExitCode ServerRequests::getNodeIdByPath(int userDbId, int driveId, const SyncPath &path, QString &nodeId) {
    // TODO: test
    QList<NodeInfo> list;
    ExitCode exitCode = getSubFolders(userDbId, driveId, QString(), list);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Requests::getSubFolders : " << exitCode);
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
        for (auto node : list) {
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
            if (exitCode != ExitCodeOk) {
                LOG_WARN(Log::instance()->getLogger(), "Error in Requests::getSubFolders : " << exitCode);
                return exitCode;
            }
        }
    }
    nodeId = current.nodeId();
    return ExitCodeOk;
}

ExitCode ServerRequests::getPathByNodeId(int userDbId, int driveId, const QString &nodeId, QString &path) {
    NodeInfo nodeInfo;
    ExitCode exitCode = getNodeInfo(userDbId, driveId, nodeId, nodeInfo, true);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Requests::getNodeInfo : " << exitCode);
        return exitCode;
    }

    path = nodeInfo.path();

    return ExitCodeOk;
}

ExitCode ServerRequests::migrateSelectiveSync(int syncDbId, std::pair<SyncPath, SyncName> &syncToMigrate) {
    Sync sync;
    bool found = false;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error while connecting to ParmsDb");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selecSync id = " << syncDbId << " not found");
        return ExitCodeDataError;
    }

    // contruct file path with folder path and dbName
    SyncPath dbPath(syncToMigrate.first);
    dbPath /= syncToMigrate.second;

    bool exists = false;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(dbPath, exists, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dbPath, ioError).c_str());
        return ExitCodeSystemError;
    }

    if (ioError == IoErrorAccessDenied) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"DB to migrate " << Path2WStr(dbPath).c_str() << L" misses search permission.");
        return ExitCodeSystemError;
    }

    if (!exists) {
        LOGW_DEBUG(Log::instance()->getLogger(), L"DB to migrate " << Path2WStr(dbPath).c_str() << L" does not exist.");
        return ExitCodeSystemError;
    }


    ExitCode code;
    QList<QPair<QString, int>> list;
    code = loadOldSelectiveSyncTable(dbPath, list);

    for (auto &pair : list) {
        std::filesystem::path path(QStr2Path(pair.first));
        int type = pair.second;
        if (!ParmsDb::instance()->insertMigrationSelectiveSync(MigrationSelectiveSync(syncDbId, path, type))) {
            return ExitCodeDbError;
        }
    }

    return code;
}

ExitCode ServerRequests::createUser(const User &user, UserInfo &userInfo) {
    if (!ParmsDb::instance()->insertUser(user)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertUser");
        return ExitCodeDbError;
    }

    // Load User info
    User userUpdated(user);
    bool updated;
    ExitCode exitCode = loadUserInfo(userUpdated, updated);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in loadUserInfo");
        return exitCode;
    }

    if (updated) {
        bool found;
        if (!ParmsDb::instance()->updateUser(userUpdated, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateUser");
            return ExitCodeDbError;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "User not found for userDbId=" << userUpdated.dbId());
            return ExitCodeDataError;
        }
    }

    userToUserInfo(userUpdated, userInfo);

    return ExitCodeOk;
}

ExitCode ServerRequests::updateUser(const User &user, UserInfo &userInfo) {
    bool found;
    if (!ParmsDb::instance()->updateUser(user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateUser");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User not found for userDbId=" << user.dbId());
        return ExitCodeDataError;
    }

    // Load User info
    User userUpdated(user);
    bool updated;
    ExitCode exitCode = loadUserInfo(userUpdated, updated);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in loadUserInfo");
        return exitCode;
    }

    userToUserInfo(userUpdated, userInfo);

    return ExitCodeOk;
}

ExitCode ServerRequests::createAccount(const Account &account, AccountInfo &accountInfo) {
    if (!ParmsDb::instance()->insertAccount(account)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertAccount");
        return ExitCodeDbError;
    }

    accountToAccountInfo(account, accountInfo);

    return ExitCodeOk;
}

ExitCode ServerRequests::createDrive(const Drive &drive, DriveInfo &driveInfo) {
    if (!ParmsDb::instance()->insertDrive(drive)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertDrive");
        return ExitCodeDbError;
    }

    // Load Drive info
    Drive driveUpdated(drive);
    Account account;
    bool updated;
    bool accountUpdated;
    bool quotaUpdated;
    ExitCode exitCode = loadDriveInfo(driveUpdated, account, updated, quotaUpdated, accountUpdated);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in User::loadDriveInfo");
        return exitCode;
    }

    if (updated) {
        bool found;
        if (!ParmsDb::instance()->updateDrive(driveUpdated, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateDrive");
            return ExitCodeDbError;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "Drive not found for driveDbId=" << driveUpdated.dbId());
            return ExitCodeDataError;
        }
    }

    driveToDriveInfo(driveUpdated, driveInfo);

    return ExitCodeOk;
}

ExitCode ServerRequests::createSync(const Sync &sync, SyncInfo &syncInfo) {
    if (!ParmsDb::instance()->insertSync(sync)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertSync");
        return ExitCodeDbError;
    }

    syncToSyncInfo(sync, syncInfo);

    return ExitCodeOk;
}

bool ServerRequests::isDisplayableError(const Error &error) {
    switch (error.exitCode()) {
        case ExitCodeUpdateRequired:
            return true;
        case ExitCodeNetworkError: {
            switch (error.exitCause()) {
                // case ExitCauseNetworkTimeout:
                case ExitCauseSocketsDefuncted:
                    return true;
                default:
                    return false;
            }
        }
        case ExitCodeLogicError: {
            return true;
        }
        case ExitCodeDataError: {
            switch (error.exitCause()) {
                case ExitCauseMigrationError:
                case ExitCauseMigrationProxyNotImplemented:
                    return true;
                default:
                    return false;
            }
        }
        case ExitCodeBackError: {
            switch (error.exitCause()) {
                case ExitCauseDriveMaintenance:
                case ExitCauseDriveNotRenew:
                case ExitCauseDriveAccessError:
                case ExitCauseHttpErrForbidden:
                case ExitCauseApiErr:
                case ExitCauseFileTooBig:
                case ExitCauseNotFound:
                case ExitCauseQuotaExceeded:
                    return true;
                default:
                    return false;
            }
        }
        case ExitCodeUnknown: {
            return error.inconsistencyType() != InconsistencyTypePathLength && error.cancelType() != CancelTypeAlreadyExistRemote;
        }
        default:
            return true;
    }
}

bool ServerRequests::isAutoResolvedError(const Error &error) {
    bool autoResolved = false;
    if (error.level() == ErrorLevelServer) {
        autoResolved = false;
    } else if (error.level() == ErrorLevelSyncPal) {
        autoResolved =
            (error.exitCode() == ExitCodeNetworkError   // Sync is paused and we try to restart it every RESTART_SYNCS_INTERVAL
             || (error.exitCode() == ExitCodeBackError  // Sync is stoped and a full sync is restarted
                 && error.exitCause() != ExitCauseDriveAccessError && error.exitCause() != ExitCauseDriveNotRenew) ||
             error.exitCode() == ExitCodeDataError);  // Sync is stoped and a full sync is restarted
    } else if (error.level() == ErrorLevelNode) {
        autoResolved = (error.conflictType() != ConflictTypeNone && !isConflictsWithLocalRename(error.conflictType())) ||
                       (error.inconsistencyType() !=
                        InconsistencyTypeNone /*&& error.inconsistencyType() != InconsistencyTypeForbiddenChar*/) ||
                       error.cancelType() != CancelTypeNone;
    }

    return autoResolved;
}

ExitCode ServerRequests::getUserFromSyncDbId(int syncDbId, User &user) {
    // Get User
    bool found;
    Sync sync;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found with dbId=" << syncDbId);
        return ExitCodeDataError;
    }

    Drive drive;
    if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found with dbId=" << sync.driveDbId());
        return ExitCodeDataError;
    }

    Account acc;
    if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), acc, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAccount");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Account not found with dbId=" << drive.accountDbId());
        return ExitCodeDataError;
    }

    if (!ParmsDb::instance()->selectUser(acc.userDbId(), user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUser");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User not found with dbId=" << acc.userDbId());
        return ExitCodeDataError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::sendLogToSupport(bool includeArchivedLog,
                                          const std::function<bool(LogUploadState, int)> &progressCallback,
                                          ExitCause &exitCause) {
    exitCause = ExitCauseUnknown;
    ExitCode exitCode = ExitCodeOk;
    std::function<bool(LogUploadState, int)> safeProgressCallback =
        progressCallback ? progressCallback : std::function<bool(LogUploadState, int)>([](LogUploadState, int) { return true; });

    safeProgressCallback(LogUploadState::Archiving, 0);

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, std::string(""), found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        // Do not return here because it is not a critical error, especially in this context where we are trying to send logs
    }

    SyncPath logUploadTempFolder;
    IoError ioError = IoErrorSuccess;

    IoHelper::logArchiverDirectoryPath(logUploadTempFolder, ioError);
    if (ioError != IoErrorSuccess) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::logArchiverDirectoryPath: "
                                                    << Utility::formatIoError(logUploadTempFolder, ioError).c_str());
        return ExitCodeSystemError;
    }

    IoHelper::createDirectory(logUploadTempFolder, ioError);
    if (ioError == IoErrorDirectoryExists) {  // If the directory already exists, we delete it and recreate it
        IoHelper::deleteDirectory(logUploadTempFolder, ioError);
        IoHelper::createDirectory(logUploadTempFolder, ioError);
    }

    if (ioError != IoErrorSuccess) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::createDirectory: " << Utility::formatIoError(logUploadTempFolder, ioError).c_str());
        exitCause = ioError == IoErrorDiskFull ? ExitCauseNotEnoughDiskSpace : ExitCauseUnknown;
        return ExitCodeSystemError;
    }

    std::function<bool(int)> progressCallbackArchivingWrapper = [&safeProgressCallback](int percent) {
        return safeProgressCallback(LogUploadState::Archiving, percent);
    };

    SyncPath archivePath;
    exitCode = LogArchiver::generateLogsSupportArchive(includeArchivedLog, logUploadTempFolder, progressCallbackArchivingWrapper,
                                                       archivePath, exitCause);
    if (exitCause == ExitCauseOperationCanceled) {
        IoHelper::deleteDirectory(logUploadTempFolder, ioError);
        LOG_INFO(Log::instance()->getLogger(),
                 "LogArchiver::generateLogsSupportArchive canceled: " << exitCode << " : " << exitCause);
        return ExitCodeOk;
    } else if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in LogArchiver::generateLogsSupportArchive: " << exitCode << " : " << exitCause);
        IoHelper::deleteDirectory(logUploadTempFolder, ioError);
        return exitCode;
    }

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, archivePath.string(), found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
    }

    // Upload archive
    std::shared_ptr<LogUploadSession> uploadSessionLog = nullptr;
    try {
        uploadSessionLog = std::make_shared<LogUploadSession>(archivePath);
    } catch (std::exception const &e) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in LogUploadSession::LogUploadSession: " << Utility::s2ws(e.what()).c_str());
        return ExitCodeSystemError;
    };

    bool canceledByUser = false;
    std::function<void(UniqueId, int percent)> progressCallbackUploadingWrapper =
        [&safeProgressCallback, &uploadSessionLog, &canceledByUser](UniqueId, int percent) {  // Progress callback
            if (!safeProgressCallback(LogUploadState::Uploading, percent)) {
                uploadSessionLog->abort();
                canceledByUser = true;
            };
        };
    uploadSessionLog->setProgressPercentCallback(progressCallbackUploadingWrapper);

    bool jobFinished = false;
    std::function<void(uint64_t)> uploadSessionLogFinisCallback = [&safeProgressCallback, &jobFinished](uint64_t) {
        safeProgressCallback(LogUploadState::Uploading, 100);
        jobFinished = true;
    };
    uploadSessionLog->setAdditionalCallback(uploadSessionLogFinisCallback);

    JobManager::instance()->queueAsyncJob(uploadSessionLog, Poco::Thread::PRIO_NORMAL);

    while (!jobFinished) {
        Utility::msleep(100);
    }

    exitCode = uploadSessionLog->exitCode();
    if (canceledByUser) {
        exitCause = ExitCauseOperationCanceled;
    } else {
        exitCause = uploadSessionLog->exitCause();
    }

    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error during log upload: " << exitCode << " : " << exitCause);
        // We do not delete the archive here. The path is stored in the app state so that the user can still try to upload it
        // manually.
        return exitCode;
    }

    IoHelper::deleteDirectory(logUploadTempFolder, ioError);  // Delete temp folder if the upload was successful

    if (exitCause != ExitCauseOperationCanceled) {
        std::string uploadDate = "";
        const std::time_t now = std::time(nullptr);
        const std::tm tm = *std::localtime(&now);
        std::ostringstream woss;
        woss << std::put_time(&tm, "%m,%d,%y,%H,%M,%S");
        uploadDate = woss.str();

        if (bool found = false;
            !ParmsDb::instance()->updateAppState(AppStateKey::LastSuccessfulLogUploadDate, uploadDate, found) || !found ||
            !ParmsDb::instance()->updateAppState(AppStateKey::LastLogUploadArchivePath, std::string{}, found) || !found) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        }
    }
    return ExitCodeOk;
}

ExitCode ServerRequests::cancelLogToSupport(ExitCause &exitCause) {
    exitCause = ExitCauseUnknown;
    AppStateValue appStateValue = LogUploadState::None;
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadState, appStateValue, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getAppState");
        return ExitCodeDbError;
    }
    LogUploadState logUploadState = std::get<LogUploadState>(appStateValue);

    if (logUploadState == LogUploadState::CancelRequested) {
        return ExitCodeOk;
    }

    if (logUploadState == LogUploadState::Canceled) {
        exitCause = ExitCauseOperationCanceled;
        return ExitCodeOk;  // The user has already canceled the operation
    }

    if (logUploadState != LogUploadState::Uploading && logUploadState != LogUploadState::Archiving) {
        return ExitCodeInvalidOperation;  // The operation is not in progress
    }

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::CancelRequested, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        return ExitCodeDbError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::createDir(int driveDbId, const QString &parentNodeId, const QString &dirName, QString &newNodeId) {
    // Get drive data
    std::shared_ptr<CreateDirJob> job = nullptr;
    try {
        job =
            std::make_shared<CreateDirJob>(driveDbId, QStr2SyncName(dirName), parentNodeId.toStdString(), QStr2SyncName(dirName));
    } catch (std::exception const &) {
        LOG_WARN(Log::instance()->getLogger(), "Error in CreateDirJob::CreateDirJob for driveDbId=" << driveDbId);
        return ExitCodeDataError;
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in CreateDirJob::runSynchronously for driveDbId=" << driveDbId);
        return exitCode;
    }

    // Extract file ID
    if (job->jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = job->jsonRes()->getObject(dataKey);
        if (dataObj) {
            std::string tmp;
            if (!JsonParserUtility::extractValue(dataObj, idKey, tmp)) {
                return ExitCodeBackError;
            }
            newNodeId = QString::fromStdString(tmp);
        }
    }

    if (newNodeId.isEmpty()) {
        return ExitCodeBackError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getPublicLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl) {
    NodeId nodeId = fileId.toStdString();

    // Create link
    std::shared_ptr<PostFileLinkJob> job;
    try {
        job = std::make_shared<PostFileLinkJob>(driveDbId, nodeId);
    } catch (std::exception const &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in PostFileLinkJob::PostFileLinkJob for driveDbId="
                                                   << driveDbId << " nodeId=" << nodeId.c_str() << " : " << e.what());
        return ExitCodeDataError;
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        std::string errorCode;
        if (job->hasErrorApi(&errorCode) && getNetworkErrorCode(errorCode) == NetworkErrorCode::fileShareLinkAlreadyExists) {
            // Get link
            std::shared_ptr<GetFileLinkJob> job2;
            try {
                job2 = std::make_shared<GetFileLinkJob>(driveDbId, nodeId);
            } catch (std::exception const &e) {
                LOG_WARN(Log::instance()->getLogger(), "Error in GetFileLinkJob::GetFileLinkJob for driveDbId="
                                                           << driveDbId << " nodeId=" << nodeId.c_str() << " : " << e.what());
                return ExitCodeDataError;
            }

            exitCode = job2->runSynchronously();
            if (exitCode != ExitCodeOk) {
                LOG_WARN(Log::instance()->getLogger(), "Error in GetFileLinkJob::GetFileLinkJob for driveDbId="
                                                           << driveDbId << " nodeId=" << nodeId.c_str() << " : " << exitCode);
                return exitCode;
            }

            Poco::JSON::Object::Ptr resObj = job2->jsonRes();
            if (!resObj) {
                LOG_WARN(Log::instance()->getLogger(),
                         "GetFileLinkJob failed for driveDbId=" << driveDbId << " nodeId=" << nodeId.c_str());
                return ExitCodeBackError;
            }

            Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
            if (!dataObj) {
                LOG_WARN(Log::instance()->getLogger(),
                         "GetFileLinkJob failed for driveDbId=" << driveDbId << " nodeId=" << nodeId.c_str());
                return ExitCodeBackError;
            }

            std::string tmp;
            if (!JsonParserUtility::extractValue(dataObj, urlKey, tmp)) {
                return ExitCodeBackError;
            }
            linkUrl = QString::fromStdString(tmp);

            return ExitCodeOk;
        } else {
            LOG_WARN(Log::instance()->getLogger(), "Error in PostFileLinkJob::PostFileLinkJob for driveDbId="
                                                       << driveDbId << " nodeId=" << nodeId.c_str() << " : " << exitCode);
            return exitCode;
        }
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        LOG_WARN(Log::instance()->getLogger(),
                 "PostFileLinkJob failed for driveDbId=" << driveDbId << " nodeId=" << nodeId.c_str());
        return ExitCodeBackError;
    }

    Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
    if (!dataObj) {
        LOG_WARN(Log::instance()->getLogger(),
                 "PostFileLinkJob failed for driveDbId=" << driveDbId << " nodeId=" << nodeId.c_str());
        return ExitCodeBackError;
    }

    std::string tmp;
    if (!JsonParserUtility::extractValue(dataObj, urlKey, tmp)) {
        return ExitCodeBackError;
    }
    linkUrl = QString::fromStdString(tmp);

    return ExitCodeOk;
}

ExitCode ServerRequests::getFolderSize(int userDbId, int driveId, const NodeId &nodeId,
                                       std::function<void(const QString &, qint64)> callback) {
    if (nodeId.empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Node ID is empty");
        return ExitCodeDataError;
    }

    // get size of folder
    std::shared_ptr<GetSizeJob> job = nullptr;
    try {
        job = std::make_shared<GetSizeJob>(userDbId, driveId, nodeId);
    } catch (std::exception const &e) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetSizeJob::GetSizeJob for userDbId=" << userDbId << " driveId=" << driveId
                                                                 << " nodeId=" << nodeId.c_str() << " : " << e.what());
        return ExitCodeDataError;
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetSizeJob::runSynchronously for userDbId=" << userDbId << " driveId=" << driveId
                                                                       << " nodeId=" << nodeId.c_str() << " : " << exitCode);
        return exitCode;
    }

    Poco::JSON::Object::Ptr resObj = job->jsonRes();
    if (!resObj) {
        // Level = Debug because access forbidden is a normal case
        LOG_DEBUG(Log::instance()->getLogger(),
                  "GetSizeJob failed for userDbId=" << userDbId << " driveId=" << driveId << " nodeId=" << nodeId.c_str());
        return ExitCodeBackError;
    }

    Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
    if (!dataObj) {
        LOG_WARN(Log::instance()->getLogger(),
                 "GetSizeJob failed for userDbId=" << userDbId << " driveId=" << driveId << " nodeId=" << nodeId.c_str());
        return ExitCodeBackError;
    }

    qint64 size = 0;
    if (!JsonParserUtility::extractValue(dataObj, sizeKey, size)) {
        return ExitCodeBackError;
    }

    callback(QString::fromStdString(nodeId), size);

    return ExitCodeOk;
}

ExitCode ServerRequests::getPrivateLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl) {
    Drive drive;
    bool found;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in selectDrive");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive with id=" << driveDbId << " not found");
        return ExitCodeDataError;
    }

    linkUrl = QString(APPLICATION_PREVIEW_URL).arg(drive.driveId()).arg(fileId);

    return ExitCodeOk;
}

ExitCode ServerRequests::getExclusionTemplateList(bool def, QList<ExclusionTemplateInfo> &list) {
    list.clear();
    for (const ExclusionTemplate &exclusionTemplate : ExclusionTemplateCache::instance()->exclusionTemplates(def)) {
        ExclusionTemplateInfo exclusionTemplateInfo;
        ServerRequests::exclusionTemplateToExclusionTemplateInfo(exclusionTemplate, exclusionTemplateInfo);
        list << exclusionTemplateInfo;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::setExclusionTemplateList(bool def, const QList<ExclusionTemplateInfo> &list) {
    std::vector<ExclusionTemplate> exclusionList;
    for (const ExclusionTemplateInfo &exclusionTemplateInfo : list) {
        ExclusionTemplate exclusionTemplate;
        ServerRequests::exclusionTemplateInfoToExclusionTemplate(exclusionTemplateInfo, exclusionTemplate);
        exclusionList.push_back(exclusionTemplate);
    }
    ExitCode exitCode = ExclusionTemplateCache::instance()->update(def, exclusionList);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ExclusionTemplateCache::save");
        return exitCode;
    }

    return ExitCodeOk;
}

#ifdef __APPLE__
ExitCode ServerRequests::getExclusionAppList(bool def, QList<ExclusionAppInfo> &list) {
    std::vector<ExclusionApp> exclusionList;
    if (!ParmsDb::instance()->selectAllExclusionApps(def, exclusionList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllExclusionApps");
        return ExitCodeDbError;
    }
    list.clear();
    for (const ExclusionApp &exclusionApp : exclusionList) {
        ExclusionAppInfo exclusionAppInfo;
        ServerRequests::exclusionAppToExclusionAppInfo(exclusionApp, exclusionAppInfo);
        list << exclusionAppInfo;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::setExclusionAppList(bool def, const QList<ExclusionAppInfo> &list) {
    std::vector<ExclusionApp> exclusionList;
    for (const ExclusionAppInfo &exclusionAppInfo : list) {
        ExclusionApp exclusionApp;
        ServerRequests::exclusionAppInfoToExclusionApp(exclusionAppInfo, exclusionApp);
        exclusionList.push_back(exclusionApp);
    }
    if (!ParmsDb::instance()->updateAllExclusionApps(def, exclusionList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAllExclusionApps");
        return ExitCodeDbError;
    }

    return ExitCodeOk;
}
#endif

ExitCode ServerRequests::getErrorInfoList(ErrorLevel level, int syncDbId, int limit, QList<ErrorInfo> &list) {
    std::vector<Error> errorList;
    if (!ParmsDb::instance()->selectAllErrors(level, syncDbId, limit, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return ExitCodeDbError;
    }

    list.clear();
    for (const Error &error : errorList) {
        if (isDisplayableError(error)) {
            ErrorInfo errorInfo;
            errorToErrorInfo(error, errorInfo);
            list << errorInfo;
        }
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getConflictList(int syncDbId, const std::unordered_set<ConflictType> &filter,
                                         std::vector<Error> &errorList) {
    if (filter.empty()) {
        if (!ParmsDb::instance()->selectConflicts(syncDbId, ConflictTypeNone, errorList)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
            return ExitCodeDbError;
        }
    } else {
        for (auto conflictType : filter) {
            if (!ParmsDb::instance()->selectConflicts(syncDbId, conflictType, errorList)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
                return ExitCodeDbError;
            }
        }
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getConflictErrorInfoList(int syncDbId, const std::unordered_set<ConflictType> &filter,
                                                  QList<ErrorInfo> &errorInfoList) {
    std::vector<Error> errorList;
    ServerRequests::getConflictList(syncDbId, filter, errorList);

    for (const Error &error : errorList) {
        if (isDisplayableError(error)) {
            ErrorInfo errorInfo;
            errorToErrorInfo(error, errorInfo);
            errorInfoList << errorInfo;
        }
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::deleteErrorsServer() {
    if (!ParmsDb::instance()->deleteErrors(ErrorLevelServer)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteErrors");
        return ExitCodeDbError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::deleteErrorsForSync(int syncDbId, bool autoResolved) {
    std::vector<Error> errorList;
    if (!ParmsDb::instance()->selectAllErrors(ErrorLevelSyncPal, syncDbId, INT_MAX, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return ExitCodeDbError;
    }

    if (!ParmsDb::instance()->selectAllErrors(ErrorLevelNode, syncDbId, INT_MAX, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return ExitCodeDbError;
    }

    for (const Error &error : errorList) {
        if (isConflictsWithLocalRename(error.conflictType())) {
            // For conflict type that rename local file
            Sync sync;
            bool found = false;
            if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_WARN(Log::instance()->getLogger(), "Sync with id=" << syncDbId << " not found");
                return ExitCodeDataError;
            }

            IoError ioError = IoErrorSuccess;
            const SyncPath dest = sync.localPath() / error.destinationPath();
            const bool success = IoHelper::checkIfPathExists(dest, found, ioError);
            if (!success) {
                LOGW_WARN(Log::instance()->getLogger(),
                          "Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(dest, ioError).c_str());
                return ExitCodeSystemError;
            }

            // If conflict file still exists, keep the error.
            if (found || ioError != IoErrorNoSuchFileOrDirectory) {
                continue;
            }
        }

        if (isAutoResolvedError(error) == autoResolved) {
            bool found = false;
            if (!ParmsDb::instance()->deleteError(error.dbId(), found)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteError for dbId=" << error.dbId());
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_WARN(Log::instance()->getLogger(), "Error not found for dbId=" << error.dbId());
                return ExitCodeDataError;
            }
        }
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::deleteInvalidTokenErrors() {
    if (!ParmsDb::instance()->deleteAllErrorsByExitCode(ExitCodeInvalidToken)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteAllErrorsByExitCode");
        return ExitCodeDbError;
    }

    return ExitCodeOk;
}

#ifdef Q_OS_MAC
ExitCode ServerRequests::deleteLiteSyncNotAllowedErrors() {
    if (!ParmsDb::instance()->deleteAllErrorsByExitCause(ExitCauseLiteSyncNotAllowed)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteAllErrorsByExitCause");
        return ExitCodeDbError;
    }

    return ExitCodeOk;
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
        return ExitCodeDbError;
    }

    if (!accountDbId) {
        if (!ParmsDb::instance()->getNewAccountDbId(accountDbId)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewAccountDbId");
            return ExitCodeDbError;
        }

        Account account;
        account.setDbId(accountDbId);
        account.setAccountId(accountId);
        account.setUserDbId(userDbId);
        exitCode = createAccount(account, accountInfo);
        if (exitCode != ExitCodeOk) {
            LOG_WARN(Log::instance()->getLogger(), "Error in createAccount");
            return exitCode;
        }

        LOG_INFO(Log::instance()->getLogger(), "New account created in DB - accountDbId="
                                                   << accountDbId << " accountId= " << accountId << " userDbId= " << userDbId);
    }

    // Create Drive in DB if needed
    int driveDbId;
    if (!ParmsDb::instance()->driveDbId(accountDbId, driveId, driveDbId)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::driveDbId");
        return ExitCodeDbError;
    }

    if (!driveDbId) {
        if (!ParmsDb::instance()->getNewDriveDbId(driveDbId)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewDriveDbId");
            return ExitCodeDbError;
        }

        Drive drive;
        drive.setDbId(driveDbId);
        drive.setDriveId(driveId);
        drive.setAccountDbId(accountDbId);
        exitCode = createDrive(drive, driveInfo);
        if (exitCode != ExitCodeOk) {
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
                                                << driveDbId << L" localFolderPath="
                                                << Path2WStr(QStr2Path(localFolderPath)).c_str() << L" serverFolderPath="
                                                << Path2WStr(QStr2Path(serverFolderPath)).c_str() << L" liteSync=" << liteSync);

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
        return ExitCodeDbError;
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
    sync.setLocalPath(QStr2Path(localFolderPath));
    sync.setTargetPath(QStr2Path(serverFolderPath));
    sync.setTargetNodeId(serverFolderNodeId.toStdString());
    sync.setPaused(true);

    // Check vfs support
    QString fsName(KDC::CommonUtility::fileSystemName(SyncName2QStr(sync.localPath().native())));
    bool supportVfs = (fsName == "NTFS" || fsName == "apfs");
    sync.setSupportVfs(supportVfs);

#if defined(Q_OS_MAC)
    sync.setVirtualFileMode(liteSync ? VirtualFileModeMac : VirtualFileModeOff);
#elif defined(Q_OS_WIN32)
    sync.setVirtualFileMode(liteSync ? VirtualFileModeWin : VirtualFileModeOff);
#else
    sync.setVirtualFileMode(VirtualFileModeOff);
#endif

    sync.setNotificationsDisabled(false);
    sync.setDbPath(std::filesystem::path());
    sync.setHasFullyCompleted(false);
    sync.setNavigationPaneClsid(navigationPaneClsid.toString().toStdString());
    exitCode = createSync(sync, syncInfo);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in createSync");
        return exitCode;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"New sync created in DB - syncDbId="
                                                << syncDbId << L" driveDbId=" << driveDbId << L" localFolderPath="
                                                << Path2WStr(sync.localPath()).c_str() << L" serverFolderPath="
                                                << Path2WStr(sync.targetPath()).c_str() << L" dbPath="
                                                << Path2WStr(sync.dbPath()).c_str());

    return ExitCodeOk;
}

ExitCode ServerRequests::loadDriveInfo(Drive &drive, Account &account, bool &updated, bool &quotaUpdated, bool &accountUpdated) {
    updated = false;
    accountUpdated = false;
    quotaUpdated = false;

    // Get drive data
    std::shared_ptr<GetInfoDriveJob> job = nullptr;
    try {
        job = std::make_shared<GetInfoDriveJob>(drive.dbId());
    } catch (std::exception const &) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetInfoDriveJob::GetInfoDriveJob for driveDbId=" << drive.dbId());
        return ExitCodeDataError;
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk && job->exitCause() != ExitCauseDriveNotRenew) {
        LOG_WARN(Log::instance()->getLogger(), "Error in GetInfoDriveJob::runSynchronously for driveDbId=" << drive.dbId());
        return exitCode;
    }

    Poco::Net::HTTPResponse::HTTPStatus httpStatus = job->getStatusCode();
    if (httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN ||
        httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to get drive info for driveDbId=" << drive.dbId());
        drive.setAccessDenied(true);
        return ExitCodeOk;
    } else if (httpStatus != Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Network error in GetInfoDriveJob::runSynchronously for driveDbId=" << drive.dbId());
        return ExitCodeNetworkError;
    }

    Poco::JSON::Object::Ptr dataObj = job->jsonRes()->getObject(dataKey);
    if (dataObj && dataObj->size() != 0) {
        std::string name;
        if (!JsonParserUtility::extractValue(dataObj, nameKey, name)) {
            return ExitCodeBackError;
        }
        if (drive.name() != name) {
            drive.setName(name);
            updated = true;
        }

        int64_t size = 0;
        if (!JsonParserUtility::extractValue(dataObj, sizeKey, size)) {
            return ExitCodeBackError;
        }
        if (drive.size() != size) {
            drive.setSize(size);
            updated = true;
            quotaUpdated = true;
        }

        bool admin = false;
        if (!JsonParserUtility::extractValue(dataObj, accountAdminKey, admin)) {
            return ExitCodeBackError;
        }
        if (drive.admin() != admin) {
            drive.setAdmin(admin);
            updated = true;
        }

        int accountId = 0;
        if (!JsonParserUtility::extractValue(dataObj, accountIdKey, accountId)) {
            return ExitCodeBackError;
        }
        if (account.accountId() != accountId) {
            account.setAccountId(accountId);
            accountUpdated = true;
        }

        // Non DB attributes
        bool inMaintenance = false;
        if (!JsonParserUtility::extractValue(dataObj, inMaintenanceKey, inMaintenance)) {
            return ExitCodeBackError;
        }
        drive.setMaintenance(inMaintenance);

        int64_t maintenanceFrom = 0;
        if (drive.maintenance()) {
            if (!JsonParserUtility::extractValue(dataObj, maintenanceAtKey, maintenanceFrom, false)) {
                return ExitCodeBackError;
            }
        }
        drive.setMaintenanceFrom(maintenanceFrom);

        bool isLocked = false;
        if (!JsonParserUtility::extractValue(dataObj, isLockedKey, isLocked)) {
            return ExitCodeBackError;
        }
        drive.setLocked(isLocked);
        drive.setAccessDenied(false);
        drive.setNotRenew(job->exitCause() == ExitCauseDriveNotRenew);

        int64_t usedSize = 0;
        if (!JsonParserUtility::extractValue(dataObj, usedSizeKey, usedSize)) {
            return ExitCodeBackError;
        }
        if (drive.usedSize() != usedSize) {
            drive.setUsedSize(usedSize);
            quotaUpdated = true;
        }
    } else {
        LOG_WARN(Log::instance()->getLogger(), "Unable to read drive info for driveDbId=" << drive.dbId());
        return ExitCodeDataError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::getThumbnail(int driveDbId, NodeId nodeId, int width, std::string &thumbnail) {
    std::shared_ptr<GetThumbnailJob> job = nullptr;
    try {
        job = std::make_shared<GetThumbnailJob>(driveDbId, nodeId, width);
    } catch (std::exception const &) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetThumbnailJob::GetThumbnailJob for driveDbId=" << driveDbId << " and nodeId=" << nodeId.c_str());
        return ExitCodeDataError;
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        return exitCode;
    }

    Poco::Net::HTTPResponse::HTTPStatus httpStatus = job->getStatusCode();
    if (httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN ||
        httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Unable to get thumbnail for driveDbId=" << driveDbId << " and nodeId=" << nodeId.c_str());
        return ExitCodeDataError;
    } else if (httpStatus != Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK) {
        LOG_WARN(Log::instance()->getLogger(), "Network error in GetThumbnailJob::runSynchronously for driveDbId="
                                                   << driveDbId << " and nodeId=" << nodeId.c_str());
        return ExitCodeNetworkError;
    }

    thumbnail = job->octetStreamRes();
    return ExitCodeOk;
}

ExitCode ServerRequests::loadUserInfo(User &user, bool &updated) {
    updated = false;

    // Get user data
    std::shared_ptr<GetInfoUserJob> job = nullptr;
    try {
        job = std::make_shared<GetInfoUserJob>(user.dbId());
    } catch (std::exception const &e) {
        std::string what = e.what();
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in GetInfoUserJob::GetInfoUserJob for userDbId=" << user.dbId() << " what = " << what.c_str());
        if (what == invalidToken) {
            return ExitCodeInvalidToken;
        }
        return ExitCodeDataError;
    }

    ExitCode exitCode = job->runSynchronously();
    if (exitCode != ExitCodeOk) {
        if (exitCode == ExitCodeInvalidToken) {
            user.setKeychainKey("");  // Invalid keychain key
        }
        return exitCode;
    }

    Poco::Net::HTTPResponse::HTTPStatus httpStatus = job->getStatusCode();
    if (httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_FORBIDDEN ||
        httpStatus == Poco::Net::HTTPResponse::HTTPStatus::HTTP_NOT_FOUND) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to get user info for userId=" << user.userId());
        return ExitCodeDataError;
    } else if (httpStatus != Poco::Net::HTTPResponse::HTTPStatus::HTTP_OK) {
        LOG_WARN(Log::instance()->getLogger(), "Network error in GetInfoUserJob::runSynchronously for userId=" << user.userId());
        return ExitCodeNetworkError;
    }

    Poco::JSON::Object::Ptr dataObj = job->jsonRes()->getObject(dataKey);
    if (dataObj && dataObj->size() != 0) {
        std::string name;
        if (!JsonParserUtility::extractValue(dataObj, displayNameKey, name)) {
            return ExitCodeBackError;
        }
        if (user.name() != name) {
            user.setName(name);
            updated = true;
        }

        std::string email;
        if (!JsonParserUtility::extractValue(dataObj, emailKey, email)) {
            return ExitCodeBackError;
        }
        if (user.email() != email) {
            user.setEmail(email);
            updated = true;
        }

        std::string avatarUrl;
        if (!JsonParserUtility::extractValue(dataObj, avatarKey, avatarUrl)) {
            return ExitCodeBackError;
        }
        if (user.avatarUrl() != avatarUrl) {
            if (avatarUrl.empty()) {
                user.setAvatar(nullptr);
                user.setAvatarUrl(std::string());
            } else if (user.avatarUrl() != avatarUrl) {
                // get avatarData
                user.setAvatarUrl(avatarUrl);
                exitCode = loadUserAvatar(user);
                if (exitCode != ExitCodeOk) {
                    return exitCode;
                }
            }
            updated = true;
        }
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::loadUserAvatar(User &user) {
    GetAvatarJob getAvatarJob = GetAvatarJob(user.avatarUrl());
    ExitCode exitCode = getAvatarJob.runSynchronously();
    if (exitCode != ExitCodeOk) {
        return exitCode;
    }

    user.setAvatar(getAvatarJob.avatar());
    return ExitCodeOk;
}

ExitCode ServerRequests::processRequestTokenFinished(const Login &login, UserInfo &userInfo, bool &userCreated) {
    // Get user
    User user;
    bool found;
    if (!ParmsDb::instance()->selectUserByUserId(login.apiToken().userId(), user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUserByUserId");
        return ExitCodeDbError;
    }
    if (found) {
        // Update User in DB
        user.setKeychainKey(login.keychainKey());

        AbstractTokenNetworkJob::updateLoginByUserDbId(login, user.dbId());

        ExitCode exitCode = updateUser(user, userInfo);
        if (exitCode != ExitCodeOk) {
            LOG_WARN(Log::instance()->getLogger(), "Error in updateUser");
            return exitCode;
        }

        userCreated = false;
    } else {
        // Create User in DB
        int dbId;
        if (!ParmsDb::instance()->getNewUserDbId(dbId)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewUserDbId");
            return ExitCodeDbError;
        }

        user.setDbId(dbId);
        user.setUserId(login.apiToken().userId());
        user.setKeychainKey(login.keychainKey());
        ExitCode exitCode = createUser(user, userInfo);
        if (exitCode != ExitCodeOk) {
            LOG_WARN(Log::instance()->getLogger(), "Error in createUser");
            return exitCode;
        }

        userCreated = true;
    }

    return ExitCodeOk;
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
        return ExitCodeSystemError;
    }

    QFileInfo selFile(path);

    if (!selFile.exists()) {
        QString parentPath = selFile.dir().path();
        if (parentPath != path) {
            return checkPathValidityRecursive(parentPath, error);
        }
        error = QObject::tr("The selected path does not exist!");
        return ExitCodeSystemError;
    }

    if (!selFile.isDir()) {
        error = QObject::tr("The selected path is not a folder!");
        return ExitCodeSystemError;
    }

    if (!selFile.isWritable()) {
        error = QObject::tr("You have no permission to write to the selected folder!");
        return ExitCodeSystemError;
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::checkPathValidityForNewFolder(const std::vector<Sync> &syncList, int driveDbId, const QString &path,
                                                       QString &error) {
    ExitCode exitCode = checkPathValidityRecursive(path, error);
    if (exitCode != ExitCodeOk) {
        LOG_WARN(Log::instance()->getLogger(), "Error in checkPathValidityRecursive : " << exitCode);
        return exitCode;
    }

    // check if the local directory isn't used yet in another sync
    Qt::CaseSensitivity cs = Qt::CaseSensitive;

    const QString userDir = QDir::cleanPath(canonicalPath(path)) + '/';

    QList<QPair<std::filesystem::path, int>> folderByDriveList;
    for (const Sync &sync : syncList) {
        folderByDriveList << qMakePair(sync.localPath(), sync.driveDbId());
    }

    for (QPair<std::filesystem::path, int> folderByDrive : folderByDriveList) {
        QString folderDir = QDir::cleanPath(canonicalPath(SyncName2QStr(folderByDrive.first.native()))) + '/';

        bool differentPaths = QString::compare(folderDir, userDir, cs) != 0;
        if (differentPaths && folderDir.startsWith(userDir, cs)) {
            error = QObject::tr(
                        "The local folder %1 contains a folder already synced. "
                        "Please pick another one!")
                        .arg(QDir::toNativeSeparators(path));
            return ExitCodeSystemError;
        }

        if (differentPaths && userDir.startsWith(folderDir, cs)) {
            error = QObject::tr(
                        "The local folder %1 is contained in a folder already synced. "
                        "Please pick another one!")
                        .arg(QDir::toNativeSeparators(path));
            return ExitCodeSystemError;
        }

        // If both pathes are equal, the drive needs to be different
        if (!differentPaths) {
            if (driveDbId == folderByDrive.second) {
                error = QObject::tr(
                            "The local folder %1 is already synced on the same drive. "
                            "Please pick another one!")
                            .arg(QDir::toNativeSeparators(path));
                return ExitCodeSystemError;
            }
        }
    }

    return ExitCodeOk;
}

ExitCode ServerRequests::syncForPath(const std::vector<Sync> &syncList, const QString &path, int &syncDbId) {
    QString absolutePath = QDir::cleanPath(path) + QLatin1Char('/');

    for (const Sync &sync : syncList) {
        const QString localPath = SyncName2QStr(sync.localPath().native()) + QLatin1Char('/');

        if (absolutePath.startsWith(localPath,
                                    (OldUtility::isWindows() || OldUtility::isMac()) ? Qt::CaseInsensitive : Qt::CaseSensitive)) {
            syncDbId = sync.dbId();
            break;
        }
    }

    return ExitCodeOk;
}

void ServerRequests::userToUserInfo(const User &user, UserInfo &userInfo) {
    userInfo.setDbId(user.dbId());
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
}

void ServerRequests::accountToAccountInfo(const Account &account, AccountInfo &accountInfo) {
    accountInfo.setDbId(account.dbId());
    accountInfo.setUserDbId(account.userDbId());
}

void ServerRequests::driveToDriveInfo(const Drive &drive, DriveInfo &driveInfo) {
    driveInfo.setDbId(drive.dbId());
    driveInfo.setAccountDbId(drive.accountDbId());
    driveInfo.setName(QString::fromStdString(drive.name()));
    driveInfo.setColor(QColor(QString::fromStdString(drive.color())));
    driveInfo.setNotifications(drive.notifications());
    driveInfo.setAdmin(drive.admin());
    driveInfo.setMaintenance(drive.maintenance());
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
    parametersInfo.setSyncHiddenFiles(parameters.syncHiddenFiles());

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
        for (const QByteArray &dialogGeometryLine : dialogGeometryLines) {
            QList<QByteArray> dialogGeometryElts = dialogGeometryLine.split(';');
            if (dialogGeometryElts.size() == 2) {
                parametersInfo.setDialogGeometry(QString(dialogGeometryElts[0]), dialogGeometryElts[1]);
            }
        }
    }
    parametersInfo.setMaxAllowedCpu(parameters.maxAllowedCpu());
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
    parameters.setSyncHiddenFiles(parametersInfo.syncHiddenFiles());

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
        for (const QString &objectName : parametersInfo.dialogGeometry().keys()) {
            dialogGeometryArr += objectName.toUtf8();
            dialogGeometryArr += ";";
            dialogGeometryArr += parametersInfo.dialogGeometry().value(objectName);
            dialogGeometryArr += "\n";
        }
        parameters.setDialogGeometry(
            std::shared_ptr<std::vector<char>>(new std::vector<char>(dialogGeometryArr.begin(), dialogGeometryArr.end())));
    }
    parameters.setMaxAllowedCpu(parametersInfo.maxAllowedCpu());
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
            LOG_DEBUG(Log::instance()->getLogger(), "Proxy pwd not found for keychainKey=" << proxyConfig.token().c_str());
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

ExitCode ServerRequests::loadOldSelectiveSyncTable(const SyncPath &syncDbPath, QList<QPair<QString, int>> &list) {
    try {
        OldSyncDb oldSyncDb(syncDbPath);

        if (!oldSyncDb.exists()) {
            LOG_WARN(Log::instance()->getLogger(), "Cannot open DB!");
            return ExitCodeSystemError;
        }

        std::list<std::pair<std::string, int>> selectiveSyncList;
        if (!oldSyncDb.selectAllSelectiveSync(selectiveSyncList)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in OldSyncDb::selectAllSelectiveSync");
            return ExitCodeDbError;
        }

        list.clear();
        for (const auto &selectiveSyncElt : selectiveSyncList) {
            list << qMakePair(QString::fromStdString(selectiveSyncElt.first), selectiveSyncElt.second);
        }

        oldSyncDb.close();
    } catch (std::runtime_error &err) {
        LOG_ERROR(Log::instance()->getLogger(),
                  "Error loadOldSelectiveSyncTable has failed, oldSyncDb may not exists or is corrupted " << err.what());
        return ExitCodeDbError;
    }

    return ExitCodeOk;
}

}  // namespace KDC
