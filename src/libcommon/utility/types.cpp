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

#include <string>
#include "types.h"
#include <Poco/UnicodeConverter.h>

namespace KDC {
namespace typesUtility {
std::wstring stringToWideString(const std::string &str) {
    std::wstring wstr;
    Poco::UnicodeConverter::convert(str, wstr);
    return wstr;
}
}  // namespace typesUtility

std::string enumClassToString(ReplicaSide e) {
    switch (e) {
        case ReplicaSide::Local:
            return "Local";
        case ReplicaSide::Remote:
            return "Remote";
        case ReplicaSide::Unknown:
            return "Unknown";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(NodeType e) {
    switch (e) {
        case NodeType::Unknown:
            return "Unknown";
        case NodeType::File:
            return "File";
        case NodeType::Directory:
            return "Directory";
    }
}

std::string enumClassToString(OperationType e) {
    switch (e) {
        case OperationType::None:
            return "None";
        case OperationType::Create:
            return "Create";
        case OperationType::Move:
            return "Move";
        case OperationType::Edit:
            return "Edit";
        case OperationType::Delete:
            return "Delete";
        case OperationType::Rights:
            return "Rights";
    }
}

std::string enumClassToString(ExitCode e) {
    switch (e) {
        case ExitCode::Unknown:
            return "Unknown";
        case ExitCode::Ok:
            return "Ok";
        case ExitCode::NeedRestart:
            return "NeedRestart";
        case ExitCode::NetworkError:
            return "NetworkError";
        case ExitCode::InvalidToken:
            return "InvalidToken";
        case ExitCode::DataError:
            return "DataError";
        case ExitCode::DbError:
            return "DbError";
        case ExitCode::BackError:
            return "BackError";
        case ExitCode::SystemError:
            return "SystemError";
        case ExitCode::FatalError:
            return "FatalError";
        case ExitCode::LogicError:
            return "LogicError";
        case ExitCode::TokenRefreshed:
            return "TokenRefreshed";
        case ExitCode::NoWritePermission:
            return "NoWritePermission";
        case ExitCode::RateLimited:
            return "RateLimited";
        case ExitCode::InvalidSync:
            return "InvalidSync";
        case ExitCode::InvalidOperation:
            return "InvalidOperation";
        case ExitCode::OperationCanceled:
            return "OperationCanceled";
        case ExitCode::UpdateRequired:
            return "UpdateRequired";
        case ExitCode::LogUploadFailed:
            return "LogUploadFailed";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(ExitCause e) {
    switch (e) {
        case ExitCause::Unknown:
            return "Unknown";
        case ExitCause::WorkerExited:
            return "WorkerExited";
        case ExitCause::DbAccessError:
            return "DbAccessError";
        case ExitCause::DbEntryNotFound:
            return "DbEntryNotFound";
        case ExitCause::InvalidSnapshot:
            return "InvalidSnapshot";
        case ExitCause::SyncDirDoesntExist:
            return "SyncDirDoesntExist";
        case ExitCause::SyncDirReadError:
            return "SyncDirReadError";
        case ExitCause::SyncDirWriteError:
            return "SyncDirWriteError";
        case ExitCause::HttpErr:
            return "HttpErr";
        case ExitCause::HttpErrForbidden:
            return "HttpErrForbidden";
        case ExitCause::RedirectionError:
            return "RedirectionError";
        case ExitCause::ApiErr:
            return "ApiErr";
        case ExitCause::InvalidSize:
            return "InvalidSize";
        case ExitCause::FileAlreadyExist:
            return "FileAlreadyExist";
        case ExitCause::FileAccessError:
            return "FileAccessError";
        case ExitCause::UnexpectedFileSystemEvent:
            return "UnexpectedFileSystemEvent";
        case ExitCause::NotEnoughDiskSpace:
            return "NotEnoughDiskSpace";
        case ExitCause::DriveAccessError:
            return "DriveAccessError";
        case ExitCause::LoginError:
            return "LoginError";
        case ExitCause::DriveMaintenance:
            return "DriveMaintenance";
        case ExitCause::DriveNotRenew:
            return "DriveNotRenew";
        case ExitCause::MigrationError:
            return "MigrationError";
        case ExitCause::MigrationProxyNotImplemented:
            return "MigrationProxyNotImplemented";
        case ExitCause::InconsistentPinState:
            return "InconsistentPinState";
        case ExitCause::FileSizeMismatch:
            return "FileSizeMismatch";
        case ExitCause::UploadNotTerminated:
            return "UploadNotTerminated";
        case ExitCause::UnableToCreateVfs:
            return "UnableToCreateVfs";
        case ExitCause::NotEnoughtMemory:
            return "NotEnoughtMemory";
        case ExitCause::FileTooBig:
            return "FileTooBig";
        case ExitCause::MoveToTrashFailed:
            return "MoveToTrashFailed";
        case ExitCause::InvalidName:
            return "InvalidName";
        case ExitCause::LiteSyncNotAllowed:
            return "LiteSyncNotAllowed";
        case ExitCause::NetworkTimeout:
            return "NetworkTimeout";
        case ExitCause::SocketsDefuncted:
            return "SocketsDefuncted";
        case ExitCause::NoSearchPermission:
            return "NoSearchPermission";
        case ExitCause::NotFound:
            return "NotFound";
        case ExitCause::QuotaExceeded:
            return "QuotaExceeded";
        case ExitCause::FullListParsingError:
            return "FullListParsingError";
        case ExitCause::OperationCanceled:
            return "OperationCanceled";
        default:
            return "No conversion to string available";
    }

}  // namespace KDC

std::string enumClassToString(ConflictType e) {
    switch (e) {
        case ConflictType::None:
            return "None";
        case ConflictType::MoveParentDelete:
            return "MoveParentDelete";
        case ConflictType::MoveDelete:
            return "MoveDelete";
        case ConflictType::CreateParentDelete:
            return "CreateParentDelete";
        case ConflictType::MoveMoveSource:
            return "MoveMoveSource";
        case ConflictType::MoveMoveDest:
            return "MoveMoveDest";
        case ConflictType::MoveCreate:
            return "MoveCreate";
        case ConflictType::EditDelete:
            return "EditDelete";
        case ConflictType::CreateCreate:
            return "CreateCreate";
        case ConflictType::EditEdit:
            return "EditEdit";
        case ConflictType::MoveMoveCycle:
            return "MoveMoveCycle";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(ConflictTypeResolution e) {
    switch (e) {
        case ConflictTypeResolution::None:
            return "None";
        case ConflictTypeResolution::DeleteCanceled:
            return "DeleteCanceled";
        case ConflictTypeResolution::FileMovedToRoot:
            return "FileMovedToRoot";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(InconsistencyType e) {
    switch (e) {
        case InconsistencyType::None:
            return "None";
        case InconsistencyType::Case:
            return "Case";
        case InconsistencyType::ForbiddenChar:
            return "ForbiddenChar";
        case InconsistencyType::ReservedName:
            return "ReservedName";
        case InconsistencyType::NameLength:
            return "NameLength";
        case InconsistencyType::PathLength:
            return "PathLength";
        case InconsistencyType::NotYetSupportedChar:
            return "NotYetSupportedChar";
        case InconsistencyType::DuplicateNames:
            return "DuplicateNames";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(CancelType e) {
    switch (e) {
        case CancelType::None:
            return "None";
        case CancelType::Create:
            return "Create";
        case CancelType::Edit:
            return "Edit";
        case CancelType::Move:
            return "Move";
        case CancelType::Delete:
            return "Delete";
        case CancelType::AlreadyExistRemote:
            return "AlreadyExistRemote";
        case CancelType::MoveToBinFailed:
            return "MoveToBinFailed";
        case CancelType::AlreadyExistLocal:
            return "AlreadyExistLocal";
        case CancelType::TmpBlacklisted:
            return "TmpBlacklisted";
        case CancelType::ExcludedByTemplate:
            return "ExcludedByTemplate";
        case CancelType::Hardlink:
            return "Hardlink";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(NodeStatus e) {
    switch (e) {
        case NodeStatus::Unknown:
            return "Unknown";
        case NodeStatus::Unprocessed:
            return "Unprocessed";
        case NodeStatus::PartiallyProcessed:
            return "PartiallyProcessed";
        case NodeStatus::Processed:
            return "Processed";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(SyncStatus e) {
    switch (e) {
        case SyncStatus::Undefined:
            return "Undefined";
        case SyncStatus::Starting:
            return "Starting";
        case SyncStatus::Running:
            return "Running";
        case SyncStatus::Idle:
            return "Idle";
        case SyncStatus::PauseAsked:
            return "PauseAsked";
        case SyncStatus::Paused:
            return "Paused";
        case SyncStatus::StopAsked:
            return "StopAsked";
        case SyncStatus::Stopped:
            return "Stopped";
        case SyncStatus::Error:
            return "Error";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(UploadSessionType e) {
    switch (e) {
        case UploadSessionType::Unknown:
            return "Unknown";
        case UploadSessionType::Standard:
            return "Standard";
        case UploadSessionType::LogUpload:
            return "LogUpload";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(SyncNodeType e) {
    switch (e) {
        case SyncNodeType::Undefined:
            return "Undefined";
        case SyncNodeType::BlackList:
            return "BlackList";
        case SyncNodeType::WhiteList:
            return "WhiteList";
        case SyncNodeType::UndecidedList:
            return "UndecidedList";
        case SyncNodeType::TmpRemoteBlacklist:
            return "TmpRemoteBlacklist";
        case SyncNodeType::TmpLocalBlacklist:
            return "TmpLocalBlacklist";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(SyncDirection e) {
    switch (e) {
        case SyncDirection::Unknown:
            return "Unknown";
        case SyncDirection::Up:
            return "Up";
        case SyncDirection::Down:
            return "Down";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(SyncFileStatus e) {
    switch (e) {
        case SyncFileStatus::Unknown:
            return "Unknown";
        case SyncFileStatus::Error:
            return "Error";
        case SyncFileStatus::Success:
            return "Success";
        case SyncFileStatus::Conflict:
            return "Conflict";
        case SyncFileStatus::Inconsistency:
            return "Inconsistency";
        case SyncFileStatus::Ignored:
            return "Ignored";
        case SyncFileStatus::Syncing:
            return "Syncing";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(SyncFileInstruction e) {
    switch (e) {
        case SyncFileInstruction::None:
            return "None";
        case SyncFileInstruction::Update:
            return "Update";
        case SyncFileInstruction::UpdateMetadata:
            return "UpdateMetadata";
        case SyncFileInstruction::Remove:
            return "Remove";
        case SyncFileInstruction::Move:
            return "Move";
        case SyncFileInstruction::Get:
            return "Get";
        case SyncFileInstruction::Put:
            return "Put";
        case SyncFileInstruction::Ignore:
            return "Ignore";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(SyncStep e) {
    switch (e) {
        case SyncStep::None:
            return "None";
        case SyncStep::Idle:
            return "Idle";
        case SyncStep::UpdateDetection1:
            return "UpdateDetection1";
        case SyncStep::UpdateDetection2:
            return "UpdateDetection2";
        case SyncStep::Reconciliation1:
            return "Reconciliation1";
        case SyncStep::Reconciliation2:
            return "Reconciliation2";
        case SyncStep::Reconciliation3:
            return "Reconciliation3";
        case SyncStep::Reconciliation4:
            return "Reconciliation4";
        case SyncStep::Propagation1:
            return "Propagation1";
        case SyncStep::Propagation2:
            return "Propagation2";
        case SyncStep::Done:
            return "Done";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(ActionType e) {
    switch (e) {
        case ActionType::Stop:
            return "Stop";
        case ActionType::Start:
            return "Start";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(ActionTarget e) {
    switch (e) {
        case ActionTarget::Drive:
            return "Drive";
        case ActionTarget::Sync:
            return "Sync";
        case ActionTarget::AllDrives:
            return "AllDrives";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(ErrorLevel e) {
    switch (e) {
        case ErrorLevel::Unknown:
            return "Unknown";
        case ErrorLevel::Server:
            return "Server";
        case ErrorLevel::SyncPal:
            return "SyncPal";
        case ErrorLevel::Node:
            return "Node";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(Language e) {
    switch (e) {
        case Language::Default:
            return "Default";
        case Language::English:
            return "English";
        case Language::French:
            return "French";
        case Language::German:
            return "German";
        case Language::Spanish:
            return "Spanish";
        case Language::Italian:
            return "Italian";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(LogLevel e) {
    switch (e) {
        case LogLevel::Debug:
            return "Debug";
        case LogLevel::Info:
            return "Info";
        case LogLevel::Warning:
            return "Warning";
        case LogLevel::Error:
            return "Error";
        case LogLevel::Fatal:
            return "Fatal";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(NotificationsDisabled e) {
    switch (e) {
        case NotificationsDisabled::Never:
            return "Never";
        case NotificationsDisabled::OneHour:
            return "OneHour";
        case NotificationsDisabled::UntilTomorrow:
            return "UntilTomorrow";
        case NotificationsDisabled::TreeDays:
            return "TreeDays";
        case NotificationsDisabled::OneWeek:
            return "OneWeek";
        case NotificationsDisabled::Always:
            return "Always";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(VirtualFileMode e) {
    switch (e) {
        case VirtualFileMode::Off:
            return "Off";
        case VirtualFileMode::Win:
            return "Win";
        case VirtualFileMode::Mac:
            return "Mac";
        case VirtualFileMode::Suffix:
            return "Suffix";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(PinState e) {
    switch (e) {
        case PinState::Inherited:
            return "Inherited";
        case PinState::AlwaysLocal:
            return "AlwaysLocal";
        case PinState::OnlineOnly:
            return "OnlineOnly";
        case PinState::Unspecified:
            return "Unspecified";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(ProxyType e) {
    switch (e) {
        case ProxyType::Undefined:
            return "Undefined";
        case ProxyType::None:
            return "None";
        case ProxyType::System:
            return "System";
        case ProxyType::HTTP:
            return "HTTP";
        case ProxyType::Socks5:
            return "Socks5";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(ExclusionTemplateComplexity e) {
    switch (e) {
        case ExclusionTemplateComplexity::Simplest:
            return "Simplest";
        case ExclusionTemplateComplexity::Simple:
            return "Simple";
        case ExclusionTemplateComplexity::Complex:
            return "Complex";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(LinkType e) {
    switch (e) {
        case LinkType::None:
            return "None";
        case LinkType::Symlink:
            return "Symlink";
        case LinkType::Hardlink:
            return "Hardlink";
        case LinkType::FinderAlias:
            return "FinderAlias";
        case LinkType::Junction:
            return "Junction";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(IoError e) {
    switch (e) {
        case IoError::Success:
            return "Success";
        case IoError::AccessDenied:
            return "AccessDenied";
        case IoError::AttrNotFound:
            return "AttrNotFound";
        case IoError::DirectoryExists:
            return "DirectoryExists";
        case IoError::DiskFull:
            return "DiskFull";
        case IoError::FileExists:
            return "FileExists";
        case IoError::FileNameTooLong:
            return "FileNameTooLong";
        case IoError::InvalidArgument:
            return "InvalidArgument";
        case IoError::InvalidDirectoryIterator:
            return "InvalidDirectoryIterator";
        case IoError::InvalidFileName:
            return "InvalidFileName";
        case IoError::IsADirectory:
            return "IsADirectory";
        case IoError::IsAFile:
            return "IsAFile";
        case IoError::MaxDepthExceeded:
            return "MaxDepthExceeded";
        case IoError::NoSuchFileOrDirectory:
            return "NoSuchFileOrDirectory";
        case IoError::ResultOutOfRange:
            return "ResultOutOfRange";
        case IoError::Unknown:
            return "Unknown";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(AppStateKey e) {
    switch (e) {
        case AppStateKey::LastServerSelfRestartDate:
            return "LastServerSelfRestartDate";
        case AppStateKey::LastClientSelfRestartDate:
            return "LastClientSelfRestartDate";
        case AppStateKey::LastSuccessfulLogUploadDate:
            return "LastSuccessfulLogUploadDate";
        case AppStateKey::LastLogUploadArchivePath:
            return "LastLogUploadArchivePath";
        case AppStateKey::LogUploadState:
            return "LogUploadState";
        case AppStateKey::LogUploadPercent:
            return "LogUploadPercent";
        case AppStateKey::LogUploadToken:
            return "LogUploadToken";
        case AppStateKey::Unknown:
            return "Unknown";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(LogUploadState e) {
    switch (e) {
        case LogUploadState::None:
            return "None";
        case LogUploadState::Archiving:
            return "Archiving";
        case LogUploadState::Uploading:
            return "Uploading";
        case LogUploadState::Success:
            return "Success";
        case LogUploadState::Failed:
            return "Failed";
        case LogUploadState::CancelRequested:
            return "CancelRequested";
        case LogUploadState::Canceled:
            return "Canceled";
        default:
            return "No conversion to string available";
    }
}

std::string enumClassToString(UpdateState e) {
    switch (e) {
        case UpdateState::Error:
            return "Error";
        case UpdateState::None:
            return "None";
        case UpdateState::Checking:
            return "Checking";
        case UpdateState::Downloading:
            return "Downloading";
        case UpdateState::Ready:
            return "Ready";
        case UpdateState::ManualOnly:
            return "ManualOnly";
        case UpdateState::Skipped:
            return "Skipped";
        default:
            return "No conversion to string available";
    }
}
}  // namespace KDC