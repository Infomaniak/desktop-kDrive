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

#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include "common/utility.h"
#include "libcommon/utility/types.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

#include <Poco/Timestamp.h>
#include <Poco/File.h>
#include <Poco/Exception.h>

#include <QDir>
#include <QStandardPaths>

namespace KDC {

namespace {
// Returns the autostart directory the linux way
// and respects the XDG_CONFIG_HOME env variable
QString getUserAutostartDir() {
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    config += QLatin1String("/autostart/");
    return config;
}
} // namespace

bool OldUtility::hasLaunchOnStartup(const QString &appName, log4cplus::Logger /*logger*/) {
    QString desktopFileLocation = getUserAutostartDir() + appName + QLatin1String(".desktop");
    return QFile::exists(desktopFileLocation);
}

void OldUtility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable, log4cplus::Logger logger) {
    QString userAutoStartPath = getUserAutostartDir();
    QString desktopFileLocation = userAutoStartPath + appName + QLatin1String(".desktop");
    if (enable) {
        if (!QDir().exists(userAutoStartPath) && !QDir().mkpath(userAutoStartPath)) {
            LOG4CPLUS_WARN(logger, "Could not create autostart folder" << userAutoStartPath.toStdString().c_str());
            return;
        }
        QFile iniFile(desktopFileLocation);
        if (!iniFile.open(QIODevice::WriteOnly)) {
            LOG4CPLUS_WARN(logger, "Could not write auto start entry" << desktopFileLocation.toStdString().c_str());
            return;
        }
        QByteArray appimageDir = qgetenv("APPIMAGE");
        LOG4CPLUS_DEBUG(logger, "APPIMAGE=" << appimageDir.toStdString().c_str());
        QTextStream ts(&iniFile);
        ts.setEncoding(QStringConverter::Utf8);
        ts << QLatin1String("[Desktop Entry]") << Qt::endl
           << QLatin1String("Name=") << guiName << Qt::endl
           << QLatin1String("GenericName=") << QLatin1String("File Synchronizer") << Qt::endl
           << QLatin1String("Exec=\"") << appimageDir << "\"" << Qt::endl
           << QLatin1String("Terminal=") << "false" << Qt::endl
           << QLatin1String("Icon=") << appName.toLower() << Qt::endl // always use lowercase for icons
           << QLatin1String("Categories=") << QLatin1String("Network") << Qt::endl
           << QLatin1String("Type=") << QLatin1String("Application") << Qt::endl
           << QLatin1String("StartupNotify=") << "false" << Qt::endl
           << QLatin1String("X-GNOME-Autostart-enabled=") << "true" << Qt::endl
           << QLatin1String("X-GNOME-Autostart-Delay=10") << Qt::endl;
    } else {
        if (!QFile::remove(desktopFileLocation)) {
            LOG4CPLUS_WARN(logger, "Could not remove autostart desktop file");
        }
    }
}

bool OldUtility::hasSystemLaunchOnStartup(const QString &appName, log4cplus::Logger logger) {
    Q_UNUSED(appName)
    Q_UNUSED(logger)
    return false;
}

} // namespace KDC
