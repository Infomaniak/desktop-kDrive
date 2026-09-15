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

#include "libcommon/utility/types.h"

#include <config.h>
#include <QColor>
#include <QString>
#include <QUrl>

#include <cstdint>

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

namespace KDC::AppConstants::Support {

[[nodiscard]] inline QUrl helpUri() {
    return QUrl{QStringLiteral("https://support.infomaniak.com/")};
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
} // namespace KDC::AppConstants::Settings
