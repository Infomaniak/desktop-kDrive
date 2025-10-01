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

#include "jobs/network/abstracttokennetworkjob.h"
#include "libcommonserver/vfs/vfs.h"

namespace KDC {

class DownloadJob : public AbstractTokenNetworkJob {
    public:
        DownloadJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath,
                    int64_t expectedSize, SyncTime creationTime, SyncTime modificationTime, bool isCreate);
        DownloadJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath,
                    int64_t expectedSize);
        ~DownloadJob() override;

        inline const NodeId &remoteNodeId() const { return _remoteFileId; }
        inline const SyncPath &localPath() const { return _localpath; }

        inline const NodeId &localNodeId() const { return _localNodeId; }
        inline SyncTime creationTime() const { return _creationTimeOut; }
        inline SyncTime modificationTime() const { return _modificationTimeOut; }
        [[nodiscard]] inline int64_t size() const { return _sizeOut; }

        [[nodiscard]] int64_t expectedSize() const { return _expectedSize; }

    private:
        virtual std::string getSpecificUrl() override;
        inline virtual ExitInfo setData() override { return ExitCode::Ok; }

        virtual bool canRun() override;
        virtual void runJob() noexcept override;
        virtual bool handleResponse(std::istream &is) override;

        bool createLink(const std::string &mimeType, const std::string &data);
        bool removeTmpFile();
        bool moveTmpFile();
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
        bool createTmpFile(std::optional<std::reference_wrapper<std::istream>> istr,
                           std::optional<std::reference_wrapper<const std::string>> data, bool &readError, bool &writeError,
                           bool &fetchCanceled, bool &fetchFinished, bool &fetchError);
        //! Create a tmp file from an std::istream
        bool createTmpFile(std::istream &is, bool &readError, bool &writeError, bool &fetchCanceled, bool &fetchFinished,
                           bool &fetchError);
        //! Create a tmp file from a std::string
        bool createTmpFile(const std::string &data, bool &writeError);
        static bool hasEnoughPlace(const SyncPath &tmpDirPath, const SyncPath &destDirPath, int64_t neededPlace,
                                   log4cplus::Logger logger);

        NodeId _remoteFileId;
        SyncPath _localpath;
        SyncPath _tmpPath;
        const int64_t _expectedSize = Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH;
        const SyncTime _creationTimeIn = 0;
        const SyncTime _modificationTimeIn = 0;

        bool _isCreate = false;
        bool _ignoreDateTime = false;
        bool _responseHandlingCanceled = false;

        NodeId _localNodeId;
        SyncTime _creationTimeOut = 0; // The effective creation time of the file on the local filesystem, it may differ from
                                       // _creationTimeIn if we fail to set it locally
        SyncTime _modificationTimeOut = 0;
        int64_t _sizeOut = -1; // Do not use 0 here as it is a valid size.

        const std::shared_ptr<Vfs> _vfs;
        bool _isHydrated{true};

        friend class TestNetworkJobs;
};

} // namespace KDC
