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
   - [LoginRequestToken](#loginrequesttoken)
6. [Signals](#signals)
   - [Signal Index](#signal-index)
   - [UserAdded](#useradded)
   - [UserUpdated](#userupdated)

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

| Request                                 | Description                                                       |
| --------------------------------------- | ----------------------------------------------------------------- |
| [LoginRequestToken](#loginrequesttoken) | Add a user via OAuth2 token (emits `UserAdded` or `UserUpdated`). |
| [UtilityUnlinkHardlinks](#utilityunlinkhardlinks) | Remove all the hardlinks of a file under the sync root to resolve a `HardlinkNotSupportedError`. |

---

<details id="loginrequesttoken">
<summary><b>LoginRequestToken</b> — Add a user via OAuth2 token</summary>

**Description:**
Adds a new user using an OAuth2 token.
If the user already exists, it will be marked as connected.
Emits `UserAdded` or `UserUpdated` upon success.

#### Request

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.LoginRequestToken,
  "params": {
    "code": "<OAuth2 authorization code>",
    "codeVerifier": "<PKCE code verifier>"
  }
}
```

#### Successful Response

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.LoginRequestToken,
  "params": {
    "userDbId": 12345
  }
}
```

#### Failure Response

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.LoginRequestToken,
  "params": {
    "code": (int)ExitCode,
    "cause": (int)ExitCause,
    "error": "<Error message>",
    "errorDescription": "<Detailed error description>"
  }
}
```

</details>

---
<details id="loginrequesttoken">
<summary><b>UserDbIdList</b> — Get list of user database IDs</summary>

**Description:**
Retrieves database IDs of all users.

#### Request

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.UserDBIDList,
  "params": {}
}
```

#### Successful Response

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.UserDBIDList,
  "params": {
    "userDbIdList": [<int>UserDbId, ...]
  }
}
```

#### Failure Response

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.UserDBIDList,
  "params": {
    "userDbIdList": [] // Empty list on failure
  }
}
```

</details>
---
<details id="utilityunlinkhardlinks">
<summary><b>UtilityUnlinkHardlinks</b> — Unlink the hardlinks of a file reported with a HardlinkNotSupportedError</summary>

**Description:**
Saves a copy of the errored file into the rescue folder if the local file is not in sync with the database, hard removes all
the links of the file located under the sync root, then removes the node of the file from the sync database and the error
from the parameters database. The next synchronization will see the remote file as a new item and will download it again as
a standard file. If the given path does not exist anymore (e.g. the reported link has been removed or the file has been
moved), the links of the file are searched under the sync root by node id, and one of them is used instead of the given path
to check whether the file is in sync with the database. Before the links are removed, the item used for this check is
verified again and replaced by another link of the file if it has been replaced meanwhile, and the items blacklisted by the
sync engine are removed from the temporary blacklist only once the node has been removed from the sync database, so that the
deletion of the links cannot propagate to the remote replica. On Windows, the size and the dates cached in the directory
entry of the link used for this check are compared with the current ones, as NTFS does not update them when the file is
modified through another link: if they are outdated, or if this cannot be checked, the file is considered as not in sync. If
no link is left, only the node and the error are removed. If the links cannot be searched, the request fails without any
database change. If the node is not present in the sync database anymore (e.g. a previous run of the action has failed
during its final cleanup step), the request only completes the cleanup by removing the error. The request is rejected before
any change if the error does not exist or is not the `SystemError` / `HardlinkNotSupported` error of the given sync, node
and path, if the node is not a file of the sync database, if the path is empty, absolute, located outside of the sync root
or a link (symbolic link, junction or alias), or if it does not refer to the node.
Emits `ErrorRemoved` upon success.

#### Request

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.UtilityUnlinkHardlinks,
  "params": {
    "syncDbId": 10,
    "errorDbId": 123,
    "nodeId": "<local node id of the errored file>",
    "path": "<relative local path of the errored file>"
  }
}
```

#### Successful Response

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.UtilityUnlinkHardlinks,
  "params": {}
}
```

</details>
---


# 📡 Signals

## Signal Index

| Signal                      | Description                     |
| --------------------------- | ------------------------------- |
| [UserAdded](#useradded)     | Emitted when a user is added.   |
| [UserUpdated](#userupdated) | Emitted when a user is updated. |

---

<details id="useradded">
<summary><b>UserAdded</b> — Triggered when a new user is added</summary>

#### JSON

```json
{
  "type": 2,
  "num": (int)SignalEnum.UserAdded,
  "params": {
    "userInfo": { <UserInfo> }
  }
}
```

</details>

---

<details id="userupdated">
<summary><b>UserUpdated</b> — Triggered when a user is updated</summary>

#### JSON

```json
{
  "type": 2,
  "num": (int)SignalEnum.UserUpdated,
  "params": {
    "userInfo": { <UserInfo> }
  }
}
```

</details>