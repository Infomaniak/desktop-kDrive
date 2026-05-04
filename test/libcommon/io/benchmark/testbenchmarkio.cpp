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

int32_t testbenchmarkio::iterations_ = 100000;

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
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(40) << "Method" << std::right << std::setw(14) << "Time (ms)" << std::setw(18)
                  << "Ns/Call" << std::setw(8) << "OK" << "\n";

        for (const auto &r: filtered) {
            const bool isIoHelper = r.method.rfind("IoHelper::", 0) == 0;
            const bool isStripped = r.method.rfind("stripped IoHelper-", 0) == 0;
            const char fill = isIoHelper ? '=' : (isStripped ? '-' : ' ');

            std::cout << std::left << std::setfill(fill) << std::setw(40) << r.method + " " << std::right << std::setw(14)
                      << std::fixed << std::setprecision(2) << " " + std::to_string(std::llround(r.time_ms)) + " "
                      << std::setw(18) << std::fixed << std::setprecision(1) << " " + std::to_string(std::llround(r.avg_ns_per_call)) + " " << std::setw(8)
                      << " " + std::string(r.success ? "Y" : "N") << std::setfill(' ') << std::endl;
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
    const std::string &cat = results_.back().category;
    const std::string &method = results_.back().method;

    // Count completed results in the current category
    int current = 0;
    for (const auto &r: results_)
        if (r.category == cat) current++;

    int total = current;
    const auto it = categoryTotals_.find(cat);
    if (it != categoryTotals_.end()) total = it->second;

    // Print category header when this is the first result of a new category
    const bool newCategory = (results_.size() == 1) || (results_[results_.size() - 2].category != cat);
    if (newCategory) std::cout << "\n  [" << cat << "]\n";

    constexpr int barWidth = 40;
    const float ratio = (total > 0) ? static_cast<float>(current) / static_cast<float>(total) : 1.0f;
    const int filled = static_cast<int>(ratio * barWidth);

    std::cout << "\r    [";
    for (int i = 0; i < barWidth; ++i) std::cout << (i < filled ? '#' : '-');
    std::cout << "] " << std::setw(2) << current << "/" << total << " (" << std::fixed << std::setprecision(0) << std::setw(3)
              << (ratio * 100.0f) << "%)  " << method << std::flush;

    if (current >= total) std::cout << "\n";
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

bool iohelper_checkIfPathExists(const SyncPath &path) {
    bool exists = false;
    IoError ioError = IoError::Unknown;
    IoHelper::checkIfPathExists(path, exists, ioError, IoHelper::PathCheckOption::Insensitive);
    return exists;
}

bool stripped_iohelper_checkIfPathExists(const SyncPath &path) {
    std::error_code ec;
    const auto s = std::filesystem::symlink_status(path, ec);
    return !ec && s.type() != std::filesystem::file_type::not_found;
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

template<typename StatT>
inline void consumeStatFields(const StatT &st) noexcept {
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
    (void) dev; (void) inode; (void) mode; (void) nlink;
    (void) uid; (void) gid;  (void) rdev;  (void) size;
    (void) atime; (void) mtime; (void) ctime;
}

bool stat_full(const SyncPath &path) {
    struct stat st{};
    if (::stat(path.string().c_str(), &st) != 0) return false;
    consumeStatFields(st);
    return true;
}

bool wstat_full(const SyncPath &path) {
    struct _stat64i32 st{};
    if (::_wstat(path.native().c_str(), &st) != 0) return false;
    consumeStatFields(st);
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
    const bool ok = (::fstat(fd, &st) == 0);
    if (ok) consumeStatFields(st);
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

bool iohelper_getFileStat(const SyncPath &path) {
    FileStat fs{};
    IoError ioError = IoError::Unknown;
    return IoHelper::getFileStat(path, &fs, ioError, IoHelper::PathCheckOption::Insensitive);
}

bool stripped_iohelper_getFileStat(const SyncPath &path) {
#if defined(KD_WINDOWS)
    struct _stat64i32 st{};
    return ::_wstat(path.native().c_str(), &st) == 0;
#else
    struct stat st{};
    return ::stat(path.string().c_str(), &st) == 0;
#endif
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
    HANDLE h = CreateFileA(path.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
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

    HANDLE h = CreateFileW(path.native().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
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

bool iohelper_openFile(const SyncPath &path) {
    std::ifstream file;
    IoError ioError = IoError::Unknown;
    bool ok = IoHelper::openFile(path, file, ioError);
    if (file.is_open()) file.close();
    return ok;
}

bool stripped_iohelper_openFile(const SyncPath &path) {
    std::ifstream f(path.native(), std::ifstream::binary);
    const bool ok = f.is_open();
    if (ok) f.close();
    return ok;
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
    HANDLE h = CreateFileA(path.string().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
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

bool iohelper_getFileSize(const SyncPath &path) {
    uint64_t size = 0;
    IoError ioError = IoError::Unknown;
    return IoHelper::getFileSize(path, size, ioError);
}

bool stripped_iohelper_getFileSize(const SyncPath &path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    return !ec && size != static_cast<std::uintmax_t>(-1);
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
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (counter = 0; counter < static_cast<uint64_t>(getiterations()); counter++) {
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

bool delete_remove_all(const SyncPath &dir) {
    uint64_t id = counter++;
    std::error_code ec;

    auto filename = (std::filesystem::path(dir) / ("bench_delete_" + std::to_string(id) + ".tmp")).string();
    std::filesystem::remove_all(filename, ec);

    return ec == std::error_code();
}

bool iohelper_deleteItem(const SyncPath &dir) {
    const auto path = (std::filesystem::path(dir) / "bench_iohelper_delete.tmp").string();
    CreateTestFile(path);
    IoError ioError = IoError::Unknown;
    return IoHelper::deleteItem(path, ioError);
}

bool stripped_iohelper_deleteItem(const SyncPath &dir) {
    uint64_t id = counter++;
    std::error_code ec;
    const auto path = std::filesystem::path(dir) / ("bench_delete_" + std::to_string(id) + ".tmp");
    std::filesystem::remove_all(path, ec);
    return !ec;
}
} // namespace DeleteTests

// ----------------------------------------------------------------------------
// MOVE FUNCTIONS
// ----------------------------------------------------------------------------
namespace MoveTests {

static std::atomic<uint64_t> counter{0};

void testinit(const SyncPath &dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (counter = 0; counter < static_cast<uint64_t>(getiterations()); counter++) {
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

bool iohelper_moveItem(const SyncPath &dir) {
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
}

bool stripped_iohelper_moveItem(const SyncPath &dir) {
    const auto src = std::filesystem::path(dir) / "bench_stripped_move_src.tmp";
    const auto dst = std::filesystem::path(dir) / "bench_stripped_move_dst.tmp";
    CreateTestFile(src.string());
    std::error_code ec;
    std::filesystem::rename(src, dst, ec);
    if (!ec) std::filesystem::remove(dst, ec);
    return !ec;
}

} // namespace MoveTests


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
    return testbenchmarkio::iterations_;
}

void setiterations(int n) {
    testbenchmarkio::iterations_ = n;
}

// ============================================================================
// RUNNER IMPLEMENTATION
// ============================================================================
void RunAllBenchmarks(const SyncPath &testFilePath, int iterations) {
    testbenchmarkio benchmark(iterations);

    // Expected benchmark count per category (must match addResult calls below)
    std::map<std::string, int> catCounts;
    catCounts["Exists"] = 5; // filesystem_status, filesystem_exists, _stat, IoHelper, stripped IoHelper
#if defined(KD_WINDOWS)
    catCounts["Exists"] += 1; // GetFileAttributesW
#endif
    catCounts["Metadata"] = 5; // stat, fstat, filesystem_full, IoHelper, stripped IoHelper
#if defined(KD_LINUX) || defined(KD_MACOS)
    catCounts["Metadata"] += 1; // lstat
#endif
#if defined(KD_LINUX)
    catCounts["Metadata"] += 1; // statx
#endif
    catCounts["Read"] = 6; // ifstream x2, fread x2, IoHelper, stripped IoHelper
#if defined(KD_WINDOWS)
    catCounts["Read"] += 2; // CreateFileA, CreateFileW
#endif
    catCounts["Create"] = 4;
    catCounts["Delete"] = 6; // filesystem_remove, DeleteFileA, crt_remove, remove_all, IoHelper, stripped IoHelper
    catCounts["Move"] = 5; // rename, MoveFileA, MoveFileW, IoHelper, stripped IoHelper
    catCounts["Size"] = 4; // filesystem, crt, IoHelper, stripped IoHelper
#if defined(KD_WINDOWS)
    catCounts["Size"] += 1; // GetFileSizeEx
#endif
    benchmark.setCategoryExpectedCounts(catCounts);

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "              IOHELPER BENCHMARK SUITE                          \n";
    std::cout << "================================================================\n";
    std::cout << "Test file: " << testFilePath << "\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "\n";

    // --- EXISTS ---
    {
        LocalTemporaryDirectory tmpDir("bench_exists");
        const SyncPath filePath = tmpDir.path() / "bench_file.tmp";
        benchmark.addResult(
                "Exists", "std::filesystem::status",
                benchmark.measure(ExistsTests::filesystem_status, filePath, ExistsTests::testinit, ExistsTests::teardown, false));
        benchmark.addResult(
                "Exists", "std::filesystem::exists",
                benchmark.measure(ExistsTests::filesystem_exists, filePath, ExistsTests::testinit, ExistsTests::teardown, false));
#if defined(KD_WINDOWS)
        benchmark.addResult("Exists", "GetFileAttributesW",
                            benchmark.measure(ExistsTests::win32_getfileattributes_w, filePath, ExistsTests::testinit,
                                              ExistsTests::teardown, false));
#endif
        benchmark.addResult(
                "Exists", "_stat()",
                benchmark.measure(ExistsTests::crt_stat, filePath, ExistsTests::testinit, ExistsTests::teardown, false));
        benchmark.addResult("Exists", "IoHelper::checkIfPathExists",
                            benchmark.measure(ExistsTests::iohelper_checkIfPathExists, filePath, ExistsTests::testinit,
                                              ExistsTests::teardown, false));
        benchmark.addResult("Exists", "stripped IoHelper-checkIfPathExists",
                            benchmark.measure(ExistsTests::stripped_iohelper_checkIfPathExists, filePath, ExistsTests::testinit,
                                              ExistsTests::teardown, false));
    }

    // --- METADATA ---
    {
        LocalTemporaryDirectory tmpDir("bench_metadata");
        const SyncPath filePath = tmpDir.path() / "bench_file.tmp";
        benchmark.addResult(
                "Metadata", "stat_full",
                benchmark.measure(MetadataTests::stat_full, filePath, MetadataTests::testinit, MetadataTests::teardown, false));
#if defined(KD_LINUX) || defined(KD_MACOS)
        benchmark.addResult(
                "Metadata", "lstat_full",
                benchmark.measure(MetadataTests::lstat_full, filePath, MetadataTests::testinit, MetadataTests::teardown, false));
#endif
        benchmark.addResult(
                "Metadata", "fstat_full",
                benchmark.measure(MetadataTests::fstat_full, filePath, MetadataTests::testinit, MetadataTests::teardown, false));
#if defined(KD_LINUX)
        benchmark.addResult(
                "Metadata", "statx_full",
                benchmark.measure(MetadataTests::statx_full, filePath, MetadataTests::testinit, MetadataTests::teardown, false));
#endif
        benchmark.addResult("Metadata", "filesystem_full",
                            benchmark.measure(MetadataTests::filesystem_full, filePath, MetadataTests::testinit,
                                              MetadataTests::teardown, false));
        benchmark.addResult("Metadata", "IoHelper::getFileStat",
                            benchmark.measure(MetadataTests::iohelper_getFileStat, filePath, MetadataTests::testinit,
                                              MetadataTests::teardown, false));
        benchmark.addResult("Metadata", "stripped IoHelper-getFileStat",
                            benchmark.measure(MetadataTests::stripped_iohelper_getFileStat, filePath, MetadataTests::testinit,
                                              MetadataTests::teardown, false));
    }

    // --- READ ---
    {
        LocalTemporaryDirectory tmpDir("bench_read");
        const SyncPath filePath = tmpDir.path() / "bench_file.tmp";
        benchmark.addResult(
                "Read", "ifstream (binary)",
                benchmark.measure(ReadTests::ifstream_binary, filePath, ReadTests::testinit, ReadTests::teardown, false));
        benchmark.addResult(
                "Read", "ifstream (text)",
                benchmark.measure(ReadTests::ifstream_text, filePath, ReadTests::testinit, ReadTests::teardown, false));
        benchmark.addResult(
                "Read", "fopen (rb)",
                benchmark.measure(ReadTests::fread_binary, filePath, ReadTests::testinit, ReadTests::teardown, false));
        benchmark.addResult("Read", "fopen (r)",
                            benchmark.measure(ReadTests::fread_text, filePath, ReadTests::testinit, ReadTests::teardown, false));
#if defined(KD_WINDOWS)
        benchmark.addResult(
                "Read", "CreateFileA",
                benchmark.measure(ReadTests::win32_readfile, filePath, ReadTests::testinit, ReadTests::teardown, false));
        benchmark.addResult(
                "Read", "CreateFileW + s2ws",
                benchmark.measure(ReadTests::win32_readfile_w, filePath, ReadTests::testinit, ReadTests::teardown, false));
#endif
        benchmark.addResult(
                "Read", "IoHelper::openFile",
                benchmark.measure(ReadTests::iohelper_openFile, filePath, ReadTests::testinit, ReadTests::teardown, false));
        benchmark.addResult("Read", "stripped IoHelper-openFile",
                            benchmark.measure(ReadTests::stripped_iohelper_openFile, filePath, ReadTests::testinit,
                                              ReadTests::teardown, false));
    }

    // --- CREATE ---
    {
        setiterations(iterations / 10);
        LocalTemporaryDirectory tmpDir("bench_create");
        const SyncPath dirPath = tmpDir.path() / "workdir";
        benchmark.addResult(
                "Create", "create (ofstream)",
                benchmark.measure(CreateTests::create_ofstream, dirPath, CreateTests::testinit, CreateTests::teardown, false));
        benchmark.addResult(
                "Create", "create (fopen)",
                benchmark.measure(CreateTests::create_fopen, dirPath, CreateTests::testinit, CreateTests::teardown, false));
        benchmark.addResult(
                "Create", "create (CreateFileA)",
                benchmark.measure(CreateTests::create_CreateFileA, dirPath, CreateTests::testinit, CreateTests::teardown, false));
        benchmark.addResult(
                "Create", "create (CreateFileW)",
                benchmark.measure(CreateTests::create_CreateFileW, dirPath, CreateTests::testinit, CreateTests::teardown, false));
        setiterations(iterations);
    }

    // --- DELETE ---
    {
        setiterations(iterations / 10);
        LocalTemporaryDirectory tmpDir("bench_delete");
        const SyncPath dirPath = tmpDir.path();
        benchmark.addResult("Delete", "delete (filesystem::remove)",
                            benchmark.measure(DeleteTests::delete_filesystem_remove, dirPath, DeleteTests::testinit,
                                              DeleteTests::teardown, false));
        benchmark.addResult(
                "Delete", "delete (DeleteFileA)",
                benchmark.measure(DeleteTests::delete_DeleteFileA, dirPath, DeleteTests::testinit, DeleteTests::teardown, false));
        benchmark.addResult(
                "Delete", "delete (CRT remove)",
                benchmark.measure(DeleteTests::delete_crt_remove, dirPath, DeleteTests::testinit, DeleteTests::teardown, false));
        benchmark.addResult(
                "Delete", "delete (filesystem::remove_all)",
                benchmark.measure(DeleteTests::delete_remove_all, dirPath, DeleteTests::testinit, DeleteTests::teardown, false));
        benchmark.addResult("Delete", "IoHelper::deleteItem",
                            benchmark.measure(DeleteTests::iohelper_deleteItem, dirPath, DeleteTests::testinit,
                                              DeleteTests::teardown, false));
        benchmark.addResult("Delete", "stripped IoHelper-deleteItem",
                            benchmark.measure(DeleteTests::stripped_iohelper_deleteItem, dirPath, DeleteTests::testinit,
                                              DeleteTests::teardown, false));
        setiterations(iterations);
    }

    // --- MOVE ---
    {
        setiterations(iterations / 10);
        LocalTemporaryDirectory tmpDir("bench_move");
        const SyncPath dirPath = tmpDir.path();
        benchmark.addResult(
                "Move", "move (std::filesystem::rename)",
                benchmark.measure(MoveTests::move_filesystem_rename, dirPath, MoveTests::testinit, MoveTests::teardown, false));
        benchmark.addResult(
                "Move", "move (MoveFileA)",
                benchmark.measure(MoveTests::move_MoveFileA, dirPath, MoveTests::testinit, MoveTests::teardown, false));
        benchmark.addResult(
                "Move", "move (MoveFileW)",
                benchmark.measure(MoveTests::move_MoveFileW, dirPath, MoveTests::testinit, MoveTests::teardown, false));
        benchmark.addResult(
                "Move", "IoHelper::moveItem",
                benchmark.measure(MoveTests::iohelper_moveItem, dirPath, MoveTests::testinit, MoveTests::teardown, false));
        benchmark.addResult("Move", "stripped IoHelper-moveItem",
                            benchmark.measure(MoveTests::stripped_iohelper_moveItem, dirPath, MoveTests::testinit,
                                              MoveTests::teardown, false));
        setiterations(iterations);
    }

    // --- SIZE ---
    {
        LocalTemporaryDirectory tmpDir("bench_size");
        const SyncPath filePath = tmpDir.path() / "bench_file.tmp";
        benchmark.addResult(
                "Size", "filesystem::file_size",
                benchmark.measure(SizeTests::filesystem_filesize, filePath, SizeTests::testinit, SizeTests::teardown, false));
#if defined(KD_WINDOWS)
        benchmark.addResult(
                "Size", "GetFileSizeEx (Win32)",
                benchmark.measure(SizeTests::win32_getfilesize, filePath, SizeTests::testinit, SizeTests::teardown, false));
#endif
        benchmark.addResult(
                "Size", "_filelength (CRT)",
                benchmark.measure(SizeTests::crt_filelength, filePath, SizeTests::testinit, SizeTests::teardown, false));
        benchmark.addResult(
                "Size", "IoHelper::getFileSize",
                benchmark.measure(SizeTests::iohelper_getFileSize, filePath, SizeTests::testinit, SizeTests::teardown, false));
        benchmark.addResult("Size", "stripped IoHelper-getFileSize",
                            benchmark.measure(SizeTests::stripped_iohelper_getFileSize, filePath, SizeTests::testinit,
                                              SizeTests::teardown, false));
    }

    // Print results and cleanup
    benchmark.printResultsByCategory();

    std::cout << "\n================================================================\n";
    std::cout << "                    BENCHMARK RESULTS\n";
    std::cout << "================================================================\n";

    // --- TAB 1: IoHelper  /  TAB 2: Best raw API  /  TAB 3: Stripped IoHelper ---
    {
        const std::map<std::string, std::string> iohelperUnderlyingApi = {
                {"IoHelper::checkIfPathExists", "std::filesystem::symlink_status"},
                {"IoHelper::getFileStat", "NtQueryInformationFile (Win) / stat (Linux/Mac)"},
                {"IoHelper::getFileSize", "std::filesystem::file_size"},
                {"IoHelper::openFile", "std::ifstream (binary, with retry loop)"},
                {"IoHelper::deleteItem", "std::filesystem::remove_all"},
                {"IoHelper::moveItem", "std::filesystem::rename"},
        };

        struct CategoryEntry {
                std::string category;
                // TAB 1 - IoHelper
                std::string iohelperMethod;
                std::string underlyingApi;
                double iohelperMs;
                double iohelperNs;
                bool iohelperOk;
                bool iohelperIsFastest;
                // TAB 2 - Best raw (excludes IoHelper:: and stripped IoHelper-)
                std::string bestAltMethod;
                double bestAltMs;
                double bestAltNs;
                bool bestAltOk;
                bool bestAltIsFastest;
                // TAB 3 - Stripped IoHelper
                std::string strippedMethod;
                double strippedMs;
                double strippedNs;
                bool strippedOk;
                bool strippedIsFastest;
        };

        std::vector<CategoryEntry> entries;

        std::map<std::string, std::vector<BenchmarkResult>> byCategory;
        for (const auto &r: benchmark.getResults()) byCategory[r.category].push_back(r);

        const std::vector<std::string> catOrder = {"Exists", "Metadata", "Read", "Size", "Create", "Delete", "Move"};
        for (const auto &cat: catOrder) {
            const auto it = byCategory.find(cat);
            if (it == byCategory.end()) continue;
            const auto &catResults = it->second;

            const BenchmarkResult *iohelperResult = nullptr;
            const BenchmarkResult *strippedResult = nullptr;
            const BenchmarkResult *bestAlt = nullptr;
            double fastestMs = std::numeric_limits<double>::max();

            for (const auto &r: catResults) {
                if (r.time_ms < fastestMs) fastestMs = r.time_ms;
                if (r.method.rfind("IoHelper::", 0) == 0) {
                    iohelperResult = &r;
                } else if (r.method.rfind("stripped IoHelper-", 0) == 0) {
                    strippedResult = &r;
                } else if (!bestAlt || r.time_ms < bestAlt->time_ms) {
                    bestAlt = &r;
                }
            }
            if (!iohelperResult || !bestAlt || !strippedResult) continue;

            CategoryEntry e;
            e.category = cat;

            e.iohelperMethod = iohelperResult->method;
            const auto apiIt = iohelperUnderlyingApi.find(iohelperResult->method);
            e.underlyingApi = (apiIt != iohelperUnderlyingApi.end()) ? apiIt->second : "unknown";
            e.iohelperMs = iohelperResult->time_ms;
            e.iohelperNs = iohelperResult->avg_ns_per_call;
            e.iohelperOk = iohelperResult->success;
            e.iohelperIsFastest = (iohelperResult->time_ms <= fastestMs);

            e.bestAltMethod = bestAlt->method;
            e.bestAltMs = bestAlt->time_ms;
            e.bestAltNs = bestAlt->avg_ns_per_call;
            e.bestAltOk = bestAlt->success;
            e.bestAltIsFastest = (bestAlt->time_ms <= fastestMs);

            e.strippedMethod = strippedResult->method;
            e.strippedMs = strippedResult->time_ms;
            e.strippedNs = strippedResult->avg_ns_per_call;
            e.strippedOk = strippedResult->success;
            e.strippedIsFastest = (strippedResult->time_ms <= fastestMs);

            entries.push_back(e);
        }

        constexpr int W_CAT = 10, W_METHOD = 44, W_MS = 14, W_NS = 14, W_OK = 6, W_FAST = 12;
        const int totalWidth = W_CAT + W_METHOD + W_MS + W_NS + W_OK + W_FAST;
        const std::string sep(totalWidth, '-');

        // ---- TAB 1: IoHelper ----
        std::cout << "\n";
        std::cout << sep << "\n";
        std::cout << "  TAB 1 - IoHelper  (production API + its underlying OS call)\n";
        std::cout << sep << "\n";
        std::cout << std::left << std::setw(W_CAT) << "Category" << std::setw(W_METHOD) << "IoHelper method" << std::right
                  << std::setw(W_MS) << "Time (ms)" << std::setw(W_NS) << "Ns/Call" << std::setw(W_OK) << "OK"
                  << std::setw(W_FAST) << "Is fastest" << "\n";
        std::cout << sep << "\n";
        for (const auto &e: entries) {
            std::cout << std::left << std::setw(W_CAT) << e.category << std::setw(W_METHOD) << e.iohelperMethod << std::right
                      << std::setw(W_MS) << std::fixed << std::setprecision(2) << e.iohelperMs << std::setw(W_NS) << std::fixed
                      << std::setprecision(1) << e.iohelperNs << std::setw(W_OK) << (e.iohelperOk ? "OK" : "FAIL")
                      << std::setw(W_FAST) << (e.iohelperIsFastest ? "Y" : "N") << "\n";
            std::cout << std::left << std::setw(W_CAT) << "" << "  -> " << e.underlyingApi << "\n";
        }
        std::cout << sep << "\n";

        // ---- TAB 2: Best raw benchmark (excludes IoHelper:: and stripped IoHelper-) ----
        std::cout << "\n";
        std::cout << sep << "\n";
        std::cout << "  TAB 2 - Best raw API  (fastest, excluding IoHelper and stripped variants)\n";
        std::cout << sep << "\n";
        std::cout << std::left << std::setw(W_CAT) << "Category" << std::setw(W_METHOD) << "Fastest method" << std::right
                  << std::setw(W_MS) << "Time (ms)" << std::setw(W_NS) << "Ns/Call" << std::setw(W_OK) << "OK"
                  << std::setw(W_FAST) << "Is fastest" << "\n";
        std::cout << sep << "\n";
        for (const auto &e: entries) {
            std::cout << std::left << std::setw(W_CAT) << e.category << std::setw(W_METHOD) << e.bestAltMethod << std::right
                      << std::setw(W_MS) << std::fixed << std::setprecision(2) << e.bestAltMs << std::setw(W_NS) << std::fixed
                      << std::setprecision(1) << e.bestAltNs << std::setw(W_OK) << (e.bestAltOk ? "OK" : "FAIL")
                      << std::setw(W_FAST) << (e.bestAltIsFastest ? "Y" : "N") << "\n";
        }
        std::cout << sep << "\n";

        // ---- TAB 3: Stripped IoHelper ----
        std::cout << "\n";
        std::cout << sep << "\n";
        std::cout << "  TAB 3 - Stripped IoHelper  (raw underlying API only, no IoHelper overhead)\n";
        std::cout << sep << "\n";
        std::cout << std::left << std::setw(W_CAT) << "Category" << std::setw(W_METHOD) << "Stripped method" << std::right
                  << std::setw(W_MS) << "Time (ms)" << std::setw(W_NS) << "Ns/Call" << std::setw(W_OK) << "OK"
                  << std::setw(W_FAST) << "Is fastest" << "\n";
        std::cout << sep << "\n";
        for (const auto &e: entries) {
            std::cout << std::left << std::setw(W_CAT) << e.category << std::setw(W_METHOD) << e.strippedMethod << std::right
                      << std::setw(W_MS) << std::fixed << std::setprecision(2) << e.strippedMs << std::setw(W_NS) << std::fixed
                      << std::setprecision(1) << e.strippedNs << std::setw(W_OK) << (e.strippedOk ? "OK" : "FAIL")
                      << std::setw(W_FAST) << (e.strippedIsFastest ? "Y" : "N") << "\n";
        }
        std::cout << sep << "\n";
    }
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
    RunAllBenchmarks(SyncPath("io_benchmark_test.tmp"), testbenchmarkio::iterations_);
}

} // namespace KDC
