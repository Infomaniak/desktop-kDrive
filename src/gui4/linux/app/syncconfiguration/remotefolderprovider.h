/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "libcommon/info/nodeinfo.h"
#include "libcommon/utility/types.h"

#include <functional>
#include <vector>

namespace KDC {

class CommService;

/** Asynchronous remote-folder data boundary shared by onboarding today and synchronization settings in the future. */
class AbstractRemoteFolderProvider {
    public:
        using NodeInfoCallback = std::function<void(const ExitInfo &, const NodeInfo &)>;
        using ChildrenCallback = std::function<void(const ExitInfo &, const std::vector<NodeInfo> &)>;
        using SizeCallback = std::function<void(const ExitInfo &, int64_t)>;

        virtual ~AbstractRemoteFolderProvider() = default;
        virtual void requestNodeInfo(UserDbId userDbId, DriveId driveId, const NodeId &nodeId,
                                     const NodeInfoCallback &callback) const = 0;
        virtual void requestChildren(UserDbId userDbId, DriveId driveId, const NodeId &nodeId,
                                     const ChildrenCallback &callback) const = 0;
        virtual void requestSize(UserDbId userDbId, DriveId driveId, const NodeId &nodeId,
                                 const SizeCallback &callback) const = 0;
};

/** Production adapter from the reusable tree contract to the GUI/server IPC service. */
class CommRemoteFolderProvider final : public AbstractRemoteFolderProvider {
    public:
        explicit CommRemoteFolderProvider(CommService &commService);

        void requestNodeInfo(UserDbId userDbId, DriveId driveId, const NodeId &nodeId,
                             const NodeInfoCallback &callback) const override;
        void requestChildren(UserDbId userDbId, DriveId driveId, const NodeId &nodeId,
                             const ChildrenCallback &callback) const override;
        void requestSize(UserDbId userDbId, DriveId driveId, const NodeId &nodeId, const SizeCallback &callback) const override;

    private:
        CommService &_commService;
};

} // namespace KDC
