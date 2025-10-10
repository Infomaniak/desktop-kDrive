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

### /!\ Warning /!\

All string value must be base64-encoded to ensure safe transmission.
### UserInfo

Represents a user known to the application.

**JSON Model:**

```json
{
  "dbId": 12345,
  "userId": "u_abc123",
  "name": "John Doe",
  "email": "john.doe@example.com",
  "avatar": "<Binary of a PNG>",
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

Represents a drive belonging to an account.

**JSON Model:**

```json
{
  "dbId": 11111,
  "accountDbId": 56789,
  "driveId": "d_abc456",
  "name": "My Drive",
  "color": #TODO: define color format,
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
  #### TODO: This structure is not final, some fields may be added/removed or changed.
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
# Requests
### `LoginRequestToken`

**Description:**
Request the server to add a new user using the provided OAuth2 token.
If the user already exists, it will simply be marked as connected.
In case of success, a signal `UserAdded` or `UserUpdated` will be emitted to notify the client of the change.

**Request JSON:**

```json
{
  [...]
  "num": (int)RequestNum.LoginRequestToken,
  "params": 
    {
        "code": "<OAuth2 authorization code>",
        "codeVerifier": "<PKCE code verifier>"
    }
}
```

**Successful Response:**

```json
{
  [...]
  "params": {
    "userDbId": (int)<DbId of the newly added or updated user>
  }
}
```

**Failure Response:**

```json
{
  [...]
  "params": {
    "code": (int)ExitCode,
    "cause": (int)ExitCause,
    "error": "<Error message>",
    "errorDescription": "<Detailed error description>"
  }
}
```

---

## Signals list
### `UserAdded`

**Description:**
Emitted when a new user is added to the application.

**Signal JSON:**
```json
{
  [...]
  "num": (int)SignalEnum.UserAdded,
  "params": {
    "userInfo": { <UserInfo> }
  }
}
```

---

### `UserUpdated`

**Description:**
Emitted when an existing user is updated in the application.

**Signal JSON:**
```json
{
  [...]
  "num": (int)SignalEnum.UserUpdated,
  "params": {
    "userInfo": { <UserInfo> }
  }
}
```