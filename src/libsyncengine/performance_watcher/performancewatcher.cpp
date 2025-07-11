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

#include "performancewatcher.h"
#include "log/log.h"

#include <log4cplus/loggingmacros.h>
#include <cmath>
#include <memory>

namespace KDC {

int PerformanceWatcher::_cpuUsagePercent = 0;
int PerformanceWatcher::_cpuUsageByProcessPercent = 0;
int PerformanceWatcher::_movingAverageCpuUsagePercent = 0;
std::list<int> PerformanceWatcher::_movingAverageValues;

uint64_t PerformanceWatcher::_lastTotalUser = 0;
uint64_t PerformanceWatcher::_lastTotalUserLow = 0;
uint64_t PerformanceWatcher::_lastTotalSys = 0;
uint64_t PerformanceWatcher::_lastTotalIdle = 0;

uint64_t PerformanceWatcher::_previousTotalTicks = 0;
uint64_t PerformanceWatcher::_previousIdleTicks = 0;

uint64_t PerformanceWatcher::_ramAvailable = 0;
uint64_t PerformanceWatcher::_ramCurrentlyUsed = 0;
uint64_t PerformanceWatcher::_ramCurrentlyUsedByProcess = 0;

PerformanceWatcher *PerformanceWatcher::_instance = nullptr;
bool PerformanceWatcher::_stop = false;

PerformanceWatcher *PerformanceWatcher::instance() {
    if (!_instance) {
        _instance = new PerformanceWatcher();
    }
    return _instance;
}

void PerformanceWatcher::stop() {
    if (_instance) {
        _stop = true;
    }
}

PerformanceWatcher::PerformanceWatcher() {
    _thread = std::make_unique<StdLoggingThread>(run);
    LOG_DEBUG(_logger, "Performance Watcher started");
}

void PerformanceWatcher::run() {
    while (true) {
        if (_stop) {
            break;
        }

        updateAllStats();

        // calculate current average cpu usage
        if (_movingAverageValues.size() == 10) {
            _movingAverageValues.pop_front();
            _movingAverageValues.push_back(_cpuUsagePercent);
            for (const auto &value: _movingAverageValues) {
                _movingAverageCpuUsagePercent += value;
            }
            _movingAverageCpuUsagePercent /= 10;
        } else {
            _movingAverageValues.push_back(_cpuUsagePercent);
        }

        Utility::msleep(1000); // Sleep for 1s
    }
}

bool PerformanceWatcher::updateCpuUsage() {
#if defined(KD_LINUX)
    double cpuUsage = 0;
    if (Utility::cpuUsage(_lastTotalUser, _lastTotalUserLow, _lastTotalSys, _lastTotalIdle, cpuUsage)) {
        _cpuUsagePercent = (int) cpuUsage;
        return true;
    }
#elif defined(KD_MACOS)
    double cpuUsage = 0;
    if (Utility::cpuUsage(_previousTotalTicks, _previousIdleTicks, cpuUsage)) {
        _cpuUsagePercent = (int) cpuUsage;
        return true;
    }
#endif
    return false;
}

double PerformanceWatcher::calculatePercent(uint64_t value, uint64_t total) {
    if (total != 0) {
        double proportion = ((double) value / (double) total);
        return trunc(proportion * 100.0);
    }
    return 0;
}

void PerformanceWatcher::logHardwareResources() {
    LOG_DEBUG(_logger, " CPU         : " << _cpuUsagePercent << " %");
    LOG_DEBUG(_logger, " CPU process : " << _cpuUsageByProcessPercent << " %");
    LOG_DEBUG(_logger, "~CPU process : " << _movingAverageCpuUsagePercent << " %");
    std::string unit;

    uint64_t ra = getRamAvailable();
    LOG_DEBUG(_logger, "RAM Available : " << bytesToBetterUnit(ra, unit) << " " << unit);
    uint64_t rcu = getRamCurrentlyUsed();
    LOG_DEBUG(_logger, "RAM Used : " << calculatePercent(rcu, ra) << " %");
    uint64_t rcup = getRamCurrentlyUsedByProcess();
    LOG_DEBUG(_logger, "RAM Used by kDrive : " << calculatePercent(rcup, ra) << " %");
}

bool PerformanceWatcher::updateRAMAvailable() {
    int errorCode = 0;
    uint64_t ra = 0;
    if (Utility::totalRamAvailable(ra, errorCode)) {
        _ramAvailable = ra;
        return true;
    }
    LOG_WARN(Log::instance()->getLogger(), "Failed to read total ram available, error : " << errorCode);
    return false;
}

bool PerformanceWatcher::updateRAMUsed() {
    int errorCode = 0;
    uint64_t rcu = 0;
    if (Utility::ramCurrentlyUsed(rcu, errorCode)) {
        _ramCurrentlyUsed = rcu;
        return true;
    }
    LOG_WARN(Log::instance()->getLogger(), "Failed to read ram currently use, error : " << errorCode);
    return false;
}

bool PerformanceWatcher::updateRAMUsedByProc() {
    int errorCode = 0;
    uint64_t rcup = 0;
    if (Utility::ramCurrentlyUsedByProcess(rcup, errorCode)) {
        _ramCurrentlyUsedByProcess = rcup;
        return true;
    }
    LOG_WARN(Log::instance()->getLogger(), "Failed to read ram currently use by process, error : " << errorCode);
    return false;
}

uint64_t PerformanceWatcher::bytesToMb(uint64_t bytes) {
    return static_cast<uint64_t>(static_cast<double>(bytes) / (1 * pow(10, 6)));
}

uint64_t PerformanceWatcher::bytesToGb(uint64_t bytes) {
    return static_cast<uint64_t>(static_cast<double>(bytes) / (1 * pow(10, 9)));
}

uint64_t PerformanceWatcher::bytesToBetterUnit(uint64_t bytes, std::string &unit) {
    uint64_t toGb = bytesToGb(bytes);
    if (toGb > 0) {
        unit = "GB";
        return toGb;
    }

    uint64_t toMb = bytesToMb(bytes);
    if (toMb > 0) {
        unit = "MB";
        return toMb;
    }

    unit = "bytes";
    return bytes;
}

void PerformanceWatcher::updateAllStats() {
    updateCpuUsage();
    updateCpuUsageByProcess();
    updateRAMAvailable();
    updateRAMUsed();
    updateRAMUsedByProc();
}

bool PerformanceWatcher::updateCpuUsageByProcess() {
    double cpuUsageByProcess = 0;
    if (Utility::cpuUsageByProcess(cpuUsageByProcess)) {
        int nbCore = ((int) std::thread::hardware_concurrency() > 0 ? (int) std::thread::hardware_concurrency() : 1);
        _cpuUsageByProcessPercent = (int) cpuUsageByProcess / nbCore;
        return true;
    }
    return false;
}


} // namespace KDC
