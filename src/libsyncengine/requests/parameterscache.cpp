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

#include "parameterscache.h"
#include "libparms/db/parmsdb.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/io/iohelper.h"
#include "utility/jsonparserutility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::shared_ptr<ParametersCache> ParametersCache::_instance = nullptr;

std::shared_ptr<ParametersCache> ParametersCache::instance(const bool isTest /*= false*/) {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<ParametersCache>(new ParametersCache(isTest));
        } catch (std::exception const &) {
            return nullptr;
        }
    }

    return _instance;
}

void ParametersCache::reset() {
    if (_instance) {
        _instance = nullptr;
    }
}

ParametersCache::ParametersCache(bool isTest /*= false*/) {
    if (isTest) {
        // In test, use extended log by default
        _parameters.setExtendedLog(true);
        return;
    }

    // Load parameters from DB
    bool found = false;
    if (!ParmsDb::instance()->selectParameters(_parameters, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectParameters");
        throw std::runtime_error("Failed to read parameters");
    }
    if (!found) {
        assert(false);
        LOG_WARN(Log::instance()->getLogger(), "Parameters not found");
        throw std::runtime_error("Failed to read parameters");
    }

    // Load parameters from settings file
    std::string jsonStr;
    SyncPath test = std::filesystem::current_path() / "kDrive_settings";
    if (const auto ioError = IoHelper::readLocalFile(test, jsonStr);
        ioError != IoError::Success && ioError != IoError::NoSuchFileOrDirectory) {
        assert(false);
        LOG_WARN(Log::instance()->getLogger(), "Failed to read parameters from settings file.");
    }

    Poco::JSON::Object::Ptr jsonObj;
    try {
        jsonObj = Poco::JSON::Parser{}.parse(jsonStr).extract<Poco::JSON::Object::Ptr>();
    } catch (const Poco::Exception &exc) {
        assert(false);
        LOG_WARN(Log::instance()->getLogger(), "Failed to parse JSON from settings file.");
    }
    std::string updateStr;
    (void) JsonParserUtility::extractValue(jsonObj, "update", updateStr, false);
    _parameters.setUpdateDeactivated(updateStr == "deactivated");
}

void ParametersCache::save(ExitCode *exitCode /*= nullptr*/) const {
    // Get old parameters
    Parameters oldParameters;
    bool found = false;
    if (!ParmsDb::instance()->selectParameters(oldParameters, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectParameters");
        if (exitCode) *exitCode = ExitCode::DbError;
        return;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Parameters not found");
        if (exitCode) *exitCode = ExitCode::DataError;
        return;
    }

    // Update parameters
    if (!ParmsDb::instance()->updateParameters(_parameters, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateParameters");
        if (exitCode) *exitCode = ExitCode::DbError;
        return;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Parameters not found");
        if (exitCode) *exitCode = ExitCode::DataError;
        return;
    }

    // Check if Log parameters have been updated
    if (oldParameters.useLog() != _parameters.useLog() || oldParameters.logLevel() != _parameters.logLevel() ||
        oldParameters.purgeOldLogs() != _parameters.purgeOldLogs()) {
        if (!Log::instance()->configure(_parameters.useLog(), _parameters.logLevel(), _parameters.purgeOldLogs())) {
            LOG_WARN(Log::instance()->getLogger(), "Error in Log::configure");
            if (exitCode) *exitCode = ExitCode::SystemError;
            return;
        }
    }

    if (exitCode) *exitCode = ExitCode::Ok;
}

void ParametersCache::setUploadSessionParallelThreads(const int count) {
    _parameters.setUploadSessionParallelJobs(count);
    save();
}

void ParametersCache::decreaseUploadSessionParallelThreads() {
    if (const int uploadSessionParallelJobs = _parameters.uploadSessionParallelJobs(); uploadSessionParallelJobs > 1) {
        const int newUploadSessionParallelJobs = uploadSessionParallelJobs - 1;
        _parameters.setUploadSessionParallelJobs(newUploadSessionParallelJobs);
        save();
        LOG_DEBUG(Log::instance()->getLogger(),
                  "Upload session max parallel threads parameters set to " << newUploadSessionParallelJobs);
    } else {
        sentry::Handler::captureMessage(sentry::Level::Warning, "AppServer::addError",
                                        "Upload session max parallel threads parameters cannot be decreased");
    }
}

} // namespace KDC
