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

#include "ptraceinfo.h"
#include <assert.h>

namespace KDC::Sentry {

PTraceInfo::PTraceInfo(PTraceName pTraceName) : _pTraceName{pTraceName} {
    switch (_pTraceName) {
        case PTraceName::None:
            assert(false && "Transaction must be different from None");
            break;
        case PTraceName::AppStart:
            _pTraceTitle = "AppStart";
            _pTraceDescription = "Strat the application";
            break;
        case PTraceName::SyncInit:
            _pTraceTitle = "Synchronisation Init";
            _pTraceDescription = "Synchronisation initialization";
            break;
        case PTraceName::ResetStatus:
            _pTraceTitle = "ResetStatus";
            _pTraceDescription = "Reseting status";
            _parentPTraceName = PTraceName::SyncInit;
            break;
        case PTraceName::Sync:
            _pTraceTitle = "Synchronisation";
            _pTraceDescription = "Synchronisation.";
            break;
        case PTraceName::UpdateDetection1:
            _pTraceTitle = "UpdateDetection1";
            _pTraceDescription = "Compute FS operations";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::UpdateUnsyncedList:
            _pTraceTitle = "UpdateUnsyncedList";
            _pTraceDescription = "Update unsynced list";
            _parentPTraceName = PTraceName::UpdateDetection1;
            break;
        case PTraceName::InferChangesFromDb:
            _pTraceTitle = "InferChangesFromDb";
            _pTraceDescription = "Infer changes from DB";
            _parentPTraceName = PTraceName::UpdateDetection1;
            break;
        case PTraceName::ExploreLocalSnapshot:
            _pTraceTitle = "ExploreLocalSnapshot";
            _pTraceDescription = "Explore local snapshot";
            _parentPTraceName = PTraceName::UpdateDetection1;
            break;
        case PTraceName::ExploreRemoteSnapshot:
            _pTraceTitle = "ExploreRemoteSnapshot";
            _pTraceDescription = "Explore remote snapshot";
            _parentPTraceName = PTraceName::UpdateDetection1;
            break;
        case PTraceName::UpdateDetection2:
            _pTraceTitle = "UpdateDetection2";
            _pTraceDescription = "UpdateTree generation";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::ResetNodes:
            _pTraceTitle = "ResetNodes";
            _pTraceDescription = "Reset nodes";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step1MoveDirectory:
            _pTraceTitle = "Step1MoveDirectory";
            _pTraceDescription = "Move directory";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step2MoveFile:
            _pTraceTitle = "Step2MoveFile";
            _pTraceDescription = "Move file";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step3DeleteDirectory:
            _pTraceTitle = "Step3DeleteDirectory";
            _pTraceDescription = "Delete directory";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step4DeleteFile:
            _pTraceTitle = "Step4DeleteFile";
            _pTraceDescription = "Delete file";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step5CreateDirectory:
            _pTraceTitle = "Step5CreateDirectory";
            _pTraceDescription = "Create directory";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step6CreateFile:
            _pTraceTitle = "Step6CreateFile";
            _pTraceDescription = "Create file";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step7EditFile:
            _pTraceTitle = "Step7EditFile";
            _pTraceDescription = "Edit file";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Step8CompleteUpdateTree:
            _pTraceTitle = "Step8CompleteUpdateTree";
            _pTraceDescription = "Complete update tree";
            _parentPTraceName = PTraceName::UpdateDetection2;
            break;
        case PTraceName::Reconciliation1:
            _pTraceTitle = "Reconciliation1";
            _pTraceDescription = "Platform inconsistency check";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::CheckLocalTree:
            _pTraceTitle = "CheckLocalTree";
            _pTraceDescription = "Check local update tree integrity";
            _parentPTraceName = PTraceName::Reconciliation1;
            break;
        case PTraceName::CheckRemoteTree:
            _pTraceTitle = "CheckRemoteTree";
            _pTraceDescription = "Check remote update tree integrity";
            _parentPTraceName = PTraceName::Reconciliation1;
            break;
        case PTraceName::Reconciliation2:
            _pTraceTitle = "Reconciliation2";
            _pTraceDescription = "Find conflicts";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::Reconciliation3:
            _pTraceTitle = "Reconciliation3";
            _pTraceDescription = "Resolve conflicts";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::Reconciliation4:
            _pTraceTitle = "Reconciliation4";
            _pTraceDescription = "Operation Generator";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::GenerateItemOperations:
            _pTraceTitle = "GenerateItemOperations";
            _pTraceDescription = "Generate the list of operations for 1000 items";
            _parentPTraceName = PTraceName::Reconciliation4;
            break;
        case PTraceName::Propagation1:
            _pTraceTitle = "Propagation1";
            _pTraceDescription = "Operation Sorter";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::Propagation2:
            _pTraceTitle = "Propagation2";
            _pTraceDescription = "Executor";
            _parentPTraceName = PTraceName::Sync;
            break;
        case PTraceName::InitProgress:
            _pTraceTitle = "InitProgress";
            _pTraceDescription = "Init the progress manager";
            _parentPTraceName = PTraceName::Propagation2;
            break;
        case PTraceName::JobGeneration:
            _pTraceTitle = "JobGeneration";
            _pTraceDescription = "Generate the list of jobs";
            _parentPTraceName = PTraceName::Propagation2;
            break;
        case PTraceName::waitForAllJobsToFinish:
            _pTraceTitle = "waitForAllJobsToFinish";
            _pTraceDescription = "Wait for all jobs to finish";
            _parentPTraceName = PTraceName::Propagation2;
            break;
        case PTraceName::LFSO_GenerateInitialSnapshot:
            _pTraceTitle = "LFSO_GenerateInitialSnapshot";
            _pTraceDescription = "Explore sync directory";
            break;
        case PTraceName::LFSO_ExploreItem:
            _pTraceTitle = "LFSO_ExploreItem(x1000)";
            _pTraceDescription = "Discover 1000 local files";
            _parentPTraceName = PTraceName::LFSO_GenerateInitialSnapshot;
            break;
        case PTraceName::LFSO_ChangeDetected:
            _pTraceTitle = "LFSO_ChangeDetected";
            _pTraceDescription = "Handle one detected changes";
            break;
        case PTraceName::RFSO_GenerateInitialSnapshot:
            _pTraceTitle = "RFSO_GenerateInitialSnapshot";
            _pTraceDescription = "Generate snapshot";
            break;
        case PTraceName::RFSO_BackRequest:
            _pTraceTitle = "RFSO_BackRequest";
            _pTraceDescription = "Request the list of all items to the backend";
            _parentPTraceName = PTraceName::RFSO_GenerateInitialSnapshot;
            break;
        case PTraceName::RFSO_ExploreItem:
            _pTraceTitle = "RFSO_ExploreItem(x1000)";
            _pTraceDescription = "Discover 1000 remote files";
            _parentPTraceName = PTraceName::RFSO_GenerateInitialSnapshot;
            break;
        case PTraceName::RFSO_ChangeDetected:
            _pTraceTitle = "RFSO_ChangeDetected";
            _pTraceDescription = "Handle one detected changes";
            break;
        default:
            assert(false && "Invalid transaction name");
            break;
    }
}
} // namespace KDC::Sentry
