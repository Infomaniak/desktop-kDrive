/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "guirequests.h"

#include "info/searchinfo.h"
#include "libcommongui/commclient.h"
#include "libcommon/utility/utility.h"

namespace KDC {

bool GuiRequests::isConnnected() {
    return CommClient::isConnected();
}

ExitCode GuiRequests::getUserDbIdList(QList<UserDbId> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::USER_DBIDLIST, QByteArray(), results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QList<qint64> tmpList;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> tmpList;

    for (const auto userDbId: tmpList) {
        list << static_cast<UserDbId>(userDbId);
    }

    return exitCode;
}

ExitCode GuiRequests::getUserInfoList(QList<UserInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::USER_INFOLIST, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getErrorInfoList(const ErrorLevel level, const SyncDbId syncDbId, const int limit, QList<ErrorInfo> &list) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << level;
    paramsStream << static_cast<qint64>(syncDbId);
    paramsStream << limit;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ERROR_INFOLIST_LEGACY, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getConflictList(const DriveDbId driveDbId, const QList<ConflictType> &filter, QList<ErrorInfo> &list) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << filter;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ERROR_GET_CONFLICTS_LEGACY, params, results)) {
        return ExitCode::SystemError;
    }

    ExitCode exitCode = ExitCode::Ok;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::deleteErrorsServer() {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ERROR_DELETE_SERVER, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteErrorsForSync(const SyncDbId syncDbId, const bool autoResolved) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);
    paramsStream << autoResolved;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ERROR_DELETE_SYNC, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteInvalidTokenErrors() {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ERROR_DELETE_INVALIDTOKEN, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::resolveConflictErrors(const DriveDbId driveDbId, const bool keepLocalVersion) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << keepLocalVersion;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ERROR_RESOLVE_CONFLICTS_LEGACY, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::resolveUnsupportedCharErrors(const DriveDbId driveDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ERROR_RESOLVE_UNSUPPORTED_CHAR_LEGACY, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::setSupportsVirtualFiles(const SyncDbId syncDbId, const bool value) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);
    paramsStream << value;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_SETSUPPORTSVIRTUALFILES, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteUser(const UserDbId userDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(userDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::USER_DELETE, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getAccountInfoList(QList<AccountInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::ACCOUNT_INFOLIST, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getDriveInfoList(QList<DriveInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::DRIVE_INFOLIST, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::updateDrive(const DriveInfo &driveInfo) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveInfo;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::DRIVE_UPDATE, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteDrive(const DriveDbId driveDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::DRIVE_DELETE, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::searchItemInDrive(const DriveDbId driveDbId, const QString &searchString, QList<SearchInfo> &list,
                                        bool &hasMore, QString &cursor) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << searchString;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::DRIVE_SEARCH, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;
    resultStream >> hasMore;
    resultStream >> cursor;

    return exitCode;
}

ExitCode GuiRequests::getSyncInfoList(QList<SyncInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_INFOLIST, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::syncStart(const SyncDbId syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_START, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::syncStop(const SyncDbId syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_STOP, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getSyncStatus(const SyncDbId syncDbId, SyncStatus &status) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_STATUS, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> status;

    return exitCode;
}

ExitCode GuiRequests::getBlacklistedNodeIdSet(const SyncDbId syncDbId, QSet<QString> &syncIdSet) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::BLACKLISTED_NODE_LIST, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> syncIdSet;

    return exitCode;
}

ExitCode GuiRequests::setBlacklistedNodeIdSet(const SyncDbId syncDbId, const QSet<QString> &syncIdSet) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);
    paramsStream << syncIdSet;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::BLACKLISTED_NODE_SETLIST, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getParameters(ParametersInfo &parametersInfo) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::PARAMETERS_INFO, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> parametersInfo;

    return exitCode;
}

ExitCode GuiRequests::updateParameters(const ParametersInfo &parametersInfo) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << parametersInfo;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::PARAMETERS_UPDATE, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getNodePath(const SyncDbId syncDbId, const QString &nodeId, QString &path) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(syncDbId);
    paramsStream << nodeId;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::NODE_PATH, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> path;

    return exitCode;
}

ExitCode GuiRequests::findGoodPathForNewSync(const QString &basePath, QString &path, QString &error) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << basePath;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> path;
    resultStream >> error;

    return exitCode;
}

ExitCode GuiRequests::requestToken(const QString &code, const QString &codeVerifier, UserDbId &userDbId, QString &error,
                                   QString &errorDescr) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << code;
    paramsStream << codeVerifier;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::LOGIN_REQUESTTOKEN, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    if (exitCode == ExitCode::Ok) {
        qint64 tmp = 0;
        resultStream >> tmp;
        userDbId = tmp;
    } else {
        resultStream >> error;
        resultStream >> errorDescr;
    }

    return exitCode;
}

ExitCode GuiRequests::getNodeInfo(const UserDbId userDbId, const DriveId driveId, const QString &nodeId, NodeInfo &nodeInfo,
                                  const bool withPath /*= false*/) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(userDbId);
    paramsStream << static_cast<qint64>(driveId);
    paramsStream << nodeId;
    paramsStream << withPath;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::NODE_INFO, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> nodeInfo;

    return exitCode;
}

ExitCode GuiRequests::getUserAvailableDrives(const UserDbId userDbId, QList<DriveAvailableInfo> &list) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(userDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::USER_AVAILABLEDRIVES, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::addSync(const UserDbId userDbId, const AccountId accountId, const DriveId driveId,
                              const QString &localFolderPath, const QString &serverFolderPath, const QString &serverFolderNodeId,
                              const bool liteSync, const QSet<QString> &blackList, const QSet<QString> &whiteList,
                              SyncDbId &syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(userDbId);
    paramsStream << static_cast<qint64>(accountId);
    paramsStream << static_cast<qint64>(driveId);
    paramsStream << localFolderPath;
    paramsStream << serverFolderPath;
    paramsStream << serverFolderNodeId;
    paramsStream << liteSync;
    paramsStream << blackList;
    paramsStream << whiteList;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_ADD, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    if (exitCode == ExitCode::Ok) {
        qint64 tmp = 0;
        resultStream >> tmp;
        syncDbId = static_cast<SyncDbId>(tmp);
    }

    return exitCode;
}

ExitCode GuiRequests::addSync(const DriveDbId driveDbId, const QString &localFolderPath, const QString &serverFolderPath,
                              const QString &serverFolderNodeId, const bool liteSync, const QSet<QString> &blackList,
                              const QSet<QString> &whiteList, SyncDbId &syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << localFolderPath;
    paramsStream << serverFolderPath;
    paramsStream << serverFolderNodeId;
    paramsStream << liteSync;
    paramsStream << blackList;
    paramsStream << whiteList;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_ADD2, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    if (exitCode == ExitCode::Ok) {
        qint64 tmp = 0;
        resultStream >> tmp;
        syncDbId = tmp;
    }

    return exitCode;
}

ExitCode GuiRequests::startSyncs(const UserDbId userDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(userDbId);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_START_AFTER_LOGIN, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteSync(const SyncDbId syncDbId) {
    const auto params = QByteArray(ArgsReader(static_cast<qint64>(syncDbId)));

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_DELETE, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Ok;
    ArgsWriter(results).write(exitCode);

    return exitCode;
}

ExitCode GuiRequests::bestAvailableVfsMode(VirtualFileMode &mode) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_BESTVFSAVAILABLEMODE_LEGACY, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> mode;

    return exitCode;
}

ExitCode GuiRequests::hasSystemLaunchOnStartup(bool &enabled) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> enabled;

    return exitCode;
}

ExitCode GuiRequests::hasLaunchOnStartup(bool &enabled) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_HASLAUNCHONSTARTUP, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> enabled;

    return exitCode;
}

ExitCode GuiRequests::setLaunchOnStartup(const bool enabled) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << enabled;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_SETLAUNCHONSTARTUP, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getAppState(const AppStateKey key, AppStateValue &value) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << key;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_GET_APPSTATE, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    if (exitCode != ExitCode::Ok) {
        return exitCode;
    }

    QString valueQStr;
    resultStream >> valueQStr;

    if (const std::string valueStr = valueQStr.toStdString(); !CommonUtility::stringToAppStateValue(valueStr, value)) {
        return ExitCode::SystemError;
    }
    return exitCode;
}

ExitCode GuiRequests::updateAppState(const AppStateKey key, const AppStateValue &value) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << key;
    std::string valueStr;
    if (!CommonUtility::appStateValueToString(value, valueStr)) {
        return ExitCode::SystemError;
    }
    paramsStream << QString::fromStdString(valueStr);

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_SET_APPSTATE, params, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getLogDirEstimatedSize(uint64_t &size) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE_LEGACY, {}, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    qint64 sizeQt = 0;
    auto exitCode = ExitCode::Ok;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> sizeQt;
    size = static_cast<uint64_t>(sizeQt);

    return exitCode;
}

ExitCode GuiRequests::sendLogToSupport(const bool sendArchivedLogs) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << sendArchivedLogs;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_SEND_LOG_TO_SUPPORT, params, results,
                                         COMM_SHORT_TIMEOUT)) { // Short timeout because the operation is asynchronous
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Ok;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::cancelLogUploadToSupport() {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT, {}, results,
                                         COMM_SHORT_TIMEOUT)) { // Short timeout because the operation is asynchronous
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Ok;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::crash() {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_CRASH, {}, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Ok;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitInfo GuiRequests::getSubFolders(const UserDbId userDbId, const DriveId driveId, const QString &nodeId, QList<NodeInfo> &list,
                                    bool withPath /*= false*/) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(userDbId);
    paramsStream << static_cast<qint64>(driveId);
    paramsStream << nodeId;
    paramsStream << withPath;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::NODE_SUBFOLDERS, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    int exitInfoInt = 0;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitInfoInt;
    resultStream >> list;

    return ExitInfo::fromInt(exitInfoInt);
}

ExitInfo GuiRequests::getSubFolders(const DriveDbId driveDbId, const QString &nodeId, QList<NodeInfo> &list,
                                    bool withPath /*= false*/) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << nodeId;
    paramsStream << withPath;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::NODE_SUBFOLDERS2, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    int exitInfoInt = 0;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitInfoInt;
    resultStream >> list;

    return ExitInfo::fromInt(exitInfoInt);
}

ExitCode GuiRequests::createMissingFolders(const DriveDbId driveDbId, const QList<QPair<QString, QString>> &serverFolderList,
                                           QString &nodeId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << serverFolderList;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::NODE_CREATEMISSINGFOLDERS_LEGACY, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> nodeId;

    return exitCode;
}

ExitCode GuiRequests::getPublicLinkUrl(const DriveDbId driveDbId, const QString &fileId, QString &linkUrl) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << fileId;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_GETPUBLICLINKURL, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> linkUrl;

    return exitCode;
}

ExitCode GuiRequests::getFolderSize(const UserDbId userDbId, const DriveId driveId, const QString &nodeId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(userDbId);
    paramsStream << static_cast<qint64>(driveId);
    paramsStream << nodeId;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::NODE_FOLDER_SIZE, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getPrivateLinkUrl(const DriveDbId driveDbId, const QString &fileId, QString &linkUrl) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << static_cast<qint64>(driveDbId);
    paramsStream << fileId;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_GETPRIVATELINKURL, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> linkUrl;

    return exitCode;
}

ExitCode GuiRequests::getNameExcluded(const QString &name, bool excluded) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << name;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::EXCLTEMPL_GETEXCLUDED, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> excluded;

    return exitCode;
}

ExitCode GuiRequests::getExclusionTemplateList(bool def, QList<ExclusionTemplateInfo> &templateList) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << def;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::EXCLTEMPL_GETLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> templateList;

    return exitCode;
}

ExitCode GuiRequests::setUserExclusionTemplateList(const QList<ExclusionTemplateInfo> &templateList) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << templateList;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::EXCLTEMPL_SETUSERLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

#ifdef Q_OS_MAC
ExitCode GuiRequests::getExclusionAppList(bool def, QList<ExclusionAppInfo> &appList) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << def;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::EXCLAPP_GETLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> appList;

    return exitCode;
}

ExitCode GuiRequests::setExclusionAppList(bool def, const QList<ExclusionAppInfo> &appList) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << def;
    paramsStream << appList;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::EXCLAPP_SETLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getFetchingAppList(QHash<QString, QString> &appTable) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::EXCLAPP_GET_FETCHING_APP_LIST, {}, results, COMM_AVERAGE_TIMEOUT)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> appTable;

    return exitCode;
}
#endif

ExitCode GuiRequests::activateLoadInfo(const bool activate) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << activate;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_ACTIVATELOADINFO, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::askForStatus() {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::SYNC_TRIGGER_PROGRESS_UPDATE, {}, results)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::checkCommStatus() {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UTILITY_CHECKCOMMSTATUS, {}, results, COMM_LONG_TIMEOUT)) {
        return ExitCode::SystemError;
    }

    auto exitCode = ExitCode::Unknown;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::changeDistributionChannel(const DistributionChannel channel) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << channel;

    if (!CommClient::instance()->execute(RequestNum::UPDATER_CHANGE_CHANNEL, params)) {
        return ExitCode::SystemError;
    }
    return ExitCode::Ok;
}

ExitCode GuiRequests::versionInfo(VersionInfo &versionInfo,
                                  const DistributionChannel channel /*= DistributionChannel::Unknown*/) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << channel;

    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UPDATER_VERSION_INFO, params, results)) {
        return ExitCode::SystemError;
    }

    if (!results.isEmpty()) {
        QDataStream resultStream(&results, QIODevice::ReadOnly);
        resultStream >> versionInfo;
    }
    return ExitCode::Ok;
}

ExitCode GuiRequests::updateState(UpdateState &state) {
    QByteArray results;
    if (!CommClient::instance()->execute(RequestNum::UPDATER_STATE, {}, results)) {
        return ExitCode::SystemError;
    }

    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> state;
    return ExitCode::Ok;
}

ExitCode GuiRequests::startInstaller() {
    if (!CommClient::instance()->execute(RequestNum::UPDATER_START_INSTALLER)) {
        return ExitCode::SystemError;
    }
    return ExitCode::Ok;
}

ExitCode GuiRequests::skipUpdate(const std::string &version) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << QString::fromStdString(version);

    if (!CommClient::instance()->execute(RequestNum::UPDATER_SKIP_VERSION, params)) {
        return ExitCode::SystemError;
    }
    return ExitCode::Ok;
}

ExitCode GuiRequests::reportClientDisplayed() {
    CommClient::instance()->execute(RequestNum::UTILITY_SEND_APP_START_TRACE);
    return ExitCode::Ok;
}

} // namespace KDC
