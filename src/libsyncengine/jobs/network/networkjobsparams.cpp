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

ActionCode getActionCode(const std::string &action) noexcept {
    static const std::unordered_map<std::string, ActionCode> actionMap = {
        {"file_create", ActionCode::actionCodeCreate},
        {"file_rename", ActionCode::actionCodeRename},
        {"file_update", ActionCode::actionCodeEdit},
        {"file_access", ActionCode::actionCodeAccess},
        {"file_trash", ActionCode::actionCodeTrash},
        {"file_delete", ActionCode::actionCodeDelete},
        {"file_move", ActionCode::actionCodeMoveIn},
        {"file_move_out", ActionCode::actionCodeMoveOut},
        {"file_restore", ActionCode::actionCodeRestore},
        {"file_share_create", ActionCode::actionCodeRestoreFileShareCreate},
        {"file_share_delete", ActionCode::actionCodeRestoreFileShareDelete},
        {"share_link_create", ActionCode::actionCodeRestoreShareLinkCreate},
        {"share_link_delete", ActionCode::actionCodeRestoreShareLinkDelete},
        {"acl_insert", ActionCode::actionCodeAccessRightInsert},
        {"acl_update", ActionCode::actionCodeAccessRightUpdate},
        {"acl_remove", ActionCode::actionCodeAccessRightRemove},
        {"acl_user_insert", ActionCode::actionCodeAccessRightUserInsert},
        {"acl_user_update", ActionCode::actionCodeAccessRightUserUpdate},
        {"acl_user_remove", ActionCode::actionCodeAccessRightUserRemove},
        {"acl_team_insert", ActionCode::actionCodeAccessRightTeamInsert},
        {"acl_team_update", ActionCode::actionCodeAccessRightTeamUpdate},
        {"acl_team_remove", ActionCode::actionCodeAccessRightTeamRemove},
        {"acl_main_users_insert", ActionCode::actionCodeAccessRightMainUsersInsert},
        {"acl_main_users_update", ActionCode::actionCodeAccessRightMainUsersUpdate},
        {"acl_main_users_remove", ActionCode::actionCodeAccessRightMainUsersRemove}
    };

    if (const auto it = actionMap.find(action); it != actionMap.cend()) return it->second;

    return ActionCode::actionCodeUnknown;
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
