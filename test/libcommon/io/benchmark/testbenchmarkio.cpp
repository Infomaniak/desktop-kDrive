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
#include "utility/types.h"

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
                                << r.avg_ns_per_call << std::setw(8) << (r.success ? "Y" : "N") << "\n";
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
                      << r.avg_ns_per_call << std::setw(8) << (r.success ? "Y" : "N") << "\n";
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


} // namespace ExistsTests

// ============================================================================
// TEST FUNCTIONS - METADATA
// ============================================================================
namespace MetadataTests {

bool stat_full(const std::string &path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) return false;

    auto dev = st.st_dev;
    auto inode = st.st_ino;
    auto mode = st.st_mode;
    auto nlink = st.st_nlink;
    auto uid = st.st_uid;
    auto gid = st.st_gid;
    auto rdev = st.st_rdev;
    auto size = st.st_size;
#if defined(KD_LINUX) || defined(KD_MACOS)
    auto blksize = st.st_blksize;
    auto blocks = st.st_blocks;
    (void) blksize;
    (void) blocks;
#endif

    auto atime = st.st_atime;
    auto mtime = st.st_mtime;
    auto ctime = st.st_ctime;

    (void) dev;
    (void) inode;
    (void) mode;
    (void) nlink;
    (void) uid;
    (void) gid;
    (void) rdev;
    (void) size;
    (void) atime;
    (void) mtime;
    (void) ctime;

    return true;
}

bool lstat_full(const std::string &path) {
#if defined(KD_LINUX) || defined(KD_MACOS)
    struct stat st{};
    if (::lstat(path.c_str(), &st) != 0) return false;

    auto inode = st.st_ino;
    auto mode = st.st_mode;
    auto size = st.st_size;

    (void) inode;
    (void) mode;
    (void) size;

    return true;
#endif
    return false;
}

bool fstat_full(const std::string &path) {
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    struct stat st{};
    bool ok = (::fstat(fd, &st) == 0);

    if (ok) {
        auto inode = st.st_ino;
        auto size = st.st_size;
        auto uid = st.st_uid;
        auto gid = st.st_gid;

        (void) inode;
        (void) size;
        (void) uid;
        (void) gid;
    }

    ::close(fd);
    return ok;
}

bool statx_full(const std::string &path) {
#if defined(KD_LINUX)
    struct statx stx{};

    int ret = ::statx(AT_FDCWD, path.c_str(), AT_STATX_SYNC_AS_STAT, STATX_BASIC_STATS | STATX_BTIME, &stx);

    if (ret != 0) return false;

    auto mask = stx.stx_mask;
    auto size = stx.stx_size;
    auto blocks = stx.stx_blocks;
    auto uid = stx.stx_uid;
    auto gid = stx.stx_gid;
    auto mode = stx.stx_mode;

    auto atime = stx.stx_atime;
    auto mtime = stx.stx_mtime;
    auto ctime = stx.stx_ctime;
    auto btime = stx.stx_btime;

    auto inode = stx.stx_ino;
    auto dev_major = stx.stx_dev_major;
    auto dev_minor = stx.stx_dev_minor;

    (void) mask;
    (void) size;
    (void) blocks;
    (void) uid;
    (void) gid;
    (void) mode;
    (void) atime;
    (void) mtime;
    (void) ctime;
    (void) btime;
    (void) inode;
    (void) dev_major;
    (void) dev_minor;

    return true;
#endif
    return false;
}

bool filesystem_full(const std::string &path) {
    std::error_code ec;

    auto status = std::filesystem::status(path, ec);
    if (ec) return false;

    auto size = std::filesystem::file_size(path, ec);
    if (ec) return false;

    auto last_write = std::filesystem::last_write_time(path, ec);
    if (ec) return false;

    auto perms = status.permissions();
    auto type = status.type();

    (void) size;
    (void) last_write;
    (void) perms;
    (void) type;

    return true;
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
} // namespace CreateTests

// ----------------------------------------------------------------------------
// DELETE FUNCTIONS
// ----------------------------------------------------------------------------
namespace DeleteTests {
bool delete_filesystem_remove(const std::string &dir) {
    try {
        const auto path = std::filesystem::path(dir) / "bench_delete_std.tmp";
        CreateTestFile(path.string());
        std::filesystem::remove(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool delete_DeleteFileA(const std::string &dir) {
#if defined(KD_WINDOWS)
    const auto filename = (std::filesystem::path(dir) / "bench_delete_DeleteFileA.tmp").string();
    CreateTestFile(filename);
    return DeleteFileA(filename.c_str()) == TRUE;
#else
    return delete_filesystem_remove(dir);
#endif
}

bool delete_crt_remove(const std::string &dir) {
    const auto filename = (std::filesystem::path(dir) / "bench_delete_crt.tmp").string();
    CreateTestFile(filename);
    return std::remove(filename.c_str()) == 0;
}
} // namespace DeleteTests

// ----------------------------------------------------------------------------
// MOVE FUNCTIONS
// ----------------------------------------------------------------------------
namespace MoveTests {
bool move_filesystem_rename(const std::string &dir) {
    try {
        const auto src = std::filesystem::path(dir) / "bench_move_src.tmp";
        const auto dst = std::filesystem::path(dir) / "bench_move_dst.tmp";
        CreateTestFile(src.string());
        std::filesystem::rename(src, dst);
        std::filesystem::remove(dst);
        return true;
    } catch (...) {
        return false;
    }
}

bool move_MoveFileA(const std::string &dir) {
#if defined(KD_WINDOWS)
    const auto src = (std::filesystem::path(dir) / "bench_move_MoveFileA_src.tmp").string();
    const auto dst = (std::filesystem::path(dir) / "bench_move_MoveFileA_dst.tmp").string();
    CreateTestFile(src);
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
    CreateTestFile(KDC::CommonUtility::ws2s(src));
    BOOL ok = MoveFileW(src.c_str(), dst.c_str());
    if (ok)
        std::filesystem::remove(KDC::CommonUtility::ws2s(dst));
    else
        std::filesystem::remove(KDC::CommonUtility::ws2s(src));
    return ok == TRUE;
#else
    return move_filesystem_rename(dir);
#endif
}

} // namespace MoveTests


// ============================================================================
// TEST FUNCTIONS - IOHELPER
// ============================================================================
namespace IoHelperTests {

bool iohelper_checkIfPathExists(const std::string &path) {
    bool exists = false;
    KDC::IoError ioError = KDC::IoError::Unknown;
    KDC::IoHelper::checkIfPathExists(path, exists, ioError, KDC::IoHelper::PathCheckOption::Insensitive);
    return exists;
}

bool iohelper_getFileStat(const std::string &path) {
    KDC::FileStat fs{};
    KDC::IoError ioError = KDC::IoError::Unknown;
    return KDC::IoHelper::getFileStat(path, &fs, ioError, KDC::IoHelper::PathCheckOption::Insensitive);
}

bool iohelper_getFileSize(const std::string &path) {
    uint64_t size = 0;
    KDC::IoError ioError = KDC::IoError::Unknown;
    return KDC::IoHelper::getFileSize(path, size, ioError);
}

bool iohelper_openFile(const std::string &path) {
    std::ifstream file;
    KDC::IoError ioError = KDC::IoError::Unknown;
    bool ok = KDC::IoHelper::openFile(path, file, ioError);
    if (file.is_open()) file.close();
    return ok;
}

bool iohelper_setFileDates(const std::string &path) {
    const auto now = static_cast<KDC::SyncTime>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    return KDC::IoHelper::setFileDates(path, now, now, false) == KDC::IoError::Success;
}

bool iohelper_deleteItem(const std::string &dir) {
    const auto path = (std::filesystem::path(dir) / "bench_iohelper_delete.tmp").string();
    CreateTestFile(path);
    KDC::IoError ioError = KDC::IoError::Unknown;
    return KDC::IoHelper::deleteItem(path, ioError);
}

bool iohelper_moveItem(const std::string &dir) {
    //try {
        const auto src = std::filesystem::path(dir) / "bench_iohelper_move_src.tmp";
        const auto dst = std::filesystem::path(dir) / "bench_iohelper_move_dst.tmp";
    CreateTestFile(src.string());
        KDC::IoError ioError = KDC::IoError::Unknown;
        const bool ok = KDC::IoHelper::moveItem(src, dst, ioError);
        if (ok)
            KDC::IoHelper::deleteItem(dst);
        else
            KDC::IoHelper::deleteItem(src);
        return ok;
    //} catch (...) {
    //    return false;
    }

} // namespace IoHelperTests

// ============================================================================
// HELPER FUNCTIONS IMPLEMENTATION
// ============================================================================

bool DeleteTestFile(const std::string &path) {
    try {
        std::filesystem::remove(path);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// HELPER FUNCTIONS - TEST FILE MANAGEMENT
// ============================================================================
bool CreateTestFile(const std::string &path, const std::string &content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
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
#if defined(KD_WINDOWS)
    benchmark.addResult("Exists", "GetFileAttributesA", benchmark.measure(ExistsTests::win32_getfileattributes_a, testFilePath));
    benchmark.addResult("Exists", "GetFileAttributesW + s2ws",
                        benchmark.measure(ExistsTests::win32_getfileattributes_w, testFilePath));
#endif
    benchmark.addResult("Exists", "_stat()", benchmark.measure(ExistsTests::crt_stat, testFilePath));
    benchmark.addResult("Exists", "IoHelper::checkIfPathExists",
                        benchmark.measure(IoHelperTests::iohelper_checkIfPathExists, testFilePath));
    // --- METADATA ---
    std::cout << "[Running Metadata Tests...]\n";
    benchmark.addResult("Metadata", "stat_full", benchmark.measure(MetadataTests::stat_full, testFilePath));
#if defined(KD_LINUX) || defined(KD_MACOS)
    benchmark.addResult("Metadata", "lstat_full", benchmark.measure(MetadataTests::lstat_full, testFilePath));
#endif
    benchmark.addResult("Metadata", "fstat_full", benchmark.measure(MetadataTests::fstat_full, testFilePath));
#if defined(KD_LINUX)
    benchmark.addResult("Metadata", "statx_full", benchmark.measure(MetadataTests::statx_full, testFilePath));
#endif
    benchmark.addResult("Metadata", "filesystem_full", benchmark.measure(MetadataTests::filesystem_full, testFilePath));
    benchmark.addResult("Metadata", "IoHelper::getFileStat",
                        benchmark.measure(IoHelperTests::iohelper_getFileStat, testFilePath));
    // --- READ ---
    std::cout << "[Running Read Tests...]\n";
    benchmark.addResult("Read", "ifstream (binary)", benchmark.measure(ReadTests::ifstream_binary, testFilePath));
    benchmark.addResult("Read", "ifstream (text)", benchmark.measure(ReadTests::ifstream_text, testFilePath));
    benchmark.addResult("Read", "fopen (rb)", benchmark.measure(ReadTests::fopen_binary, testFilePath));
    benchmark.addResult("Read", "fopen (r)", benchmark.measure(ReadTests::fopen_text, testFilePath));
#if defined(KD_WINDOWS)
    benchmark.addResult("Read", "CreateFileA", benchmark.measure(ReadTests::win32_createfile, testFilePath));
    benchmark.addResult("Read", "CreateFileW + s2ws", benchmark.measure(ReadTests::win32_createfile_w, testFilePath));
#endif
    benchmark.addResult("Read", "IoHelper::openFile", benchmark.measure(IoHelperTests::iohelper_openFile, testFilePath));

    // --- WRITE ---
    std::cout << "[Running Write Tests...]\n";
    benchmark.addResult("Write", "filesystem::last_write_time",
                        benchmark.measure(WriteTests::filesystem_last_write_time, testFilePath));
#if defined(KD_WINDOWS)
    benchmark.addResult("Write", "SetFileTime (Win32)", benchmark.measure(WriteTests::win32_setfiletime, testFilePath));
#endif
    benchmark.addResult("Write", "ofstream (append)", benchmark.measure(WriteTests::ofstream_append, testFilePath));
    benchmark.addResult("Write", "IoHelper::setFileDates",
                        benchmark.measure(IoHelperTests::iohelper_setFileDates, testFilePath));

    // --- CREATE ---
    std::cout << "[Running Create Tests...]\n";
    benchmark.addResult("Create", "create (ofstream)", benchmark.measure(CreateTests::create_ofstream, testFilePath));
    benchmark.addResult("Create", "create (fopen)", benchmark.measure(CreateTests::create_fopen, testFilePath));
    benchmark.addResult("Create", "create (CreateFileA)", benchmark.measure(CreateTests::create_CreateFileA, testFilePath));
    benchmark.addResult("Create", "create (CreateFileW)", benchmark.measure(CreateTests::create_CreateFileW, testFilePath));

    // --- DELETE ---
    std::cout << "[Running Delete Tests...]\n";
    benchmark.addResult("Delete", "delete (filesystem::remove)",
                        benchmark.measure(DeleteTests::delete_filesystem_remove, testFilePath));
    benchmark.addResult("Delete", "delete (DeleteFileA)", benchmark.measure(DeleteTests::delete_DeleteFileA, testFilePath));
    benchmark.addResult("Delete", "delete (CRT remove)", benchmark.measure(DeleteTests::delete_crt_remove, testFilePath));
    benchmark.addResult("Delete", "IoHelper::deleteItem",
                        benchmark.measure(IoHelperTests::iohelper_deleteItem, testFilePath));

    // --- MOVE ---
    std::cout << "[Running Move Tests...]\n";
    benchmark.addResult("Move", "move (std::filesystem::rename)",
                        benchmark.measure(MoveTests::move_filesystem_rename, testFilePath));
    benchmark.addResult("Move", "move (MoveFileA)", benchmark.measure(MoveTests::move_MoveFileA, testFilePath));
    benchmark.addResult("Move", "move (MoveFileW)", benchmark.measure(MoveTests::move_MoveFileW, testFilePath));
    benchmark.addResult("Move", "IoHelper::moveItem",
                        benchmark.measure(IoHelperTests::iohelper_moveItem, testFilePath));

    // --- SIZE ---
    std::cout << "[Running Size Tests...]\n";
    // Size tests are not boolean-returning; measure by adapting wrapper to bool conversion for timing only.
    benchmark.addResult("Size", "filesystem::file_size",
                        benchmark.measure(
                                [](const std::string &p) {
                                    (void) SizeTests::filesystem_filesize(p);
                                    return true;
                                },
                                testFilePath));
#if defined(KD_WINDOWS)
    benchmark.addResult("Size", "GetFileSizeEx (Win32)",
                        benchmark.measure(
                                [](const std::string &p) {
                                    (void) SizeTests::win32_getfilesize(p);
                                    return true;
                                },
                                testFilePath));
#endif
    benchmark.addResult("Size", "_filelength (CRT)",
                        benchmark.measure(
                                [](const std::string &p) {
                                    (void) SizeTests::crt_filelength(p);
                                    return true;
                                },
                                testFilePath));
    benchmark.addResult("Size", "IoHelper::getFileSize",
                        benchmark.measure(IoHelperTests::iohelper_getFileSize, testFilePath));

    // Print results and cleanup
    benchmark.printResultsByCategory();
    benchmark.printResults();

    // --- TAB 1: IoHelper  /  TAB 2: Best raw API  (per category) ---
    {
        // Underlying OS/stdlib API used by each IoHelper method (from source inspection of iohelper.cpp)
        const std::map<std::string, std::string> iohelperUnderlyingApi = {
                {"IoHelper::checkIfPathExists", "std::filesystem::symlink_status"},
                {"IoHelper::getFileStat", "NtQueryInformationFile (Win) / stat (Linux/Mac)"},
                {"IoHelper::getFileSize", "std::filesystem::file_size"},
                {"IoHelper::openFile", "std::ifstream (binary, with retry loop)"},
                {"IoHelper::setFileDates", "SetFileTime (Win) / utimensat (Linux/Mac)"},
                {"IoHelper::deleteItem", "std::filesystem::remove_all"},
                {"IoHelper::moveItem", "std::filesystem::rename"},
        };

        struct CategoryEntry {
                std::string category;
                std::string iohelperMethod;
                std::string underlyingApi;
                double iohelperMs;
                double iohelperNs;
                bool iohelperOk;
                std::string bestAltMethod;
                double bestAltMs;
                double bestAltNs;
                bool bestAltOk;
        };

        std::vector<CategoryEntry> entries;

        std::map<std::string, std::vector<BenchmarkResult>> byCategory;
        for (const auto &r: benchmark.getResults()) byCategory[r.category].push_back(r);

        const std::vector<std::string> catOrder = {"Exists", "Metadata", "Read", "Write", "Size", "Create", "Delete", "Move"};
        for (const auto &cat: catOrder) {
            const auto it = byCategory.find(cat);
            if (it == byCategory.end()) continue;
            const auto &catResults = it->second;

            const BenchmarkResult *iohelperResult = nullptr;
            const BenchmarkResult *bestAlt = nullptr;
            for (const auto &r: catResults) {
                if (r.method.rfind("IoHelper::", 0) == 0) {
                    iohelperResult = &r;
                } else if (!bestAlt || r.time_ms < bestAlt->time_ms) {
                    bestAlt = &r;
                }
            }
            if (!iohelperResult || !bestAlt) continue;

            CategoryEntry e;
            e.category = cat;
            e.iohelperMethod = iohelperResult->method;
            const auto apiIt = iohelperUnderlyingApi.find(iohelperResult->method);
            e.underlyingApi = (apiIt != iohelperUnderlyingApi.end()) ? apiIt->second : "unknown";
            e.iohelperMs = iohelperResult->time_ms;
            e.iohelperNs = iohelperResult->avg_ns_per_call;
            e.iohelperOk = iohelperResult->success;
            e.bestAltMethod = bestAlt->method;
            e.bestAltMs = bestAlt->time_ms;
            e.bestAltNs = bestAlt->avg_ns_per_call;
            e.bestAltOk = bestAlt->success;
            entries.push_back(e);
        }

        constexpr int W_CAT = 10, W_METHOD = 44, W_MS = 14, W_NS = 14, W_OK = 6;
        const int totalWidth = W_CAT + W_METHOD + W_MS + W_NS + W_OK;
        const std::string sep(totalWidth, '-');

        // ---- TAB 1: IoHelper ----
        std::cout << "\n";
        std::cout << sep << "\n";
        std::cout << "  TAB 1 - IoHelper  (production API + its underlying OS call)\n";
        std::cout << sep << "\n";
        std::cout << std::left << std::setw(W_CAT) << "Category" << std::setw(W_METHOD) << "IoHelper method / underlying API"
                  << std::right << std::setw(W_MS) << "Time (ms)" << std::setw(W_NS) << "Ns/Call" << std::setw(W_OK) << "OK"
                  << "\n";
        std::cout << sep << "\n";
        for (const auto &e: entries) {
            // Row 1: IoHelper method name + time
            std::cout << std::left << std::setw(W_CAT) << e.category << std::setw(W_METHOD) << e.iohelperMethod << std::right
                      << std::setw(W_MS) << std::fixed << std::setprecision(2) << e.iohelperMs << std::setw(W_NS) << std::fixed
                      << std::setprecision(1) << e.iohelperNs << std::setw(W_OK) << (e.iohelperOk ? "OK" : "FAIL") << "\n";
            // Row 2: underlying API (indented, no times)
            std::cout << std::left << std::setw(W_CAT) << "" << "  -> " << e.underlyingApi << "\n";
        }
        std::cout << sep << "\n";

        // ---- TAB 2: Best raw benchmark ----
        std::cout << "\n";
        std::cout << sep << "\n";
        std::cout << "  TAB 2 - Best raw API  (fastest individual call in each category)\n";
        std::cout << sep << "\n";
        std::cout << std::left << std::setw(W_CAT) << "Category" << std::setw(W_METHOD) << "Fastest method" << std::right
                  << std::setw(W_MS) << "Time (ms)" << std::setw(W_NS) << "Ns/Call" << std::setw(W_OK) << "OK"
                  << "\n";
        std::cout << sep << "\n";
        for (const auto &e: entries) {
            std::cout << std::left << std::setw(W_CAT) << e.category << std::setw(W_METHOD) << e.bestAltMethod << std::right
                      << std::setw(W_MS) << std::fixed << std::setprecision(2) << e.bestAltMs << std::setw(W_NS) << std::fixed
                      << std::setprecision(1) << e.bestAltNs << std::setw(W_OK) << (e.bestAltOk ? "OK" : "FAIL") << "\n";
        }
        std::cout << sep << "\n";
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
