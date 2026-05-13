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

#include "jobs/network/abstracttokennetworkjob.h"

#include "libcommonserver/vfs/vfs.h"
#include "libcommonserver/io/cachedirectory.h"

namespace KDC {

class DownloadJob : public AbstractTokenNetworkJob {
    public:
        struct FileDownloadInfo {
                const DriveDbId driveDbId = 0;
                const NodeId remoteFileId;
                const SyncPath localPath;
                const int64_t expectedSize = Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH;
                const SyncTime creationTime = 0;
                SyncTime modificationTime = 0;
                bool isCreate = false;
        };
        enum class DateTimePolicy {
            IgnoreDateTime,
            ApplyDateTime
        };

        DownloadJob(const std::shared_ptr<Vfs> vfs, std::shared_ptr<CacheDirectory> cacheDirectory,
                    const FileDownloadInfo &fileDownloadInfo, DateTimePolicy dateTimePolicy);
        ~DownloadJob() override;

        const RemoteNodeId &remoteNodeId() const { return _fileDownloadInfo.remoteFileId; }
        const SyncPath &localPath() const { return _fileDownloadInfo.localPath; }

        const NodeId &localNodeId() const { return _localNodeId; }
        SyncTime creationTime() const { return _creationTimeOut; }
        SyncTime modificationTime() const { return _modificationTimeOut; }
        [[nodiscard]] inline int64_t size() const { return _sizeOut; }

        [[nodiscard]] int64_t expectedSize() const { return _fileDownloadInfo.expectedSize; }

    private:
        std::string getSpecificUrl() override;
        inline ExitInfo setData() override { return ExitCode::Ok; }

        ExitInfo canRun() override;
        ExitInfo runJob() noexcept override;
        ExitInfo handleResponse(std::istream &is) override;

        ExitInfo createLink(const std::string &mimeType, const std::string &data);
        ExitInfo createFile(std::istream &is, bool &fetchedCanceled);
        bool removeTmpFile();
        ExitInfo moveTmpFile();
        //! Create a tmp file from a std::istream or a std::string
        /*!
          \param istr is a stream used to read the file data.
          \param data is a string containing the file data.
          \param readError will be true if a read error occurred on the input stream.
          \param fetchCanceled will be true if the read on the input stream has been canceled by the user.
          \param fetchFinished will be true if the read on the input stream has succeeded.
          \param fetchError will be true if the read on the input stream has failed.
          \return true if no unexpected error occurred, false otherwise.
        */
        ExitInfo createTmpFile(std::optional<std::reference_wrapper<std::istream>> istr,
                               std::optional<std::reference_wrapper<const std::string>> data, bool &readError, bool &writeError,
                               bool &fetchCanceled, bool &fetchFinished, bool &fetchError);
        //! Create a tmp file from an std::istream
        ExitInfo createTmpFile(std::istream &is, bool &readError, bool &writeError, bool &fetchCanceled, bool &fetchFinished,
                               bool &fetchError);
        //! Create a tmp file from a std::string
        ExitInfo createTmpFile(const std::string &data, bool &writeError);
        static bool hasEnoughPlace(const SyncPath &tmpDirPath, const SyncPath &destDirPath, int64_t neededPlace,
                                   log4cplus::Logger logger);

        const std::shared_ptr<Vfs> _vfs;
        std::shared_ptr<CacheDirectory> _cacheDirectory;
        FileDownloadInfo _fileDownloadInfo;

        SyncPath _tmpPath;
        DateTimePolicy _dateTimePolicy;
        bool _responseHandlingCanceled = false;

        NodeId _localNodeId;
        SyncTime _creationTimeOut = 0; // The effective creation time of the file on the local filesystem, it may differ from
                                       // _creationTimeIn if we fail to set it locally
        SyncTime _modificationTimeOut = 0;
        int64_t _sizeOut = -1; // Do not use 0 here as it is a valid size.

        bool _isHydrated{true};

        friend class TestNetworkJobs;
};

} // namespace KDC
