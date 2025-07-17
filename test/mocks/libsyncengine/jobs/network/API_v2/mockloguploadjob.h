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

#include "testincludes.h"
#include "jobs/network/kDrive_API/upload/loguploadjob.h"
#include "utility/types.h"

namespace KDC {
class MockLogUploadJob : public LogUploadJob {
    public:
        using LogUploadJob::LogUploadJob;
        MockLogUploadJob(bool includeArchivedLog, const std::function<void(LogUploadState, int)> &progressCallback) :
            LogUploadJob(includeArchivedLog, progressCallback, [](const Error &) {}) {}
        void setInitMock(std::function<ExitInfo()> init) { _init = init; }
        ExitInfo init() override {
            if (_init) {
                return _init();
            }
            return LogUploadJob::init();
        }

        void setArchiveMock(std::function<ExitInfo(SyncPath &generatedArchivePath)> archive) { _archive = archive; }
        ExitInfo archive(SyncPath &generatedArchivePath) override {
            if (_archive) {
                return _archive(generatedArchivePath);
            }
            return LogUploadJob::archive(generatedArchivePath);
        }

        void setUploadMock(std::function<ExitInfo(const SyncPath &archivePath)> upload) { _upload = upload; }
        ExitInfo upload(const SyncPath &archivePath) override {
            if (_upload) {
                return _upload(archivePath);
            }
            return LogUploadJob::upload(archivePath);
        }

        void setFinalizeMock(std::function<void()> finalize) { _finalize = finalize; }
        void finalize() override {
            if (_finalize) {
                _finalize();
            }
            LogUploadJob::finalize();
        }

    private:
        std::function<ExitInfo()> _init;
        std::function<ExitInfo(SyncPath &generatedArchivePath)> _archive;
        std::function<ExitInfo(const SyncPath &archivePath)> _upload;
        std::function<void()> _finalize;
};
} // namespace KDC
