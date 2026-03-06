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
        public static string ExitCode = "code";
        public static string ExitCause = "cause";

        public static string OAuthCode = "code";
        public static string OAuthCodeVerifier = "codeVerifier";

        public static string UserDbId = "userDbId";
        public static string UserDbIds = "userDbIds";
        public static string UserInfoList = "userInfoList";
        public static string UserInfo = "userInfo";

        public static string AccountDbId = "accountDbId";
        public static string AccountId = "accountId";
        public static string AccountName = "accountName";
        public static string AccountInfoList = "accountInfoList";
        public static string AccountInfo = "accountInfo";

        public static string DriveDbId = "driveDbId";
        public static string DriveInfoList = "driveInfoList";
        public static string DriveInfo = "driveInfo";
        public static string DriveId = "driveId";

        public static string SyncInfoList = "syncInfoList";
        public static string SyncDbId = "syncDbId";
        public static string SyncInfo = "syncInfo";
        public static string SyncProgress = "syncProgress";
        public static string SyncStep = "syncStep";
        public static string SyncStatus = "syncStatus";
        public static string SyncFileItemInfo = "itemInfo";
        public static string RestartSync = "restartSync";

        public static string ErrorDbId = "errorDbId";
        public static string ErrorInfo = "errorInfo";
        public static string ErrorInfoList = "errorInfoList";
        public static string ErrorMessage = "errorMessage";

        public static string NodeId = "nodeId";
        public static string NodeInfo = "nodeInfo";
        public static string NodeIdList = "nodeIdList";
        public static string LocalFolderPath = "localFolderPath";
        public static string ServerFolderPath = "serverFolderPath";
        public static string ServerFolderNodeId = "serverFolderNodeId";
        public static string LiteSync = "liteSync";
        public static string BlackList = "blackList";

        public static string SearchString = "searchString";
        public static string SearchInfoList = "searchInfoList";

        public static string NodeSubFolderInfoList = "nodeSubFolderInfoList";
        public static string FolderSize = "folderSize";
        public static string WithPath = "withPath";
        public static string ParmsInfo = "parametersInfo";

        public static string ExclusionTemplatesList = "exclusionTemplateList";
        public static string Default = "default";

        public static string Limit = "limit";
        public static string IsValid = "isValid";
        public static string Path = "path";
        public static string BasePath = "basePath";
        public static string GoodPath = "goodPath";
        public static string BestMode = "bestMode";
        public static string Value = "value";
        public static string Size = "size";

        public static string LinkUrl = "linkUrl";

        public static string VersionInfo = "versionInfo";
        public static string DriveAvailableInfoList = "driveAvailableInfoList";

        public static string UpdateChannel = "channel";
        public static string UpdateState = "updateState";
        public static string State = "state";
        public static string Percentage = "percentage";

        public static string IncludeArchivedLogs = "includeArchivedLogs";

        static public string RelativePath = "relativePath";
        static public string ReplicaSide = "replicaSide";
        static public string NodeConflictInfo = "nodeConflictInfo";
    }
}
