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

#include "parametersupdatejob.h"
#include "appserver.h"
#include "version.h"
#include "keychainmanager/keychainmanager.h"
#include "requests/parameterscache.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/network/proxy.h"

// Output parameters keys
static const auto inParamsParametersInfo = "parametersInfo";


namespace KDC {

ParametersUpdateJob::ParametersUpdateJob(std::shared_ptr<CommManager> commManager, int requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::PARAMETERS_UPDATE;
}

ExitInfo ParametersUpdateJob::deserializeInputParms() {
    try {
        readParamValue(inParamsParametersInfo, _parametersInfo, dynamicVar2Struct<ParametersInfo>);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ParametersUpdateJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}


ExitInfo ParametersUpdateJob::process() {
    // Retrieve current settings
    const Parameters parameters = ParametersCache::instance()->parameters();
    std::string pwd;
    if (parameters.proxyConfig().needsAuth()) {
        // Read pwd from keystore
        bool found = false;
        if (!KeyChainManager::instance()->readDataFromKeystore(parameters.proxyConfig().token(), pwd, found)) {
            LOG_WARN(_logger, "Failed to read proxy pwd from keychain");
        }
        if (!found) {
            LOG_DEBUG(_logger, "Proxy pwd not found for keychainKey=" << parameters.proxyConfig().token());
        }
    }

    // Update parameters
    const ExitCode exitCode = ServerRequests::updateParameters(_parametersInfo);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::updateParameters");
        addError(Error(ERR_ID, exitCode));
    }

    // extendedLog change propagation
    if (parameters.extendedLog() != _parametersInfo.extendedLog()) {
        _commManager->appServer().logExtendedLogActivationMessage(_parametersInfo.extendedLog());
        const std::scoped_lock lock(AppServer::vfsMapMutex);
        for (const auto &[_, vfs]: AppServer::vfsMap) {
            vfs->setExtendedLog(_parametersInfo.extendedLog());
        }
    }

    // Language change propagation
    if (parameters.language() != _parametersInfo.language()) {
        CommonUtility::setupTranslations(&_commManager->appServer(), _parametersInfo.language());
    }

    // ProxyConfig change propagation
    if (parameters.proxyConfig().type() != _parametersInfo.proxyConfigInfo().type() ||
        parameters.proxyConfig().hostName() != _parametersInfo.proxyConfigInfo().hostName().toStdString() ||
        parameters.proxyConfig().port() != _parametersInfo.proxyConfigInfo().port() ||
        parameters.proxyConfig().needsAuth() != _parametersInfo.proxyConfigInfo().needsAuth() ||
        parameters.proxyConfig().user() != _parametersInfo.proxyConfigInfo().user().toStdString() ||
        pwd != _parametersInfo.proxyConfigInfo().pwd().toStdString()) {
        Proxy::instance()->setProxyConfig(ParametersCache::instance()->parameters().proxyConfig());
    }

    if (KDRIVE_VERSION_MAJOR >= 4) {
        // Sentry activation change propagation
        if (parameters.sentryEnabled() != _parametersInfo.sentryEnabled()) {
            sentry::Handler::instance()->setIsSentryActivated(_parametersInfo.sentryEnabled());
        }
    }

    return exitCode;
}

} // namespace KDC
