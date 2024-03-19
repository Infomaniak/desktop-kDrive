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

#pragma once

#include <string>

#define THREAD_POOL_MIN_CAPACITY 10

namespace KDC {
static const uint64_t chunkMinSize = 10 * 1024 * 1024;                // 10MB
static const uint64_t chunkMaxSize = 100 * 1024 * 1024;               // 100MB
static const uint64_t useUploadSessionThreshold = 100 * 1024 * 1024;  // if file size > 100MB -> start upload session
static const uint64_t optimalTotalChunks = 200;
static const uint64_t maxTotalChunks = 10000;  // Theorical max. file size 10'000 * 100MB = 1TB

/*
 * Static string
 */
// Content types
static const std::string mimeTypeJson = "application/json";
static const std::string mimeTypeOctetStream = "application/octet-stream";
static const std::string mimeTypeImageJpeg = "image/jpeg";

// Link content types
static const std::string mimeTypeSymlink = "inode/symlink";
static const std::string mimeTypeHardlink = "inode/hardlink";
static const std::string mimeTypeFinderAlias = "application/x-macos";
static const std::string mimeTypeJunction = "inode/junction";

// OAuth
const std::string grantTypeKey = "grant_type";
const std::string codeVerifierKey = "code_verifier";
const std::string responseTypeKey = "response_type";
const std::string clientIdKey = "client_id";
const std::string grantTypeAuthorization = "authorization_code";
const std::string usernameKey = "username";
const std::string grantTypePassword = "password";
const std::string accessTypeKey = "access_type";
const std::string passwordKey = "password";
const std::string durationKey = "duration";
const std::string infiniteKey = "infinite";

const std::string refreshTokenKey = "refresh_token";

// Reply keys
static const std::string resultKey = "result";

static const std::string dataKey = "data";
static const std::string cursorKey = "cursor";
static const std::string hasMoreKey = "has_more";
static const std::string tokenKey = "token";
static const std::string preferenceKey = "preference";

static const std::string changesKey = "changes";
static const std::string actionsKey = "actions";
static const std::string actionKey = "action";
static const std::string fileIdKey = "file_id";
static const std::string fileTypeKey = "file_type";
static const std::string timestampKey = "timestamp";
static const std::string createdAtKey = "created_at";
static const std::string lastModifiedAtKey = "last_modified_at";
static const std::string pathKey = "path";
static const std::string destinationKey = "destination";

static const std::string filesKey = "files";
static const std::string idKey = "id";
static const std::string parentIdKey = "parent_id";
static const std::string nameKey = "name";
static const std::string emailKey = "email";
static const std::string avatarKey = "avatar";
static const std::string displayNameKey = "display_name";
static const std::string typeKey = "type";
static const std::string sizeKey = "size";
static const std::string visibilityKey = "visibility";
static const std::string accountIdKey = "account_id";
static const std::string driveIdKey = "drive_id";
static const std::string driveNameKey = "drive_name";
static const std::string accountAdminKey = "account_admin";
static const std::string capabilitiesKey = "capabilities";
static const std::string canWriteKey = "can_write";
static const std::string redirectUriKey = "redirect_uri";

static const std::string totalNbItemKey = "total";
static const std::string pageKey = "page";
static const std::string pagesKey = "pages";
static const std::string itemsPerPageKey = "items_per_page";

static const std::string inMaintenanceKey = "in_maintenance";
static const std::string maintenanceAtKey = "maintenance_at";
static const std::string maintenanceReasonKey = "maintenance_reason";
static const std::string isLockedKey = "is_locked";
static const std::string usedSizeKey = "used_size";

static const std::string colorKey = "color";

static const std::string errorCodePathKey = "/Error/Code";
static const std::string redirectUrlPathKey = "/html/body/a";

static const std::string dirKey = "dir";
static const std::string fileKey = "file";
static const std::string urlKey = "url";

static const std::string symbolicLinkKey = "symbolic_link";

/// Action type
static const std::string createAction = "file_create";
static const std::string renameAction = "file_rename";
static const std::string editAction = "file_update";
static const std::string accessAction = "file_access";
static const std::string trashAction = "file_trash";
static const std::string deleteAction = "file_delete";
static const std::string moveInAction = "file_move";
static const std::string moveOutAction = "file_move_out";
static const std::string restoreAction = "file_restore";
static const std::string restoreFileShareCreate = "file_share_create";
static const std::string restoreFileShareDelete = "file_share_delete";
static const std::string restoreShareLinkCreate = "share_link_create";
static const std::string restoreShareLinkDelete = "share_link_delete";
//// Rights
/// TODO : implement all rights
static const std::string accessRightInsert = "acl_insert";
static const std::string accessRightUpdate = "acl_update";
static const std::string accessRightRemove = "acl_remove";
static const std::string accessRightUserInsert = "acl_user_insert";
static const std::string accessRightUserUpdate = "acl_user_update";
static const std::string accessRightUserRemove = "acl_user_remove";
static const std::string accessRightTeamInsert = "acl_team_insert";
static const std::string accessRightTeamUpdate = "acl_team_update";
static const std::string accessRightTeamRemove = "acl_team_remove";
static const std::string accessRightMainUsersInsert = "acl_main_users_insert";
static const std::string accessRightMainUsersUpdate = "acl_main_users_update";
static const std::string accessRightMainUsersRemove = "acl_main_users_remove";

/// Visibility
static const std::string isRootKey = "is_root";
static const std::string isTeamSpaceKey = "is_team_space";
static const std::string isSharedSpaceKey = "is_shared_space";
/// Items per page
static const std::string nbItemPerPage = "1000";  // Default # of items max returned per page

/// Errors
static const std::string errorKey = "error";
static const std::string reasonKey = "reason";
static const std::string codeKey = "code";
static const std::string descriptionKey = "description";
static const std::string contextKey = "context";
/// Error codes
static const std::string forbiddenError = "forbidden_error";
static const std::string notAuthorized = "not_authorized";
static const std::string productMaintenance = "product_maintenance";
static const std::string driveIsInMaintenanceError = "drive_is_in_maintenance_error";
static const std::string fileShareLinkAlreadyExists = "file_share_link_already_exists";
static const std::string objectNotFound = "object_not_found";
static const std::string refreshTokenRevoked = "refresh_token_revoked";
static const std::string invalidGrant = "invalid_grant";
static const std::string notRenew = "not_renew";
static const std::string validationFailed = "validation_failed";
static const std::string uploadNotTerminatedError = "upload_not_terminated_error";
static const std::string uploadError = "upload_error";
static const std::string destinationAlreadyExists = "destination_already_exists";
static const std::string conflictError = "conflict_error";
static const std::string accessDenied = "access_denied";
static const std::string fileTooBigError = "limit_exceeded_error";
/// Error descriptions
static const std::string storageObjectIsNotOk = "storage_object_is_not_ok";

static const std::string invalidToken = "Invalid Token";
}  // namespace KDC
