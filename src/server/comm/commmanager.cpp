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
#include "guijobs/guijobfactory.h"
#include "config.h"
#include "libcommon/utility/logiffail.h"
#include "libcommon/utility/utility.h"
#include "libcommon/theme/theme.h"
#include "libcommon/comm.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"

// TODO: To remove later
#include "oldcommserver.h"

#ifndef KD_LINUX
#include "extcommserver.h"
#endif

#include "guicommserver.h"
#include "guijobs/abstractguijob.h"
#include "guijobs/loginrequesttokenjob.h"

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QStandardPaths>
#include <Poco/JSON/Parser.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {
CommManager::CommManager(const SyncPalMap &syncPalMap, const VfsMap &vfsMap) :
    _syncPalMap(syncPalMap),
    _vfsMap(vfsMap),
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    _extCommServer(std::make_shared<ExtCommServer>("Extension Comm Server")),
#endif
    _guiCommServer(std::make_shared<GuiCommServer>("GUI Comm Server")),
    _guiJobFactory(std::make_unique<GuiJobFactory>()) {
#if defined(KD_MACOS)
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
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    _extCommServer->setNewConnectionCbk(std::bind(&CommManager::onNewExtConnection, this));
    _extCommServer->setLostConnectionCbk(std::bind(&CommManager::onLostExtConnection, this, std::placeholders::_1));
#endif
    _guiCommServer->setNewConnectionCbk(std::bind(&CommManager::onNewGuiConnection, this));
    _guiCommServer->setLostConnectionCbk(std::bind(&CommManager::onLostGuiConnection, this, std::placeholders::_1));
}

CommManager::~CommManager() {
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    _extCommServer.reset();
#endif
    _guiCommServer.reset();

    // Check that there is no memory leak
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    assert(_extCommServer.use_count() == 0);
#endif
    assert(_guiCommServer.use_count() == 0);
}

void CommManager::start() {
    // Start Gui CommServer
    LOGW_INFO(Log::instance()->getLogger(), L"Starting " << CommonUtility::s2ws(_guiCommServer->name()));
    if (!_guiCommServer->listen()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Can't start " << CommonUtility::s2ws(_guiCommServer->name()));
        _addErrorCbk(Error(ERR_ID, ExitCode::SystemError, ExitCause::Unknown));
    } else {
        LOGW_INFO(Log::instance()->getLogger(), CommonUtility::s2ws(_guiCommServer->name()) << L" started");
    }

#if defined(KD_MACOS) || defined(KD_WINDOWS)
    // Start Ext CommServer
    LOGW_INFO(Log::instance()->getLogger(), L"Starting " << CommonUtility::s2ws(_extCommServer->name()));
    if (!_extCommServer->listen()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Can't start " << CommonUtility::s2ws(_extCommServer->name()));
        _addErrorCbk(Error(ERR_ID, ExitCode::SystemError, ExitCause::Unknown));
    } else {
        LOGW_INFO(Log::instance()->getLogger(), CommonUtility::s2ws(_extCommServer->name()) << L" started");
    }
#endif
}

void CommManager::stop() {
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    _extCommServer->close();
#endif
    _guiCommServer->close();
}

#if defined(KD_MACOS) || defined(KD_WINDOWS)
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
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommonUtility::commString2WStr(commandLineStr));

    auto job =
            std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr, std::list<std::shared_ptr<AbstractCommChannel>>());
    if (ExitInfo exitInfo = job->runSynchronously(); !exitInfo) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in ExtensionJob::runSynchronously - cmd=" << CommonUtility::commString2WStr(commandLineStr));
    }
}

void CommManager::executeExtCommand(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Execute command - cmd=" << CommonUtility::commString2WStr(commandLineStr));

    if (channel) {
        auto job = std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr,
                                                  std::list<std::shared_ptr<AbstractCommChannel>>({channel}));
        if (ExitInfo exitInfo = job->runSynchronously(); !exitInfo) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::runSynchronously - cmd=" << CommonUtility::commString2WStr(commandLineStr));
        }
    }
}

void CommManager::broadcastExtCommand(const CommString &commandLineStr) {
    LOGW_DEBUG(Log::instance()->getLogger(), L"Broadcast command - cmd=" << CommonUtility::commString2WStr(commandLineStr));

    if (!_extCommServer->connections().empty()) {
        auto job = std::make_shared<ExtensionJob>(shared_from_this(), commandLineStr, _extCommServer->connections());
        if (ExitInfo exitInfo = job->runSynchronously(); !exitInfo) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in ExtensionJob::runSynchronously - cmd=" << CommonUtility::commString2WStr(commandLineStr));
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
        _addErrorCbk(Error(ERR_ID, ExitCode::DbError, ExitCause::Unknown));
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

    while (channel->canReadMessage() || channel->bytesAvailable() > 0) {
        CommString query = channel->readMessage();
        if (query.empty()) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to read message or empty message");
            break;
        }
        LOGW_INFO(Log::instance()->getLogger(), L"Received Extension message - msg=" << CommonUtility::commString2WStr(query)
                                                                                     << L" channel="
                                                                                     << CommonUtility::s2ws(channel->id()));
        executeExtCommand(query, channel);
    }
}

void CommManager::onLostExtConnection(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "Lost ext connection - channel=" << channel->id());
}

#endif

void CommManager::executeGuiQuery(const CommString &commandLineStr, std::shared_ptr<AbstractCommChannel> channel) {
    // Deserialize generic parameters
    int requestId;
    RequestNum requestNum;
    Poco::DynamicStruct inParams;
    if (!AbstractGuiJob::deserializeGenericInputParms(commandLineStr, requestId, requestNum, inParams)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in AbstractGuiJob::deserializeGenericInputParms - query="
                                                        << CommonUtility::commString2WStr(commandLineStr));
        return;
    }

    // Create and execute GUI job
    std::shared_ptr<AbstractGuiJob> job = _guiJobFactory->make(requestNum, shared_from_this(), requestId, inParams, channel);
    if (!job) {
        LOG_WARN(Log::instance()->getLogger(), "Job not implemented: num=" << requestNum);
        return;
    }

    job->runJob();
}

void CommManager::onNewGuiConnection() {
    std::shared_ptr<AbstractCommChannel> channel = _guiCommServer->nextPendingConnection();
    if (!channel) return;

    LOG_INFO(Log::instance()->getLogger(), "New gui connection - channel=" << channel->id());
    channel->setReadyReadCbk(std::bind(&CommManager::onGuiQueryReceived, this, std::placeholders::_1));
}

void CommManager::onGuiQueryReceived(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "onQueryReceived");
    LOG_IF_FAIL(Log::instance()->getLogger(), channel)

    while (channel->canReadMessage()) {
        CommString query = channel->readMessage();
        if (query.empty()) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to read message or empty message");
            break;
        }
        LOGW_INFO(Log::instance()->getLogger(), L"Received GUI message - msg=" << CommonUtility::commString2WStr(query)
                                                                               << L" channel="
                                                                               << CommonUtility::s2ws(channel->id()));
        executeGuiQuery(query, channel);
    }
}

void CommManager::onLostGuiConnection(std::shared_ptr<AbstractCommChannel> channel) {
    LOG_INFO(Log::instance()->getLogger(), "Lost gui connection - sender=" << channel->id());
}
} // namespace KDC
