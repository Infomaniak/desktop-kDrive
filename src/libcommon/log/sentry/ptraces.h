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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "abstractptrace.h"
#include "abstractscopedptrace.h"
#include "abstractcounterscopedptrace.h"

namespace KDC::sentry::pTraces {

struct None : public AbstractPTrace {
        None() :
            AbstractPTrace({}){};
        explicit None(const SyncDbId syncDbId) :
            AbstractPTrace({}, syncDbId) {}
        void start() final { /* Do nothing */
        }
        void stop([[maybe_unused]] PTraceStatus status = PTraceStatus::Ok) final { /* Do nothing */
        }
        void restart() final { /* Do nothing */
        }
};

/*
 Basic performance traces are useful for operation that are spread accross multiple scopes.
 They should be used with care as they do not provide security against multiple/missing start() or stop() calls.
 The start() and stop() methods can be called on different objects.
 | PTraces::Basic::AppStart().start();
 | PTraces::Basic::AppStart().stop();
 is equivalent to (but less efficient):
 | PTraces::Basic::AppStart perfMonitor();
 | perfMonitor.start();
 | perfMonitor.stop();
 */
namespace basic {
struct AppStart : public AbstractPTrace {
        [[nodiscard]] AppStart() :
            AbstractPTrace({"AppStart", "Start the application", PTraceName::AppStart}) {}
};

struct Sync : public AbstractPTrace {
        [[nodiscard]] explicit Sync(const SyncDbId syncDbId) :
            AbstractPTrace({"Synchronisation", "Synchronisation initialization", PTraceName::Sync}, syncDbId){};
};

struct UpdateDetection1 : public AbstractPTrace {
        [[nodiscard]] explicit UpdateDetection1(const SyncDbId syncDbId) :
            AbstractPTrace({"UpdateDetection1", "Compute FS operations", PTraceName::UpdateDetection1, PTraceName::Sync},
                           syncDbId) {}
};

struct UpdateDetection2 : public AbstractPTrace {
        [[nodiscard]] explicit UpdateDetection2(const SyncDbId syncDbId) :
            AbstractPTrace({"UpdateDetection2", "UpdateTree generation", PTraceName::UpdateDetection2, PTraceName::Sync},
                           syncDbId) {}
};

struct Reconciliation1 : public AbstractPTrace {
        [[nodiscard]] explicit Reconciliation1(const SyncDbId syncDbId) :
            AbstractPTrace({"Reconciliation1", "Platform inconsistency check", PTraceName::Reconciliation1, PTraceName::Sync},
                           syncDbId){};
};

struct Reconciliation2 : public AbstractPTrace {
        [[nodiscard]] explicit Reconciliation2(const SyncDbId syncDbId) :
            AbstractPTrace({"Reconciliation2", "Find conflicts", PTraceName::Reconciliation2, PTraceName::Sync}, syncDbId) {}
};

struct Reconciliation3 : public AbstractPTrace {
        [[nodiscard]] explicit Reconciliation3(const SyncDbId syncDbId) :
            AbstractPTrace({"Reconciliation3", "Resolve conflicts", PTraceName::Reconciliation3, PTraceName::Sync}, syncDbId) {}
};

struct Reconciliation4 : public AbstractPTrace {
        [[nodiscard]] explicit Reconciliation4(const SyncDbId syncDbId) :
            AbstractPTrace({"Reconciliation4", "Operation Generator", PTraceName::Reconciliation4, PTraceName::Sync}, syncDbId) {}
};

struct Propagation1 : public AbstractPTrace {
        [[nodiscard]] explicit Propagation1(const SyncDbId syncDbId) :
            AbstractPTrace({"Propagation1", "Operation Sorter", PTraceName::Propagation1, PTraceName::Sync}, syncDbId) {}
};

struct Propagation2 : public AbstractPTrace {
        [[nodiscard]] explicit Propagation2(const SyncDbId syncDbId) :
            AbstractPTrace({"Propagation2", "Executor", PTraceName::Propagation2, PTraceName::Sync}, syncDbId) {}
};
} // namespace basic

/* Scoped performance traces will automatically start when the object is created and stop when the object is
 destroyed.
 Some scoped performance traces expect to be manually stopped. In this case, the stop() method must be called, else the trace will
 be considered as aborted when the object will be destroyed. Such traces are indicated by a comment in their class definition.
 */
namespace scoped {
struct LFSOChangeDetected : public AbstractScopedPTrace {
        explicit LFSOChangeDetected(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"LFSO_ChangeDetected", "Handle one detected changes", PTraceName::LFSOChangeDetected,
                                  PTraceName::None, 0.01},
                                 PTraceStatus::Ok, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct LFSOGenerateInitialSnapshot : public AbstractScopedPTrace {
        explicit LFSOGenerateInitialSnapshot(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"LFSO_GenerateInitialSnapshot", "Generate snapshot", PTraceName::LFSOGenerateInitialSnapshot},
                                 PTraceStatus::Aborted, syncDbId) {}
};

struct RFSOChangeDetected : public AbstractScopedPTrace {
        explicit RFSOChangeDetected(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"RFSO_ChangeDetected", "Handle one detected changes", PTraceName::RFSOChangeDetected, PTraceName::None, 0.1},
                    PTraceStatus::Ok, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct RFSOGenerateInitialSnapshot : public AbstractScopedPTrace {
        explicit RFSOGenerateInitialSnapshot(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"RFSO_GenerateInitialSnapshot", "Generate snapshot", PTraceName::RFSOGenerateInitialSnapshot},
                                 PTraceStatus::Aborted, syncDbId){};
};

// This scoped performance trace expects to be manually stopped.
struct RFSOBackRequest : public AbstractScopedPTrace {
        explicit RFSOBackRequest(bool fromChangeDetected, const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"RFSO_BackRequest", "Request the list of all items to the backend", PTraceName::RFSOBackRequest,
                     (fromChangeDetected ? PTraceName::RFSOChangeDetected : PTraceName::RFSOGenerateInitialSnapshot)},
                    PTraceStatus::Aborted, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct ResetStatus : public AbstractScopedPTrace {
        explicit ResetStatus(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"ResetStatus", "Reseting vfs status", PTraceName::ResetStatus}, PTraceStatus::Aborted,
                                 syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct Sync : public AbstractScopedPTrace {
        explicit Sync(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"Synchronisation", "Synchronisation.", PTraceName::Sync}, PTraceStatus::Aborted, syncDbId) {}
};

struct UpdateUnsyncedList : public AbstractScopedPTrace {
        explicit UpdateUnsyncedList(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"UpdateUnsyncedList", "Update unsynced list.", PTraceName::UpdateUnsyncedList, PTraceName::UpdateDetection1},
                    PTraceStatus::Ok, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct InferChangesFromDb : public AbstractScopedPTrace {
        explicit InferChangesFromDb(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"InferChangesFromDb", "Infer changes from DB", PTraceName::InferChangesFromDb, PTraceName::UpdateDetection1},
                    PTraceStatus::Aborted, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct ExploreLocalSnapshot : public AbstractScopedPTrace {
        explicit ExploreLocalSnapshot(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"ExploreLocalSnapshot", "Explore local snapshot", PTraceName::ExploreLocalSnapshot,
                                  PTraceName::UpdateDetection1},
                                 PTraceStatus::Aborted, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct ExploreRemoteSnapshot : public AbstractScopedPTrace {
        explicit ExploreRemoteSnapshot(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"ExploreRemoteSnapshot", "Explore remote snapshot", PTraceName::ExploreRemoteSnapshot,
                                  PTraceName::UpdateDetection1},
                                 PTraceStatus::Aborted, syncDbId) {}
};

struct Step1MoveDirectory : public AbstractScopedPTrace {
        explicit Step1MoveDirectory(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"Step1MoveDirectory", "Move directory", PTraceName::Step1MoveDirectory, PTraceName::UpdateDetection2},
                    PTraceStatus::Ok, syncDbId) {}
};

struct Step2MoveFile : public AbstractScopedPTrace {
        explicit Step2MoveFile(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"Step2MoveFile", "Move File", PTraceName::Step2MoveFile, PTraceName::UpdateDetection2},
                                 PTraceStatus::Ok, syncDbId) {}
};

struct Step3DeleteDirectory : public AbstractScopedPTrace {
        explicit Step3DeleteDirectory(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"Step3DeleteDirectory", "Delete directory", PTraceName::Step3DeleteDirectory, PTraceName::UpdateDetection2},
                    PTraceStatus::Ok, syncDbId) {}
};

struct Step4DeleteFile : public AbstractScopedPTrace {
        explicit Step4DeleteFile(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"Step4DeleteFile", "Delete file", PTraceName::Step4DeleteFile, PTraceName::UpdateDetection2},
                                 PTraceStatus::Ok, syncDbId) {}
};

struct Step5CreateDirectory : public AbstractScopedPTrace {
        explicit Step5CreateDirectory(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"Step5CreateDirectory", "Create directory", PTraceName::Step5CreateDirectory, PTraceName::UpdateDetection2},
                    PTraceStatus::Ok, syncDbId) {}
};

struct Step6CreateFile : public AbstractScopedPTrace {
        explicit Step6CreateFile(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"Step6CreateFile", "Create file", PTraceName::Step6CreateFile, PTraceName::UpdateDetection2},
                                 PTraceStatus::Ok, syncDbId) {}
};

struct Step7EditFile : public AbstractScopedPTrace {
    public:
        explicit Step7EditFile(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"Step7EditFile", "Edit file", PTraceName::Step7EditFile, PTraceName::UpdateDetection2},
                                 PTraceStatus::Ok, syncDbId) {}
};

struct Step8CompleteUpdateTree : public AbstractScopedPTrace {
    public:
        explicit Step8CompleteUpdateTree(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"Step8CompleteUpdateTree", "Complete update tree", PTraceName::Step8CompleteUpdateTree,
                                  PTraceName::UpdateDetection2},
                                 PTraceStatus::Ok, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct CheckLocalTree : public AbstractScopedPTrace {
        explicit CheckLocalTree(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"CheckLocalTree", "Check local update tree integrity", PTraceName::CheckLocalTree,
                                  PTraceName::Reconciliation1},
                                 PTraceStatus::Aborted, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct CheckRemoteTree : public AbstractScopedPTrace {
        explicit CheckRemoteTree(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"CheckRemoteTree", "Check remote update tree integrity", PTraceName::CheckRemoteTree,
                                  PTraceName::Reconciliation1},
                                 PTraceStatus::Aborted, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct InitProgress : public AbstractScopedPTrace {
        explicit InitProgress(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"InitProgress", "Init the progress manager", PTraceName::InitProgress, PTraceName::Propagation2},
                    PTraceStatus::Aborted, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct JobGeneration : public AbstractScopedPTrace {
        explicit JobGeneration(const SyncDbId syncDbId) :
            AbstractScopedPTrace(
                    {"JobGeneration", "Generate the list of jobs", PTraceName::JobGeneration, PTraceName::Propagation2},
                    PTraceStatus::Aborted, syncDbId) {}
};

// This scoped performance trace expects to be manually stopped.
struct waitForAllJobsToFinish : public AbstractScopedPTrace {
        explicit waitForAllJobsToFinish(const SyncDbId syncDbId) :
            AbstractScopedPTrace({"waitForAllJobsToFinish", "Wait for all jobs to finish", PTraceName::waitForAllJobsToFinish,
                                  PTraceName::Propagation2},
                                 PTraceStatus::Aborted, syncDbId) {}
};
} // namespace scoped

/* CounterScoped performance traces will start when the object is created and will be stopped after nbOfCyclePerTrace() calls to
 * start(). A new trace will also be started as soon as the counter is reached. When the object is destroyed, the running trace
 * will be cancelled. Stop() and restart() methods will not have any effect on CounterScoped performance traces.
 *
 * It is useful when you want to trace short part of the code that are called multiple times successively.
 */
namespace counterScoped {
struct LFSOExploreItem : public AbstractCounterScopedPTrace {
        explicit LFSOExploreItem(const bool fromChangeDetected, const SyncDbId syncDbId) :
            AbstractCounterScopedPTrace(
                    {"LFSO_ExploreItem(x1000)", "Discover 1000 local files", PTraceName::LFSOExploreItem,
                     (fromChangeDetected ? PTraceName::LFSOChangeDetected : PTraceName::LFSOGenerateInitialSnapshot)},
                    1000, syncDbId) {}
};

struct RFSOExploreItem : public AbstractCounterScopedPTrace {
        explicit RFSOExploreItem(const bool fromChangeDetected, const SyncDbId syncDbId) :
            AbstractCounterScopedPTrace(
                    {"RFSO_ExploreItem(x1000)", "Discover 1000 remote files", PTraceName::RFSOExploreItem,
                     (fromChangeDetected ? PTraceName::RFSOChangeDetected : PTraceName::RFSOGenerateInitialSnapshot)},
                    1000, syncDbId) {}
};

struct GenerateItemOperations : public AbstractCounterScopedPTrace {
        explicit GenerateItemOperations(const SyncDbId syncDbId) :
            AbstractCounterScopedPTrace({"GenerateItemOperations", "Generate the list of operations for 1000 items",
                                         PTraceName::GenerateItemOperations, PTraceName::Reconciliation4},
                                        1000, syncDbId) {}
};
} // namespace counterScoped

} // namespace KDC::sentry::pTraces
