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
#include <map>
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

namespace KDC {
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

        template<typename Func, typename Init, typename Teardown>
        double measure(Func func, const SyncPath &path, Init init, Teardown teardown, bool per_iteration = false);
        void addResult(const std::string &category, const std::string &method, double time_ms, bool success = true);

        void setCategoryExpectedCounts(const std::map<std::string, int> &counts) { categoryTotals_ = counts; }

        void printResults() const;
        void printResultsByCategory() const;

        const std::vector<BenchmarkResult> &getResults() const;
        void reset();

    private:
        bool warmup_;
        std::map<std::string, int> categoryTotals_;
        std::vector<BenchmarkResult> results_;

        void printProgress() const;
};

// ============================================================================
// TEST FUNCTIONS - CATEGORY 1: EXISTS
// ============================================================================
namespace ExistsTests {
void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

bool filesystem_status(const SyncPath &path);
bool filesystem_exists(const SyncPath &path);
bool win32_getfileattributes_w(const SyncPath &path);
bool crt_stat(const SyncPath &path);
} // namespace ExistsTests

// ============================================================================
// TEST FUNCTIONS - CATEGORY 2: METADATA (retrieve metadata: perms, times, size)
// ============================================================================
namespace MetadataTests {
void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

bool stat_full(const SyncPath &path);
bool statx_full(const SyncPath &path);
bool fstat_full(const SyncPath &path);
bool filesystem_full(const SyncPath &path);
} // namespace MetadataTests

// ============================================================================
// TEST FUNCTIONS - CATEGORY 3: READ
// ============================================================================
namespace ReadTests {
void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

bool ifstream_binary(const SyncPath &path);
bool ifstream_text(const SyncPath &path);
bool fread_binary(const SyncPath &path);
bool fread_text(const SyncPath &path);
bool win32_readfile(const SyncPath &path);
bool win32_readfile_w(const SyncPath &path);
} // namespace ReadTests

// ============================================================================
// TEST FUNCTIONS - CATEGORY 4: FILE SIZE
// ============================================================================
namespace SizeTests {
void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

bool filesystem_filesize(const SyncPath &path);
bool win32_getfilesize(const SyncPath &path);
bool crt_filelength(const SyncPath &path);
} // namespace SizeTests

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
bool CreateTestFile(const SyncPath &path, const std::string &content = "Benchmark test data");
bool DeleteTestFile(const SyncPath &path);
int getiterations(void);

// ============================================================================
// CREATE FUNCTIONS
// ============================================================================
namespace CreateTests {
void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

bool create_ofstream(const SyncPath &dir);
bool create_fopen(const SyncPath &dir);
bool create_CreateFileA(const SyncPath &dir);
bool create_CreateFileW(const SyncPath &dir);
} // namespace CreateTests

// ----------------------------------------------------------------------------
// DELETE FUNCTIONS
// ----------------------------------------------------------------------------
namespace DeleteTests {
void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

bool delete_filesystem_remove(const SyncPath &dir);
bool delete_DeleteFileA(const SyncPath &dir);
bool delete_crt_remove(const SyncPath &dir);
} // namespace DeleteTests

// ----------------------------------------------------------------------------
// MOVE FUNCTIONS
// ----------------------------------------------------------------------------
namespace MoveTests {
void testinit(const SyncPath &path);
void teardown(const SyncPath &path);

bool move_filesystem_rename(const SyncPath &dir);
bool move_MoveFileA(const SyncPath &dir);
bool move_MoveFileW(const SyncPath &dir);
} // namespace MoveTests

// ============================================================================
// TEST FUNCTIONS - IOHELPER (wraps the real IoHelper API used in production)
// ============================================================================
namespace IoHelperTests {
bool iohelper_checkIfPathExists(const SyncPath &path);
bool iohelper_getFileStat(const SyncPath &path);
bool iohelper_getFileSize(const SyncPath &path);
bool iohelper_openFile(const SyncPath &path);
bool iohelper_setFileDates(const SyncPath &path);
bool iohelper_deleteItem(const SyncPath &dir);
bool iohelper_moveItem(const SyncPath &dir);
} // namespace IoHelperTests

// ============================================================================
// RUNNER: Execute all benchmarks
// ============================================================================
void RunAllBenchmarks(const SyncPath &testFilePath = "io_benchmark_test.tmp", int iterations = 50000);

// ============================================================================
// TEMPLATE IMPLEMENTATION
// ============================================================================
template<typename Func, typename Init, typename Teardown>
double testbenchmarkio::measure(Func func, const SyncPath &path, Init init, Teardown teardown, bool per_iteration) {
    volatile bool result = false;

    // Warmup
    if (!per_iteration) init(path);
    for (int32_t i = 0; i < 10; ++i) {
        if (per_iteration) init(path);
        result ^= func(path);
        if (per_iteration) teardown(path);
    }
    if (!per_iteration) teardown(path);


    // Init once if not per-iteration
    if (!per_iteration) init(path);

    auto start = std::chrono::high_resolution_clock::now();

    for (int32_t i = 0; i < getiterations(); ++i) {
        if (per_iteration) init(path);

        result ^= func(path);

        if (per_iteration) teardown(path);
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Teardown once if not per-iteration
    if (!per_iteration) teardown(path);

    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

class BenchmarkIOHelper : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(BenchmarkIOHelper);
        CPPUNIT_TEST(runAllIOBenchmarks);
        CPPUNIT_TEST_SUITE_END();

    public:
        virtual void setUp() override;
        virtual void tearDown() override;

        void runAllIOBenchmarks();
};

} // namespace KDC
