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

#include "abstracttokennetworkjob.h"
#include <chrono>

namespace KDC {

class LogUploadJob : public AbstractJob, public std::enable_shared_from_this<LogUploadJob> {
    public:
        LogUploadJob(bool includeArchivedLog, const std::function<void(LogUploadState, int)> &progressCallback);

        void runJob() override;
        void abort() override;
        static void cancelUpload();
        /*! Returns the estimated size of the log files in bytes.
         * Actual size may be different due to compression.
         * \param size The estimated size of the log files in bytes.
         * \param ioError The error object to be filled in case of error.
         * \return True if the size was retrieved successfully, false otherwise.
         */
        static bool getLogDirEstimatedSize(uint64_t &size, IoError &ioError);

        /*! Generates a support archive containing the logs and the parms.db file.
         * \param includeArchivedLogs If true, the archived logs from previous sessions will be included.
         * \param outputDir The path of the directory where the archive will be generated.
         * \param progressCallback The callback to be called with the progress percentage, the callback retruns false if the user
         *      cancels the operation (else true).
         * \param archivePath The path to the generated archive.
         * \param exitCause The exit cause to be filled in case of error. If no error occurred, it will be set to
         *      ExitCause::Unknown;
         * \param test If true, the archive will be generated with test.zip name.
         * \return The exit code of the operation.
         */
        ExitCode generateLogsSupportArchive(bool includeArchivedLogs, const SyncPath &outputDir,
                                            const std::function<bool(int)> &progressCallback, SyncPath &archivePath,
                                            ExitCause &exitCause, bool test = false);

    private:
        static std::mutex _runningJobMutex;
        static std::shared_ptr<LogUploadJob> _runningJob;
        bool _includeArchivedLog;
        SyncPath _tmpJobWorkingDir;
        std::function<void(LogUploadState, int)> _progressCallback;
        std::chrono::time_point<std::chrono::system_clock> _lastProgressUpdateTimeStamp;
        LogUploadState _previousState{LogUploadState::None};
        int _previousProgress{-1};
        ExitInfo init();
        ExitInfo archive(SyncPath &generatedArchivePath);
        ExitInfo upload(const SyncPath &archivePath);
        void finalize();
        bool canRun() override;
        /* Return the path to a temporary directory where the job can work.
         * The directory will be created if it does not exist.
         * The caller is responsible for deleting the directory when it is no longer needed.
         * The directory will be created in the log directory.
         * ie: /.../kDrive-logdir/tmpLogArchive_XXXXXX
         */
        ExitInfo getTmpJobWorkingDir(SyncPath &tmpJobWorkingDir) const;

        // Generate the archive name based on the drive IDs and the current date and time.
        ExitInfo getArchiveName(SyncName &archiveName) const;

        ExitInfo copyLogsTo(const SyncPath &outputPath, bool includeArchivedLogs) const;
        ExitInfo copyParmsDbTo(const SyncPath &outputPath) const;

        ExitInfo generateArchive(const SyncPath &directoryToCompress, const SyncPath &destPath,
                                 const SyncName &archiveNameWithoutExtension, SyncPath &finalPath);

        /*! Generates a file containing the user description.
         * The file will contain: Current OS, current architecture, current version, current user(s), current drive(s).
         * \param outputPath The path where the file will be generated.
         * \param exitCause The exit cause to be filled in case of error. If no error occurred, it will be set to
         *      ExitCause::Unknown;
         * \return The exit code of the operation.
         */
        ExitInfo generateUserDescriptionFile(const SyncPath &outputPath) const;


        // Update the log upload state in the database (appstate table).
        void updateLogUploadState(LogUploadState newState) const;

        ExitInfo notifyLogUploadProgress(LogUploadState newState, int progressPercent);
        // Handle job failure.
        void handleJobFailure(const ExitInfo &exitInfo, bool clearTmpDir = false);
        [[nodiscard]] static bool getFileSize(const SyncPath &path, uint64_t &size);
};

} // namespace KDC
