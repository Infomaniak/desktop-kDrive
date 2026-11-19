/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#pragma once

#include "libcommon/data/drive.h"
#include "libcommon/utility/types.h"

#include <config.h>
#include <QColor>
#include <QLocale>
#include <QString>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#ifndef KDRIVE_QML_MODULE_URI
#error "KDRIVE_QML_MODULE_URI is not defined, check target_compile_definitions in src/gui4/linux/CMakeLists.txt"
#endif

namespace KDC::AppConstants::Qml {

// URI of the QML module, defined by CMake so C++ and qt_add_qml_module() can never disagree.
inline constexpr char moduleUri[] = KDRIVE_QML_MODULE_URI;

} // namespace KDC::AppConstants::Qml

namespace KDC::AppConstants::Drive {

[[nodiscard]] inline QColor defaultColor() {
    return QColor{QStringLiteral("#0098FF")};
}

// Standalone kDrive offers, as opposed to kSuite and my kSuite ones. The API names them without the product ("Team").
[[nodiscard]] inline bool isStandaloneDrivePack(const std::string_view packName) {
    constexpr std::array<std::string_view, 4> standaloneDrivePacks{"free", "solo", "team", "pro"};
    return std::ranges::find(standaloneDrivePacks, packName) != standaloneDrivePacks.end();
}

// Language of the Infomaniak shop pages: the application language when the site offers it, English otherwise.
[[nodiscard]] inline QString shopLanguageCode() {
    constexpr std::array<std::string_view, 7> shopLanguages{"en", "fr", "de", "es", "it", "pt", "nl"};
    const QString languageCode = QLocale().name().left(2);
    const bool supported = std::ranges::find(shopLanguages, languageCode.toStdString()) != shopLanguages.end();
    return supported ? languageCode : QStringLiteral("en");
}

// Shop page offering more storage for `drive`, chosen from its pack. It also renews a drive that is not renewed.
[[nodiscard]] inline QUrl upsellUri(const KDC::Drive &drive) {
    switch (drive.packInfo().upsellTarget()) {
        case UpsellTarget::StandAloneToMykSuitePlus:
        case UpsellTarget::StandAloneTokSuitePro:
            return QUrl{QStringLiteral("https://shop.infomaniak.com/order/ksuite?migration=drive&drive_id=%1&lang=%2")
                                .arg(static_cast<qulonglong>(drive.driveId()))
                                .arg(shopLanguageCode())};
        case UpsellTarget::kSuiteUpgrade:
            // The kSuite id is missing until the drive info is loaded from the API: fall back to the shop home.
            if (drive.packInfo().kSuiteId() <= 0) break;
            return QUrl{QStringLiteral("https://shop.infomaniak.com/order2/ksuite/%1")
                                .arg(static_cast<qlonglong>(drive.packInfo().kSuiteId()))};
        case UpsellTarget::MykSuiteToMykSuitePlus:
            return QUrl{QStringLiteral("https://shop.infomaniak.com/order/myksuite")};
        case UpsellTarget::Unknown:
        case UpsellTarget::EnumEnd:
            break;
    }
    return QUrl{QStringLiteral("https://shop.infomaniak.com/")};
}

} // namespace KDC::AppConstants::Drive

namespace KDC::AppConstants::Login {

[[nodiscard]] inline QUrl signupUri() {
    return QUrl{QStringLiteral("https://welcome.infomaniak.com/signup")};
}

} // namespace KDC::AppConstants::Login

namespace KDC::AppConstants::Onboarding {

[[nodiscard]] inline QUrl driveOffersUri() {
    return QUrl{QStringLiteral("https://www.infomaniak.com/gtl/myksuite#prices")};
}

[[nodiscard]] inline QUrl freeDriveOrderUri() {
    return QUrl{QStringLiteral("https://shop.infomaniak.com/order/select/drive")};
}

} // namespace KDC::AppConstants::Onboarding

namespace KDC::AppConstants::Storage {

// The sync engine stops syncing below 250 MB of free space; warning earlier gives the user time to react.
inline constexpr uint64_t diskAlmostFullThresholdBytes = 1000ULL * 1000 * 1000;
inline constexpr int32_t freeSpacePollIntervalMs = 60 * 1000;

} // namespace KDC::AppConstants::Storage

namespace KDC::AppConstants::Updater {

// Quiet period without a visible window before restarting on an installed update: a window closed to be reopened right
// away does not trigger the restart.
inline constexpr int32_t restartQuietDelayMs = 30 * 1000;

} // namespace KDC::AppConstants::Updater

namespace KDC::AppConstants::Matomo {
// Matomo site of the kDrive v4 desktop application; the legacy client reports to MATOMO_SITE_ID.
inline constexpr int32_t siteId = 41;
// Visit-scoped custom dimension of site 41 holding the app version, as major.minor.patch.
inline constexpr int32_t appVersionDimensionId = 1;
// Page views and events are reported under kdrive://<category>.
inline constexpr char actionUrlBase[] = "kdrive://";

[[nodiscard]] inline QUrl trackingEndpointUri() {
    return QUrl{QString::fromLatin1(MATOMO_URL) + QStringLiteral("/matomo.php")};
}
} // namespace KDC::AppConstants::Matomo

namespace KDC::AppConstants::Support {

[[nodiscard]] inline QUrl helpUri() {
    return QUrl{QStringLiteral("https://support.infomaniak.com/")};
}

// FAQ article presenting the new kDrive app, opened from the Home banner shown after an update from a version before 4.
[[nodiscard]] inline QUrl v4OnboardingFaqUri() {
    return QUrl{QStringLiteral("https://faq.infomaniak.com/2923")};
}

} // namespace KDC::AppConstants::Support

namespace KDC::AppConstants::WebDrive {
Q_NAMESPACE

// Exposed to QML as `WebDrive` (registered in AppClientLinux::setupQmlEngine).
enum class Destination : uint8_t {
    Favorites = 0,
    Shared,
    OnlineDrive,
    Trash,
};
Q_ENUM_NS(Destination)

[[nodiscard]] inline QUrl destinationUri(const DriveId driveId, const Destination destination) {
    using Qt::StringLiterals::operator""_s;

    // TODO manage Custom brand here.

    QString path;
    switch (destination) {
        case Destination::Favorites:
            path = u"favorites"_s;
            break;
        case Destination::Shared:
            path = u"shared-with-me"_s;
            break;
        case Destination::OnlineDrive:
            path = u"files"_s;
            break;
        case Destination::Trash:
            path = u"trash"_s;
            break;
    }

    return QUrl{u"https://kdrive.infomaniak.com/app/drive/%1/%2"_s.arg(static_cast<qulonglong>(driveId)).arg(path)};
}

// Opens a file or folder in the web app, which resolves its location from its node id (same URL as macOS and Windows).
[[nodiscard]] inline QUrl itemRedirectUri(const DriveId driveId, const NodeId &nodeId) {
    using Qt::StringLiterals::operator""_s;

    return QUrl{u"https://kdrive.infomaniak.com/app/drive/%1/redirect/%2"_s.arg(static_cast<qulonglong>(driveId))
                        .arg(QString::fromStdString(nodeId))};
}

} // namespace KDC::AppConstants::WebDrive

namespace KDC::AppConstants::Settings {
[[nodiscard]] inline QUrl downloadUri() {
    return QUrl{QString::fromLatin1(APPLICATION_DOWNLOAD_URL)};
}
[[nodiscard]] inline QUrl trashHelpUri() {
    return QUrl{QString::fromLatin1(LEARNMORE_MOVE_TO_TRASH_URL)};
}
[[nodiscard]] inline QUrl licenseUri() {
    return QUrl{QStringLiteral("https://github.com/Infomaniak/desktop-kDrive/blob/develop/LICENSE")};
}
[[nodiscard]] inline QUrl sourcesUri() {
    return QUrl{QStringLiteral("https://github.com/Infomaniak/desktop-kDrive")};
}
// License notices of the bundled components, relative to the executable folder (AppImage usr/bin to usr/share/doc).
inline constexpr auto thirdPartyLicensesRelativePath = "../share/doc/kDrive";
} // namespace KDC::AppConstants::Settings

namespace KDC::AppConstants::SyncConfiguration {
// Largest blacklist the kDrive API accepts: the daemon sends it as `without_ids` with every listing request.
inline constexpr qsizetype maxExcludedFolders = 3000;
// Node id of a drive's root folder in the kDrive API, as the macOS and Windows clients use it.
inline constexpr char driveRootNodeId[] = "1";
} // namespace KDC::AppConstants::SyncConfiguration
