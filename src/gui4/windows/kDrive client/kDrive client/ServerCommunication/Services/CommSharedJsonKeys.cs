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

namespace Infomaniak.kDrive.ServerCommunication.CommStruct
{
    // Shared JSON keys used in server communication
    public struct JsonKeys
    {
        static public string ExitCode = "code";
        static public string ExitCause = "cause";

        static public string OAuthCode = "code";
        static public string OAuthCodeVerifier = "codeVerifier";

        static public string UserDbId = "userDbId";
        static public string UserDbIds = "userDbIds";
        static public string UserInfoList = "userInfoList";
        static public string UserInfo = "userInfo";

        static public string AccountDbId = "accountDbId";
        static public string AccountId = "accountId";
        static public string AccountName = "accountName";
        static public string AccountInfoList = "accountInfoList";
        static public string AccountInfo = "accountInfo";

        static public string DriveDbId = "driveDbId";
        static public string DriveInfoList = "driveInfoList";
        static public string DriveInfo = "driveInfo";
        static public string DriveId = "driveId";

        static public string SyncInfoList = "syncInfoList";
        static public string SyncDbId = "syncDbId";
        static public string SyncInfo = "syncInfo";
        static public string SyncProgress = "syncProgress";
        static public string SyncStep = "syncStep";
        static public string SyncStatus = "syncStatus";
        static public string SyncFileItemInfo = "itemInfo";
        static public string RestartSync = "restartSync";

        static public string ErrorDbId = "errorDbId";
        static public string ErrorInfo = "errorInfo";
        static public string ErrorInfoList = "errorInfoList";
        static public string ErrorMessage = "errorMessage";

        static public string NodeId = "nodeId";
        static public string NodeInfo = "nodeInfo";
        static public string NodeIdList = "nodeIdList";
        static public string LocalFolderPath = "localFolderPath";
        static public string ServerFolderPath = "serverFolderPath";
        static public string ServerFolderNodeId = "serverFolderNodeId";
        static public string LiteSync = "liteSync";
        static public string BlackList = "blackList";

        static public string SearchString = "searchString";
        static public string SearchInfoList = "searchInfoList";

        static public string NodeSubFolderInfoList = "nodeSubFolderInfoList";
        static public string FolderSize = "folderSize";
        static public string WithPath = "withPath";
        static public string ParmsInfo = "parametersInfo";

        static public string ExclusionTemplatesList = "exclusionTemplateList";
        static public string Default = "default";

        static public string Limit = "limit";
        static public string IsValid = "isValid";
        static public string Path = "path";
        static public string BasePath = "basePath";
        static public string GoodPath = "goodPath";
        static public string BestMode = "bestMode";
        static public string Value = "value";
        static public string Size= "size";

        static public string LinkUrl = "linkUrl";

        static public string VersionInfo = "versionInfo";
        static public string DriveAvailableInfoList = "driveAvailableInfoList";

        static public string UpdateChannel = "channel";
        static public string UpdateState = "updateState";
        static public string State = "state";
        static public string Percentage = "percentage";

        static public string IncludeArchivedLogs = "includeArchivedLogs";

        static public string RelativePath = "relativePath";
        static public string ReplicaSide = "replicaSide";
        static public string NodeVersionInfo = "nodeVersionInfo";
    }
}
