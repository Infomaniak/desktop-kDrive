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

#include "parameters.h"

#define DEFAULT_VERSION "1"
#define DEFAULT_BIG_FOLDERS_SIZE_LIMIT 500 // MB

#define UPLOAD_SESSION_PARALLEL_THREADS 3

namespace KDC {

int Parameters::_uploadSessionParallelJobsDefault = UPLOAD_SESSION_PARALLEL_THREADS;

Parameters::Parameters() :
    _language(Language::Default),
    _monoIcons(false),
    _autoStart(true),
    _moveToTrash(true),
    _notificationsDisabled(NotificationsDisabled::Never),
    _useLog(true),
    _logLevel(LogLevel::Debug),
    _extendedLog(false),
    _purgeOldLogs(true),
    _proxyConfig(ProxyConfig()),
    _useBigFolderSizeLimit(false),
    _bigFolderSizeLimit(DEFAULT_BIG_FOLDERS_SIZE_LIMIT),
    _darkTheme(false),
    _showShortcuts(true),
    _updateFileAvailable(std::string()),
    _updateTargetVersion(std::string()),
    _autoUpdateAttempted(false),
    _seenVersion(std::string()),
    _dialogGeometry(std::shared_ptr<std::vector<char>>()),
    _maxAllowedCpu(50),
    _uploadSessionParallelJobs(UPLOAD_SESSION_PARALLEL_THREADS) {}

} // namespace KDC
