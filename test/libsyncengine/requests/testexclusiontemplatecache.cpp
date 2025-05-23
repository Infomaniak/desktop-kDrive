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

#include "testexclusiontemplatecache.h"
#include "libparms/db/parmsdb.h"
#include "requests/parameterscache.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

static const std::vector<ExclusionTemplate> excludedTemplates = {
        ExclusionTemplate(".parms.db"), ExclusionTemplate(".sync_*.db"), ExclusionTemplate(".parms.db-shm"),
        ExclusionTemplate(".parms.db-wal"), ExclusionTemplate(".sync_*.db-shm"), ExclusionTemplate(".sync_*.db-wal"),
        ExclusionTemplate(".sentry-native_client"), ExclusionTemplate(".sentry-native_server"),
        ExclusionTemplate("*_conflict_*_*_*"), ExclusionTemplate("*_blacklisted_*_*_*"), ExclusionTemplate("*~"),
        ExclusionTemplate("~$*"), ExclusionTemplate("*.~*"), ExclusionTemplate("._*"), ExclusionTemplate("~*.tmp"),
        ExclusionTemplate("*.idlk"), ExclusionTemplate("*.lock"), ExclusionTemplate("*.lck"), ExclusionTemplate("*.part"),
        ExclusionTemplate(".~lock.*"), ExclusionTemplate("*.symform"), ExclusionTemplate("*.symform-store"),
        ExclusionTemplate("*.unison"), ExclusionTemplate(".directory"), ExclusionTemplate(".sync.ffs_db"),
        ExclusionTemplate(".synkron.*"), ExclusionTemplate("*.crdownload"),
#if defined(__APPLE__)
        // macOS only
        ExclusionTemplate(".fuse_hidden*"), ExclusionTemplate("*.kate-swp"), ExclusionTemplate(".DS_Store"),
        ExclusionTemplate(".ds_store"), ExclusionTemplate(".TemporaryItems"), ExclusionTemplate(".Trashes"),
        ExclusionTemplate(".DocumentRevisions-V100"), ExclusionTemplate(".fseventd"), ExclusionTemplate(".apdisk"),
        ExclusionTemplate("*.photoslibrary"), ExclusionTemplate("*.tvlibrary"), ExclusionTemplate("*.musiclibrary"),
        ExclusionTemplate("Icon\r*"), ExclusionTemplate(".Spotlight-V100"), ExclusionTemplate("*.lnk")
#elif defined(_WIN32)
        // Windows only
        ExclusionTemplate("*.kate-swp"), ExclusionTemplate("System Volume Information"), ExclusionTemplate("Thumbs.db"),
        ExclusionTemplate("Desktop.ini"), ExclusionTemplate("*.filepart"), ExclusionTemplate("*.app")
#else
        // Linux only
        ExclusionTemplate(".fuse_hidden*"), ExclusionTemplate("*.kate-swp"), ExclusionTemplate("*.gnucash.tmp-*"),
        ExclusionTemplate(".Trash-*"), ExclusionTemplate(".nfs*"), ExclusionTemplate("*.app"), ExclusionTemplate("*.lnk")
#endif
};

// List of names that should be rejected
static const std::vector<std::string> rejectedFiles = {
        "test~",
        ".test~",
        "*.~*",
        ".~",
        "test.~test",
        "test.~",
        ".~test",
        "~test.tmp",
        "testfile_conflict_20220913_130102_abcdefghij.txt",
        "testfile_conflict_test_20220913_130102_abcdefghij.txt",
        "_conflict___",
        "testfile_blacklisted_20220913_130102_abcdefghij.txt",
#if defined(__APPLE__)
        ".DS_Store",
        ".ds_store",
        ".apdisk",
        "Icon\r*",
        "Icon\r",
        "Icon\rtest",
#elif defined(_WIN32)
        "test.kate-swp",
        "System Volume Information",
        "*.app"
#else
        ".fuse_hidden1",
        ".gnucash.tmp-",
        "test.gnucash.tmp-test",
        "test.test.gnucash.tmp-test"
#endif
};

// List of names that should be accepted
static const std::vector<std::string> acceptedFiles = {"~test",
                                                       "test.test~test",
                                                       "test.tmp",
                                                       "~.tmp2",
                                                       "testfile_conflict_130102_abcdefghij.txt",
                                                       "conflict_20220913_130102_abcdefghij.txt",
                                                       "testfile_blacklisted_130102_abcdefghij.txt",
#if defined(__APPLE__)
                                                       "test.apdisk",
                                                       "test_Icon\rtest"
#elif defined(_WIN32)
                                                       "test.testkate-swp",
                                                       "system volume information",
                                                       "System test Volume Information"
#else
                                                       "test.fuse_hidden"
                                                       "test.gnucash.test.tmp-test"
#endif
};

void TestExclusionTemplateCache::setUp() {
    TestBase::start();
    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    ExclusionTemplateCache::instance()->update(true, excludedTemplates);
}

void TestExclusionTemplateCache::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
}

void TestExclusionTemplateCache::testIsExcluded() {
    // Test rejected files
    for (const auto &str: rejectedFiles) {
        bool isWarning = false;
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(str, isWarning));
        CPPUNIT_ASSERT(!isWarning);
    }

    // Test accepted files
    for (const auto &str: acceptedFiles) {
        bool isWarning = true;
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcluded(str, isWarning));
        CPPUNIT_ASSERT(!isWarning);
    }

#ifndef _WIN32
    {
        // Test hidden file
        const SyncPath testPath = testhelpers::localTestDirPath / ".my_hidden_file.txt";
        bool isWarning = true;
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcluded(testPath, isWarning));
    }

    {
        // Test hidden folder
        const SyncPath testPath = testhelpers::localTestDirPath / ".my_hidden_folder/AA/my_file.txt";
        bool isWarning = true;
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcluded(testPath, isWarning));
    }
#endif
}

} // namespace KDC
