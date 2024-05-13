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

#include "networkjobsparams.h"

#include <map>

namespace KDC {

using enum ActionCode;

struct StringHash {
        using is_transparent = void; // Enables heterogeneous operations.

        std::size_t operator()(std::string_view sv) const {
            std::hash<std::string_view> hasher;
            return hasher(sv);
        }
};

ActionCode getActionCode(const std::string &action) noexcept {
    static const std::unordered_map<std::string, ActionCode, StringHash, std::equal_to<>> actionMap = {
        {"file_create", actionCodeCreate},
        {"file_rename", actionCodeRename},
        {"file_update", actionCodeEdit},
        {"file_access", actionCodeAccess},
        {"file_trash", actionCodeTrash},
        {"file_delete", actionCodeDelete},
        {"file_move", actionCodeMoveIn},
        {"file_move_out", actionCodeMoveOut},
        {"file_restore", actionCodeRestore},
        {"file_share_create", actionCodeRestoreFileShareCreate},
        {"file_share_delete", actionCodeRestoreFileShareDelete},
        {"share_link_create", actionCodeRestoreShareLinkCreate},
        {"share_link_delete", actionCodeRestoreShareLinkDelete},
        {"acl_insert", actionCodeAccessRightInsert},
        {"acl_update", actionCodeAccessRightUpdate},
        {"acl_remove", actionCodeAccessRightRemove},
        {"acl_user_insert", actionCodeAccessRightUserInsert},
        {"acl_user_update", actionCodeAccessRightUserUpdate},
        {"acl_user_remove", actionCodeAccessRightUserRemove},
        {"acl_team_insert", actionCodeAccessRightTeamInsert},
        {"acl_team_update", actionCodeAccessRightTeamUpdate},
        {"acl_team_remove", actionCodeAccessRightTeamRemove},
        {"acl_main_users_insert", actionCodeAccessRightMainUsersInsert},
        {"acl_main_users_update", actionCodeAccessRightMainUsersUpdate},
        {"acl_main_users_remove", actionCodeAccessRightMainUsersRemove}
    };

    if (const auto it = actionMap.find(action); it != actionMap.cend()) return it->second;

    return actionCodeUnknown;
};

NetworkErrorCode getNetworkErrorCode(const std::string &errorCode) noexcept {
    static const std::map<std::string, NetworkErrorCode, std::less<std::string>> errorCodeMap = {
        {"forbidden_error", NetworkErrorCode::forbiddenError},
        {"not_authorized", NetworkErrorCode::notAuthorized},
        {"product_maintenance", NetworkErrorCode::productMaintenance},
        {"drive_is_in_maintenance_error", NetworkErrorCode::driveIsInMaintenanceError},
        {"file_share_link_already_exists", NetworkErrorCode::fileShareLinkAlreadyExists},
        {"object_not_found", NetworkErrorCode::objectNotFound},
        {"invalid_grant", NetworkErrorCode::invalidGrant},
        {"validation_failed", NetworkErrorCode::validationFailed},
        {"upload_not_terminated_error", NetworkErrorCode::uploadNotTerminatedError},
        {"upload_error", NetworkErrorCode::uploadError},
        {"destination_already_exists", NetworkErrorCode::destinationAlreadyExists},
        {"conflict_error", NetworkErrorCode::conflictError},
        {"access_denied", NetworkErrorCode::accessDenied},
        {"limit_exceeded_error", NetworkErrorCode::fileTooBigError},
        {"quota_exceeded_error", NetworkErrorCode::quotaExceededError}};

    if (const auto it = errorCodeMap.find(errorCode); it != errorCodeMap.cend()) return it->second;

    return NetworkErrorCode::unknownError;
}
NetworkErrorReason getNetworkErrorReason(const std::string &errorReason) noexcept {
    static const std::map<std::string, NetworkErrorReason, std::less<std::string>> errorReasonMap = {
        {"refresh_token_revoked", NetworkErrorReason::refreshTokenRevoked}, {"not_renew", NetworkErrorReason::notRenew}};

    if (const auto it = errorReasonMap.find(errorReason); it != errorReasonMap.cend()) return it->second;

    return NetworkErrorReason::unknownReason;
}

}  // namespace KDC
