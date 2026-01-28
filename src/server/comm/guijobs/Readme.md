# kDrive Interprocess Communication

## 📖 Table of Contents
1. [Overview](#overview)
2. [JSON Message Model](#json-message-model)
3. [General JSON Structure](#general-json-structure)
4. [Common Data Structures](#common-data-structures)
   - [UserInfo](#userinfo)
   - [AccountInfo](#accountinfo)
   - [DriveInfo](#driveinfo)
   - [SyncInfo](#syncinfo)
5. [Requests](#requests)
   - [Request Index](#request-index)
   - [Login Requests](#login-requests)
   - [User Requests](#user-requests)
   - [Account Requests](#account-requests)
   - [Drive Requests](#drive-requests)
   - [Sync Requests](#sync-requests)
   - [Node Requests](#node-requests)
   - [Blacklist Requests](#blacklist-requests)
   - [Error Requests](#error-requests)
   - [Exclusion App Requests](#exclusion-app-requests)
   - [Exclusion Template Requests](#exclusion-template-requests)
   - [Parameters Requests](#parameters-requests)
   - [Utility Requests](#utility-requests)
   - [Updater Requests](#updater-requests)
6. [Signals](#signals)
   - [Signal Index](#signal-index)
   - [User Signals](#user-signals)
   - [Account Signals](#account-signals)
   - [Drive Signals](#drive-signals)
   - [Sync Signals](#sync-signals)
   - [Error Signals](#error-signals)
   - [Utility Signals](#utility-signals)
   - [Updater Signals](#updater-signals)

---

## 🧩 Overview

The **kDrive desktop application** is composed of two main components:

| Component | Description |
| ---------- | ------------ |
| **Server** | Handles synchronization, file operations, and backend communication. |
| **Client (GUI)** | Provides the graphical interface and user interaction. |

Communication between them is done through **JSON-based messages** over different transports depending on the OS:

| Platform | Transport Mechanism |
| --------- | ------------------ |
| **Windows** | TCP sockets |
| **Linux** | TCP sockets |
| **macOS** | XPC (Interprocess Communication) |

---

## 🔁 JSON Message Model

The communication is **bidirectional**:

- The **client** sends **requests**.
- The **server** replies with **responses** (same request `id`).
- The **server** also emits **signals** (unsolicited messages) for events (sync state, errors, etc.).

Each request includes a **unique `id`** for correlation.

---

## 🧱 General JSON Structure

### Request
```json
{
  "type": 1,               // 1 = Request/Response, 2 = Signal
  "id": 0,                 // Unique identifier for this request
  "num": 1,                // Request identifier (see RequestNum enum)
  "params": { ... }        // Parameters for this request
}
````

### Response

```json
{
  "type": 1,
  "id": 0,
  "num": 1,
  "params": { ... }        // Response data or error
}
```

### Signal

```json
{
  "type": 2,
  "id": 0,                 // Optional for signals
  "num": 1,                // Signal identifier (see SignalEnum)
  "params": { ... }        // Event data
}
```

---

## 🧩 Common Data Structures

> ⚠️ **All string values must be Base64-encoded** for safe transmission.

### UserInfo

```json
{
  "dbId": 12345,
  "userId": "u_abc123",
  "name": "John Doe",
  "email": "john.doe@example.com",
  "avatar": "<Binary PNG>",
  "isConnected": true,
  "isStaff": false
}
```

---

### AccountInfo

```json
{
  "dbId": 56789,
  "userDbId": 12345
}
```

---

### DriveInfo

```json
{
  "dbId": 11111,
  "accountDbId": 56789,
  "driveId": "d_abc456",
  "name": "My Drive",
  "color": "#TODO",
  "notification": true,
  "maintenance": false,
  "locked": false,
  "accessDenied": false
}
```

---

### SyncInfo

> ⚠️ Structure not final — subject to change.

```json
{
  "dbId": 22222,
  "driveDbId": 11111,
  "localPath": "C:\\Users\\John\\Documents\\MyDrive",
  "targetPath": "/Documents/MyDrive",
  "targetNodeId": "n_xyz789",
  "supportOnlineMode": true,
  "onlineMode": false
}
```

---

# 📤 Requests

## Request Index

### Login Requests
| Request | Description |
|---------|-------------|
| [LoginRequestToken](#loginrequesttoken) | Add a user via OAuth2 token |

### User Requests
| Request | Description |
|---------|-------------|
| [UserDbIdList](#userdbidlist) | Get list of all user database IDs |
| [UserInfoList](#userinfolist) | Get detailed info for all users |
| [UserDelete](#userdelete) | Delete a user and all associated syncs |
| [UserAvailableDrives](#useravailabledrives) | Get available drives for a user |

### Account Requests
| Request | Description |
|---------|-------------|
| [AccountInfoList](#accountinfolist) | Get list of all accounts |

### Drive Requests
| Request | Description |
|---------|-------------|
| [DriveInfoList](#driveinfolist) | Get list of all drives |
| [DriveUpdate](#driveupdate) | Update drive metadata |
| [DriveDelete](#drivedelete) | Delete a drive and associated syncs |
| [DriveSearch](#drivesearch) | Search for files/folders in a drive |

### Sync Requests
| Request | Description |
|---------|-------------|
| [SyncInfoList](#syncinfolist) | Get list of all syncs |
| [SyncAdd](#syncadd) | Create a new sync |
| [SyncAdd2](#syncadd2) | Create a new sync (extended version) |
| [SyncDelete](#syncdelete) | Delete a sync |
| [SyncStart](#syncstart) | Start a sync |
| [SyncStop](#syncstop) | Stop/pause a sync |
| [SyncStatus](#syncstatus) | Get current sync status |
| [SyncStartAfterLogin](#syncstartafterlogin) | Start all syncs for a user after login |
| [SyncGetPublicLinkUrl](#syncgetpubliclinkurl) | Get public sharing link for a node |
| [SyncGetPrivateLinkUrl](#syncgetprivatelinkurl) | Get private sharing link for a file |
| [SyncTriggerProgressUpdate](#synctriggerprogressupdate) | Trigger sync progress update notification |
| [SyncSetSupportsVirtualFiles](#syncsetsupportsvirtualfiles) | Enable/disable virtual file support |
| [SyncOfflineFilesSize](#syncofflinefilessize) | Get estimated size of offline files |

### Node Requests
| Request | Description |
|---------|-------------|
| [NodePath](#nodepath) | Get file path for a node ID |
| [NodeInfo](#nodeinfo) | Get detailed info about a node |
| [NodeSubFolders](#nodesubfolders) | List subfolders of a node |
| [NodeSubFolders2](#nodesubfolders2) | List subfolders (using driveDbId) |
| [NodeFolderSize](#nodefoldersize) | Calculate total size of a folder |
| [NodeCreateMissingFolders](#nodecreatemissingfolders) | Create missing folders in hierarchy |

### Blacklist Requests
| Request | Description |
|---------|-------------|
| [BlacklistedNodeList](#blacklistednodelist) | Get list of blacklisted nodes |
| [BlacklistedNodeSetList](#blacklistednodesetlist) | Update blacklist with new nodes |

### Error Requests
| Request | Description |
|---------|-------------|
| [ErrorInfoList](#errorinfolist) | Get list of recent errors |

### Exclusion App Requests (macOS only)
| Request | Description |
|---------|-------------|
| [ExclAppGetList](#exclappgetlist) | Get excluded applications list |
| [ExclAppSetList](#exclappsetlist) | Set excluded applications |
| [ExclAppGetFetchingAppList](#exclappgetfetchingapplist) | Get currently fetching apps from VFS |

### Exclusion Template Requests
| Request | Description |
|---------|-------------|
| [ExclTemplGetExcluded](#excltemplgetexcluded) | Check if name matches exclusion templates |
| [ExclTemplGetList](#excltemplgetlist) | Get exclusion templates list |
| [ExclTemplSetList](#excltemplsetlist) | Set exclusion templates |
| [ExclTemplPropagateChange](#excltemplpropagatechange) | Propagate template changes to all syncs |

### Parameters Requests
| Request | Description |
|---------|-------------|
| [ParametersInfo](#parametersinfo) | Get application parameters |
| [ParametersUpdate](#parametersupdate) | Update application parameters |

### Utility Requests
| Request | Description |
|---------|-------------|
| [UtilityFindGoodPathForNewSync](#utilityfindgoodpathfornewsync) | Find valid path for new sync |
| [UtilityBestVfsAvailableMode](#utilitybestvfsavailablemode) | Get best VFS mode for a path |
| [UtilityIsPathValidForNewSync](#utilityispathvalidfornewsync) | Validate path for new sync |
| [UtilityActivateLoadInfo](#utilityactivateloadinfo) | Trigger progress update and load user info |
| [UtilityCheckCommStatus](#utilitycheckcommstatus) | Verify communication status |
| [UtilityHasSystemLaunchOnStartup](#utilityhassystemlaunchonstartup) | Check if app launches on startup |
| [UtilitySetAppState](#utilitysetappstate) | Update application state |
| [UtilityGetAppState](#utilitygetappstate) | Get application state value |
| [UtilitySendLogToSupport](#utilitysendlogtosupport) | Upload logs to support |
| [UtilityCancelLogToSupport](#utilitycancellogtosupport) | Cancel log upload |
| [UtilityGetLogEstimatedSize](#utilitygetlogestimatedsize) | Get estimated log size |
| [UtilityQuit](#utilityquit) | Quit the application |
| [UtilitySendAppStartTrace](#utilitysendappstarttrace) | Send app startup trace |

### Updater Requests
| Request | Description |
|---------|-------------|
| [UpdaterVersionInfo](#updaterversioninfo) | Get version info for update channel |
| [UpdaterState](#updaterstate) | Get current update state |
| [UpdaterStartInstaller](#updaterstartinstaller) | Launch update installer |
| [UpdaterSkipVersion](#updaterskipversion) | Skip a version from updates |

---

## Login Requests

<details id="loginrequesttoken">
<summary><b>LoginRequestToken</b> — Add a user via OAuth2 token</summary>

**Description:**
Adds a new user using an OAuth2 token. If the user already exists, it will be marked as connected. Emits `UserAdded` or `UserUpdated` upon success.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `code` | string | Input | OAuth2 authorization code |
| `codeVerifier` | string | Input | PKCE code verifier |
| `userDbId` | int | Output | Database ID of created/updated user |
| `error` | string | Output (on failure) | Error message |
| `errorDescription` | string | Output (on failure) | Detailed error description |

</details>

---

## User Requests

<details id="userdbidlist">
<summary><b>UserDbIdList</b> — Get list of user database IDs</summary>

**Description:**
Retrieves database IDs of all registered users.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `userDbIdList` | int[] | Output | Array of user database IDs |

</details>

<details id="userinfolist">
<summary><b>UserInfoList</b> — Get detailed info for all users</summary>

**Description:**
Retrieves detailed information for all registered users including name, email, avatar, and connection status.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `userInfoList` | UserInfo[] | Output | Array of UserInfo objects |

</details>

<details id="userdelete">
<summary><b>UserDelete</b> — Delete a user and all associated syncs</summary>

**Description:**
Deletes a user from the database along with all their associated accounts, drives, and syncs.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `userDbId` | int | Input | Database ID of user to delete |
| (none) | - | Output | No output parameters |

</details>

<details id="useravailabledrives">
<summary><b>UserAvailableDrives</b> — Get available drives for a user</summary>

**Description:**
Retrieves the list of drives available to a specific user.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `userDbId` | int | Input | Database ID of the user |
| `driveAvailableInfoList` | DriveAvailableInfo[] | Output | Array of available drive information |

</details>

---

## Account Requests

<details id="accountinfolist">
<summary><b>AccountInfoList</b> — Get list of all accounts</summary>

**Description:**
Retrieves information about all user accounts in the system.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `accountInfoList` | AccountInfo[] | Output | Array of AccountInfo objects |

</details>

---

## Drive Requests

<details id="driveinfolist">
<summary><b>DriveInfoList</b> — Get list of all drives</summary>

**Description:**
Retrieves information about all available drives.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `driveInfoList` | DriveInfo[] | Output | Array of DriveInfo objects |

</details>

<details id="driveupdate">
<summary><b>DriveUpdate</b> — Update drive metadata</summary>

**Description:**
Updates the metadata for a drive in the database (e.g., name, color, notifications).

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `driveInfo` | DriveInfo | Input | Updated drive information |
| (none) | - | Output | No output parameters |

</details>

<details id="drivedelete">
<summary><b>DriveDelete</b> — Delete a drive and associated syncs</summary>

**Description:**
Deletes a drive from the database along with all its associated syncs.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `driveDbId` | int | Input | Database ID of drive to delete |
| (none) | - | Output | No output parameters |

</details>

<details id="drivesearch">
<summary><b>DriveSearch</b> — Search for files/folders in a drive</summary>

**Description:**
Searches for files and folders within a sync by name or content.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of the sync to search |
| `searchString` | string | Input | Search query string |
| `searchInfoList` | SearchInfo[] | Output | Array of matching search results |
| `hasMore` | bool | Output | Indicates if more results are available |

</details>

---

## Sync Requests

<details id="syncinfolist">
<summary><b>SyncInfoList</b> — Get list of all syncs</summary>

**Description:**
Retrieves information about all configured syncs.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `syncInfoList` | SyncInfo[] | Output | Array of SyncInfo objects |

</details>

<details id="syncadd">
<summary><b>SyncAdd</b> — Create a new sync</summary>

**Description:**
Creates a new sync between a local folder and a remote drive folder. Initializes VFS and SyncPal.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `userDbId` | int | Input | Database ID of the user |
| `accountId` | int | Input | Account ID |
| `driveId` | int | Input | Drive ID |
| `localFolderPath` | string | Input | Path to local folder |
| `serverFolderPath` | string | Input | Path on server |
| `serverFolderNodeId` | string | Input | Node ID of server folder |
| `liteSync` | bool | Input | Enable lite sync mode |
| `blackList` | NodeId[] | Input | List of nodes to exclude |
| `syncInfo` | SyncInfo | Output | Information about created sync |

</details>

<details id="syncadd2">
<summary><b>SyncAdd2</b> — Create a new sync (extended version)</summary>

**Description:**
Creates a new sync on a specific drive. Extended version that uses driveDbId.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `driveDbId` | int | Input | Database ID of the drive |
| `localFolderPath` | string | Input | Path to local folder |
| `serverFolderPath` | string | Input | Path on server |
| `serverFolderNodeId` | string | Input | Node ID of server folder |
| `liteSync` | bool | Input | Enable lite sync mode |
| `blackList` | NodeId[] | Input | List of nodes to exclude |
| `syncInfo` | SyncInfo | Output | Information about created sync |

</details>

<details id="syncdelete">
<summary><b>SyncDelete</b> — Delete a sync</summary>

**Description:**
Deletes a sync from the database and stops the sync task.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of sync to delete |
| (none) | - | Output | No output parameters |

</details>

<details id="syncstart">
<summary><b>SyncStart</b> — Start a sync</summary>

**Description:**
Starts a sync by validating settings, creating VFS, and initializing SyncPal.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of sync to start |
| (none) | - | Output | No output parameters |

</details>

<details id="syncstop">
<summary><b>SyncStop</b> — Stop/pause a sync</summary>

**Description:**
Pauses or stops a sync without removing VFS.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of sync to stop |
| (none) | - | Output | No output parameters |

</details>

<details id="syncstatus">
<summary><b>SyncStatus</b> — Get current sync status</summary>

**Description:**
Retrieves the current status of a sync (running, paused, error, etc.).

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of the sync |
| `syncStatus` | SyncStatus (enum) | Output | Current status of the sync |

</details>

<details id="syncstartafterlogin">
<summary><b>SyncStartAfterLogin</b> — Start all syncs for a user after login</summary>

**Description:**
Starts all syncs associated with a user after successful login.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `userDbId` | int | Input | Database ID of the user |
| (none) | - | Output | No output parameters |

</details>

<details id="syncgetpubliclinkurl">
<summary><b>SyncGetPublicLinkUrl</b> — Get public sharing link for a node</summary>

**Description:**
Generates a public sharing link for a file or folder node.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `driveDbId` | int | Input | Database ID of the drive |
| `nodeId` | NodeId | Input | Node ID to share |
| `linkUrl` | string | Output | Generated public link URL |

</details>

<details id="syncgetprivatelinkurl">
<summary><b>SyncGetPrivateLinkUrl</b> — Get private sharing link for a file</summary>

**Description:**
Generates a private sharing link for a file.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `driveDbId` | int | Input | Database ID of the drive |
| `fileId` | string | Input | File ID to share |
| `linkUrl` | string | Output | Generated private link URL |

</details>

<details id="synctriggerprogressupdate">
<summary><b>SyncTriggerProgressUpdate</b> — Trigger sync progress update notification</summary>

**Description:**
Triggers an update of sync progress information to the GUI.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

<details id="syncsetsupportsvirtualfiles">
<summary><b>SyncSetSupportsVirtualFiles</b> — Enable/disable virtual file support</summary>

**Description:**
Enables or disables virtual file support for a sync.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of the sync |
| `value` | bool | Input | Enable (true) or disable (false) virtual files |
| (none) | - | Output | No output parameters |

</details>

<details id="syncofflinefilessize">
<summary><b>SyncOfflineFilesSize</b> — Get estimated size of offline files</summary>

**Description:**
Calculates the estimated total size of offline files for a sync.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of the sync |
| `size` | uint64 | Output | Estimated size in bytes |

</details>

---

## Node Requests

<details id="nodepath">
<summary><b>NodePath</b> — Get file path for a node ID</summary>

**Description:**
Retrieves the file path corresponding to a node ID within a sync.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of the sync |
| `nodeId` | NodeId | Input | Node ID to look up |
| `path` | string | Output | File path for the node |

</details>

<details id="nodeinfo">
<summary><b>NodeInfo</b> — Get detailed info about a node</summary>

**Description:**
Retrieves detailed information about a specific node (file or folder).

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `userDbId` | int | Input | Database ID of the user |
| `driveId` | int | Input | Drive ID |
| `nodeId` | NodeId | Input | Node ID to query |
| `withPath` | bool | Input | Include path in response |
| `nodeInfo` | NodeInfo | Output | Detailed node information |

</details>

<details id="nodesubfolders">
<summary><b>NodeSubFolders</b> — List subfolders of a node</summary>

**Description:**
Lists all subfolders within a specified node.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `userDbId` | int | Input | Database ID of the user |
| `driveId` | int | Input | Drive ID |
| `nodeId` | NodeId | Input | Parent node ID |
| `withPath` | bool | Input | Include paths in response |
| `nodeSubFolderInfoList` | NodeInfo[] | Output | Array of subfolder information |

</details>

<details id="nodesubfolders2">
<summary><b>NodeSubFolders2</b> — List subfolders (using driveDbId)</summary>

**Description:**
Lists all subfolders within a specified node. Alternate version using driveDbId.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `driveDbId` | int | Input | Database ID of the drive |
| `nodeId` | NodeId | Input | Parent node ID |
| `withPath` | bool | Input | Include paths in response |
| `nodeSubFolderInfoList` | NodeInfo[] | Output | Array of subfolder information |

</details>

<details id="nodefoldersize">
<summary><b>NodeFolderSize</b> — Calculate total size of a folder</summary>

**Description:**
Calculates the total size of a folder and its contents. This is a low priority operation.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `userDbId` | int | Input | Database ID of the user |
| `driveId` | int | Input | Drive ID |
| `nodeId` | NodeId | Input | Node ID of the folder |
| `folderSize` | int64 | Output | Total size in bytes |

</details>

<details id="nodecreatemissingfolders">
<summary><b>NodeCreateMissingFolders</b> — Create missing folders in hierarchy</summary>

**Description:**
Creates any missing folders in a folder hierarchy path.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `driveDbId` | int | Input | Database ID of the drive |
| `folderList` | FolderItem[] | Input | List of folders to create |
| `parentNodeId` | NodeId | Output | Node ID of the parent folder |

</details>

---

## Blacklist Requests

<details id="blacklistednodelist">
<summary><b>BlacklistedNodeList</b> — Get list of blacklisted nodes</summary>

**Description:**
Retrieves the list of node IDs that are blacklisted (excluded) from a sync.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of the sync |
| `nodeIdList` | NodeId[] | Output | Array of blacklisted node IDs |

</details>

<details id="blacklistednodesetlist">
<summary><b>BlacklistedNodeSetList</b> — Update blacklist with new nodes</summary>

**Description:**
Updates the blacklist for a sync with a new set of node IDs and notifies the sync engine.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `syncDbId` | int | Input | Database ID of the sync |
| `nodeIdList` | NodeId[] | Input | New list of blacklisted node IDs |
| (none) | - | Output | No output parameters |

</details>

---

## Error Requests

<details id="errorinfolist">
<summary><b>ErrorInfoList</b> — Get list of recent errors</summary>

**Description:**
Retrieves a list of recent errors with an optional limit.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `limit` | int | Input | Maximum number of errors to return |
| `errorInfoList` | ErrorInfo[] | Output | Array of error information |

</details>

---

## Exclusion App Requests

> ⚠️ **macOS only** - These requests are only available on macOS.

<details id="exclappgetlist">
<summary><b>ExclAppGetList</b> — Get excluded applications list</summary>

**Description:**
Retrieves the list of applications excluded from VFS operations.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `default` | bool | Input | Get default list (true) or custom list (false) |
| `applicationList` | ExclusionAppInfo[] | Output | Array of excluded applications |

</details>

<details id="exclappsetlist">
<summary><b>ExclAppSetList</b> — Set excluded applications</summary>

**Description:**
Sets the list of applications to exclude from VFS operations and updates Mac VFS.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `default` | bool | Input | Set as default list |
| `applicationList` | ExclusionAppInfo[] | Input | Array of applications to exclude |
| (none) | - | Output | No output parameters |

</details>

<details id="exclappgetfetchingapplist">
<summary><b>ExclAppGetFetchingAppList</b> — Get currently fetching apps from VFS</summary>

**Description:**
Retrieves the list of applications currently fetching files through VFS.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `applicationTable` | AppTable | Output | Table of fetching applications |

</details>

---

## Exclusion Template Requests

<details id="excltemplgetexcluded">
<summary><b>ExclTemplGetExcluded</b> — Check if name matches exclusion templates</summary>

**Description:**
Checks if a file or folder name matches any exclusion template.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `name` | string | Input | File or folder name to check |
| `isExcluded` | bool | Output | True if name matches an exclusion template |

</details>

<details id="excltemplgetlist">
<summary><b>ExclTemplGetList</b> — Get exclusion templates list</summary>

**Description:**
Retrieves the list of exclusion templates with NFC normalization applied.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `default` | bool | Input | Get default list (true) or custom list (false) |
| `exclusionTemplateList` | ExclusionTemplateInfo[] | Output | Array of exclusion templates |

</details>

<details id="excltemplsetlist">
<summary><b>ExclTemplSetList</b> — Set exclusion templates</summary>

**Description:**
Sets the exclusion templates with normalization expansion.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `default` | bool | Input | Set as default list |
| `exclusionTemplateList` | ExclusionTemplateInfo[] | Input | Array of exclusion templates |
| (none) | - | Output | No output parameters |

</details>

<details id="excltemplpropagatechange">
<summary><b>ExclTemplPropagateChange</b> — Propagate template changes to all syncs</summary>

**Description:**
Propagates exclusion template changes to all active syncs.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

---

## Parameters Requests

<details id="parametersinfo">
<summary><b>ParametersInfo</b> — Get application parameters</summary>

**Description:**
Retrieves the current application parameters.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `parametersInfo` | ParametersInfo | Output | Current application parameters |

</details>

<details id="parametersupdate">
<summary><b>ParametersUpdate</b> — Update application parameters</summary>

**Description:**
Updates application parameters and propagates changes to all syncs.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `parametersInfo` | ParametersInfo | Input | New parameter values |
| (none) | - | Output | No output parameters |

</details>

---

## Utility Requests

<details id="utilityfindgoodpathfornewsync">
<summary><b>UtilityFindGoodPathForNewSync</b> — Find valid path for new sync</summary>

**Description:**
Finds a valid path for creating a new sync by validating and potentially adjusting the provided base path.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `basePath` | string | Input | Base path to validate/adjust |
| `goodPath` | string | Output | Valid path for the new sync |
| `errorMessage` | string | Output | Error message if validation fails |

</details>

<details id="utilitybestvfsavailablemode">
<summary><b>UtilityBestVfsAvailableMode</b> — Get best VFS mode for a path</summary>

**Description:**
Determines the best available VFS mode for a given path by checking filesystem compatibility (NTFS/APFS).

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `path` | string | Input | Path to check |
| `bestMode` | VirtualFileMode (enum) | Output | Best available VFS mode |

</details>

<details id="utilityispathvalidfornewsync">
<summary><b>UtilityIsPathValidForNewSync</b> — Validate path for new sync</summary>

**Description:**
Validates whether a path is suitable for creating a new sync.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `path` | string | Input | Path to validate |
| `isValid` | bool | Output | True if path is valid for new sync |

</details>

<details id="utilityactivateloadinfo">
<summary><b>UtilityActivateLoadInfo</b> — Trigger progress update and load user info</summary>

**Description:**
Triggers sync progress update and loads user information.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

<details id="utilitycheckcommstatus">
<summary><b>UtilityCheckCommStatus</b> — Verify communication status</summary>

**Description:**
Verifies the communication status between client and server.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

<details id="utilityhassystemlaunchonstartup">
<summary><b>UtilityHasSystemLaunchOnStartup</b> — Check if app launches on startup</summary>

**Description:**
Checks if the application is configured to launch on system startup.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `enabled` | bool | Output | True if app launches on startup |

</details>

<details id="utilitysetappstate">
<summary><b>UtilitySetAppState</b> — Update application state</summary>

**Description:**
Updates application state in the database with a given key-value pair.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `key` | AppStateKey (enum) | Input | State key to update |
| `value` | string | Input | New state value |
| (none) | - | Output | No output parameters |

</details>

<details id="utilitygetappstate">
<summary><b>UtilityGetAppState</b> — Get application state value</summary>

**Description:**
Retrieves an application state value by key from the database.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `key` | AppStateKey (enum) | Input | State key to retrieve |
| `value` | AppStateValue (variant) | Output | State value |

</details>

<details id="utilitysendlogtosupport">
<summary><b>UtilitySendLogToSupport</b> — Upload logs to support</summary>

**Description:**
Initiates log upload to support with option to include archived logs.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `includeArchivedLogs` | bool | Input | Include archived logs in upload |
| (none) | - | Output | No output parameters |

</details>

<details id="utilitycancellogtosupport">
<summary><b>UtilityCancelLogToSupport</b> — Cancel log upload</summary>

**Description:**
Cancels an ongoing log upload to support.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

<details id="utilitygetlogestimatedsize">
<summary><b>UtilityGetLogEstimatedSize</b> — Get estimated log size</summary>

**Description:**
Calculates the estimated total size of log files.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `logSize` | uint64 | Output | Estimated log size in bytes |

</details>

<details id="utilityquit">
<summary><b>UtilityQuit</b> — Quit the application</summary>

**Description:**
Gracefully shuts down the application.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

<details id="utilitysendappstarttrace">
<summary><b>UtilitySendAppStartTrace</b> — Send app startup trace</summary>

**Description:**
Sends application startup trace for performance/debugging analysis.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

---

## Updater Requests

<details id="updaterversioninfo">
<summary><b>UpdaterVersionInfo</b> — Get version info for update channel</summary>

**Description:**
Retrieves version information for a specified update channel.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `channel` | VersionChannel (enum) | Input | Update channel to query |
| `versionInfo` | VersionInfo | Output | Version information |

</details>

<details id="updaterstate">
<summary><b>UpdaterState</b> — Get current update state</summary>

**Description:**
Gets the current state of the updater (checking, downloading, ready, etc.).

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| `updateState` | UpdateState (enum) | Output | Current update state |

</details>

<details id="updaterstartinstaller">
<summary><b>UpdaterStartInstaller</b> — Launch update installer</summary>

**Description:**
Launches the update installer executable.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| (none) | - | Input | No input parameters required |
| (none) | - | Output | No output parameters |

</details>

<details id="updaterskipversion">
<summary><b>UpdaterSkipVersion</b> — Skip a version from updates</summary>

**Description:**
Marks a specific version to be skipped from future update checks.

| Parameter | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `skippedVersion` | string | Input | Version string to skip |
| (none) | - | Output | No output parameters |

</details>

---

# 📡 Signals

## Signal Index

### User Signals
| Signal | Description |
|--------|-------------|
| [UserAdded](#useradded) | Emitted when a user is added |
| [UserUpdated](#userupdated) | Emitted when a user is updated |
| [UserRemoved](#userremoved) | Emitted when a user is removed |

### Account Signals
| Signal | Description |
|--------|-------------|
| [AccountAdded](#accountadded) | Emitted when an account is added |
| [AccountUpdated](#accountupdated) | Emitted when an account is updated |
| [AccountRemoved](#accountremoved) | Emitted when an account is removed |

### Drive Signals
| Signal | Description |
|--------|-------------|
| [DriveAdded](#driveadded) | Emitted when a drive is added |
| [DriveUpdated](#driveupdated) | Emitted when a drive is updated |
| [DriveRemoved](#driveremoved) | Emitted when a drive is removed |

### Sync Signals
| Signal | Description |
|--------|-------------|
| [SyncAdded](#syncadded) | Emitted when a sync is added |
| [SyncUpdated](#syncupdated) | Emitted when a sync is updated |
| [SyncRemoved](#syncremoved) | Emitted when a sync is removed |
| [SyncProgressInfo](#syncprogressinfo) | Reports sync progress updates |
| [SyncCompletedItem](#synccompleteditem) | Signals file/item sync completion |

### Error Signals
| Signal | Description |
|--------|-------------|
| [ErrorAdded](#erroradded) | Emitted when an error is added |
| [ErrorRemoved](#errorremoved) | Emitted when an error is removed |

### Utility Signals
| Signal | Description |
|--------|-------------|
| [UtilityShowNotification](#utilityshownotification) | Display a notification |
| [UtilityShowSettings](#utilityshowsettings) | Open settings dialog |
| [UtilityShowSynthesis](#utilityshowsynthesis) | Display sync synthesis |
| [UtilityLogUploadState](#utilityloguploadstate) | Reports log upload progress |
| [UtilityQuit](#utilityquitsignal) | Notification that app is quitting |

### Updater Signals
| Signal | Description |
|--------|-------------|
| [UpdaterShowDialog](#updatershowdialog) | Display update dialog |
| [UpdaterStateChanged](#updaterstatechanged) | Notification of update state change |

---

## User Signals

<details id="useradded">
<summary><b>UserAdded</b> — Triggered when a new user is added</summary>

**Description:**
Emitted when a new user has been successfully added to the system.

| Parameter | Type | Description |
|-----------|------|-------------|
| `userInfo` | UserInfo | Information about the added user |

</details>

<details id="userupdated">
<summary><b>UserUpdated</b> — Triggered when a user is updated</summary>

**Description:**
Emitted when user information has been updated (e.g., name, email, connection status).

| Parameter | Type | Description |
|-----------|------|-------------|
| `userInfo` | UserInfo | Updated user information |

</details>

<details id="userremoved">
<summary><b>UserRemoved</b> — Triggered when a user is removed</summary>

**Description:**
Emitted when a user has been removed from the system.

| Parameter | Type | Description |
|-----------|------|-------------|
| `userDbId` | int | Database ID of the removed user |

</details>

---

## Account Signals

<details id="accountadded">
<summary><b>AccountAdded</b> — Triggered when an account is added</summary>

**Description:**
Emitted when a new account has been added.

| Parameter | Type | Description |
|-----------|------|-------------|
| `accountInfo` | AccountInfo | Information about the added account |

</details>

<details id="accountupdated">
<summary><b>AccountUpdated</b> — Triggered when an account is updated</summary>

**Description:**
Emitted when account information has been updated.

| Parameter | Type | Description |
|-----------|------|-------------|
| `accountInfo` | AccountInfo | Updated account information |

</details>

<details id="accountremoved">
<summary><b>AccountRemoved</b> — Triggered when an account is removed</summary>

**Description:**
Emitted when an account has been removed.

| Parameter | Type | Description |
|-----------|------|-------------|
| `accountDbId` | int | Database ID of the removed account |

</details>

---

## Drive Signals

<details id="driveadded">
<summary><b>DriveAdded</b> — Triggered when a drive is added</summary>

**Description:**
Emitted when a new drive has been added.

| Parameter | Type | Description |
|-----------|------|-------------|
| `driveInfo` | DriveInfo | Information about the added drive |

</details>

<details id="driveupdated">
<summary><b>DriveUpdated</b> — Triggered when a drive is updated</summary>

**Description:**
Emitted when drive information has been updated.

| Parameter | Type | Description |
|-----------|------|-------------|
| `driveInfo` | DriveInfo | Updated drive information |

</details>

<details id="driveremoved">
<summary><b>DriveRemoved</b> — Triggered when a drive is removed</summary>

**Description:**
Emitted when a drive has been removed.

| Parameter | Type | Description |
|-----------|------|-------------|
| `driveDbId` | int | Database ID of the removed drive |

</details>

---

## Sync Signals

<details id="syncadded">
<summary><b>SyncAdded</b> — Triggered when a sync is added</summary>

**Description:**
Emitted when a new sync has been created.

| Parameter | Type | Description |
|-----------|------|-------------|
| `syncInfo` | SyncInfo | Information about the added sync |

</details>

<details id="syncupdated">
<summary><b>SyncUpdated</b> — Triggered when a sync is updated</summary>

**Description:**
Emitted when sync information has been updated (e.g., status change).

| Parameter | Type | Description |
|-----------|------|-------------|
| `syncInfo` | SyncInfo | Updated sync information |

</details>

<details id="syncremoved">
<summary><b>SyncRemoved</b> — Triggered when a sync is removed</summary>

**Description:**
Emitted when a sync has been deleted.

| Parameter | Type | Description |
|-----------|------|-------------|
| `syncDbId` | int | Database ID of the removed sync |

</details>

<details id="syncprogressinfo">
<summary><b>SyncProgressInfo</b> — Reports sync progress updates</summary>

**Description:**
Emitted periodically to report sync progress including current status and step.

| Parameter | Type | Description |
|-----------|------|-------------|
| `syncDbId` | int | Database ID of the sync |
| `syncStatus` | SyncStatus (enum) | Current sync status |
| `syncStep` | SyncStep (enum) | Current sync step |
| `syncProgress` | SyncProgress | Progress information |

</details>

<details id="synccompleteditem">
<summary><b>SyncCompletedItem</b> — Signals file/item sync completion</summary>

**Description:**
Emitted when a file or folder has completed syncing.

| Parameter | Type | Description |
|-----------|------|-------------|
| `syncDbId` | int | Database ID of the sync |
| `itemInfo` | SyncFileItemInfo | Information about the completed item |

</details>

---

## Error Signals

<details id="erroradded">
<summary><b>ErrorAdded</b> — Triggered when an error is added</summary>

**Description:**
Emitted when a new error has occurred and been logged.

| Parameter | Type | Description |
|-----------|------|-------------|
| `errorInfo` | ErrorInfo | Information about the error |

</details>

<details id="errorremoved">
<summary><b>ErrorRemoved</b> — Triggered when an error is removed</summary>

**Description:**
Emitted when an error has been cleared or resolved.

| Parameter | Type | Description |
|-----------|------|-------------|
| `errorDbId` | int | Database ID of the removed error |

</details>

---

## Utility Signals

<details id="utilityshownotification">
<summary><b>UtilityShowNotification</b> — Display a notification</summary>

**Description:**
Signals the GUI to display a notification with a custom title and message.

| Parameter | Type | Description |
|-----------|------|-------------|
| `title` | string | Notification title |
| `message` | string | Notification message |

</details>

<details id="utilityshowsettings">
<summary><b>UtilityShowSettings</b> — Open settings dialog</summary>

**Description:**
Signals the GUI to open the settings dialog.

| Parameter | Type | Description |
|-----------|------|-------------|
| (none) | - | No parameters |

</details>

<details id="utilityshowsynthesis">
<summary><b>UtilityShowSynthesis</b> — Display sync synthesis</summary>

**Description:**
Signals the GUI to display the sync synthesis/summary view.

| Parameter | Type | Description |
|-----------|------|-------------|
| (none) | - | No parameters |

</details>

<details id="utilityloguploadstate">
<summary><b>UtilityLogUploadState</b> — Reports log upload progress</summary>

**Description:**
Emitted during log upload to report progress and state.

| Parameter | Type | Description |
|-----------|------|-------------|
| `state` | LogUploadState (enum) | Current upload state |
| `percentage` | int | Upload progress percentage (0-100) |

</details>

<details id="utilityquitsignal">
<summary><b>UtilityQuit</b> — Notification that app is quitting</summary>

**Description:**
Emitted to notify the GUI that the application is shutting down.

| Parameter | Type | Description |
|-----------|------|-------------|
| (none) | - | No parameters |

</details>

---

## Updater Signals

<details id="updatershowdialog">
<summary><b>UpdaterShowDialog</b> — Display update dialog</summary>

**Description:**
Signals the GUI to display the update dialog with version details.

| Parameter | Type | Description |
|-----------|------|-------------|
| `versionInfo` | VersionInfo | Information about the available update |

</details>

<details id="updaterstatechanged">
<summary><b>UpdaterStateChanged</b> — Notification of update state change</summary>

**Description:**
Emitted when the update state changes (e.g., checking → downloading → ready).

| Parameter | Type | Description |
|-----------|------|-------------|
| `updateState` | UpdateState (enum) | New update state |

</details>