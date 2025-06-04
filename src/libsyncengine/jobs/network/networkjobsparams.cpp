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

#include "networkjobsparams.h"

#include <map>
#include <unordered_map>

namespace KDC {

ActionCode getActionCode(const std::string &action) noexcept {
    static const std::unordered_map<std::string, ActionCode, StringHashFunction, std::equal_to<>> actionMap = {
            {"file_create", ActionCode::ActionCodeCreate},
            {"file_rename", ActionCode::ActionCodeRename},
            {"file_update", ActionCode::ActionCodeEdit},
            {"file_access", ActionCode::ActionCodeAccess},
            {"file_trash", ActionCode::ActionCodeTrash},
            {"file_delete", ActionCode::ActionCodeDelete},
            {"file_move", ActionCode::ActionCodeMoveIn},
            {"file_move_out", ActionCode::ActionCodeMoveOut},
            {"file_restore", ActionCode::ActionCodeRestore},
            {"file_share_create", ActionCode::ActionCodeRestoreFileShareCreate},
            {"file_share_delete", ActionCode::ActionCodeRestoreFileShareDelete},
            {"share_link_create", ActionCode::ActionCodeRestoreShareLinkCreate},
            {"share_link_delete", ActionCode::ActionCodeRestoreShareLinkDelete},
            {"acl_insert", ActionCode::ActionCodeAccessRightInsert},
            {"acl_update", ActionCode::ActionCodeAccessRightUpdate},
            {"acl_remove", ActionCode::ActionCodeAccessRightRemove},
            {"acl_user_insert", ActionCode::ActionCodeAccessRightUserInsert},
            {"acl_user_update", ActionCode::ActionCodeAccessRightUserUpdate},
            {"acl_user_remove", ActionCode::ActionCodeAccessRightUserRemove},
            {"acl_team_insert", ActionCode::ActionCodeAccessRightTeamInsert},
            {"acl_team_update", ActionCode::ActionCodeAccessRightTeamUpdate},
            {"acl_team_remove", ActionCode::ActionCodeAccessRightTeamRemove},
            {"acl_main_users_insert", ActionCode::ActionCodeAccessRightMainUsersInsert},
            {"acl_main_users_update", ActionCode::ActionCodeAccessRightMainUsersUpdate},
            {"acl_main_users_remove", ActionCode::ActionCodeAccessRightMainUsersRemove}};

    if (const auto it = actionMap.find(action); it != actionMap.cend()) return it->second;

    return ActionCode::ActionCodeUnknown;
};

NetworkErrorCode getNetworkErrorCode(const std::string &errorCode) noexcept {
    static const std::map<std::string, NetworkErrorCode, std::less<std::string>> errorCodeMap = {
            {"forbidden_error", NetworkErrorCode::ForbiddenError},
            {"not_authorized", NetworkErrorCode::NotAuthorized},
            {"product_maintenance", NetworkErrorCode::ProductMaintenance},
            {"drive_is_in_maintenance_error", NetworkErrorCode::DriveIsInMaintenanceError},
            {"file_share_link_already_exists", NetworkErrorCode::FileShareLinkAlreadyExists},
            {"object_not_found", NetworkErrorCode::ObjectNotFound},
            {"invalid_grant", NetworkErrorCode::InvalidGrant},
            {"validation_failed", NetworkErrorCode::ValidationFailed},
            {"upload_not_terminated_error", NetworkErrorCode::UploadNotTerminatedError},
            {"upload_error", NetworkErrorCode::UploadError},
            {"destination_already_exists", NetworkErrorCode::DestinationAlreadyExists},
            {"conflict_error", NetworkErrorCode::ConflictError},
            {"access_denied", NetworkErrorCode::AccessDenied},
            {"limit_exceeded_error", NetworkErrorCode::FileTooBigError},
            {"quota_exceeded_error", NetworkErrorCode::QuotaExceededError}};

    if (const auto it = errorCodeMap.find(errorCode); it != errorCodeMap.cend()) return it->second;

    return NetworkErrorCode::UnknownError;
}
NetworkErrorReason getNetworkErrorReason(const std::string &errorReason) noexcept {
    static const std::map<std::string, NetworkErrorReason, std::less<std::string>> errorReasonMap = {
            {"refresh_token_revoked", NetworkErrorReason::RefreshTokenRevoked},
            {"not_renew", NetworkErrorReason::NotRenew},
            {"technical", NetworkErrorReason::Technical}};

    if (const auto it = errorReasonMap.find(errorReason); it != errorReasonMap.cend()) return it->second;

    return NetworkErrorReason::UnknownReason;
}

} // namespace KDC
