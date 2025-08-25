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
#include "extcommserver_mac.h"
#include "guicommserver_mac.h"
#elif defined(_WIN32)
#include "extcommserver.h"
#include "guicommserver.h"
#else
#include "guicommserver.h"
#endif

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QStandardPaths>

#include <log4cplus/loggingmacros.h>

#define finderExtQuerySeparator Str("\\/")
#define guiArgSeparator Str(";")

namespace KDC {

CommManager::CommManager(const std::unordered_map<int, std::shared_ptr<SyncPal>> &syncPalMap,
                         const std::unordered_map<int, std::shared_ptr<Vfs>> &vfsMap) :
    _syncPalMap(syncPalMap),
    _vfsMap(vfsMap),
#if defined(__APPLE__) || defined(_WIN32)
    _extCommServer(std::make_shared<ExtCommServer>("Extension Comm Server")),
#endif
    _guiCommServer(std::make_shared<GuiCommServer>("GUI Comm Server")) {
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

    // Set CommServer(s) callbacks
#if defined(__APPLE__) || defined(_WIN32)
    _extCommServer->setNewConnectionCbk(std::bind(&CommManager::onNewExtConnection, this));
    _extCommServer->setLostConnectionCbk(std::bind(&CommManager::onLostExtConnection, this, std::placeholders::_1));
#endif
    _guiCommServer->setNewConnectionCbk(std::bind(&CommManager::onNewGuiConnection, this));
    _guiCommServer->setLostConnectionCbk(std::bind(&CommManager::onLostGuiConnection, this, std::placeholders::_1));
}

CommManager::~CommManager() {
#if defined(__APPLE__) || defined(_WIN32)
    _extCommServer.reset();
#endif
    _guiCommServer.reset();

    // Check that there is no memory leak
#if defined(__APPLE__) || defined(_WIN32)
    assert(_extCommServer.use_count() == 0);
#endif
    assert(_guiCommServer.use_count() == 0);
}

void CommManager::start() {
    // Start Gui CommServer
    LOGW_INFO(Log::instance()->getLogger(), L"Starting " << CommonUtility::s2ws(_guiCommServer->name()));
    if (!_guiCommServer->listen({})) {
        LOGW_WARN(Log::instance()->getLogger(), L"Can't start " << CommonUtility::s2ws(_guiCommServer->name()));
        _addError(Error(errId(), ExitCode::SystemError, ExitCause::Unknown));
    } else {
        LOGW_INFO(Log::instance()->getLogger(), CommonUtility::s2ws(_guiCommServer->name()) << L" started");
    }

#if defined(__APPLE__) || defined(_WIN32)
    // Start Ext CommServer
#if defined(_WIN32)
    SyncPath pipePath = createPipe();
    LOGW_INFO(Log::instance()->getLogger(),
              L"Starting " << Utility::s2ws(_extCommServer->name()) << L": " << Utility::formatSyncPath(pipePath));
    if (!_extCommServer->listen(pipePath)) {
#else
    LOGW_INFO(Log::instance()->getLogger(), L"Starting " << CommonUtility::s2ws(_extCommServer->name()));
    if (!_extCommServer->listen({})) {
#endif
        LOGW_WARN(Log::instance()->getLogger(), L"Can't start " << CommonUtility::s2ws(_extCommServer->name()));
        _addError(Error(errId(), ExitCode::SystemError, ExitCause::Unknown));
    } else {
        LOGW_INFO(Log::instance()->getLogger(), CommonUtility::s2ws(_extCommServer->name()) << L" started");
    }
#endif
}

void CommManager::stop() {
#if defined(__APPLE__) || defined(_WIN32)
    _extCommServer->close();
#endif
    _guiCommServer->close();
}

#if defined(__APPLE__) || defined(_WIN32)
void CommManager::registerSync(const SyncPath &localPath) {
    CommString command(Str("REGISTER_PATH"));
    command.append(messageCdeSeparator);
    command.append(localPath.native());
    broadcastExtCommand(command);
}

void CommManager::unregisterSync(const SyncPath &localPath) {
    CommString command(Str("UNREGISTER_PATH"));
    command.append(messageCdeSeparator);
    command.append(localPath.native());
    broadcastExtCommand(command);
}

void CommManager::executeCommandDirect(const CommString &commandLineStr, bool broadcast) {
    if (broadcast) {
        broadcastExtCommand(commandLineStr);
    } else {
        executeExtCommand(commandLineStr);
    }
}

void CommManager::executeExtCommand(const CommString &commandLineStr) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommString2WStr(commandLineStr));

    std::shared_ptr<ExtensionJob> job =
            std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr, std::list<std::shared_ptr<AbstractCommChannel>>());
    if (ExitInfo exitInfo = job->runSynchronously(); exitInfo.code() != ExitCode::Ok) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in ExtensionJob::runSynchronously - cmd=" << CommString2WStr(commandLineStr));
    }
}

void CommManager::executeExtCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommString2WStr(commandLineStr));

    if (channel) {
        std::shared_ptr<ExtensionJob> job = std::make_shared<ExtensionJob>(
                shared_from_this(), commandLineStr, std::list<std::shared_ptr<AbstractCommChannel>>({channel}));
        if (ExitInfo exitInfo = job->runSynchronously(); exitInfo.code() != ExitCode::Ok) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::runSynchronously - cmd=" << CommString2WStr(commandLineStr));
        }
    }
}

void CommManager::broadcastExtCommand(const CommString &commandLineStr) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommString2WStr(commandLineStr));

    if (!_extCommServer->connections().empty()) {
        std::shared_ptr<ExtensionJob> job =
                std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr, _extCommServer->connections());
        if (ExitInfo exitInfo = job->runSynchronously(); exitInfo.code() != ExitCode::Ok) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::runSynchronously - cmd=" << CommString2WStr(commandLineStr));
        }
    }
}

void CommManager::onNewExtConnection() {
    std::shared_ptr<AbstractCommChannel> channel = _extCommServer->nextPendingConnection();
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
            registerSync(sync.localPath().native());
        }
    }
}

void CommManager::onExtQueryReceived(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_IF_FAIL(Log::instance()->getLogger(), channel)

    while (channel->canReadLine()) {
        CommString line;
        while (!line.ends_with(finderExtQuerySeparator)) {
            if (!channel->canReadLine()) {
                LOGW_WARN(Log::instance()->getLogger(), L"Failed to parse Extension message - msg="
                                                                << CommString2WStr(line) << L" channel="
                                                                << CommonUtility::s2ws(channel->id()));
                return;
            }
            line.append(channel->readLine());
        }
        // Remove the separator
        line.erase(line.find(finderExtQuerySeparator));
        LOGW_INFO(Log::instance()->getLogger(), L"Received Extension message - msg=" << CommString2WStr(line) << L" channel="
                                                                                     << CommonUtility::s2ws(channel->id()));
        executeExtCommand(line, channel);
    }
}

void CommManager::onLostExtConnection(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "Lost ext connection - channel=" << channel->id());
}
#endif

void CommManager::onNewGuiConnection() {
    std::shared_ptr<AbstractCommChannel> channel = _guiCommServer->connections().back();
    if (!channel) return;

    LOG_INFO(Log::instance()->getLogger(), "New gui connection - channel=" << channel->id());
    channel->setReadyReadCbk(std::bind(&CommManager::onGuiQueryReceived, this, std::placeholders::_1));
}

void CommManager::onGuiQueryReceived(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "onQueryReceived");
    LOG_IF_FAIL(Log::instance()->getLogger(), channel)

    while (channel->canReadLine()) {
        CommString query = channel->readLine();
        LOGW_INFO(Log::instance()->getLogger(), L"Query received: " << CommString2WStr(query));

        const auto queryArgs = CommonUtility::splitCommString(query, guiArgSeparator);
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

void CommManager::onLostGuiConnection(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "Lost gui connection - sender=" << channel->id());
}

#if defined(_WIN32)
SyncPath CommManager::createPipe() {
    // Get pipe file path
    std::string name(QStr2Str(Theme::instance()->appName()));
    name.append("-");
    name.append(Utility::userName());

    SyncPath pipePath = SyncPath(R"(\\.\pipe\)") / Str2SyncName(name);

    // Delete/create pipe file
    SocketCommServer::removeServer(pipePath);
    if (const QFileInfo info(Path2QStr(pipePath)); !info.dir().exists()) {
        if (info.dir().mkpath(".")) {
            QFile::setPermissions(Path2QStr(pipePath),
                                  QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
            LOGW_DEBUG(Log::instance()->getLogger(), L"Pipe created: " << Utility::formatSyncPath(pipePath));
        } else {
            LOGW_WARN(Log::instance()->getLogger(), L"Failed to create pipe: " << Utility::formatSyncPath(pipePath));
        }
    }

    return pipePath;
}
#endif

} // namespace KDC
