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

#include "log/log.h"

#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

bool setFileDates(const SyncPath &filePath, std::optional<KDC::SyncTime> creationDate,
                  std::optional<KDC::SyncTime> modificationDate, bool symlink, bool &exists);

static bool init_private() {
    return true;
}

static void makeMessage() {}

bool moveItemToTrash(const SyncPath &itemPath, std::string &errorStr);
bool preventSleeping(bool enable);
bool preventSleeping();
void restartFinderExtension();

static bool moveItemToTrash_private(const SyncPath &itemPath) {
    if (itemPath.empty()) {
        LOG_WARN(Log::instance()->getLogger(), "Path is empty");
        return false;
    }

    std::string errorStr;
    if (!moveItemToTrash(itemPath, errorStr)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in moveItemToTrash - err=" << errorStr.c_str());
        return false;
    }

    return true;
}

static bool preventSleeping_private(bool enable) {
    return preventSleeping(enable);
}

static void restartFinderExtension_private() {
    return restartFinderExtension();
}

static bool totalRamAvailable_private(uint64_t &ram, int &errorCode) {
    int mib[2];
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    uint64_t physical_memory;
    size_t length = sizeof(int64_t);
    if (sysctl(mib, 2, &physical_memory, &length, nullptr, 0) == 0) {
        ram = physical_memory;
        return true;
    }
    errorCode = errno;
    return false;
}

static bool ramCurrentlyUsed_private(uint64_t &ram, int &errorCode) {
    vm_size_t page_size;
    mach_port_t mach_port;
    mach_msg_type_number_t count;
    vm_statistics64_data_t vm_stats;

    mach_port = mach_host_self();
    count = sizeof(vm_stats) / sizeof(natural_t);
    if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
        KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t) &vm_stats, &count)) {
        uint64_t used_memory = (vm_stats.active_count + vm_stats.inactive_count + vm_stats.wire_count) * page_size;
        ram = used_memory;
        return true;
    }
    errorCode = errno;
    return false;
}

static bool ramCurrentlyUsedByProcess_private(uint64_t &ram, int &errorCode) {
    struct task_basic_info t_info {};
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t) &t_info, &t_info_count)) {
        errorCode = errno;
        return false;
    }
    // resident_size (real physical memory used) correspond to the RAM
    ram = t_info.resident_size;
    return true;
}

static bool cpuUsage_private(uint64_t &previousTotalTicks, uint64_t &previousIdleTicks, double &percent) {
    host_cpu_load_info_data_t cpuinfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t) &cpuinfo, &count) == KERN_SUCCESS) {
        uint64_t totalTicks = 0;
        for (auto cpu_tick: cpuinfo.cpu_ticks) totalTicks += cpu_tick;
        uint64_t idleTicks = cpuinfo.cpu_ticks[CPU_STATE_IDLE];

        uint64_t totalTicksSinceLastTime = totalTicks - previousTotalTicks;
        uint64_t idleTicksSinceLastTime = idleTicks - previousIdleTicks;
        uint64_t proportion = (totalTicksSinceLastTime > 0)
                                      ? static_cast<uint64_t>(((float) idleTicksSinceLastTime) / totalTicksSinceLastTime)
                                      : 0.0;
        percent = 1.0f - proportion;
        percent *= 100.0;

        previousTotalTicks = totalTicks;
        previousIdleTicks = idleTicks;
        return true;
    }

    return false;
}

static std::string executeCommand(const std::string &command) {
    std::array<char, 128> buffer{};
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Erreur lors de l'exécution de la commande");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

static bool cpuUsageByProcess_private(double &percent) {
    percent = 0;

    pid_t pid = getpid();
    const std::string getCPUCommand = "ps -p " + std::to_string(pid) + " -o %cpu | awk 'FNR == 2 {print $1}'";
    std::string cpuUsageStr;
    try {
        cpuUsageStr = executeCommand(getCPUCommand);
    } catch (...) {
        return false;
    }
    double cpuUsage;
    std::istringstream iss(cpuUsageStr);
    if (!(iss >> cpuUsage)) {
        // log error
        return false;
    }
    percent = cpuUsage;
    return true;
}

static bool setFileDates_private(const KDC::SyncPath &filePath, std::optional<KDC::SyncTime> creationDate,
                                 std::optional<KDC::SyncTime> modificationDate, bool symlink, bool &exists) {
    return KDC::setFileDates(filePath, creationDate, modificationDate, symlink, exists);
}
} // namespace KDC
