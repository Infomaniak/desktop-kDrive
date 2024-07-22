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

#pragma once
#include "libcommon/utility/types.h"

namespace KDC {

class LogArchiver {
    public:
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
         *      ExitCauseUnknown;
         * \param test If true, the archive will be generated with test.zip name.
         * \return The exit code of the operation.
         */
        static ExitCode generateLogsSupportArchive(bool includeArchivedLogs, const SyncPath &outputDir,
                                                   std::function<bool(int)> progressCallback, SyncPath &archivePath,
                                                   ExitCause &exitCause, bool test = false);

    private:
        friend class TestLogArchiver;
        static ExitCode copyLogsTo(const SyncPath &outputPath, bool includeArchivedLogs, ExitCause &exitCause);
        static ExitCode copyParmsDbTo(const SyncPath &outputPath, ExitCause &exitCause);

        /*! Compresses the log files in the given directory.
         * This method will not create an archive, it will only compress the files in the directory.
         * The compressed files will have the same name as the original files with the .gz extension.
         * \param directoryToCompress The directory containing the log files to compress.
         * \param progressCallback The callback to be called with the progress percentage, the callback retruns false if the user
         *      cancels the operation (else true). 
         * \param exitCause The exit cause to be filled in case of error. If no error occurred,
         *      it will be set to ExitCauseUnknown; 
         * \return The exit code of the operation.
         */
        static ExitCode compressLogFiles(const SyncPath &directoryToCompress, std::function<bool(int)> progressCallback,
                                         ExitCause &exitCause);

        /*! Generates a file containing the user description.
         * The file will contain: Current OS, current architecture, current version, current user(s), current drive(s).
         * \param outputPath The path where the file will be generated.
         * \param exitCause The exit cause to be filled in case of error. If no error occurred, it will be set to
         *      ExitCauseUnknown; 
         * \return The exit code of the operation.
         */
        static ExitCode generateUserDescriptionFile(const SyncPath &outputPath, ExitCause &exitCause);
};

}  // namespace KDC
