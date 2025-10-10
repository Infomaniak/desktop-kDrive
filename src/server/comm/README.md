# kDrive Interprocess Communication

## Overview

The **kDrive desktop application** is composed of two main components:

* **Server** — Handles synchronization, file operations, and backend communication.
* **Client (GUI)** — Provides the graphical interface and user interaction.

To exchange information, these two components communicate using **JSON-based messages**.
The **transport layer** used to send and receive these JSON messages depends on the operating system:

| Platform | Transport Mechanism              |
| -------- | -------------------------------- |
| Windows  | TCP sockets                      |
| Linux    | TCP sockets                      |
| macOS    | XPC (Interprocess Communication) |

---

## JSON Message Model

The communication is **bidirectional**:

* The **client** can send **requests** to the server.
* The **server** replies with **responses** associated with the same request ID.
* The **server** can also send **signals** (unsolicited messages) to inform the client about events such as sync status changes, errors, etc.

Each request initiated by the client has a **unique `id`**, allowing responses to be matched with their corresponding requests.

---

## General JSON Structure

### Request

```json
{
  "type": 1,               // 1 = Request/Response, 2 = Signal
  "id": 0,                 // Unique identifier for this request
  "num": 1,                // Request identifier (see RequestNum enum)
  "params": { ... }        // Parameters for this request
}
```

### Response

```json
{
  "type": 1,               // 1 = Request/Response, 2 = Signal
  "id": 0,                 // Same ID as the originating request
  "num": 1,                // Request identifier (see RequestNum enum)
  "params": { ... }        // Server response data or error details
}
```

### Signal

```json
{
  "type": 2,               // 1 = Request/Response, 2 = Signal
  "id": 0,                 // Optional: may be unused for signals
  "num": 1,                // Signal identifier (see SignalEnum)
  "params": { ... }        // Event or state change data
}
```

---

## Common Data Structures

### UserInfo

Represents a user known to the application.

**JSON Model:**

```json
{
  "dbId": 12345,
  "userId": "u_abc123",
  "name": "John Doe",
  "email": "john.doe@example.com",
  "avatar": "<base64-encoded-bytes>",
  "isConnected": true,
  "isStaff": false
}
```

---

### AccountInfo

Represents an account (organization) associated with a user.

**JSON Model:**

```json
{
  "dbId": 56789,
  "userDbId": 12345
}
```

---

### DriveInfo

Represents a drive (cloud space) belonging to an account.

**JSON Model:**

```json
{
  "dbId": 11111,
  "accountDbId": 56789,
  "driveId": "d_abc456",
  "name": "My Drive",
  "color": 3,
  "notification": true,
  "maintenance": false,
  "locked": false,
  "accessDenied": false
}
```

---

### SyncInfo

Represents a synchronization configuration between a local folder and a remote drive path.

**JSON Model:**

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
## Requests list
### `UserInfoList`

**Description:**
Retrieves information about all users stored in the server’s database.

**Request JSON:**

```json
{
  [...]
  "num": (int)RequestNum.UserInfoList,
  "params": null
}
```

**Successful Response:**

```json
{
  [...]
  "params": {
    "userInfos": [{UserInfo json model}, ...]
  }
}
```

**Failure Response:**

```json
{
  "type": 1,
  "id": 42,
  "num": (int)RequestNum.UserInfoList,
  "params": {
    "code": (int)ExitCode,
    "cause": (int)ExitCause
  }
}
```

---

## Signals list
