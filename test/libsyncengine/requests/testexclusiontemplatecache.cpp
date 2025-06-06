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
#elif defined(_WIN32)
        "System Volume Information",
#else
        ".fuse_hidden1",
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
        CPPUNIT_ASSERT_MESSAGE(str + " is not excluded", ExclusionTemplateCache::instance()->isExcluded(str, isWarning));
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
        CPPUNIT_ASSERT_MESSAGE(testPath.string() + " should not be excluded",
                               !ExclusionTemplateCache::instance()->isExcluded(testPath, isWarning));
    }

    {
        // Test hidden folder
        const SyncPath testPath = testhelpers::localTestDirPath / ".my_hidden_folder/AA/my_file.txt";
        bool isWarning = true;
        CPPUNIT_ASSERT_MESSAGE(testPath.string() + " should not be excluded",
                               !ExclusionTemplateCache::instance()->isExcluded(testPath, isWarning));
    }
#endif
}
void TestExclusionTemplateCache::testCacheFolderIsExcluded() {
    SyncPath cachePath;
    CPPUNIT_ASSERT(IoHelper::cacheDirectoryPath(cachePath));
    CPPUNIT_ASSERT(!cachePath.empty());
    bool isWarning = false;
    CPPUNIT_ASSERT_MESSAGE(cachePath.filename().string() + " is not excluded",
                           ExclusionTemplateCache::instance()->isExcluded(cachePath.filename(), isWarning));
    CPPUNIT_ASSERT(!isWarning);
}


} // namespace KDC
