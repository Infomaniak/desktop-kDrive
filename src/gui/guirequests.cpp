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

#include "guirequests.h"
#include "libcommongui/commclient.h"

namespace KDC {

bool GuiRequests::isConnnected() {
    return CommClient::isConnected();
}

ExitCode GuiRequests::getUserDbIdList(QList<int> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_USER_DBIDLIST, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getUserInfoList(QList<UserInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_USER_INFOLIST, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getUserIdFromUserDbId(int userDbId, int &userId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_USER_ID_FROM_USERDBID, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> userId;

    return exitCode;
}

ExitCode GuiRequests::getErrorInfoList(ErrorLevel level, int syncDbId, int limit, QList<ErrorInfo> &list) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << level;
    paramsStream << syncDbId;
    paramsStream << limit;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ERROR_INFOLIST, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getConflictList(int driveDbId, QList<ConflictType> filter, QList<ErrorInfo> &list) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << filter;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ERROR_GET_CONFLICTS, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode = ExitCodeOk;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::deleteErrorsServer() {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ERROR_DELETE_SERVER, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteErrorsForSync(int syncDbId, bool autoResolved) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << autoResolved;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ERROR_DELETE_SYNC, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteInvalidTokenErrors() {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ERROR_DELETE_INVALIDTOKEN, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::resolveConflictErrors(int driveDbId, bool keepLocalVersion) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << keepLocalVersion;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ERROR_RESOLVE_CONFLICTS, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::resolveUnsupportedCharErrors(int driveDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ERROR_RESOLVE_UNSUPPORTED_CHAR, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::setSupportsVirtualFiles(int syncDbId, bool value) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << value;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_SETSUPPORTSVIRTUALFILES, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::setRootPinState(int syncDbId, PinState pinState) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << pinState;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_SETROOTPINSTATE, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteUser(int userDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_USER_DELETE, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getAccountInfoList(QList<AccountInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_ACCOUNT_INFOLIST, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getDriveInfoList(QList<DriveInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_DRIVE_INFOLIST, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getDriveIdFromDriveDbId(int driveDbId, int &driveId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_DRIVE_ID_FROM_DRIVEDBID, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> driveId;

    return exitCode;
}

ExitCode GuiRequests::getDriveIdFromSyncDbId(int syncDbId, int &driveId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_DRIVE_ID_FROM_SYNCDBID, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> driveId;

    return exitCode;
}

ExitCode GuiRequests::getDriveDefaultColor(QColor &color) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_DRIVE_DEFAULTCOLOR, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> color;

    return exitCode;
}

ExitCode GuiRequests::updateDrive(const DriveInfo &driveInfo) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveInfo;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_DRIVE_UPDATE, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteDrive(int driveDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_DRIVE_DELETE, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getSyncInfoList(QList<SyncInfo> &list) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_INFOLIST, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::syncStart(int syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_START, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::syncStop(int syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_STOP, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getSyncStatus(int syncDbId, SyncStatus &status) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_STATUS, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> status;

    return exitCode;
}

ExitCode GuiRequests::getSyncIsRunning(int syncDbId, bool &running) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_ISRUNNING, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> running;

    return exitCode;
}

ExitCode GuiRequests::getSyncIdSet(int syncDbId, SyncNodeType type, QSet<QString> &syncIdSet) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << type;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNCNODE_LIST, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> syncIdSet;

    return exitCode;
}

ExitCode GuiRequests::setSyncIdSet(int syncDbId, SyncNodeType type, const QSet<QString> &syncIdSet) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << type;
    paramsStream << syncIdSet;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNCNODE_SETLIST, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getParameters(ParametersInfo &parametersInfo) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_PARAMETERS_INFO, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
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
    if (!CommClient::instance()->execute(REQUEST_NUM_PARAMETERS_UPDATE, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getNodePath(int syncDbId, QString nodeId, QString &path) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << nodeId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_NODE_PATH, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> path;

    return exitCode;
}

ExitCode GuiRequests::findGoodPathForNewSync(int driveDbId, const QString &basePath, QString &path, QString &error) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << basePath;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_FINDGOODPATHFORNEWSYNC, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> path;
    resultStream >> error;

    return exitCode;
}

#ifdef Q_OS_WIN
ExitCode GuiRequests::showInExplorerNavigationPane(bool &show) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_SHOWSHORTCUT, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> show;

    return exitCode;
}

ExitCode GuiRequests::setShowInExplorerNavigationPane(const bool &show) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << show;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_SETSHOWSHORTCUT, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}
#endif

ExitCode GuiRequests::requestToken(const QString &code, const QString &codeVerifier, int &userDbId, QString &error,
                                   QString &errorDescr) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << code;
    paramsStream << codeVerifier;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_LOGIN_REQUESTTOKEN, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    if (exitCode == ExitCodeOk) {
        resultStream >> userDbId;
    } else {
        resultStream >> error;
        resultStream >> errorDescr;
    }

    return exitCode;
}

ExitCode GuiRequests::getNodeInfo(int userDbId, int driveId, const QString &nodeId, NodeInfo &nodeInfo,
                                  bool withPath /*= false*/) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;
    paramsStream << driveId;
    paramsStream << nodeId;
    paramsStream << withPath;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_NODE_INFO, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> nodeInfo;

    return exitCode;
}

ExitCode GuiRequests::getUserAvailableDrives(int userDbId, QHash<int, DriveAvailableInfo> &list) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_USER_AVAILABLEDRIVES, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::addSync(int userDbId, int accountId, int driveId, const QString localFolderPath,
                              const QString &serverFolderPath, const QString &serverFolderNodeId, bool smartSync,
                              QSet<QString> blackList, QSet<QString> whiteList, int &syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;
    paramsStream << accountId;
    paramsStream << driveId;
    paramsStream << localFolderPath;
    paramsStream << serverFolderPath;
    paramsStream << serverFolderNodeId;
    paramsStream << smartSync;
    paramsStream << blackList;
    paramsStream << whiteList;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_ADD, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    if (exitCode == ExitCodeOk) {
        resultStream >> syncDbId;
    }

    return exitCode;
}

ExitCode GuiRequests::addSync(int driveDbId, const QString &localFolderPath, const QString &serverFolderPath,
                              const QString &serverFolderNodeId, bool smartSync, QSet<QString> blackList, QSet<QString> whiteList,
                              int &syncDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << localFolderPath;
    paramsStream << serverFolderPath;
    paramsStream << serverFolderNodeId;
    paramsStream << smartSync;
    paramsStream << blackList;
    paramsStream << whiteList;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_ADD2, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    if (exitCode == ExitCodeOk) {
        resultStream >> syncDbId;
    }

    return exitCode;
}

ExitCode GuiRequests::startSyncs(int userDbId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_START_AFTER_LOGIN, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::deleteSync(int syncDbId) {
    const auto params = QByteArray(ArgsReader(syncDbId));

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_DELETE, params, results, COMM_LONG_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode = ExitCodeOk;
    ArgsWriter(results).write(exitCode);

    return exitCode;
}

ExitCode GuiRequests::propagateSyncListChange(int syncDbId, bool restartSync) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << restartSync;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_PROPAGATE_SYNCLIST_CHANGE, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::bestAvailableVfsMode(VirtualFileMode &mode) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_BESTVFSAVAILABLEMODE, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> mode;

    return exitCode;
}

ExitCode GuiRequests::propagateExcludeListChange() {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_EXCLTEMPL_PROPAGATE_CHANGE, QByteArray(), results, COMM_LONG_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::hasSystemLaunchOnStartup(bool &enabled) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_HASSYSTEMLAUNCHONSTARTUP, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> enabled;

    return exitCode;
}

ExitCode GuiRequests::hasLaunchOnStartup(bool &enabled) {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_HASLAUNCHONSTARTUP, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> enabled;

    return exitCode;
}

ExitCode GuiRequests::setLaunchOnStartup(bool enabled) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << enabled;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_SETLAUNCHONSTARTUP, params, results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getSubFolders(int userDbId, int driveId, const QString &nodeId, QList<NodeInfo> &list,
                                    bool withPath /*= false*/) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;
    paramsStream << driveId;
    paramsStream << nodeId;
    paramsStream << withPath;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_NODE_SUBFOLDERS, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::getSubFolders(int driveDbId, const QString &nodeId, QList<NodeInfo> &list, bool withPath /*= false*/) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << nodeId;
    paramsStream << withPath;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_NODE_SUBFOLDERS2, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> list;

    return exitCode;
}

ExitCode GuiRequests::createMissingFolders(int driveDbId, const QList<QPair<QString, QString>> &serverFolderList,
                                           QString &nodeId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << serverFolderList;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_NODE_CREATEMISSINGFOLDERS, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> nodeId;

    return exitCode;
}

ExitCode GuiRequests::getPublicLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << fileId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_GETPUBLICLINKURL, params, results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> linkUrl;

    return exitCode;
}

ExitCode GuiRequests::getFolderSize(int userDbId, int driveId, const QString &nodeId) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;
    paramsStream << driveId;
    paramsStream << nodeId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_NODE_FOLDER_SIZE, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getPrivateLinkUrl(int driveDbId, const QString &fileId, QString &linkUrl) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << fileId;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_GETPRIVATELINKURL, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
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
    if (!CommClient::instance()->execute(REQUEST_NUM_EXCLTEMPL_GETEXCLUDED, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
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
    if (!CommClient::instance()->execute(REQUEST_NUM_EXCLTEMPL_GETLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> templateList;

    return exitCode;
}

ExitCode GuiRequests::setExclusionTemplateList(bool def, const QList<ExclusionTemplateInfo> &templateList) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << def;
    paramsStream << templateList;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_EXCLTEMPL_SETLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
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
    if (!CommClient::instance()->execute(REQUEST_NUM_EXCLAPP_GETLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
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
    if (!CommClient::instance()->execute(REQUEST_NUM_EXCLAPP_SETLIST, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::getFetchingAppList(QHash<QString, QString> &appTable) {
    QByteArray params;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_GET_FETCHING_APP_LIST, params, results, COMM_AVERAGE_TIMEOUT)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;
    resultStream >> appTable;

    return exitCode;
}
#endif

ExitCode GuiRequests::activateLoadInfo(bool activate) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << activate;

    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_ACTIVATELOADINFO, params, results, COMM_SHORT_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::askForStatus() {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_SYNC_ASKFORSTATUS, QByteArray(), results)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

ExitCode GuiRequests::checkCommStatus() {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_CHECKCOMMSTATUS, QByteArray(), results, COMM_AVERAGE_TIMEOUT)) {
        return ExitCodeSystemError;
    }

    ExitCode exitCode;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> exitCode;

    return exitCode;
}

}  // namespace KDC
