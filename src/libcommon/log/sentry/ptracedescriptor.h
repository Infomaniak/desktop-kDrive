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
#pragma once
#include <string>

namespace KDC::sentry {

// Each PTraceName need to be defined in PTraceDescriptor constructor.
enum class PTraceName {
    None,
    AppStart,
    SyncInit,
    /*SyncInit*/ ResetStatus,
    Sync,
    /*Sync*/ UpdateDetection1,
    /*Sync*/ /*UpdateDetection1*/ UpdateUnsyncedList,
    /*Sync*/ /*UpdateDetection1*/ InferChangesFromDb,
    /*Sync*/ /*UpdateDetection1*/ ExploreLocalSnapshot,
    /*Sync*/ /*UpdateDetection1*/ ExploreRemoteSnapshot,
    /*Sync*/ UpdateDetection2,
    /*Sync*/ /*UpdateDetection2*/ ResetNodes,
    /*Sync*/ /*UpdateDetection2*/ Step1MoveDirectory,
    /*Sync*/ /*UpdateDetection2*/ Step2MoveFile,
    /*Sync*/ /*UpdateDetection2*/ Step3DeleteDirectory,
    /*Sync*/ /*UpdateDetection2*/ Step4DeleteFile,
    /*Sync*/ /*UpdateDetection2*/ Step5CreateDirectory,
    /*Sync*/ /*UpdateDetection2*/ Step6CreateFile,
    /*Sync*/ /*UpdateDetection2*/ Step7EditFile,
    /*Sync*/ /*UpdateDetection2*/ Step8CompleteUpdateTree,
    /*Sync*/ Reconciliation1,
    /*Sync*/ /*Reconciliation1*/ CheckLocalTree,
    /*Sync*/ /*Reconciliation1*/ CheckRemoteTree,
    /*Sync*/ Reconciliation2,
    /*Sync*/ Reconciliation3,
    /*Sync*/ Reconciliation4,
    /*Sync*/ /*Reconciliation4*/ GenerateItemOperations,
    /*Sync*/ Propagation1,
    /*Sync*/ Propagation2,
    /*Sync*/ /*Propagation2*/ InitProgress,
    /*Sync*/ /*Propagation2*/ JobGeneration,
    /*Sync*/ /*Propagation2*/ waitForAllJobsToFinish,
    LFSOGenerateInitialSnapshot,
    /*LFSO_GenerateInitialSnapshot*/ LFSOExploreItem,
    LFSOChangeDetected,
    RFSOGenerateInitialSnapshot,
    /*RFSO_GenerateInitialSnapshot*/ RFSOBackRequest,
    /*RFSO_GenerateInitialSnapshot*/ RFSOExploreItem,
    RFSOChangeDetected,
};

// The PTraceDescriptor structure is used to store information about the different performance traces available throughout the
// application.
struct PTraceDescriptor {
        PTraceDescriptor() = default;
        PTraceDescriptor(std::string pTraceTitle, std::string pTraceDescription, const PTraceName pTraceName,
                         const PTraceName parentPTraceName = PTraceName::None, const double _customSampleRate = 1.0) :
            _pTraceName{pTraceName}, _parentPTraceName{parentPTraceName}, _pTraceTitle{std::move(pTraceTitle)},
            _pTraceDescription{std::move(pTraceDescription)}, _customSampleRate{_customSampleRate} {}

        const PTraceName _pTraceName = PTraceName::None;
        const PTraceName _parentPTraceName = PTraceName::None;
        const std::string _pTraceTitle;
        const std::string _pTraceDescription;
        const double _customSampleRate =
                1.0; // Final sample rate is _customSampleRate * sentry sample rate (see sentry::Handler::init).
};
} // namespace KDC::sentry
