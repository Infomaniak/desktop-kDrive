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
#include "test_utility/localtemporarydirectory.h"

namespace KDC {

static int32_t iterations_ = 10000;

// ============================================================================
// BENCHMARK ENGINE IMPLEMENTATION
// ============================================================================
testbenchmarkio::testbenchmarkio(int iterations, bool warmup) :
    warmup_(warmup) {
    iterations_ = iterations;
}

void testbenchmarkio::addResult(const std::string &category, const std::string &method, double time_ms, bool success) {
    BenchmarkResult r;
    r.category = category;
    r.method = method;
    r.time_ms = time_ms;
    r.avg_ns_per_call = (time_ms * 1'000'000.0) / iterations_;
    r.success = success;
    results_.push_back(r);
    printProgress();
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
    std::vector<std::string> categories = {"Exists", "Metadata", "Read", "Size", "Create", "Delete", "Move"};

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

void testbenchmarkio::printProgress() const {
    const int current = static_cast<int>(results_.size());
    const int total = (total_ > 0) ? total_ : current;
    constexpr int barWidth = 40;

    const float ratio = (total > 0) ? static_cast<float>(current) / static_cast<float>(total) : 1.0f;
    const int filled = static_cast<int>(ratio * barWidth);

    const std::string &lastMethod = results_.back().method;
    const std::string &lastCategory = results_.back().category;

    std::cout << "\r  [";
    for (int i = 0; i < barWidth; ++i)
        std::cout << (i < filled ? '#' : '-');
    std::cout << "] " << std::setw(2) << current << "/" << total
              << " (" << std::fixed << std::setprecision(0) << std::setw(3) << (ratio * 100.0f) << "%)  "
              << std::left << std::setw(10) << lastCategory << "  " << lastMethod
              << std::flush;

    if (current >= total)
        std::cout << "\n";
}

// ============================================================================
// TEST FUNCTIONS - EXISTS (basic existence checks)
// ============================================================================
namespace ExistsTests {

void testinit(const SyncPath &path) {
    std::ofstream f(path, std::ios::binary);
    if (f) {
        f << "data";
    }
}

void teardown(const SyncPath &path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

bool filesystem_status(const SyncPath &path) {
    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    return status.type() != std::filesystem::file_type::none;
}

bool filesystem_exists(const SyncPath &path) {
    return std::filesystem::exists(path);
}

bool win32_getfileattributes_w(const SyncPath &path) {
    DWORD attrs = GetFileAttributesW(path.native().c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool crt_stat(const SyncPath &path) {
    struct _stat buffer;
    return _stat(path.string().c_str(), &buffer) == 0;
}


} // namespace ExistsTests

// ============================================================================
// TEST FUNCTIONS - METADATA
// ============================================================================
namespace MetadataTests {

void testinit(const SyncPath &path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    // Write a bit of data to ensure metadata is meaningful
    f << "benchmark_metadata_test_content";
}

void teardown(const SyncPath &path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

bool stat_full(const SyncPath &path) {
    struct stat st{};
    if (::stat(path.string().c_str(), &st) != 0) return false;

    volatile auto dev = st.st_dev;
    volatile auto inode = st.st_ino;
    volatile auto mode = st.st_mode;
    volatile auto nlink = st.st_nlink;
    volatile auto uid = st.st_uid;
    volatile auto gid = st.st_gid;
    volatile auto rdev = st.st_rdev;
    volatile auto size = st.st_size;

#if defined(__linux__) || defined(__APPLE__)
    volatile auto blksize = st.st_blksize;
    volatile auto blocks = st.st_blocks;
    (void) blksize;
    (void) blocks;
#endif

    volatile auto atime = st.st_atime;
    volatile auto mtime = st.st_mtime;
    volatile auto ctime = st.st_ctime;

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

bool wstat_full(const SyncPath &path) {
    struct _stat64i32 st{};
    if (::_wstat(path.native().c_str(), &st) != 0) return false;

    volatile auto dev = st.st_dev;
    volatile auto inode = st.st_ino;
    volatile auto mode = st.st_mode;
    volatile auto nlink = st.st_nlink;
    volatile auto uid = st.st_uid;
    volatile auto gid = st.st_gid;
    volatile auto rdev = st.st_rdev;
    volatile auto size = st.st_size;

#if defined(__linux__) || defined(__APPLE__)
    volatile auto blksize = st.st_blksize;
    volatile auto blocks = st.st_blocks;
    (void) blksize;
    (void) blocks;
#endif

    volatile auto atime = st.st_atime;
    volatile auto mtime = st.st_mtime;
    volatile auto ctime = st.st_ctime;

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

bool statx_full(const SyncPath &path) {
#if defined(KD_LINUX) or defined(KD_MACOS)
    struct statx stx{};

    int ret = ::statx(AT_FDCWD, path.string().c_str(), AT_STATX_SYNC_AS_STAT, STATX_ALL, &stx);

    if (ret != 0) return false;

    volatile auto mask = stx.stx_mask;
    volatile auto size = stx.stx_size;
    volatile auto blocks = stx.stx_blocks;
    volatile auto uid = stx.stx_uid;
    volatile auto gid = stx.stx_gid;
    volatile auto mode = stx.stx_mode;

    volatile auto atime = stx.stx_atime;
    volatile auto mtime = stx.stx_mtime;
    volatile auto ctime = stx.stx_ctime;
    volatile auto btime = stx.stx_btime;

    volatile auto inode = stx.stx_ino;
    volatile auto dev_major = stx.stx_dev_major;
    volatile auto dev_minor = stx.stx_dev_minor;

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
#else
    (void) path;
    return false;
#endif
}

bool fstat_full(const SyncPath &path) {
    int fd = ::open(path.string().c_str(), O_RDONLY);
    if (fd < 0) return false;

    struct stat st{};
    bool ok = (::fstat(fd, &st) == 0);

    if (ok) {
        volatile auto dev = st.st_dev;
        volatile auto inode = st.st_ino;
        volatile auto mode = st.st_mode;
        volatile auto nlink = st.st_nlink;
        volatile auto uid = st.st_uid;
        volatile auto gid = st.st_gid;
        volatile auto rdev = st.st_rdev;
        volatile auto size = st.st_size;

#if defined(__linux__) || defined(__APPLE__)
        volatile auto blksize = st.st_blksize;
        volatile auto blocks = st.st_blocks;
        (void) blksize;
        (void) blocks;
#endif

        volatile auto atime = st.st_atime;
        volatile auto mtime = st.st_mtime;
        volatile auto ctime = st.st_ctime;

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
    }

    ::close(fd);
    return ok;
}

bool filesystem_full(const SyncPath &path) {
    std::error_code ec;

    auto status = std::filesystem::status(path, ec);
    if (ec) return false;

    auto size = std::filesystem::file_size(path, ec);
    if (ec) return false;

    auto last_write = std::filesystem::last_write_time(path, ec);
    if (ec) return false;

    volatile auto perms = status.permissions();
    volatile auto type = status.type();
    volatile auto sz = size;
    volatile auto lw = last_write.time_since_epoch().count();

    (void) perms;
    (void) type;
    (void) sz;
    (void) lw;

    return true;
}

} // namespace MetadataTests

// ============================================================================
// TEST FUNCTIONS - READ
// ============================================================================
namespace ReadTests {

constexpr size_t BUFFER_SIZE = 4096;

void testinit(const SyncPath &path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;

    std::string content(BUFFER_SIZE, 'x');
    f.write(content.data(), content.size());
}

void teardown(const SyncPath &path) {
    std::filesystem::remove(path);
}

bool ifstream_binary(const SyncPath &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    static thread_local std::vector<char> buffer(BUFFER_SIZE);
    f.read(buffer.data(), buffer.size());

    return f.good() || f.gcount() > 0;
}

bool ifstream_text(const SyncPath &path) {
    std::ifstream f(path);
    if (!f) return false;

    static thread_local std::string buffer(BUFFER_SIZE, '\0');
    f.read(buffer.data(), buffer.size());

    return f.good() || f.gcount() > 0;
}

bool fread_binary(const SyncPath &path) {
    FILE *f = fopen(path.string().c_str(), "rb");
    if (!f) return false;

    static thread_local std::vector<char> buffer(BUFFER_SIZE);
    size_t read = fread(buffer.data(), 1, buffer.size(), f);

    fclose(f);
    return read > 0;
}

bool fread_text(const SyncPath &path) {
    FILE *f = fopen(path.string().c_str(), "r");
    if (!f) return false;

    static thread_local std::string buffer(BUFFER_SIZE, '\0');
    size_t read = fread(buffer.data(), 1, buffer.size(), f);

    fclose(f);
    return read > 0;
}

bool win32_readfile(const SyncPath &path) {
#if defined(KD_WINDOWS)
    HANDLE h = CreateFileA(path.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    static thread_local std::vector<char> buffer(BUFFER_SIZE);

    DWORD read = 0;
    BOOL ok = ReadFile(h, buffer.data(), (DWORD) buffer.size(), &read, nullptr);

    CloseHandle(h);
    return ok && read > 0;
#else
    return ifstream_binary(path);
#endif
}

bool win32_readfile_w(const SyncPath &path) {
#if defined(KD_WINDOWS)

    HANDLE h = CreateFileW(path.native().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    static thread_local std::vector<char> buffer(BUFFER_SIZE);

    DWORD read = 0;
    BOOL ok = ReadFile(h, buffer.data(), (DWORD) buffer.size(), &read, nullptr);

    CloseHandle(h);
    return ok && read > 0;
#else
    return ifstream_binary(path);
#endif
}

} // namespace ReadTests

// ============================================================================
// TEST FUNCTIONS - SIZE
// ============================================================================
namespace SizeTests {

void testinit(const SyncPath &path) {
    constexpr std::size_t size = 1024 * 1024; // 1 MB

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return;

    std::string buffer(4096, 'A'); // 4 KB chunk

    std::size_t written = 0;
    while (written < size) {
        std::size_t to_write = std::min(buffer.size(), size - written);
        f.write(buffer.data(), to_write);
        written += to_write;
    }
}

void teardown(const SyncPath &path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

bool filesystem_filesize(const SyncPath &path) {
    std::error_code ec;
    return std::filesystem::file_size(path, ec) != static_cast<uint64_t>(-1);
}

bool win32_getfilesize(const SyncPath &path) {
    HANDLE h = CreateFileA(path.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);
    return static_cast<uint64_t>(size.QuadPart) != static_cast<uint64_t>(-1);
}

bool crt_filelength(const SyncPath &path) {
    struct _stat buffer;
    if (_stat(path.string().c_str(), &buffer) != 0) return false;
    return static_cast<uint64_t>(buffer.st_size) != static_cast<uint64_t>(-1);
}

} // namespace SizeTests

// ============================================================================
// CREATE FUNCTIONS (Benchmark-ready)
// ============================================================================
namespace CreateTests {

void testinit(const SyncPath &path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
}

void teardown(const SyncPath &path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

// Helper to generate unique filenames per iteration
inline std::filesystem::path make_path(const SyncPath &dir, const std::string &prefix) {
    static std::atomic<uint64_t> counter{0};
    return std::filesystem::path(dir) / (prefix + "_" + std::to_string(counter++) + ".tmp");
}

bool create_ofstream(const SyncPath &dir) {
    const auto path = make_path(dir, "bench_create_ofstream");

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write("x", 1);
    f.close();

    std::filesystem::remove(path);
    return true;
}

bool create_fopen(const SyncPath &dir) {
    const auto path = make_path(dir, "bench_create_fopen").string();

    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;

    fwrite("x", 1, 1, f);
    fclose(f);

    std::filesystem::remove(path);
    return true;
}

bool create_CreateFileA(const SyncPath &dir) {
#if defined(KD_WINDOWS)
    const auto path = make_path(dir, "bench_create_CreateFileA").string();

    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    WriteFile(h, "x", 1, &written, nullptr);

    CloseHandle(h);
    std::filesystem::remove(path);
    return written == 1;
#else
    return create_ofstream(dir);
#endif
}

bool create_CreateFileW(const SyncPath &dir) {
#if defined(KD_WINDOWS)
    const auto path = make_path(dir, "bench_create_CreateFileW");
    const std::wstring wpath = CommonUtility::s2ws(path.string());

    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    WriteFile(h, "x", 1, &written, nullptr);

    CloseHandle(h);
    std::filesystem::remove(path);
    return written == 1;
#else
    return create_ofstream(dir);
#endif
}

} // namespace CreateTests

// ----------------------------------------------------------------------------
// DELETE FUNCTIONS
// ----------------------------------------------------------------------------
namespace DeleteTests {

static std::atomic<uint64_t> counter{0};

void testinit(const SyncPath &dir) {
    for (counter = 0; counter < iterations_; counter++) {
        auto path = std::filesystem::path(dir) / ("bench_delete_" + std::to_string(counter) + ".tmp");
        CreateTestFile(path.string());
    }
    counter = 0;
}

void teardown(const SyncPath &dir) {}

bool delete_filesystem_remove(const SyncPath &dir) {
    uint64_t id = counter++;

    auto path = std::filesystem::path(dir) / ("bench_delete_" + std::to_string(id) + ".tmp");

    return std::filesystem::remove(path);
}

bool delete_DeleteFileA(const SyncPath &dir) {
#if defined(KD_WINDOWS)
    uint64_t id = counter++;

    auto filename = (std::filesystem::path(dir) / ("bench_delete_" + std::to_string(id) + ".tmp")).string();

    return DeleteFileA(filename.c_str()) == TRUE;
#else
    return delete_filesystem_remove(dir);
#endif
}

bool delete_crt_remove(const SyncPath &dir) {
    uint64_t id = counter++;

    auto filename = (std::filesystem::path(dir) / ("bench_delete_" + std::to_string(id) + ".tmp")).string();

    return std::remove(filename.c_str()) == 0;
}
} // namespace DeleteTests

// ----------------------------------------------------------------------------
// MOVE FUNCTIONS
// ----------------------------------------------------------------------------
namespace MoveTests {

static std::atomic<uint64_t> counter{0};

void testinit(const SyncPath &dir) {
    for (counter = 0; counter < iterations_; counter++) {
        auto path = std::filesystem::path(dir) / ("bench_move_" + std::to_string(counter) + "_src.tmp");
        CreateTestFile(path.string());
    }
    counter = 0;
}

void teardown(const SyncPath &dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

bool move_filesystem_rename(const SyncPath &dir) {
    uint64_t id = counter++;

    auto src = std::filesystem::path(dir) / ("bench_move_" + std::to_string(id) + "_src.tmp");
    auto dst = std::filesystem::path(dir) / ("bench_move_" + std::to_string(id) + "_dst.tmp");

    std::error_code ec;
    std::filesystem::rename(src, dst, ec);

    return !ec;
}

bool move_MoveFileA(const SyncPath &dir) {
#if defined(KD_WINDOWS)
    uint64_t id = counter++;

    auto src = (std::filesystem::path(dir) / ("bench_move_" + std::to_string(id) + "_src.tmp")).string();
    auto dst = (std::filesystem::path(dir) / ("bench_move_" + std::to_string(id) + "_dst.tmp")).string();

    return MoveFileA(src.c_str(), dst.c_str()) == TRUE;
#else
    return move_filesystem_rename(dir);
#endif
}

bool move_MoveFileW(const SyncPath &dir) {
#if defined(KD_WINDOWS)
    uint64_t id = counter++;

    auto src = CommonUtility::s2ws((std::filesystem::path(dir) / ("bench_move_" + std::to_string(id) + "_src.tmp")).string());
    auto dst = CommonUtility::s2ws((std::filesystem::path(dir) / ("bench_move_" + std::to_string(id) + "_dst.tmp")).string());

    return MoveFileW(src.c_str(), dst.c_str()) == TRUE;
#else
    return move_filesystem_rename(dir);
#endif
}

} // namespace MoveTests


// ============================================================================
// TEST FUNCTIONS - IOHELPER
// ============================================================================
namespace IoHelperTests {

bool iohelper_checkIfPathExists(const SyncPath &path) {
    bool exists = false;
    IoError ioError = IoError::Unknown;
    IoHelper::checkIfPathExists(path, exists, ioError, IoHelper::PathCheckOption::Insensitive);
    return exists;
}

bool iohelper_getFileStat(const SyncPath &path) {
    FileStat fs{};
    IoError ioError = IoError::Unknown;
    return IoHelper::getFileStat(path, &fs, ioError, IoHelper::PathCheckOption::Insensitive);
}

bool iohelper_getFileSize(const SyncPath &path) {
    uint64_t size = 0;
    IoError ioError = IoError::Unknown;
    return IoHelper::getFileSize(path, size, ioError);
}

bool iohelper_openFile(const SyncPath &path) {
    std::ifstream file;
    IoError ioError = IoError::Unknown;
    bool ok = IoHelper::openFile(path, file, ioError);
    if (file.is_open()) file.close();
    return ok;
}

bool iohelper_setFileDates(const SyncPath &path) {
    const auto now = static_cast<SyncTime>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    return IoHelper::setFileDates(path, now, now, false) == IoError::Success;
}

bool iohelper_deleteItem(const SyncPath &dir) {
    const auto path = (std::filesystem::path(dir) / "bench_iohelper_delete.tmp").string();
    CreateTestFile(path);
    IoError ioError = IoError::Unknown;
    return IoHelper::deleteItem(path, ioError);
}

bool iohelper_moveItem(const SyncPath &dir) {
    // try {
    const auto src = std::filesystem::path(dir) / "bench_iohelper_move_src.tmp";
    const auto dst = std::filesystem::path(dir) / "bench_iohelper_move_dst.tmp";
    CreateTestFile(src.string());
    IoError ioError = IoError::Unknown;
    const bool ok = IoHelper::moveItem(src, dst, ioError);
    if (ok)
        IoHelper::deleteItem(dst);
    else
        IoHelper::deleteItem(src);
    return ok;
    //} catch (...) {
    //    return false;
}

} // namespace IoHelperTests

// ============================================================================
// HELPER FUNCTIONS IMPLEMENTATION
// ============================================================================

bool DeleteTestFile(const SyncPath &path) {
    try {
        std::filesystem::remove(path);
        return true;
    } catch (...) {
        return false;
    }
}

bool CreateTestFile(const SyncPath &path, const std::string &content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
}

int getiterations(void) {
    return iterations_;
}

// ============================================================================
// RUNNER IMPLEMENTATION
// ============================================================================
void RunAllBenchmarks(const SyncPath &testFilePath, int iterations) {
    LocalTemporaryDirectory tmpDir;
    // Setup
    const auto testFilePathFull = tmpDir.path() / testFilePath;
    if (!CreateTestFile(testFilePathFull)) {
        std::cerr << "Failed to create test file: " << testFilePathFull << "\n";
        return;
    }

    testbenchmarkio benchmark(iterations);

    // Compute the total number of addResult calls (platform-dependent)
    int expectedCount = 0;
    expectedCount += 4; // Exists: filesystem_status, filesystem_exists, _stat, IoHelper
#if defined(KD_WINDOWS)
    expectedCount += 2; // Exists: GetFileAttributesA, GetFileAttributesW
#endif
    expectedCount += 4; // Metadata: stat, fstat, filesystem_full, IoHelper
#if defined(KD_LINUX) || defined(KD_MACOS)
    expectedCount += 1; // Metadata: lstat
#endif
#if defined(KD_LINUX)
    expectedCount += 1; // Metadata: statx
#endif
    expectedCount += 5; // Read: ifstream x2, fread x2, IoHelper
#if defined(KD_WINDOWS)
    expectedCount += 2; // Read: CreateFileA, CreateFileW
#endif
    expectedCount += 4; // Create
    expectedCount += 4; // Delete
    expectedCount += 4; // Move
    expectedCount += 3; // Size: filesystem, crt, IoHelper
#if defined(KD_WINDOWS)
    expectedCount += 1; // Size: GetFileSizeEx
#endif
    benchmark.setExpectedCount(expectedCount);

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "              IOHELPER BENCHMARK SUITE                          \n";
    std::cout << "================================================================\n";
    std::cout << "Test file: " << testFilePath << "\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "\n";

    // --- EXISTS ---
    std::cout << "\n[Running Exists Tests...]\n";
    benchmark.addResult(
            "Exists", "std::filesystem::status",
            benchmark.measure(ExistsTests::filesystem_status, testFilePath, ExistsTests::testinit, ExistsTests::teardown, false));
    benchmark.addResult(
            "Exists", "std::filesystem::exists",
            benchmark.measure(ExistsTests::filesystem_exists, testFilePath, ExistsTests::testinit, ExistsTests::teardown, false));
#if defined(KD_WINDOWS)
    benchmark.addResult("Exists", "GetFileAttributesW",
                        benchmark.measure(ExistsTests::win32_getfileattributes_w, testFilePath, ExistsTests::testinit,
                                          ExistsTests::teardown, false));
#endif
    benchmark.addResult(
            "Exists", "_stat()",
            benchmark.measure(ExistsTests::crt_stat, testFilePath, ExistsTests::testinit, ExistsTests::teardown, false));
    benchmark.addResult("Exists", "IoHelper::checkIfPathExists",
                        benchmark.measure(IoHelperTests::iohelper_checkIfPathExists, testFilePath, ExistsTests::testinit,
                                          ExistsTests::teardown, false));
    // --- METADATA ---
    // std::cout << "[Running Metadata Tests...]\n";
    benchmark.addResult(
            "Metadata", "stat_full",
            benchmark.measure(MetadataTests::stat_full, testFilePath, MetadataTests::testinit, MetadataTests::teardown, false));
#if defined(KD_LINUX) || defined(KD_MACOS)
    benchmark.addResult(
            "Metadata", "lstat_full",
            benchmark.measure(MetadataTests::lstat_full, testFilePath, MetadataTests::testinit, MetadataTests::teardown, false));
#endif
    benchmark.addResult(
            "Metadata", "fstat_full",
            benchmark.measure(MetadataTests::fstat_full, testFilePath, MetadataTests::testinit, MetadataTests::teardown, false));
#if defined(KD_LINUX)
    benchmark.addResult(
            "Metadata", "statx_full",
            benchmark.measure(MetadataTests::statx_full, testFilePath, MetadataTests::testinit, MetadataTests::teardown, false));
#endif
    benchmark.addResult("Metadata", "filesystem_full",
                        benchmark.measure(MetadataTests::filesystem_full, testFilePath, MetadataTests::testinit,
                                          MetadataTests::teardown, false));
    benchmark.addResult("Metadata", "IoHelper::getFileStat",
                        benchmark.measure(IoHelperTests::iohelper_getFileStat, testFilePath, MetadataTests::testinit,
                                          MetadataTests::teardown, false));
    // --- READ ---
    // std::cout << "[Running Read Tests...]\n";
    benchmark.addResult(
            "Read", "ifstream (binary)",
            benchmark.measure(ReadTests::ifstream_binary, testFilePath, ReadTests::testinit, ReadTests::teardown, false));
    benchmark.addResult(
            "Read", "ifstream (text)",
            benchmark.measure(ReadTests::ifstream_text, testFilePath, ReadTests::testinit, ReadTests::teardown, false));
    benchmark.addResult(
            "Read", "fopen (rb)",
            benchmark.measure(ReadTests::fread_binary, testFilePath, ReadTests::testinit, ReadTests::teardown, false));
    benchmark.addResult("Read", "fopen (r)",
                        benchmark.measure(ReadTests::fread_text, testFilePath, ReadTests::testinit, ReadTests::teardown, false));
#if defined(KD_WINDOWS)
    benchmark.addResult(
            "Read", "CreateFileA",
            benchmark.measure(ReadTests::win32_readfile, testFilePath, ReadTests::testinit, ReadTests::teardown, false));
    benchmark.addResult(
            "Read", "CreateFileW + s2ws",
            benchmark.measure(ReadTests::win32_readfile_w, testFilePath, ReadTests::testinit, ReadTests::teardown, false));
#endif
    benchmark.addResult(
            "Read", "IoHelper::openFile",
            benchmark.measure(IoHelperTests::iohelper_openFile, testFilePath, ReadTests::testinit, ReadTests::teardown, false));

    // --- CREATE ---
    //std::cout << "[Running Create Tests...]\n";
    benchmark.addResult(
            "Create", "create (ofstream)",
            benchmark.measure(CreateTests::create_ofstream, testFilePath, CreateTests::testinit, CreateTests::teardown, false));
    benchmark.addResult(
            "Create", "create (fopen)",
            benchmark.measure(CreateTests::create_fopen, testFilePath, CreateTests::testinit, CreateTests::teardown, false));
    benchmark.addResult("Create", "create (CreateFileA)",
                        benchmark.measure(CreateTests::create_CreateFileA, testFilePath, CreateTests::testinit,
                                          CreateTests::teardown, false));
    benchmark.addResult("Create", "create (CreateFileW)",
                        benchmark.measure(CreateTests::create_CreateFileW, testFilePath, CreateTests::testinit,
                                          CreateTests::teardown, false));

    // --- DELETE ---
    //std::cout << "[Running Delete Tests...]\n";
    benchmark.addResult("Delete", "delete (filesystem::remove)",
                        benchmark.measure(DeleteTests::delete_filesystem_remove, testFilePath, DeleteTests::testinit,
                                          DeleteTests::teardown, true));
    benchmark.addResult(
            "Delete", "delete (DeleteFileA)",
            benchmark.measure(DeleteTests::delete_DeleteFileA, testFilePath, DeleteTests::testinit, DeleteTests::teardown, true));
    benchmark.addResult(
            "Delete", "delete (CRT remove)",
            benchmark.measure(DeleteTests::delete_crt_remove, testFilePath, DeleteTests::testinit, DeleteTests::teardown, true));
    benchmark.addResult("Delete", "IoHelper::deleteItem",
                        benchmark.measure(IoHelperTests::iohelper_deleteItem, testFilePath, DeleteTests::testinit,
                                          DeleteTests::teardown, true));

    // --- MOVE ---
    // std::cout << "[Running Move Tests...]\n";
    benchmark.addResult(
            "Move", "move (std::filesystem::rename)",
            benchmark.measure(MoveTests::move_filesystem_rename, testFilePath, MoveTests::testinit, MoveTests::teardown, true));
    benchmark.addResult(
            "Move", "move (MoveFileA)",
            benchmark.measure(MoveTests::move_MoveFileA, testFilePath, MoveTests::testinit, MoveTests::teardown, true));
    benchmark.addResult(
            "Move", "move (MoveFileW)",
            benchmark.measure(MoveTests::move_MoveFileW, testFilePath, MoveTests::testinit, MoveTests::teardown, true));
    benchmark.addResult(
            "Move", "IoHelper::moveItem",
            benchmark.measure(IoHelperTests::iohelper_moveItem, testFilePath, MoveTests::testinit, MoveTests::teardown, true));

    // --- SIZE ---
    // std::cout << "[Running Size Tests...]\n";
    benchmark.addResult(
            "Size", "filesystem::file_size",
            benchmark.measure(SizeTests::filesystem_filesize, testFilePath, SizeTests::testinit, SizeTests::teardown, true));
#if defined(KD_WINDOWS)
    benchmark.addResult(
            "Size", "GetFileSizeEx (Win32)",
            benchmark.measure(SizeTests::win32_getfilesize, testFilePath, SizeTests::testinit, SizeTests::teardown, true));
#endif
    benchmark.addResult(
            "Size", "_filelength (CRT)",
            benchmark.measure(SizeTests::crt_filelength, testFilePath, SizeTests::testinit, SizeTests::teardown, true));
    benchmark.addResult(
            "Size", "IoHelper::getFileSize",
            benchmark.measure(IoHelperTests::iohelper_getFileSize, testFilePath, SizeTests::testinit, SizeTests::teardown, true));

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

    (void) DeleteTestFile(testFilePathFull);
}

// ============================================================================
// CppUnit wrapper to run all IO tests from a single test
// ============================================================================

void BenchmarkIOHelper::setUp() {
    TestBase::start();
}

void BenchmarkIOHelper::tearDown() {
    TestBase::stop();
}

void BenchmarkIOHelper::runAllIOBenchmarks() {
    // Name and iterations can be adjusted or parameterized if needed
    RunAllBenchmarks(SyncPath("io_benchmark_test.tmp"), iterations_);
}

} // namespace KDC
