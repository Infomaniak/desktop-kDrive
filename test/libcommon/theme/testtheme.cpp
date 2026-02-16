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
#include <QGuiApplication>

namespace KDC {

void TestTheme::testSingleton() {
    const Theme *const instance1 = Theme::instance();
    CPPUNIT_ASSERT_MESSAGE("Theme instance should not be null", instance1 != nullptr);

    const Theme *const instance2 = Theme::instance();
    CPPUNIT_ASSERT_MESSAGE("Second instance should be same", instance1 == instance2);
}

void TestTheme::testAppNames() {
    const Theme *const theme = Theme::instance();

    const std::string appName = theme->appName();
    CPPUNIT_ASSERT_MESSAGE("App name should not be empty", !appName.empty());

    const std::string clientName = theme->appClientName();
    CPPUNIT_ASSERT_MESSAGE("Client name should not be empty", !clientName.empty());
}

void TestTheme::testVersion() {
    const Theme *const theme = Theme::instance();

    const std::string version = theme->version();
    CPPUNIT_ASSERT_MESSAGE("Version should not be empty", !version.empty());

    const auto versionOutput = theme->versionSwitchOutput();
    CPPUNIT_ASSERT_MESSAGE("Version switch output should not be empty", !versionOutput.isEmpty());
}

void TestTheme::testUrls() {
    const Theme *theme = Theme::instance();

    const QString helpUrl = theme->helpUrl();
    CPPUNIT_ASSERT_MESSAGE("Help URL should not be empty", !helpUrl.isEmpty());

    const QString conflictUrl = theme->conflictHelpUrl();
    CPPUNIT_ASSERT_MESSAGE("Conflict URL should not be empty", !conflictUrl.isEmpty());

    const QString debugUrl = theme->debugReporterUrl();
    CPPUNIT_ASSERT_MESSAGE("Debug URL should not be empty", !debugUrl.isEmpty());

    // Test with different languages
    const QString feedbackEn = theme->feedbackUrl(Language::English);
    const QString feedbackFr = theme->feedbackUrl(Language::French);
    CPPUNIT_ASSERT_MESSAGE("Feedback URLs should differ by language", feedbackEn != feedbackFr);
}

void TestTheme::testSystrayIcons() {
    int argc = 1;
    char *argv[] = {const_cast<char *>("test")};
    QGuiApplication app(argc, argv);

    Theme *const theme = Theme::instance();

    // Test that the theme icon getter returns a valid icon
    QIcon icon = theme->syncStateIcon(SyncStatus::Running, false, false, false);
#if defined(KD_LINUX)
    CPPUNIT_ASSERT_MESSAGE("Icon should be null", icon.isNull());
#else
    CPPUNIT_ASSERT_MESSAGE("Icon should be valid", !icon.isNull());
#endif

    icon = theme->syncStateIcon(SyncStatus::Running, true, false, true);
#if defined(KD_LINUX)
    CPPUNIT_ASSERT_MESSAGE("Icon should be null", icon.isNull());
#else
    CPPUNIT_ASSERT_MESSAGE("Icon should be valid", !icon.isNull());
#endif

    icon = theme->syncStateIcon(SyncStatus::Running, true, true, true);
#if defined(KD_LINUX)
    CPPUNIT_ASSERT_MESSAGE("Icon should be null", icon.isNull());
#else
    CPPUNIT_ASSERT_MESSAGE("Icon should be valid", !icon.isNull());
#endif

    // Test mono icon setting
    theme->setSystrayUseMonoIcons(true);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Mono icons setting should apply", true, theme->systrayUseMonoIcons());

    theme->setSystrayUseMonoIcons(false);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Mono icons setting should toggle", false, theme->systrayUseMonoIcons());
}

void TestTheme::testSharingConfig() {
    const Theme *const theme = Theme::instance();

    CPPUNIT_ASSERT(theme->linkSharing());
    CPPUNIT_ASSERT(!theme->userGroupSharing());
}

} // namespace KDC
