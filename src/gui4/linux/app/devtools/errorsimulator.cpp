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

#include "app/devtools/errorsimulator.h"

#include "app/cache/mainselectionstore.h"
#include "app/errors/blockingerrorresolver.h"
#include "app/errors/syncerrorclassifier.h"
#include "app/services/cachepopulator.h"
#include "communicationlayer/signaldispatcher.h"
#include "libcommon/comm.h"
#include "libcommon/data/error.h"

#include <QLoggingCategory>
#include <QObject>

#include <Poco/Dynamic/Struct.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcErrorSimulator, "gui.v4.errorsimulator", QtInfoMsg)

constexpr char simulatorName[] = "ErrorSimulator";
constexpr char acceptedScenarios[] = "all, asleep, wakingup, maintenance, notrenew, accessdenied, loggedout, update-required";

// Longer than the 255-byte file name limit of common Linux filesystems.
constexpr size_t tooLongNameLength = 260;

struct Fixture {
        SyncErrorKind kind{SyncErrorKind::Unknown};
        Error error;
};

struct BlockingScenario {
        std::string_view name;
        ExitCode exitCode{ExitCode::Unknown};
        ExitCause exitCause{ExitCause::Unknown};
};

constexpr std::array blockingScenarios{
        BlockingScenario{"asleep", ExitCode::BackError, ExitCause::DriveAsleep},
        BlockingScenario{"wakingup", ExitCode::BackError, ExitCause::DriveWakingUp},
        BlockingScenario{"maintenance", ExitCode::BackError, ExitCause::DriveMaintenance},
        BlockingScenario{"notrenew", ExitCode::BackError, ExitCause::DriveNotRenew},
        BlockingScenario{"accessdenied", ExitCode::BackError, ExitCause::DriveAccessError},
        BlockingScenario{"loggedout", ExitCode::InvalidToken, ExitCause::Unknown},
};

Error simulatedError(const ErrorLevel level) {
    Error error;
    error.setLevel(level);
    error.setFunctionName(simulatorName);
    error.setWorkerName(simulatorName);
    return error;
}

Error nodeError(const std::filesystem::path &path, const NodeType nodeType = NodeType::File) {
    Error error = simulatedError(ErrorLevel::Node);
    error.setNodeType(nodeType);
    error.setPath(path);
    return error;
}

Error conflictError(const std::filesystem::path &path, const ConflictType conflictType, const NodeType nodeType) {
    Error error = nodeError(path, nodeType);
    error.setConflictType(conflictType);
    return error;
}

Error cancelError(const std::filesystem::path &path, const CancelType cancelType) {
    Error error = nodeError(path);
    error.setCancelType(cancelType);
    return error;
}

Error inconsistencyError(const std::filesystem::path &path, const InconsistencyType inconsistencyType) {
    Error error = nodeError(path);
    error.setInconsistencyType(inconsistencyType);
    return error;
}

Error nodeExitError(const std::filesystem::path &path, const ExitCode exitCode, const ExitCause exitCause) {
    Error error = nodeError(path);
    error.setExitCode(exitCode);
    error.setExitCause(exitCause);
    return error;
}

Error syncPalError(const ExitCode exitCode, const ExitCause exitCause) {
    Error error = simulatedError(ErrorLevel::SyncPal);
    error.setNodeType(NodeType::Unknown);
    error.setExitCode(exitCode);
    error.setExitCause(exitCause);
    return error;
}

// At least one fixture per kind, in the `SyncErrorKind` order, with the field values of the classifier matchers.
std::vector<Fixture> fixtures() {
    using enum SyncErrorKind;

    Error moveCancel = cancelError("Simulated/Projects/Budget.xlsx", CancelType::Move);
    moveCancel.setDestinationPath("Simulated/Archives/Budget.xlsx");

    return {
            {Conflict, conflictError("Simulated/Annual report.pdf", ConflictType::EditEdit, NodeType::File)},
            {Conflict, conflictError("Simulated/Holidays.jpg", ConflictType::CreateCreate, NodeType::File)},
            {Conflict, conflictError("Simulated/Shared folder", ConflictType::CreateCreate, NodeType::Directory)},
            {CaseError, inconsistencyError("Simulated/README.md", InconsistencyType::Case)},
            {CreateCancel, cancelError("Simulated/New document.docx", CancelType::Create)},
            {DeleteCancel, cancelError("Simulated/Old notes.txt", CancelType::Delete)},
            {EditCancel, cancelError("Simulated/Contract.pdf", CancelType::Edit)},
            {MoveCancel, moveCancel},
            {FileLocked, nodeExitError("Simulated/Presentation.pptx", ExitCode::BackError, ExitCause::FileLocked)},
            {FileRescued, cancelError("Simulated/Draft.odt", CancelType::FileRescued)},
            {FileTooBig, nodeExitError("Simulated/Backup.iso", ExitCode::BackError, ExitCause::FileTooBig)},
            {ForbiddenCharEndWithSpace,
             inconsistencyError("Simulated/Meeting notes ", InconsistencyType::ForbiddenCharEndWithSpace)},
            {ForbiddenChar, inconsistencyError("Simulated/Invoice:2026.pdf", InconsistencyType::ForbiddenChar)},
            {ForbiddenCharOnlySpaces, inconsistencyError("Simulated/   ", InconsistencyType::ForbiddenCharOnlySpaces)},
            {NameLength,
             inconsistencyError("Simulated/" + std::string(tooLongNameLength, 'a') + ".txt", InconsistencyType::NameLength)},
            {PathLength, inconsistencyError("Simulated/Very/Deep/Path/Photo.png", InconsistencyType::PathLength)},
            {NotEnoughDiskSpace,
             nodeExitError("Simulated/Videos/Movie.mkv", ExitCode::SystemError, ExitCause::NotEnoughDiskSpace)},
            {QuotaExceeded, nodeExitError("Simulated/Archive.zip", ExitCode::BackError, ExitCause::QuotaExceeded)},
            {ReservedName, inconsistencyError("Simulated/CON", InconsistencyType::ReservedName)},
            {TemporaryBlacklisted, cancelError("Simulated/Spreadsheet.ods", CancelType::TmpBlacklisted)},
            {ExcludedByTemplate, cancelError("Simulated/Cache.tmp", CancelType::ExcludedByTemplate)},
            {GenericErrForbidden, nodeExitError("Simulated/Readonly.txt", ExitCode::BackError, ExitCause::HttpErrForbidden)},
            {HardLink, cancelError("Simulated/Hard link.txt", CancelType::Hardlink)},
            {InvalidLinkTarget, cancelError("Simulated/Broken link", CancelType::InvalidLinkTarget)},
            {LocalAccess, nodeExitError("Simulated/Private.key", ExitCode::SystemError, ExitCause::FileAccessError)},
            {BackErrorDriveAccess, syncPalError(ExitCode::BackError, ExitCause::DriveAccessError)},
            {BackErrorDriveAsleep, syncPalError(ExitCode::BackError, ExitCause::DriveAsleep)},
            {BackErrorDriveMaintenance, syncPalError(ExitCode::BackError, ExitCause::DriveMaintenance)},
            {BackErrorDriveNotRenew, syncPalError(ExitCode::BackError, ExitCause::DriveNotRenew)},
            {InvalidSyncDirAccess, syncPalError(ExitCode::InvalidSync, ExitCause::SyncDirAccessError)},
            {InvalidSyncDirNesting, syncPalError(ExitCode::InvalidSync, ExitCause::SyncDirNestingError)},
            {InvalidToken, syncPalError(ExitCode::InvalidToken, ExitCause::Unknown)},
            {NetworkOther, syncPalError(ExitCode::NetworkError, ExitCause::NetworkTimeout)},
            {SystemNotEnoughDiskSpace, syncPalError(ExitCode::SystemError, ExitCause::NotEnoughDiskSpace)},
            {SystemSyncDirAccess, syncPalError(ExitCode::SystemError, ExitCause::SyncDirAccessError)},
            {SystemSyncDirDiskMissing, syncPalError(ExitCode::SystemError, ExitCause::SyncDirDiskMissing)},
            {DataSyncDirChanged, syncPalError(ExitCode::DataError, ExitCause::SyncDirChanged)},
            {TemporaryDirAccess, syncPalError(ExitCode::SystemError, ExitCause::TmpDirAccessError)},
            {NotEnoughINotifyWatches, syncPalError(ExitCode::SystemError, ExitCause::NotEnoughINotifyWatches)},
            // A LiteSync error: its kind does not exist on Linux.
            {Unknown, syncPalError(ExitCode::SystemError, ExitCause::UnableToStartVfs)},
    };
}

// A new kind without fixture, or a classifier change that the fixtures do not follow, fails here.
void checkFixtures(const std::vector<Fixture> &allFixtures) {
    for (const auto &fixture: allFixtures) {
        assert(classifySyncError(fixture.error) == fixture.kind && "Simulated error classified as another kind");
    }

    for (auto value = static_cast<std::underlying_type_t<SyncErrorKind>>(0);
         value < static_cast<std::underlying_type_t<SyncErrorKind>>(SyncErrorKind::EnumEnd); ++value) {
        [[maybe_unused]] const auto kind = static_cast<SyncErrorKind>(value);
        assert(std::ranges::any_of(allFixtures, [kind](const Fixture &fixture) { return fixture.kind == kind; }) &&
               "Sync error kind without simulated error");
    }
}

std::optional<std::vector<Error>> scenarioErrors(const std::string_view scenario) {
    const auto allFixtures = fixtures();
    checkFixtures(allFixtures);

    if (scenario == "all") {
        std::vector<Error> errors;
        for (const auto &fixture: allFixtures) {
            if (!blockingErrorKind(fixture.error).has_value()) {
                errors.push_back(fixture.error);
            }
        }
        return errors;
    }

    if (scenario == "update-required") {
        Error error = simulatedError(ErrorLevel::Server);
        error.setExitCode(ExitCode::UpdateRequired);
        return std::vector<Error>{error};
    }

    const auto blocking =
            std::ranges::find_if(blockingScenarios, [scenario](const BlockingScenario &value) { return value.name == scenario; });
    if (blocking == blockingScenarios.end()) {
        return std::nullopt;
    }

    Error error = syncPalError(blocking->exitCode, blocking->exitCause);
    assert(blockingErrorKind(error).has_value() && "Simulated blocking error is not blocking");
    return std::vector<Error>{error};
}

void dispatchErrorAdded(const SignalDispatcher &signalDispatcher, const Error &error) {
    Poco::DynamicStruct errorInfo;
    error.toDynamicStruct(errorInfo);
    Poco::DynamicStruct params;
    params[msgParamErrorInfo] = errorInfo;
    signalDispatcher.dispatch(SignalNum::UTILITY_ERROR_ADDED, params);
}

void injectErrors(const SignalDispatcher &signalDispatcher, const MainSelectionStore &selectionStore, const QString &scenario) {
    auto errors = scenarioErrors(scenario.toStdString());
    if (!errors.has_value()) {
        qCWarning(lcErrorSimulator) << "Unknown scenario, nothing injected | scenario:" << scenario
                                    << "/ accepted:" << acceptedScenarios;
        return;
    }

    const auto syncDbId = static_cast<SyncDbId>(selectionStore.currentSyncDbId());
    if (syncDbId == 0) {
        qCWarning(lcErrorSimulator) << "No synchronization selected, nothing injected";
        return;
    }

    // Negative ids never collide with real errors, whose SQLite rowid is always positive. Times keep the fixture order.
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto count = static_cast<int64_t>(errors->size());
    for (int64_t index = 0; index < count; ++index) {
        Error &error = (*errors)[static_cast<size_t>(index)];
        error.setDbId(-(index + 1));
        error.setTime(now - count + index);
        if (error.level() != ErrorLevel::Server) {
            error.setSyncDbId(syncDbId);
        }
        dispatchErrorAdded(signalDispatcher, error);
    }
    qCWarning(lcErrorSimulator) << "Simulated errors injected | scenario:" << scenario << "/ count:" << count
                                << "/ syncDbId:" << syncDbId;
}

} // namespace

void installErrorSimulator(const CachePopulator &cachePopulator, const SignalDispatcher &signalDispatcher,
                           const MainSelectionStore &selectionStore, const QObject *const context) {
    const QString scenario = qEnvironmentVariable("KDRIVE_SIMULATE_ERRORS");
    if (scenario.isEmpty()) {
        return;
    }

    qCWarning(lcErrorSimulator) << "Error simulator enabled | scenario:" << scenario;
    // Queued: the cache pipeline is live and the synchronization selected, and the dispatcher is not inside a handler.
    (void) QObject::connect(
            &cachePopulator, &CachePopulator::bootstrapCompleted, context,
            [&signalDispatcher, &selectionStore, scenario] { injectErrors(signalDispatcher, selectionStore, scenario); },
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
}

} // namespace KDC
