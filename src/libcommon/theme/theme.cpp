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
#include <QOperatingSystemVersion>
#include <QPainter>
#include <QSslSocket>

namespace KDC {

Theme *Theme::instance() {
    return &_instance;
}

Theme::~Theme() {}

std::string Theme::appName() const {
    return APPLICATION_NAME;
}

std::string Theme::appClientName() const {
    return std::string(APPLICATION_NAME) + "_client";
}

std::string Theme::version() const {
    return KDRIVE_VERSION_STRING;
}

QIcon Theme::applicationIcon() const {
    return themeIcon(QStringLiteral(APPLICATION_ICON_NAME "-icon"));
}

QIcon Theme::themeIcon(const QString &name, bool sysTray, bool sysTrayMenuVisible) const {
    Q_UNUSED(sysTrayMenuVisible)
    QString osType;
    QString flavor;

    osType = "windows";
#if defined(KD_MACOS)
    osType = "mac";
#endif

    if (sysTray) {
        if (_mono) {
            flavor = CommonUtility::hasDarkSystray() ? QString("white") : QString("black");
#if defined(KD_MACOS)
            if (QOperatingSystemVersion::current() > QOperatingSystemVersion::MacOSCatalina) {
                flavor = QString("black");
            }
#endif
        } else {
            flavor = QString("colored");
        }
    } else {
        flavor = QString("colored");
    }

    QString key = name + "," + flavor;
    QIcon &cached = _iconCache[key];
    if (cached.isNull()) {
        QString pixmapName = QString(":/client/resources/icons/theme/%1/%2/%3.svg").arg(osType, flavor, name);
        if (QFile::exists(pixmapName)) {
            QList<int> sizes;
            sizes << 16 << 22 << 32 << 48 << 64 << 128 << 256 << 512 << 1024;
            foreach (int size, sizes) {
                cached.addPixmap(QIcon(pixmapName).pixmap(QSize(size, size)));
            }
        }
    }

#ifdef Q_OS_MAC
    // This defines the icon as a template and enables automatic macOS color handling
    // See https://bugreports.qt.io/browse/QTBUG-42109
    cached.setIsMask(_mono && sysTray);
#endif

    return cached;
}

void Theme::updateIconWithText(QIcon &icon, QString text) const {
    const QList<QSize> &sizes = icon.availableSizes();
    for (const QSize &size: sizes) {
        QPixmap px = icon.pixmap(size);
        QPainter painter(&px);
        const int pictSize = size.width() / 2;
        QRect rect(size.width() - pictSize, size.height() - pictSize, pictSize, pictSize);

        // Draw red circle
        painter.setPen(Qt::red);
        painter.setBrush(Qt::red);
        painter.drawEllipse(rect);

        // Draw text
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", pictSize - 2, QFont::Bold));
        painter.drawText(rect, Qt::AlignCenter, text);

        icon.addPixmap(px);
    }
}

Theme::Theme() :
    QObject(nullptr) {}

QString Theme::helpUrl() const {
#ifdef APPLICATION_HELP_URL
    return QString::fromLatin1(APPLICATION_HELP_URL);
#else
    return QString();
#endif
}

QString Theme::feedbackUrl(const Language language) const {
    switch (language) {
        case Language::French:
            return FEEDBACK_FR_URL;
        case Language::German:
            return FEEDBACK_DE_URL;
        case Language::Spanish:
            return FEEDBACK_ES_URL;
        case Language::Italian:
            return FEEDBACK_IT_URL;
        case Language::Default:
        case Language::English:
        case Language::Dutch:
        case Language::Swedish:
        case Language::Portuguese:
        case Language::Polish:
        case Language::Norwegian:
        case Language::Finnish:
        case Language::Danish:
        case Language::Greek:
        case Language::EnumEnd:
        default:
            break;
    }
    return FEEDBACK_EN_URL;
}

QString Theme::conflictHelpUrl() const {
#ifdef APPLICATION_CONFLICT_HELP_URL
    return QString::fromLatin1(APPLICATION_CONFLICT_HELP_URL);
#else
    return QString();
#endif
}

void Theme::setSystrayUseMonoIcons(bool mono) {
    _mono = mono;
    emit systrayUseMonoIconsChanged(mono);
}

bool Theme::systrayUseMonoIcons() const {
    return _mono;
}

QString Theme::debugReporterUrl() const {
    return DEBUGREPORTER_SUBMIT_URL;
}

QIcon Theme::syncStateIcon(KDC::SyncStatus status, bool sysTray, bool sysTrayMenuVisible, bool alert) const {
    // FIXME: Mind the size!
    QString statusIcon;

    switch (status) {
        case KDC::SyncStatus::Undefined:
            // this can happen if no sync connections are configured.
            statusIcon = QLatin1String("state-information");
            break;
        case KDC::SyncStatus::Starting:
        case KDC::SyncStatus::Running:
            statusIcon = QLatin1String("state-sync");
            break;
        case KDC::SyncStatus::Idle:
            statusIcon = QLatin1String("state-ok");
            break;
        case KDC::SyncStatus::PauseAsked:
        case KDC::SyncStatus::Paused:
        case KDC::SyncStatus::StopAsked:
        case KDC::SyncStatus::Stopped:
            statusIcon = QLatin1String("state-pause");
            break;
        case KDC::SyncStatus::Error:
            statusIcon = QLatin1String("state-error");
            break;
        default:
            statusIcon = QLatin1String("state-error");
    }

    QIcon icon = themeIcon(statusIcon, sysTray, sysTrayMenuVisible);

    if (sysTray && alert) updateIconWithText(icon, QString("!"));

    return icon;
}


QIcon Theme::folderOfflineIcon(bool sysTray, bool sysTrayMenuVisible) const {
    return themeIcon(QLatin1String("state-pause"), sysTray, sysTrayMenuVisible);
}

bool Theme::linkSharing() const {
    return true;
}

bool Theme::userGroupSharing() const {
    return false;
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
