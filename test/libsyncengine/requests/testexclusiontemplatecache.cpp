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
#include "libsyncengine/propagation/executor/filerescuer.h"
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
        "A\n_blacklisted_20240824_081432_s2L5tFynHP_blacklisted_20240824_180957.eml",
#if defined(KD_MACOS)
        ".DS_Store",
#elif defined(KD_WINDOWS)
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
#if defined(KD_MACOS)
                                                       "test.apdisk",
                                                       "test_Icon\rtest"
#elif defined(KD_WINDOWS)
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
    ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
}

void TestExclusionTemplateCache::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    TestBase::stop();
    ExclusionTemplateCache::reset();
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

#ifndef KD_WINDOWS
    {
        // Test hidden file
        const SyncPath testPath = testhelpers::localTestDirPath() / ".my_hidden_file.txt";
        bool isWarning = true;
        CPPUNIT_ASSERT_MESSAGE(testPath.string() + " should not be excluded",
                               !ExclusionTemplateCache::instance()->isExcluded(testPath, isWarning));
    }

    {
        // Test hidden folder
        const SyncPath testPath = testhelpers::localTestDirPath() / ".my_hidden_folder/AA/my_file.txt";
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

void TestExclusionTemplateCache::testRescueFolderIsExcluded() {
    bool isWarning = false;
    CPPUNIT_ASSERT_MESSAGE(FileRescuer::rescueFolderName().string() + " is not excluded",
                           ExclusionTemplateCache::instance()->isExcluded(FileRescuer::rescueFolderName(), isWarning));
    CPPUNIT_ASSERT(!isWarning);
}

void TestExclusionTemplateCache::testNFCNFDExclusion() {
    // Ensure that all the values in the default sync exclusion list can be normalized to both NFC and NFD forms
    {
        auto exclusionCacheDef = ExclusionTemplateCache::instance()->exclusionTemplates(true);
        for (const auto &exclusionTemplate: exclusionCacheDef) {
            SyncName result;
            CPPUNIT_ASSERT(CommonUtility::normalizedSyncName(Str2SyncName(exclusionTemplate.templ()), result,
                                                             UnicodeNormalization::NFC));
            CPPUNIT_ASSERT(CommonUtility::normalizedSyncName(Str2SyncName(exclusionTemplate.templ()), result,
                                                             UnicodeNormalization::NFD));
        }
    }

    // Ensure that ExclusionTemplateCache is cheking NFC and NFD names
    {
        // None of the name should be excluded by default
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfcSyncName()));
        CPPUNIT_ASSERT(!ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfdSyncName()));

        // Generate all the exclusion templates
        const ExclusionTemplate nfcDefExclTemplate(SyncName2Str(testhelpers::makeNfcSyncName()), false, true);
        const ExclusionTemplate nfdDefExclTemplate(SyncName2Str(testhelpers::makeNfdSyncName()), false, true);
        const ExclusionTemplate nfcUsrExclTemplate(SyncName2Str(testhelpers::makeNfcSyncName()), false, false);
        const ExclusionTemplate nfdUsrExclTemplate(SyncName2Str(testhelpers::makeNfdSyncName()), false, false);

        // Test with only NFC version in def exclusion list
        auto exclusionCacheDef = ExclusionTemplateCache::instance()->exclusionTemplates(true);
        exclusionCacheDef.push_back(nfcDefExclTemplate);
        ExclusionTemplateCache::instance()->update(true, exclusionCacheDef);
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfcSyncName()));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfdSyncName()));

        // Test with only NFD version in def exclusion list
        exclusionCacheDef.pop_back();
        exclusionCacheDef.push_back(nfdDefExclTemplate);
        ExclusionTemplateCache::instance()->update(true, exclusionCacheDef);
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfcSyncName()));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfdSyncName()));

        // Reset def exclusion list
        exclusionCacheDef.pop_back();
        ExclusionTemplateCache::instance()->update(true, exclusionCacheDef);

        // Test with only NFC version in def exclusion list
        auto exclusionCacheUsr = ExclusionTemplateCache::instance()->exclusionTemplates(false);
        exclusionCacheUsr.push_back(nfcUsrExclTemplate);
        ExclusionTemplateCache::instance()->update(false, exclusionCacheUsr);
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfcSyncName()));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfdSyncName()));

        // Test with only NFD version in def exclusion list
        exclusionCacheUsr.pop_back();
        exclusionCacheUsr.push_back(nfdUsrExclTemplate);
        ExclusionTemplateCache::instance()->update(false, exclusionCacheUsr);
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfcSyncName()));
        CPPUNIT_ASSERT(ExclusionTemplateCache::instance()->isExcluded(testhelpers::makeNfdSyncName()));
    }
}

void TestExclusionTemplateCache::testaddRegexForAllNormalizationForms() {
    ExclusionTemplateCache::instance()->_regexPatterns.clear();

    ExclusionTemplate nfcTemplate(SyncName2Str(testhelpers::makeNfcSyncName()));
    std::string nfcRegex = SyncName2Str(testhelpers::makeNfcSyncName()); // Don't need to be a valid regex for this test.
    ExclusionTemplate nfdTemplate(SyncName2Str(testhelpers::makeNfdSyncName()));
    std::string nfdRegex = SyncName2Str(testhelpers::makeNfdSyncName()); // Don't need to be a valid regex for this test.

    // Ensure that both encoding are inserted
    ExclusionTemplateCache::instance()->addRegexForAllNormalizationForms(nfcRegex, nfcTemplate);
    CPPUNIT_ASSERT_EQUAL(size_t(2), ExclusionTemplateCache::instance()->_regexPatterns.size());

    ExclusionTemplateCache::instance()->_regexPatterns.clear();
    ExclusionTemplateCache::instance()->addRegexForAllNormalizationForms(nfdRegex, nfdTemplate);
    CPPUNIT_ASSERT_EQUAL(size_t(2), ExclusionTemplateCache::instance()->_regexPatterns.size());

    // Ensure that we cannot have a duplicated regex
    ExclusionTemplateCache::instance()->_regexPatterns.clear();
    ExclusionTemplateCache::instance()->addRegexForAllNormalizationForms(nfdRegex, nfdTemplate);
    CPPUNIT_ASSERT_EQUAL(size_t(2), ExclusionTemplateCache::instance()->_regexPatterns.size());
    ExclusionTemplateCache::instance()->addRegexForAllNormalizationForms(nfdRegex, nfdTemplate);
    CPPUNIT_ASSERT_EQUAL(size_t(2), ExclusionTemplateCache::instance()->_regexPatterns.size());
    ExclusionTemplateCache::instance()->addRegexForAllNormalizationForms(nfcRegex, nfdTemplate);
    CPPUNIT_ASSERT_EQUAL(size_t(2), ExclusionTemplateCache::instance()->_regexPatterns.size());
}


} // namespace KDC
