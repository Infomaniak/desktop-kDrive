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

#include "testexclusiontemplatecache.h"
#include "libparms/db/parmsdb.h"

#include <filesystem>

using namespace CppUnit;

namespace KDC {

static const std::vector<std::string> excludedTemplates = {"*~",
                                                           "~$*",
                                                           ".~lock.*",
                                                           "~*.tmp",
                                                           "*.~*",
                                                           "Icon\r*",
                                                           ".DS_Store",
                                                           ".ds_store",
                                                           "._*",
                                                           "Thumbs.db",
                                                           "System Volume Information",
                                                           ".*.sw?",
                                                           ".*.*sw?",
                                                           ".TemporaryItems",
                                                           ".Trashes",
                                                           ".DocumentRevisions-V100",
                                                           ".Trash-*",
                                                           ".fseventd",
                                                           ".apdisk",
                                                           ".directory",
                                                           "*.part",
                                                           "*.filepart",
                                                           "*.crdownload",
                                                           "*.kate-swp",
                                                           "*.gnucash.tmp-*",
                                                           ".synkron.*",
                                                           ".sync.ffs_db",
                                                           ".symform",
                                                           ".symform-store",
                                                           ".fuse_hidden*",
                                                           "*.unison",
                                                           ".nfs*",
                                                           "My Saved Places.",
                                                           "*_conflict_*_*_*"};

static const std::vector<std::string> rejectedFiles = {
    // *~
    "test~",
    ".test~"
    // *.~*
    ,
    "*.~*",
    ".~",
    "test.~test",
    "test.~",
    ".~test"
    // Icon\r*
    ,
    "Icon\r*",
    "Icon\r",
    "Icon\rtest"
    // .*.sw?
    // .*.*sw?
    ,
    ".*.sw?",
    ".*.*sw?",
    ".testsw?",
    ".*.sw?",
    "..*sw?",
    ".test.sw?",
    "..testsw?",
    ".test.testsw?",
    "..sw?",
    ".123.123sw?",
    "/Applications/kDrive.app/Contents/Resources/.test.testsw?",
    "/Applications/kDrive.app/.test.testsw?/Contents/Resources"
    // My Saved Places.
    ,
    "My Saved Places.",
    "testfile_conflict_20220913_130102_abcdefghij.txt"};

static const std::vector<std::string> acceptedFiles = {
    // *~
    "~test"
    // *.~*
    ,
    "test.test~test"
    // Icon\r*
    ,
    "test Icon\r"
    // .*.sw?
    // .*.*sw?
    ,
    "test.sw?",
    "/Applications/kDrive.app/Contents/Resources/test.testsw?",
    "/Applications/kDrive.app.testsw?/Contents/Resources",
    "/Applications/kDrive.app.test.testsw?/Contents/Resources"
    // My Saved Places.
    ,
    "test My Saved Places."};

void TestExclusionTemplateCache::setUp() {
    // Create parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    std::vector<ExclusionTemplate> exclusionTemplates;
    for (const auto &templ : excludedTemplates) {
        exclusionTemplates.push_back(ExclusionTemplate(templ));
    }
    ExclusionTemplateCache::instance()->update(true, exclusionTemplates);
}

void TestExclusionTemplateCache::tearDown() {
    ParmsDb::reset();
}

void TestExclusionTemplateCache::testIsExcluded() {
    // Exclude hidden files
    Parameters params;
    bool found = false;
    CPPUNIT_ASSERT(ParmsDb::instance()->selectParameters(params, found) && found);
    params.setSyncHiddenFiles(false);
    CPPUNIT_ASSERT(ParmsDb::instance()->updateParameters(params, found) && found);

    // Test rejected files
    for (const auto &str : rejectedFiles) {
        bool isWarning = false;
        bool isExcluded = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcludedTemplate(str, isWarning));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->checkIfIsExcluded("", str, isWarning, isExcluded, ioError));
        if (!isExcluded) {
            std::cout << str << "\n\n";
        }
        CPPUNIT_ASSERT(!isWarning);
        CPPUNIT_ASSERT(isExcluded);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // Test accepted files
    for (const auto &str : acceptedFiles) {
        bool isWarning = true;
        bool isExcluded = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcludedTemplate(str, isWarning));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->checkIfIsExcluded("", str, isWarning, isExcluded, ioError));
        CPPUNIT_ASSERT(!isWarning);
        CPPUNIT_ASSERT(!isExcluded);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    {
        // Test exclude hidden file
        SyncPath testPath = ".my_hidden_file.txt";
        bool isWarning = true;
        bool isExcluded = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcludedTemplate(testPath, isWarning));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->checkIfIsExcluded("", testPath, isWarning, isExcluded, ioError));
        CPPUNIT_ASSERT(!isWarning);
        CPPUNIT_ASSERT(isExcluded);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    {
        // Test exclude hidden folder
        SyncPath testPath = "A/.my_hidden_folder/AA/my_file.txt";
        bool isWarning = true;
        bool isExcluded = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcludedTemplate(testPath, isWarning));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->checkIfIsExcluded("", testPath, isWarning, isExcluded, ioError));
        CPPUNIT_ASSERT(!isWarning);
        CPPUNIT_ASSERT(isExcluded);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }

    // Include hidden files
    params.setSyncHiddenFiles(true);
    CPPUNIT_ASSERT(ParmsDb::instance()->updateParameters(params, found) && found);

    {
        // Test include hidden file
        SyncPath testPath = ".my_hidden_file.txt";
        bool isWarning = true;
        bool isExcluded = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcludedTemplate(testPath, isWarning));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->checkIfIsExcluded("", testPath, isWarning, isExcluded, ioError));
        CPPUNIT_ASSERT(!isWarning);
        CPPUNIT_ASSERT(!isExcluded);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
    {
        // Test include hidden folder
        SyncPath testPath = "A/.my_hidden_folder/AA/my_file.txt";
        bool isWarning = true;
        bool isExcluded = false;
        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcludedTemplate(testPath, isWarning));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->checkIfIsExcluded("", testPath, isWarning, isExcluded, ioError));
        CPPUNIT_ASSERT(!isWarning);
        CPPUNIT_ASSERT(!isExcluded);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
    }
}

}  // namespace KDC
