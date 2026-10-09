/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/errors/syncerrorclassifier.h"

#include <algorithm>
#include <vector>

namespace KDC {

namespace {

// Accepted values for each field of an error. A field left empty by the helpers below only accepts its neutral value
// (`None` or `Unknown`), as the macOS `SynchroErrorKindMatcher`.
struct Matcher {
        SyncErrorKind kind{SyncErrorKind::Unknown};
        ErrorLevel level{ErrorLevel::Unknown};
        std::vector<NodeType> nodeTypes;
        std::vector<CancelType> cancelTypes{CancelType::None};
        std::vector<InconsistencyType> inconsistencyTypes{InconsistencyType::None};
        std::vector<ConflictType> conflictTypes{ConflictType::None};
        std::vector<ExitCode> exitCodes{ExitCode::Unknown};
        std::vector<ExitCause> exitCauses{ExitCause::Unknown};
};

template<typename T>
bool contains(const std::vector<T> &values, const T value) {
    return std::ranges::find(values, value) != values.end();
}

bool matches(const Matcher &matcher, const Error &error) {
    return error.level() == matcher.level && contains(matcher.nodeTypes, error.nodeType()) &&
           contains(matcher.cancelTypes, error.cancelType()) && contains(matcher.inconsistencyTypes, error.inconsistencyType()) &&
           contains(matcher.conflictTypes, error.conflictType()) && contains(matcher.exitCodes, error.exitCode()) &&
           contains(matcher.exitCauses, error.exitCause());
}

// Matcher of a node-level error, about a file or a directory.
Matcher node(const SyncErrorKind kind) {
    Matcher matcher;
    matcher.kind = kind;
    matcher.level = ErrorLevel::Node;
    matcher.nodeTypes = {NodeType::File, NodeType::Directory};
    return matcher;
}

Matcher nodeWithCancel(const SyncErrorKind kind, const CancelType cancelType) {
    Matcher matcher = node(kind);
    matcher.cancelTypes = {cancelType};
    return matcher;
}

Matcher nodeWithInconsistency(const SyncErrorKind kind, std::vector<InconsistencyType> inconsistencyTypes) {
    Matcher matcher = node(kind);
    matcher.inconsistencyTypes = std::move(inconsistencyTypes);
    return matcher;
}

Matcher nodeWithExit(const SyncErrorKind kind, const ExitCode exitCode, const ExitCause exitCause) {
    Matcher matcher = node(kind);
    matcher.exitCodes = {exitCode};
    matcher.exitCauses = {exitCause};
    return matcher;
}

// Matcher of a synchronization-level error. Most of them have no node type.
Matcher syncPal(const SyncErrorKind kind, const ExitCode exitCode, std::vector<ExitCause> exitCauses) {
    Matcher matcher;
    matcher.kind = kind;
    matcher.level = ErrorLevel::SyncPal;
    matcher.nodeTypes = {NodeType::Unknown};
    matcher.exitCodes = {exitCode};
    matcher.exitCauses = std::move(exitCauses);
    return matcher;
}

// Synchronization-level error about the synchronization root, which may carry its node type.
Matcher syncRoot(const SyncErrorKind kind, const ExitCode exitCode, const ExitCause exitCause) {
    Matcher matcher = syncPal(kind, exitCode, {exitCause});
    matcher.nodeTypes = {NodeType::File, NodeType::Directory, NodeType::Unknown};
    return matcher;
}

// The first matcher accepting an error gives its kind: the order is part of the contract. It follows the declaration
// order of `SyncErrorKind`, which is also the macOS `SynchroErrorKind.allCases` order.
const std::vector<Matcher> &matchers() {
    static const std::vector<Matcher> table = [] {
        Matcher conflict = node(SyncErrorKind::Conflict);
        conflict.conflictTypes = {ConflictType::CreateCreate, ConflictType::EditEdit};
        conflict.exitCodes = {ExitCode::Unknown, ExitCode::Ok};

        return std::vector<Matcher>{
                conflict,
                nodeWithInconsistency(SyncErrorKind::CaseError, {InconsistencyType::Case}),
                nodeWithCancel(SyncErrorKind::CreateCancel, CancelType::Create),
                nodeWithCancel(SyncErrorKind::DeleteCancel, CancelType::Delete),
                nodeWithCancel(SyncErrorKind::EditCancel, CancelType::Edit),
                nodeWithCancel(SyncErrorKind::MoveCancel, CancelType::Move),
                nodeWithExit(SyncErrorKind::FileLocked, ExitCode::BackError, ExitCause::FileLocked),
                nodeWithCancel(SyncErrorKind::FileRescued, CancelType::FileRescued),
                nodeWithExit(SyncErrorKind::FileTooBig, ExitCode::BackError, ExitCause::FileTooBig),
                nodeWithInconsistency(SyncErrorKind::ForbiddenCharEndWithSpace, {InconsistencyType::ForbiddenCharEndWithSpace}),
                nodeWithInconsistency(SyncErrorKind::ForbiddenChar,
                                      {InconsistencyType::ForbiddenChar, InconsistencyType::NotYetSupportedChar}),
                nodeWithInconsistency(SyncErrorKind::ForbiddenCharOnlySpaces, {InconsistencyType::ForbiddenCharOnlySpaces}),
                nodeWithInconsistency(SyncErrorKind::NameLength, {InconsistencyType::NameLength}),
                nodeWithInconsistency(SyncErrorKind::PathLength, {InconsistencyType::PathLength}),
                nodeWithExit(SyncErrorKind::NotEnoughDiskSpace, ExitCode::SystemError, ExitCause::NotEnoughDiskSpace),
                nodeWithExit(SyncErrorKind::QuotaExceeded, ExitCode::BackError, ExitCause::QuotaExceeded),
                nodeWithInconsistency(SyncErrorKind::ReservedName, {InconsistencyType::ReservedName}),
                nodeWithCancel(SyncErrorKind::TemporaryBlacklisted, CancelType::TmpBlacklisted),
                nodeWithCancel(SyncErrorKind::ExcludedByTemplate, CancelType::ExcludedByTemplate),
                nodeWithExit(SyncErrorKind::GenericErrForbidden, ExitCode::BackError, ExitCause::HttpErrForbidden),
                nodeWithCancel(SyncErrorKind::HardLink, CancelType::Hardlink),
                nodeWithCancel(SyncErrorKind::InvalidLinkTarget, CancelType::InvalidLinkTarget),
                nodeWithExit(SyncErrorKind::LocalAccess, ExitCode::SystemError, ExitCause::FileAccessError),
                syncPal(SyncErrorKind::BackErrorDriveAccess, ExitCode::BackError, {ExitCause::DriveAccessError}),
                syncPal(SyncErrorKind::BackErrorDriveAsleep, ExitCode::BackError,
                        {ExitCause::DriveAsleep, ExitCause::DriveWakingUp}),
                syncPal(SyncErrorKind::BackErrorDriveMaintenance, ExitCode::BackError, {ExitCause::DriveMaintenance}),
                syncPal(SyncErrorKind::BackErrorDriveNotRenew, ExitCode::BackError, {ExitCause::DriveNotRenew}),
                syncPal(SyncErrorKind::InvalidSyncDirAccess, ExitCode::InvalidSync, {ExitCause::SyncDirAccessError}),
                syncPal(SyncErrorKind::InvalidSyncDirNesting, ExitCode::InvalidSync, {ExitCause::SyncDirNestingError}),
                syncPal(SyncErrorKind::InvalidToken, ExitCode::InvalidToken, {ExitCause::Unknown}),
                syncPal(SyncErrorKind::NetworkOther, ExitCode::NetworkError,
                        {ExitCause::Unknown, ExitCause::SocketsDefuncted, ExitCause::NetworkTimeout}),
                syncPal(SyncErrorKind::SystemNotEnoughDiskSpace, ExitCode::SystemError, {ExitCause::NotEnoughDiskSpace}),
                syncRoot(SyncErrorKind::SystemSyncDirAccess, ExitCode::SystemError, ExitCause::SyncDirAccessError),
                syncRoot(SyncErrorKind::SystemSyncDirDiskMissing, ExitCode::SystemError, ExitCause::SyncDirDiskMissing),
                syncRoot(SyncErrorKind::DataSyncDirChanged, ExitCode::DataError, ExitCause::SyncDirChanged),
                syncPal(SyncErrorKind::TemporaryDirAccess, ExitCode::SystemError, {ExitCause::TmpDirAccessError}),
                syncPal(SyncErrorKind::NotEnoughINotifyWatches, ExitCode::SystemError, {ExitCause::NotEnoughINotifyWatches}),
        };
    }();
    return table;
}

} // namespace

SyncErrorKind classifySyncError(const Error &error) {
    const auto &table = matchers();
    const auto match = std::ranges::find_if(table, [&error](const Matcher &matcher) { return matches(matcher, error); });
    return match == table.end() ? SyncErrorKind::Unknown : match->kind;
}

bool isUserResolvableConflict(const Error &error) {
    return error.level() == ErrorLevel::Node &&
           (error.conflictType() == ConflictType::CreateCreate || error.conflictType() == ConflictType::EditEdit);
}

std::string_view toString(const SyncErrorKind kind) {
    using enum SyncErrorKind;

    switch (kind) {
        case Conflict:
            return "Conflict";
        case CaseError:
            return "CaseError";
        case CreateCancel:
            return "CreateCancel";
        case DeleteCancel:
            return "DeleteCancel";
        case EditCancel:
            return "EditCancel";
        case MoveCancel:
            return "MoveCancel";
        case FileLocked:
            return "FileLocked";
        case FileRescued:
            return "FileRescued";
        case FileTooBig:
            return "FileTooBig";
        case ForbiddenCharEndWithSpace:
            return "ForbiddenCharEndWithSpace";
        case ForbiddenChar:
            return "ForbiddenChar";
        case ForbiddenCharOnlySpaces:
            return "ForbiddenCharOnlySpaces";
        case NameLength:
            return "NameLength";
        case PathLength:
            return "PathLength";
        case NotEnoughDiskSpace:
            return "NotEnoughDiskSpace";
        case QuotaExceeded:
            return "QuotaExceeded";
        case ReservedName:
            return "ReservedName";
        case TemporaryBlacklisted:
            return "TemporaryBlacklisted";
        case ExcludedByTemplate:
            return "ExcludedByTemplate";
        case GenericErrForbidden:
            return "GenericErrForbidden";
        case HardLink:
            return "HardLink";
        case InvalidLinkTarget:
            return "InvalidLinkTarget";
        case LocalAccess:
            return "LocalAccess";
        case BackErrorDriveAccess:
            return "BackErrorDriveAccess";
        case BackErrorDriveAsleep:
            return "BackErrorDriveAsleep";
        case BackErrorDriveMaintenance:
            return "BackErrorDriveMaintenance";
        case BackErrorDriveNotRenew:
            return "BackErrorDriveNotRenew";
        case InvalidSyncDirAccess:
            return "InvalidSyncDirAccess";
        case InvalidSyncDirNesting:
            return "InvalidSyncDirNesting";
        case InvalidToken:
            return "InvalidToken";
        case NetworkOther:
            return "NetworkOther";
        case SystemNotEnoughDiskSpace:
            return "SystemNotEnoughDiskSpace";
        case SystemSyncDirAccess:
            return "SystemSyncDirAccess";
        case SystemSyncDirDiskMissing:
            return "SystemSyncDirDiskMissing";
        case DataSyncDirChanged:
            return "DataSyncDirChanged";
        case TemporaryDirAccess:
            return "TemporaryDirAccess";
        case NotEnoughINotifyWatches:
            return "NotEnoughINotifyWatches";
        case Unknown:
            return "Unknown";
        case EnumEnd:
            break;
    }
    return "EnumEnd";
}

} // namespace KDC
