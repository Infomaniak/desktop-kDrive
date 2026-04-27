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

#pragma once

// ============================================================================
// IOHELPER BENCHMARK HEADER
// ============================================================================
// Compatible: Visual Studio 2026 (MSVC 195+)
// Standard: C++17
// ============================================================================

#include "testincludes.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <cstdint>

#include <fcntl.h>
#if defined(KD_LINUX) || defined(KD_MACOS)
#include <unistd.h>
#include <linux/stat.h>
#endif
#define _GNU_SOURCE
#include <system_error>

// Windows API
#include <windows.h>
#include <io.h>
#include <sys/stat.h>

// ============================================================================
// BENCHMARK RESULTS STRUCT
// ============================================================================
struct BenchmarkResult {
        std::string category;
        std::string method;
        double time_ms;
        double avg_ns_per_call;
        bool success;
};

// ============================================================================
// BENCHMARK ENGINE CLASS
// ============================================================================
class testbenchmarkio {
    public:
        testbenchmarkio(int iterations = 50000, bool warmup = true);

        template<typename Func>
        double measure(Func func, const std::string &path);

        void addResult(const std::string &category, const std::string &method, double time_ms, bool success = true);

        void printResults() const;
        void printResultsByCategory() const;

        const std::vector<BenchmarkResult> &getResults() const;
        void reset();

        int getIterations() const { return iterations_; }

    private:
        int iterations_;
        bool warmup_;
        std::vector<BenchmarkResult> results_;
};

// ============================================================================
// TEST FUNCTIONS - CATEGORY 1: EXISTS (No file handle opened)
// These are quick checks to determine if the file exists using different APIs
// ============================================================================
namespace ExistsTests {
bool filesystem_status(const std::string &path);
bool filesystem_exists(const std::string &path);
bool win32_getfileattributes_a(const std::string &path);
bool win32_getfileattributes_w(const std::string &path);
bool crt_stat(const std::string &path);
// bool crt_access(const std::string &path); // Removed: not a real existence check for drive client
} // namespace ExistsTests

// ============================================================================
// TEST FUNCTIONS - CATEGORY 2: METADATA (retrieve metadata: perms, times, size)
// ============================================================================
namespace MetadataTests {
bool stat_full(const std::string &path);
bool lstat_full(const std::string &path);
bool fstat_full(const std::string &path);
bool statx_full(const std::string &path);
bool filesystem_full(const std::string &path);
} // namespace MetadataTests

// ============================================================================
// TEST FUNCTIONS - CATEGORY 4: FILE SIZE
// ============================================================================
namespace SizeTests {
uint64_t filesystem_filesize(const std::string &path);
uint64_t win32_getfilesize(const std::string &path);
uint64_t crt_filelength(const std::string &path);
} // namespace SizeTests

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
bool CreateTestFile(const std::string &path, const std::string &content = "Benchmark test data");
bool DeleteTestFile(const std::string &path);

// ============================================================================
// CREATE FUNCTIONS
// ============================================================================
namespace CreateTests {
bool create_ofstream(const std::string &dir);
bool create_fopen(const std::string &dir);
bool create_CreateFileA(const std::string &dir);
bool create_CreateFileW(const std::string &dir);
} // namespace CreateTests

// ----------------------------------------------------------------------------
// DELETE FUNCTIONS
// ----------------------------------------------------------------------------
namespace DeleteTests {
bool delete_filesystem_remove(const std::string &dir);
bool delete_DeleteFileA(const std::string &dir);
bool delete_crt_remove(const std::string &dir);
} // namespace DeleteTests

// ----------------------------------------------------------------------------
// MOVE FUNCTIONS
// ----------------------------------------------------------------------------
namespace MoveTests {
bool move_filesystem_rename(const std::string &dir);
bool move_MoveFileA(const std::string &dir);
bool move_MoveFileW(const std::string &dir);
} // namespace MoveTests

// ============================================================================
// TEST FUNCTIONS - IOHELPER (wraps the real IoHelper API used in production)
// ============================================================================
namespace IoHelperTests {
bool iohelper_checkIfPathExists(const std::string &path);
bool iohelper_getFileStat(const std::string &path);
bool iohelper_getFileSize(const std::string &path);
bool iohelper_openFile(const std::string &path);
bool iohelper_setFileDates(const std::string &path);
bool iohelper_deleteItem(const std::string &dir);
bool iohelper_moveItem(const std::string &dir);
} // namespace IoHelperTests

// ============================================================================
// RUNNER: Execute all benchmarks
// ============================================================================
void RunAllBenchmarks(const std::string &testFilePath = "io_benchmark_test.tmp", int iterations = 50000);

// ============================================================================
// TEMPLATE IMPLEMENTATION (Must be in header)
// ============================================================================
template<typename Func>
inline double testbenchmarkio::measure(Func func, const std::string &path) {
    // Warmup phase
    if (warmup_) {
        for (int32_t i = 0; i < 10; ++i) func(path);
    }

    // Timed phase
    auto start = std::chrono::high_resolution_clock::now();
    volatile bool result = false;
    for (int32_t i = 0; i < iterations_; ++i) {
        result = func(path);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

namespace KDC {

class BenchmarkIOHelper : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(BenchmarkIOHelper);
        CPPUNIT_TEST(runAllIOBenchmarks);
        CPPUNIT_TEST_SUITE_END();

    public:
        virtual void setUp() override;
        virtual void tearDown() override;

        void runAllIOBenchmarks();

    private:
        int32_t iterations_ = 10000;
};

} // namespace KDC
