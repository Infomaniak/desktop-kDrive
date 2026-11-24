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

#include "theme.h"
#include "config.h"
#include "version.h"
#include "utility/utility.h"

#include <QIcon>
#include <QSslSocket>

namespace KDC {

Theme *Theme::instance() {
    return &_instance;
}

Theme::~Theme() {}

std::string Theme::appName() const {
    return APPLICATION_NAME;
}

std::string Theme::version() const {
    return KDRIVE_VERSION_STRING;
}

QIcon Theme::applicationIcon() const {
    QIcon icon;
    for (const auto size: {16, 32, 128, 256, 512}) {
        icon.addFile(QStringLiteral(":/client/theme/colored/" APPLICATION_ICON_NAME "-icon-%1.png").arg(size), QSize(size, size));
    }
    return icon;
}

Theme::Theme() :
    QObject(nullptr) {}

QString Theme::feedbackUrl(const Language language) {
    auto resolvedLanguage = language;
    if (resolvedLanguage == Language::Default) {
        resolvedLanguage = CommonUtility::strToLanguage(CommonUtility::languageCode(Language::Default));
    }

    switch (resolvedLanguage) {
        case Language::French:
            return FEEDBACK_FR_URL;
        case Language::German:
            return FEEDBACK_DE_URL;
        case Language::Spanish:
            return FEEDBACK_ES_URL;
        case Language::Italian:
            return FEEDBACK_IT_URL;
        case Language::English:
        case Language::Dutch:
        case Language::Swedish:
        case Language::Portuguese:
        case Language::Polish:
        case Language::Norwegian:
        case Language::Finnish:
        case Language::Danish:
        case Language::Greek:
        case Language::Default:
        case Language::EnumEnd:
        default:
            break;
    }
    return FEEDBACK_EN_URL;
}

QString Theme::versionSwitchOutput() const {
    QString helpText;
    QTextStream stream(&helpText);
    stream << QString::fromStdString(appName()) << QLatin1String(" version ") << QString::fromStdString(version()) << Qt::endl;
    stream << "Using Qt " << qVersion() << ", built against Qt " << QT_VERSION_STR << Qt::endl;
    stream << "Using '" << QSslSocket::sslLibraryVersionString() << "'" << Qt::endl;

    return helpText;
}

} // namespace KDC
