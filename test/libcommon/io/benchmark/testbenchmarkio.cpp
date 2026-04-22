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
#include "testbenchmarkio.h"

// ============================================================================
// BENCHMARK ENGINE IMPLEMENTATION
// ============================================================================
testbenchmarkio::testbenchmarkio(int iterations, bool warmup) :
    iterations_(iterations),
    warmup_(warmup) {}

void testbenchmarkio::addResult(const std::string &category, const std::string &method, double time_ms, bool success) {
    BenchmarkResult r;
    r.category = category;
    r.method = method;
    r.time_ms = time_ms;
    r.avg_ns_per_call = (time_ms * 1'000'000.0) / iterations_;
    r.success = success;
    results_.push_back(r);
}

void testbenchmarkio::printResults() const {
    std::vector<BenchmarkResult> sorted = results_;
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.time_ms < b.time_ms; });

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "                    BENCHMARK RESULTS                           \n";
    std::cout << "================================================================\n";
    std::cout << "Iterations: " << iterations_ << "\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << std::left << std::setw(12) << "Category" << std::setw(40) << "Method" << std::right << std::setw(14)
              << "Time (ms)" << std::setw(18) << "Ns/Call" << std::setw(8) << "OK" << "\n";
    std::cout << "----------------------------------------------------------------\n";

    for (const auto &r: sorted) {
        std::cout << std::left << std::setw(12) << r.category << std::setw(40) << r.method << std::right << std::setw(14)
                  << std::fixed << std::setprecision(2) << r.time_ms << std::setw(18) << std::fixed << std::setprecision(1)
                  << r.avg_ns_per_call << std::setw(8) << (r.success ? "✓" : "✗") << "\n";
    }

    std::cout << "================================================================\n";
}

void testbenchmarkio::printResultsByCategory() const {
    std::vector<std::string> categories = {"Exists", "Metadata", "Read", "Write", "Size", "Create", "Delete", "Move"};

    for (const auto &cat: categories) {
        std::vector<BenchmarkResult> filtered;
        for (const auto &r: results_) {
            if (r.category == cat) filtered.push_back(r);
        }

        if (filtered.empty()) continue;

        std::sort(filtered.begin(), filtered.end(), [](const auto &a, const auto &b) { return a.time_ms < b.time_ms; });

        std::cout << "\n[" << cat << "]\n";
        std::cout << "----------------------------------------------------------------\n";
        std::cout << std::left << std::setw(40) << "Method" << std::right << std::setw(14) << "Time (ms)" << std::setw(18)
                  << "Ns/Call" << std::setw(8) << "OK" << "\n";

        for (const auto &r: filtered) {
            std::cout << std::left << std::setw(40) << r.method << std::right << std::setw(14) << std::fixed
                      << std::setprecision(2) << r.time_ms << std::setw(18) << std::fixed << std::setprecision(1)
                      << r.avg_ns_per_call << std::setw(8) << (r.success ? "✓" : "✗") << "\n";
        }
    }
}

const std::vector<BenchmarkResult> &testbenchmarkio::getResults() const {
    return results_;
}

void testbenchmarkio::reset() {
    results_.clear();
}

// ============================================================================
// TEST FUNCTIONS - EXISTS (basic existence checks)
// ============================================================================
namespace ExistsTests {

bool filesystem_status(const std::string &path) {
    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    return status.type() != std::filesystem::file_type::none;
}

bool filesystem_exists(const std::string &path) {
    return std::filesystem::exists(path);
}

bool win32_getfileattributes_a(const std::string &path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool win32_getfileattributes_w(const std::string &path) {
    std::wstring wpath = KDC::CommonUtility::s2ws(path);
    DWORD attrs = GetFileAttributesW(wpath.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool crt_stat(const std::string &path) {
    struct _stat buffer;
    return _stat(path.c_str(), &buffer) == 0;
}

bool crt_access(const std::string &path) {
    return _access(path.c_str(), 0) == 0;
}

} // namespace ExistsTests


// ============================================================================
// TEST FUNCTIONS - METADATA (detailed metadata retrieval)
// ============================================================================
namespace MetadataTests {

bool filesystem_permissions(const std::string &path) {
    // Check that we can read permissions via status().permissions()
    std::error_code ec;
    auto st = std::filesystem::status(path, ec);
    if (ec) return false;
    auto perms = st.permissions();
    // Ensure at least one permission flag is present
    return perms != std::filesystem::perms::none;
}

bool crt_stat_mode(const std::string &path) {
    struct _stat buffer;
    if (_stat(path.c_str(), &buffer) != 0) return false;
    // Ensure mode bits are present (owner/group/other)
#if defined(_WIN32)
    // MSVC CRT exposes read/write flags via _S_IREAD/_S_IWRITE
    return (buffer.st_mode & (_S_IREAD | _S_IWRITE)) != 0;
#else
    return (buffer.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0;
#endif
}

bool last_write_time(const std::string &path) {
    try {
        auto t = std::filesystem::last_write_time(path);
        (void) t;
        return true;
    } catch (...) {
        return false;
    }
}

bool file_size(const std::string &path) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    return !ec;
}

bool win32_getfileattributes_ex(const std::string &path) {
#if defined(KD_WINDOWS)
    WIN32_FILE_ATTRIBUTE_DATA data;
    return GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) == TRUE;
#else
    (void)path;
    return false;
#endif
}

} // namespace MetadataTests

// ============================================================================
// TEST FUNCTIONS - READ
// ============================================================================
namespace ReadTests {

bool ifstream_binary(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

bool ifstream_text(const std::string &path) {
    std::ifstream f(path);
    return f.good();
}

bool fopen_binary(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (f) fclose(f);
    return f != nullptr;
}

bool fopen_text(const std::string &path) {
    FILE *f = fopen(path.c_str(), "r");
    if (f) fclose(f);
    return f != nullptr;
}

bool win32_createfile(const std::string &path) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return true;
    }
    return false;
}

bool win32_createfile_w(const std::string &path) {
    std::wstring wpath = KDC::CommonUtility::s2ws(path);
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return true;
    }
    return false;
}

bool win32_findfirstfile(const std::string &path) {
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(path.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        return true;
    }
    return false;
}

} // namespace ReadTests

// ============================================================================
// TEST FUNCTIONS - WRITE
// ============================================================================
namespace WriteTests {

bool filesystem_last_write_time(const std::string &path) {
    try {
        auto now = std::filesystem::file_time_type::clock::now();
        std::filesystem::last_write_time(path, now);
        return true;
    } catch (...) {
        return false;
    }
}

bool win32_setfiletime(const std::string &path) {
    HANDLE h = CreateFileA(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    SetFileTime(h, nullptr, nullptr, &ft);
    CloseHandle(h);
    return true;
}

bool ofstream_append(const std::string &path) {
    std::ofstream f(path, std::ios::app);
    return f.good();
}

} // namespace WriteTests

// ============================================================================
// TEST FUNCTIONS - SIZE
// ============================================================================
namespace SizeTests {

uint64_t filesystem_filesize(const std::string &path) {
    std::error_code ec;
    return static_cast<uint64_t>(std::filesystem::file_size(path, ec));
}

uint64_t win32_getfilesize(const std::string &path) {
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER size;
    GetFileSizeEx(h, &size);
    CloseHandle(h);
    return static_cast<uint64_t>(size.QuadPart);
}

uint64_t crt_filelength(const std::string &path) {
    struct _stat buffer;
    if (_stat(path.c_str(), &buffer) != 0) return 0;
    return static_cast<uint64_t>(buffer.st_size);
}

} // namespace SizeTests

// ============================================================================
// CREATE FUNCTIONS
// ============================================================================
namespace CreateTests {

bool create_ofstream(const std::string &dir) {
    try {
        const auto path = std::filesystem::path(dir) / "bench_create_ofstream.tmp";
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f << "x";
        f.close();
        std::filesystem::remove(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool create_fopen(const std::string &dir) {
    const auto path = (std::filesystem::path(dir) / "bench_create_fopen.tmp").string();
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fputs("x", f);
    fclose(f);
    std::filesystem::remove(path);
    return true;
}

bool create_CreateFileA(const std::string &dir) {
#if defined(KD_WINDOWS)
    const std::string filename = (std::filesystem::path(dir) / "bench_create_CreateFileA.tmp").string();
    HANDLE h = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    std::filesystem::remove(filename);
    return true;
#else
    return create_ofstream(dir);
#endif
}

bool create_CreateFileW(const std::string &dir) {
#if defined(KD_WINDOWS)
    const std::wstring filename =
            KDC::CommonUtility::s2ws((std::filesystem::path(dir) / "bench_create_CreateFileW.tmp").string());
    HANDLE h = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    std::filesystem::remove(KDC::CommonUtility::ws2s(filename));
    return true;
#else
    return create_ofstream(dir);
#endif
}
}

// ----------------------------------------------------------------------------
// DELETE FUNCTIONS
// ----------------------------------------------------------------------------
namespace DeleteTests {
bool delete_filesystem_remove(const std::string &dir) {
    try {
        const auto path = std::filesystem::path(dir) / "bench_delete_std.tmp";
        ::CreateTestFile(path.string());
        std::filesystem::remove(path);
        return !std::filesystem::exists(path);
    } catch (...) {
        return false;
    }
}

bool delete_DeleteFileA(const std::string &dir) {
#if defined(KD_WINDOWS)
    const auto filename = (std::filesystem::path(dir) / "bench_delete_DeleteFileA.tmp").string();
    ::CreateTestFile(filename);
    BOOL ok = DeleteFileA(filename.c_str());
    return ok && !std::filesystem::exists(filename);
#else
    return delete_filesystem_remove(dir);
#endif
}

bool delete_crt_remove(const std::string &dir) {
    const auto filename = (std::filesystem::path(dir) / "bench_delete_crt.tmp").string();
    ::CreateTestFile(filename);
    return std::remove(filename.c_str()) == 0;
}
}

// ----------------------------------------------------------------------------
// MOVE FUNCTIONS
// ----------------------------------------------------------------------------
namespace MoveTests {
bool move_filesystem_rename(const std::string &dir) {
    try {
        const auto src = std::filesystem::path(dir) / "bench_move_src.tmp";
        const auto dst = std::filesystem::path(dir) / "bench_move_dst.tmp";
        ::CreateTestFile(src.string());
        std::filesystem::rename(src, dst);
        bool ok = std::filesystem::exists(dst) && !std::filesystem::exists(src);
        std::filesystem::remove(dst);
        return ok;
    } catch (...) {
        return false;
    }
}

bool move_MoveFileA(const std::string &dir) {
#if defined(KD_WINDOWS)
    const auto src = (std::filesystem::path(dir) / "bench_move_MoveFileA_src.tmp").string();
    const auto dst = (std::filesystem::path(dir) / "bench_move_MoveFileA_dst.tmp").string();
    ::CreateTestFile(src);
    BOOL ok = MoveFileA(src.c_str(), dst.c_str());
    if (ok) {
        std::filesystem::remove(dst);
    } else {
        if (std::filesystem::exists(src)) std::filesystem::remove(src);
    }
    return ok == TRUE;
#else
    return move_filesystem_rename(dir);
#endif
}

bool move_MoveFileW(const std::string &dir) {
#if defined(KD_WINDOWS)
    const auto src = KDC::CommonUtility::s2ws((std::filesystem::path(dir) / "bench_move_MoveFileW_src.tmp").string());
    const auto dst = KDC::CommonUtility::s2ws((std::filesystem::path(dir) / "bench_move_MoveFileW_dst.tmp").string());
    ::CreateTestFile(KDC::CommonUtility::ws2s(src));
    BOOL ok = MoveFileW(src.c_str(), dst.c_str());
    if (ok) std::filesystem::remove(KDC::CommonUtility::ws2s(dst));
    else std::filesystem::remove(KDC::CommonUtility::ws2s(src));
    return ok == TRUE;
#else
    return move_filesystem_rename(dir);
#endif
}

} // namespace OpsTests


// ============================================================================
// HELPER FUNCTIONS IMPLEMENTATION
// ============================================================================
bool CreateTestFile(const std::string &path, const std::string &content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
}

bool DeleteTestFile(const std::string &path) {
    try {
        std::filesystem::remove(path);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// RUNNER IMPLEMENTATION
// ============================================================================
void RunAllBenchmarks(const std::string &testFilePath, int iterations) {
    // Setup
    if (!CreateTestFile(testFilePath)) {
        std::cerr << "Failed to create test file: " << testFilePath << "\n";
        return;
    }

    testbenchmarkio benchmark(iterations);

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "              IOHELPER BENCHMARK SUITE                          \n";
    std::cout << "================================================================\n";
    std::cout << "Test file: " << testFilePath << "\n";
    std::cout << "Iterations: " << iterations << "\n";

    // --- EXISTS ---
    std::cout << "\n[Running Exists Tests...]\n";
    benchmark.addResult("Exists", "std::filesystem::status", benchmark.measure(ExistsTests::filesystem_status, testFilePath));
    benchmark.addResult("Exists", "std::filesystem::exists", benchmark.measure(ExistsTests::filesystem_exists, testFilePath));
    benchmark.addResult("Exists", "GetFileAttributesA",
                        benchmark.measure(ExistsTests::win32_getfileattributes_a, testFilePath));
    benchmark.addResult("Exists", "GetFileAttributesW + s2ws",
                        benchmark.measure(ExistsTests::win32_getfileattributes_w, testFilePath));
    benchmark.addResult("Exists", "_stat()", benchmark.measure(ExistsTests::crt_stat, testFilePath));
    benchmark.addResult("Exists", "_access()", benchmark.measure(ExistsTests::crt_access, testFilePath));
    // New metadata benchmarks that actually read metadata fields
    benchmark.addResult("Metadata", "std::filesystem::permissions", benchmark.measure(MetadataTests::filesystem_permissions, testFilePath));
    benchmark.addResult("Metadata", "_stat() mode bits", benchmark.measure(MetadataTests::crt_stat_mode, testFilePath));
    benchmark.addResult("Metadata", "std::filesystem::last_write_time", benchmark.measure(MetadataTests::last_write_time, testFilePath));
    benchmark.addResult("Metadata", "std::filesystem::file_size", benchmark.measure(MetadataTests::file_size, testFilePath));
    benchmark.addResult("Metadata", "GetFileAttributesEx (Win32)", benchmark.measure(MetadataTests::win32_getfileattributes_ex, testFilePath));

    // --- READ ---
    std::cout << "[Running Read Tests...]\n";
    benchmark.addResult("Read", "ifstream (binary)", benchmark.measure(ReadTests::ifstream_binary, testFilePath));
    benchmark.addResult("Read", "ifstream (text)", benchmark.measure(ReadTests::ifstream_text, testFilePath));
    benchmark.addResult("Read", "fopen (rb)", benchmark.measure(ReadTests::fopen_binary, testFilePath));
    benchmark.addResult("Read", "fopen (r)", benchmark.measure(ReadTests::fopen_text, testFilePath));
    benchmark.addResult("Read", "CreateFileA", benchmark.measure(ReadTests::win32_createfile, testFilePath));
    benchmark.addResult("Read", "CreateFileW + s2ws", benchmark.measure(ReadTests::win32_createfile_w, testFilePath));
    benchmark.addResult("Read", "FindFirstFileA", benchmark.measure(ReadTests::win32_findfirstfile, testFilePath));

    // --- WRITE ---
    std::cout << "[Running Write Tests...]\n";
    benchmark.addResult("Write", "filesystem::last_write_time", benchmark.measure(WriteTests::filesystem_last_write_time, testFilePath));
    benchmark.addResult("Write", "SetFileTime (Win32)", benchmark.measure(WriteTests::win32_setfiletime, testFilePath));
    benchmark.addResult("Write", "ofstream (append)", benchmark.measure(WriteTests::ofstream_append, testFilePath));

    // --- CREATE ---
    std::cout << "[Running Create Tests...]\n";
    benchmark.addResult("Create", "create (ofstream)", benchmark.measure(CreateTests::create_ofstream, testFilePath));
    benchmark.addResult("Create", "create (fopen)", benchmark.measure(CreateTests::create_fopen, testFilePath));
    benchmark.addResult("Create", "create (CreateFileA)", benchmark.measure(CreateTests::create_CreateFileA, testFilePath));
    benchmark.addResult("Create", "create (CreateFileW)", benchmark.measure(CreateTests::create_CreateFileW, testFilePath));

    // --- DELETE ---
    std::cout << "[Running Delete Tests...]\n";
    benchmark.addResult("Delete", "delete (filesystem::remove)", benchmark.measure(DeleteTests::delete_filesystem_remove, testFilePath));
    benchmark.addResult("Delete", "delete (DeleteFileA)", benchmark.measure(DeleteTests::delete_DeleteFileA, testFilePath));
    benchmark.addResult("Delete", "delete (CRT remove)", benchmark.measure(DeleteTests::delete_crt_remove, testFilePath));

    // --- MOVE ---
    std::cout << "[Running Move Tests...]\n";
    benchmark.addResult("Move", "move (std::filesystem::rename)", benchmark.measure(MoveTests::move_filesystem_rename, testFilePath));
    benchmark.addResult("Move", "move (MoveFileA)", benchmark.measure(MoveTests::move_MoveFileA, testFilePath));
    benchmark.addResult("Move", "move (MoveFileW)", benchmark.measure(MoveTests::move_MoveFileW, testFilePath));

    // --- SIZE ---
    std::cout << "[Running Size Tests...]\n";
    // Size tests are not boolean-returning; measure by adapting wrapper to bool conversion for timing only.
    benchmark.addResult("Size", "filesystem::file_size", benchmark.measure([](const std::string &p){ (void)SizeTests::filesystem_filesize(p); return true; }, testFilePath));
    benchmark.addResult("Size", "GetFileSizeEx (Win32)", benchmark.measure([](const std::string &p){ (void)SizeTests::win32_getfilesize(p); return true; }, testFilePath));
    benchmark.addResult("Size", "_filelength (CRT)", benchmark.measure([](const std::string &p){ (void)SizeTests::crt_filelength(p); return true; }, testFilePath));

    // Print results and cleanup
    benchmark.printResultsByCategory();
    benchmark.printResults();

    // --- OPS: grouped comparison (Write / Delete / Move) ---
    const auto &allResults = benchmark.getResults();
    std::map<std::string, std::vector<BenchmarkResult>> opsByType;
    std::map<std::string, std::pair<double, int>> apiAggregates; // api -> (totalMs, count)

    for (const auto &r: allResults) {
        if (r.category != "Ops") continue;
        std::string method = r.method;
        std::string type;
        if (method.find("create") != std::string::npos) type = "Write";
        else if (method.find("delete") != std::string::npos) type = "Delete";
        else if (method.find("move") != std::string::npos) type = "Move";
        else type = "Other";

        opsByType[type].push_back(r);

        // Extract API name inside parentheses if present
        const auto p1 = method.find('(');
        const auto p2 = method.find(')');
        std::string api = (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) ? method.substr(p1+1, p2-p1-1) : method;
        auto &agg = apiAggregates[api];
        agg.first += r.time_ms;
        agg.second += 1;
    }

    // Print grouped Ops
    std::cout << "\n[OPS COMPARISON BY TYPE]\n";
    for (const auto &group: {std::string("Write"), std::string("Delete"), std::string("Move")}) {
        auto it = opsByType.find(group);
        if (it == opsByType.end()) continue;
        auto vec = it->second;
        std::sort(vec.begin(), vec.end(), [](const BenchmarkResult &a, const BenchmarkResult &b){ return a.time_ms < b.time_ms; });

        std::cout << "\n[" << group << "]\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << std::left << std::setw(30) << "API" << std::right << std::setw(14) << "Time (ms)" << std::setw(12) << "OK" << "\n";
        std::cout << "------------------------------------------------------------\n";
        for (const auto &r: vec) {
            // extract api
            const auto p1 = r.method.find('(');
            const auto p2 = r.method.find(')');
            std::string api = (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) ? r.method.substr(p1+1, p2-p1-1) : r.method;
            std::cout << std::left << std::setw(30) << api << std::right << std::setw(14) << std::fixed << std::setprecision(2)
                      << r.time_ms << std::setw(12) << (r.success ? "✓" : "✗") << "\n";
        }
    }

    // Compute overall per-API averages and rank
    if (!apiAggregates.empty()) {
        struct ApiAvg { std::string api; double avg; int count; };
        std::vector<ApiAvg> avgs;
        for (const auto &kv: apiAggregates) {
            avgs.push_back({kv.first, kv.second.first / kv.second.second, kv.second.second});
        }
        std::sort(avgs.begin(), avgs.end(), [](const ApiAvg &a, const ApiAvg &b){ return a.avg < b.avg; });

        const double best = avgs.front().avg;
        std::cout << "\n[OVERALL API RANKING] (lower avg time across Ops is better)\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << std::left << std::setw(30) << "API" << std::right << std::setw(12) << "Avg (ms)" << std::setw(12) << "Rel" << std::setw(8) << "Cnt" << "\n";
        std::cout << "------------------------------------------------------------\n";
        for (const auto &a: avgs) {
            double rel = (a.avg - best) / best * 100.0;
            std::cout << std::left << std::setw(30) << a.api << std::right << std::setw(12) << std::fixed << std::setprecision(2)
                      << a.avg << std::setw(11) << (rel <= 0.0 ? "best" : (std::to_string((int)rel) + "%")) << std::setw(8) << a.count << "\n";
        }
    }

    (void) DeleteTestFile(testFilePath);
}

// ============================================================================
// CppUnit wrapper to run all IO tests from a single test
// ============================================================================
namespace KDC {

void BenchmarkIOHelper::setUp() {
    TestBase::start();
}

void BenchmarkIOHelper::tearDown() {
    TestBase::stop();
}

void BenchmarkIOHelper::runAllIOBenchmarks() {
    // Name and iterations can be adjusted or parameterized if needed
    ::RunAllBenchmarks("io_benchmark_test.tmp", iterations_);
}

} // namespace KDC
