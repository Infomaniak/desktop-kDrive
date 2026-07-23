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

#include <QColor>
#include <QString>
#include <QUrl>

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
