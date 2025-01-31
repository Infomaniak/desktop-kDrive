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
#include "../networkjobsparams.h"
#include <libcommonserver/vfs/vfs.h>

namespace KDC {

class DownloadJob : public AbstractTokenNetworkJob {
    public:
        DownloadJob(const std::shared_ptr<Vfs>& vfs, int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath,
                    int64_t expectedSize, SyncTime creationTime, SyncTime modtime, bool isCreate);
        DownloadJob(const std::shared_ptr<Vfs> &vfs, int driveDbId, const NodeId &remoteFileId, const SyncPath &localpath,
                    int64_t expectedSize);
        ~DownloadJob() override;

        inline const NodeId &remoteNodeId() const { return _remoteFileId; }
        inline const SyncPath &localPath() const { return _localpath; }

        inline const NodeId &localNodeId() const { return _localNodeId; }
        inline SyncTime modtime() const { return _modtimeIn; }

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &, bool &) override {}
        inline virtual ExitInfo setData() override { return ExitCode::Ok; }

        virtual bool canRun() override;
        virtual void runJob() noexcept override;
        virtual bool handleResponse(std::istream &is) override;

        bool createLink(const std::string &mimeType, const std::string &data);
        bool removeTmpFile();
        bool moveTmpFile(bool &restartSync);
        //! Create a tmp file from an std::istream or a std::string
        /*!
          \param istr is a stream used to read the file data.
          \param data is a string containing the file data.
          \param readError will be true if a read error occured on the input stream.
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
        bool hasEnoughPlace(const SyncPath &tmpDirPath, const SyncPath &destDirPath, int64_t neededPlace);

        NodeId _remoteFileId;
        SyncPath _localpath;
        SyncPath _tmpPath;
        int64_t _expectedSize = Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH;
        SyncTime _creationTime = 0;
        SyncTime _modtimeIn = 0;
        bool _isCreate = false;
        bool _ignoreDateTime = false;
        bool _responseHandlingCanceled = false;

        NodeId _localNodeId;
        const std::shared_ptr<Vfs> _vfs;
        bool _isHydrated = false;
};

} // namespace KDC
