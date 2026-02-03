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

#include "testtheme.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/types.h"

#include <QIcon>

namespace KDC {

void TestTheme::testSingleton() {
    Theme *instance1 = Theme::instance();
    CPPUNIT_ASSERT_MESSAGE("Theme instance should not be null", instance1 != nullptr);

    Theme *instance2 = Theme::instance();
    CPPUNIT_ASSERT_MESSAGE("Second instance should be same", instance1 == instance2);
}

void TestTheme::testAppNames() {
    Theme *theme = Theme::instance();

    std::string appName = theme->appName();
    CPPUNIT_ASSERT_MESSAGE("App name should not be empty", !appName.empty());

    std::string clientName = theme->appClientName();
    CPPUNIT_ASSERT_MESSAGE("Client name should not be empty", !clientName.empty());
}

void TestTheme::testVersion() {
    Theme *theme = Theme::instance();

    std::string version = theme->version();
    CPPUNIT_ASSERT_MESSAGE("Version should not be empty", !version.empty());

    auto versionOutput = theme->versionSwitchOutput();
    // Should contain version info
}

void TestTheme::testUrls() {
    Theme *theme = Theme::instance();

    QString helpUrl = theme->helpUrl();
    CPPUNIT_ASSERT_MESSAGE("Help URL should not be empty", !helpUrl.isEmpty());

    QString conflictUrl = theme->conflictHelpUrl();
    CPPUNIT_ASSERT_MESSAGE("Conflict URL should not be empty", !conflictUrl.isEmpty());

    QString debugUrl = theme->debugReporterUrl();
    CPPUNIT_ASSERT_MESSAGE("Debug URL should not be empty", !debugUrl.isEmpty());

    // Test with different languages
    QString feedbackEn = theme->feedbackUrl(Language::English);
    QString feedbackFr = theme->feedbackUrl(Language::French);
    CPPUNIT_ASSERT_MESSAGE("Feedback URLs should differ by language", feedbackEn != feedbackFr);
}

void TestTheme::testSystrayIcons() {
    Theme *theme = Theme::instance();

    // Test theme icon getter doesn't crash
    try {
        QIcon icon = theme->syncStateIcon(SyncStatus::Running, false, false, false);
        CPPUNIT_ASSERT_MESSAGE("Icon should be valid", !icon.isNull());
    } catch (...) {
        CPPUNIT_FAIL("Getting sync state icon should not throw");
    }

    // Test mono icon setting
    theme->setSystrayUseMonoIcons(true);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Mono icons setting should apply", true, theme->systrayUseMonoIcons());

    theme->setSystrayUseMonoIcons(false);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Mono icons setting should toggle", false, theme->systrayUseMonoIcons());
}

void TestTheme::testSharingConfig() {
    Theme *theme = Theme::instance();

    bool linkSharing = theme->linkSharing();
    bool userGroupSharing = theme->userGroupSharing();

    // Just verify these return valid bool values, actual values depend on configuration
    // But they shouldn't crash or throw
}

} // namespace KDC
