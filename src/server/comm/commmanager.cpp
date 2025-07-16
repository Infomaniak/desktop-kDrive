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

#include "commmanager.h"
#include "extensionjob.h"
#include "config.h"
#include "libcommon/utility/logiffail.h"
#include "libcommon/utility/utility.h"
#include "libcommon/theme/theme.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

// TODO: To remove later
#include "oldcommserver.h"

#if defined(__APPLE__)
#include "commserver_mac.h"
#else
#include "commserver.h"
#endif

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QStandardPaths>

#include <log4cplus/loggingmacros.h>

#define QUERY_END_SEPARATOR Str("\\/\n")
#define QUERY_ARG_SEPARATOR Str(";")

namespace KDC {

CommManager::CommManager(const std::unordered_map<int, std::shared_ptr<SyncPal>> &syncPalMap,
                         const std::unordered_map<int, std::shared_ptr<Vfs>> &vfsMap) :
    _syncPalMap(syncPalMap),
    _vfsMap(vfsMap),
    _commServer(std::make_unique<CommServer>()) {
#ifdef __APPLE__
    // Tell the Finder to use the Extension (checking it from System Preferences -> Extensions)
    std::string cmd("pluginkit -v -e use -i ");
    cmd.append(APPLICATION_REV_DOMAIN);
    cmd.append(".Extension");
    system(cmd.c_str());

    // Add it again. This was needed for Mojave to trigger a load.
    // TODO: Still needed?
    SyncPath extPath(CommonUtility::getExtensionPath());
    if (std::filesystem::exists(extPath)) {
        // Bundled app (i.e. not test executable)
        std::string cmd("pluginkit -v -a ");
        cmd.append(extPath.native());
        system(cmd.c_str());
    }
#endif

    // Set CommServer callbacks
    _commServer->setNewExtConnectionCbk(std::bind(&CommManager::onNewExtConnection, this));
    _commServer->setNewGuiConnectionCbk(std::bind(&CommManager::onNewGuiConnection, this));
    _commServer->setLostExtConnectionCbk(std::bind(&CommManager::onLostExtConnection, this, std::placeholders::_1));
    _commServer->setLostGuiConnectionCbk(std::bind(&CommManager::onLostGuiConnection, this, std::placeholders::_1));

    // Start comm server
    SyncPath socketPath;
#if defined(_WIN32) or defined(__unix__)
    socketPath = createSocket();
#endif
    if (!_commServer->listen(socketPath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Can't start server: " << Utility::formatSyncPath(socketPath));
        _addError(Error(errId(), ExitCode::SystemError, ExitCause::Unknown));
    } else {
        LOGW_INFO(Log::instance()->getLogger(), L"Server started: " << Utility::formatSyncPath(socketPath));
    }
}

CommManager::~CommManager() {
    _commServer->close();
}

void CommManager::onNewExtConnection() {
    std::shared_ptr<AbstractCommChannel> channel = _commServer->nextPendingConnection();
    if (!channel) return;

    LOG_INFO(Log::instance()->getLogger(), "New ext connection - channel=" << channel->id());
    channel->setReadyReadCbk(std::bind(&CommManager::onExtQueryReceived, this, std::placeholders::_1));

    // Load sync list
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        _addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
        return;
    }

    for (const Sync &sync: syncList) {
        if (!sync.paused()) {
            registerSync(sync.dbId());
        }
    }
}

void CommManager::onLostExtConnection(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "Lost ext connection - channel=" << channel->id());
}

void CommManager::onNewGuiConnection() {
    std::shared_ptr<AbstractCommChannel> channel = _commServer->guiConnection();
    if (!channel) return;

    LOG_INFO(Log::instance()->getLogger(), "New gui connection - channel=" << channel->id());
    channel->setReadyReadCbk(std::bind(&CommManager::onGuiQueryReceived, this, std::placeholders::_1));
}

void CommManager::onLostGuiConnection(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "Lost gui connection - sender=" << channel->id());
}

void CommManager::onGuiQueryReceived(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "onQueryReceived");
    LOG_IF_FAIL(Log::instance()->getLogger(), channel)

    while (channel->canReadLine()) {
        CommString query = channel->readLine();
        LOGW_INFO(Log::instance()->getLogger(), L"Query received: " << CommString2WStr(query));

        const auto queryArgs = CommonUtility::splitCommString(query, QUERY_ARG_SEPARATOR);
        // Query ID
        const int id = std::stoi(queryArgs[0]);
        // Query type
        const RequestNum num = static_cast<RequestNum>(std::stoi(queryArgs[1]));

        if (num == RequestNum::SYNC_START || num == RequestNum::SYNC_STOP) {
            const int syncDbId = std::stoi(queryArgs[2]);
            QByteArray params;
            QDataStream paramsStream(&params, QIODevice::WriteOnly);
            paramsStream << syncDbId;

            emit OldCommServer::instance().get()->requestReceived(id, num, params);
        }
    }
}

void CommManager::onExtQueryReceived(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_IF_FAIL(Log::instance()->getLogger(), channel)

    while (channel->canReadLine()) {
        CommString line;
        while (!line.ends_with(QUERY_END_SEPARATOR)) {
            if (!channel->canReadLine()) {
                LOGW_WARN(Log::instance()->getLogger(), L"Failed to parse Extension message - msg="
                                                                << CommString2WStr(line) << L" channel="
                                                                << Utility::s2ws(channel->id()));
                return;
            }
            line.append(channel->readLine());
        }
        // Remove the separator
        line.erase(line.find(QUERY_END_SEPARATOR));
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Received Extension message - msg=" << CommString2WStr(line) << L" channel=" << Utility::s2ws(channel->id()));
        executeCommand(line, channel);
    }
}

bool CommManager::tryToRetrieveSync(const int syncDbId, Sync &sync) const {
    if (!_registeredSyncs.contains(syncDbId)) return false; // Make sure not to register twice to each connected client

    bool found = false;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync - syncDbId=" << syncDbId);
        _addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
        return false;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in sync table - syncDbId=" << syncDbId);
        _addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
        return false;
    }

    return true;
}

void CommManager::registerSync(int syncDbId) {
    Sync sync;
    if (!tryToRetrieveSync(syncDbId, sync)) return;

    CommString command(Str("REGISTER_PATH"));
    command.append(messageCdeSeparator);
    command.append(sync.localPath().native());
    broadcastCommand(command);

    _registeredSyncs.insert(syncDbId);
}

void CommManager::unregisterSync(int syncDbId) {
    Sync sync;
    if (!tryToRetrieveSync(syncDbId, sync)) return;

    CommString command(Str("UNREGISTER_PATH"));
    command.append(messageCdeSeparator);
    command.append(sync.localPath().native());
    broadcastCommand(command);

    _registeredSyncs.erase(syncDbId);
}

SyncPath CommManager::createSocket() {
    // Get socket file path
    SyncPath socketPath;
#ifdef _WIN32
    std::string name(APPLICATION_SHORTNAME);
    name.append("-");
    name.append(Utility::userName());
    socketPath = SyncPath(R"(\\.\pipe\)") / Str2SyncName(name);
#else
    socketPath = QStr2Path(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)) /
                 QStr2SyncName(Theme::instance()->appName()) / Str("socket");
#endif

    // Delete/create socket file
    CommServer::removeServer(socketPath);
    if (const QFileInfo info(Path2QStr(socketPath)); !info.dir().exists()) {
        if (info.dir().mkpath(".")) {
            QFile::setPermissions(Path2QStr(socketPath),
                                  QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
            LOGW_DEBUG(Log::instance()->getLogger(), L"Socket created: " << Utility::formatSyncPath(socketPath));
        } else {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to create socket: " << Utility::formatSyncPath(socketPath));
        }
    }

    return socketPath;
}

void CommManager::executeCommandDirect(const CommString &commandLineStr, bool broadcast) {
    if (broadcast) {
        executeCommand(commandLineStr);
    } else {
        broadcastCommand(commandLineStr);
    }
}

void CommManager::executeCommand(const CommString &commandLineStr) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommString2WStr(commandLineStr));

    std::shared_ptr<ExtensionJob> job = std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr);
    if (ExitInfo exitInfo = job->runSynchronously(); exitInfo.code() != ExitCode::Ok) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in ExtensionJob::runSynchronously - cmd=" << CommString2WStr(commandLineStr));
    }
}

void CommManager::executeCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommString2WStr(commandLineStr));

    std::shared_ptr<ExtensionJob> job = std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr, channel);
    if (ExitInfo exitInfo = job->runSynchronously(); exitInfo.code() != ExitCode::Ok) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in ExtensionJob::runSynchronously - cmd=" << CommString2WStr(commandLineStr));
    }
}

void CommManager::broadcastCommand(const CommString &commandLineStr) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommString2WStr(commandLineStr));

    std::shared_ptr<ExtensionJob> job =
            std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr, _commServer->extConnections());
    if (ExitInfo exitInfo = job->runSynchronously(); exitInfo.code() != ExitCode::Ok) {
        LOGW_DEBUG(Log::instance()->getLogger(),
                   L"Error in ExtensionJob::runSynchronously - cmd=" << CommString2WStr(commandLineStr));
    }
}

} // namespace KDC
