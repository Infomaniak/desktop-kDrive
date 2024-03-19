/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "libcommonserver/utility/utility.h"
#include "log/log.h"

#include <thread>
#include <cstdint>
#include <list>

#include <Poco/Thread.h>

namespace KDC {

class PerformanceWatcher {
    public:
        static PerformanceWatcher *instance();
        static void stop();

        void logHardwareResources();

        inline uint64_t getRamAvailable() const { return _ramAvailable; }
        inline uint64_t getRamCurrentlyUsed() const { return _ramCurrentlyUsed; }
        inline uint64_t getRamCurrentlyUsedByProcess() const { return _ramCurrentlyUsedByProcess; }
        inline int getCpuUsageByProcessPercent() { return _cpuUsageByProcessPercent; }
        inline int getMovingAverageCpuUsagePercent() { return _movingAverageCpuUsagePercent; }

    private:
        PerformanceWatcher();

        static void run();

        static PerformanceWatcher *_instance;
        static bool _stop;

        log4cplus::Logger _logger = Log::instance()->getLogger();
        std::unique_ptr<std::thread> _thread = nullptr;

        static void updateAllStats();

        static int _cpuUsagePercent;
        static int _cpuUsageByProcessPercent;
        static int _movingAverageCpuUsagePercent;
        static std::list<int> _movingAverageValues;

        static uint64_t _lastTotalUser;
        static uint64_t _lastTotalUserLow;
        static uint64_t _lastTotalSys;
        static uint64_t _lastTotalIdle;

        static uint64_t _previousTotalTicks;
        static uint64_t _previousIdleTicks;

        static uint64_t _ramAvailable;
        static uint64_t _ramCurrentlyUsed;
        static uint64_t _ramCurrentlyUsedByProcess;

        static bool updateCpuUsage();
        static bool updateCpuUsageByProcess();
        static bool updateRAMAvailable();
        static bool updateRAMUsed();
        static bool updateRAMUsedByProc();

        uint64_t bytesToBetterUnit(uint64_t bytes, std::string &unit);
        static double calculatePercent(uint64_t value, uint64_t total);
        static uint64_t bytesToGb(uint64_t bytes);
        static uint64_t bytesToMb(uint64_t bytes);
};

}  // namespace KDC
