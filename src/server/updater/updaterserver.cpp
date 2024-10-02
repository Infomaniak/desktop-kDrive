#include "updaterserver.h"
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

#include "updaterserver.h"
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
#include "sparkleupdater.h"
#endif
#include "kdcupdater.h"
#include "common/utility.h"
#include "version.h"
#include "config.h"
#include "libcommon/theme/theme.h"
#include "libcommonserver/log/log.h"

#include <QUrl>
#include <QUrlQuery>
#include <QProcess>

#include <log4cplus/loggingmacros.h>

namespace KDC {

UpdaterServer *UpdaterServer::_instance = 0;

UpdaterServer *UpdaterServer::instance() {
    if (!_instance) {
        _instance = create();
    }
    return _instance;
}

QUrl UpdaterServer::updateUrl() {
    QUrl updateBaseUrl = QUrl(QLatin1String(APPLICATION_UPDATE_URL));
    if (!updateBaseUrl.isValid() || updateBaseUrl.host() == ".") {
        return QUrl();
    }

    auto urlQuery = getQueryParams();

#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    urlQuery.addQueryItem(QLatin1String("sparkle"), QLatin1String("true"));
#endif

#if defined(Q_OS_WIN)
    urlQuery.addQueryItem(QLatin1String("msi"), QLatin1String("true"));
#endif

    updateBaseUrl.setQuery(urlQuery);

    return updateBaseUrl;
}

QString UpdaterServer::version() const {
    QString version;

#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    version = qobject_cast<SparkleUpdater *>(instance())->version();
#else
    version = qobject_cast<KDCUpdater *>(instance())->updateVersion();
#endif

    return version;
}

bool UpdaterServer::isKDCUpdater() {
    KDCUpdater *kdcupdater = qobject_cast<KDCUpdater *>(instance());
    return (kdcupdater != nullptr);
}

bool UpdaterServer::isSparkleUpdater() {
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    SparkleUpdater *sparkleUpdater = qobject_cast<SparkleUpdater *>(instance());
    return (sparkleUpdater != nullptr);
#else
    return false;
#endif
}

QString UpdaterServer::statusString() const {
    return instance()->statusString();
}

bool UpdaterServer::downloadCompleted() const {
    KDCUpdater *kdcupdater = qobject_cast<KDCUpdater *>(instance());
    if (kdcupdater) {
        return kdcupdater->downloadState() == KDCUpdater::DownloadComplete ||
               kdcupdater->downloadState() == KDCUpdater::LastVersionSkipped;
    }

    return false;
}

bool UpdaterServer::updateFound() const {
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    SparkleUpdater *sparkleUpdater = qobject_cast<SparkleUpdater *>(instance());
    if (sparkleUpdater) {
        return sparkleUpdater->updateFound();
    }
#endif

    return false;
}

void UpdaterServer::startInstaller() const {
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    SparkleUpdater *updater = qobject_cast<SparkleUpdater *>(instance());
    if (updater) {
        updater->slotStartInstaller();
    }
#elif defined(Q_OS_WIN32)
    KDCUpdater *updater = qobject_cast<KDCUpdater *>(instance());
    if (updater) {
        updater->slotStartInstaller();
    }
#endif
}


QUrlQuery UpdaterServer::getQueryParams() {
    QUrlQuery query;
    Theme *theme = Theme::instance();
    QString platform = QLatin1String("stranger");
    bool useTest = QProcessEnvironment::systemEnvironment().value("KDRIVE_PREPROD_UPDATE") == "1";
    if (OldUtility::isLinux()) {
#ifdef __arm__
        platform = QLatin1String("linux-arm");
#elif __x86_x64__
        platform = QLatin1String("linux-amd");
#endif
    } else if (OldUtility::isBSD()) {
        platform = QLatin1String("bsd");
    } else if (OldUtility::isWindows()) {
        platform = useTest ? QLatin1String("win32test") : QLatin1String("windows");
    } else if (OldUtility::isMac()) {
        platform = useTest ? QLatin1String("macostest") : QLatin1String("macos");
    }

    QString sysInfo = getSystemInfo();
    if (!sysInfo.isEmpty()) {
        query.addQueryItem(QLatin1String("client"), sysInfo);
    }
    query.addQueryItem(QLatin1String("version"), clientVersion());
    query.addQueryItem(QLatin1String("platform"), platform);

    QString suffix = QString::fromLatin1(KDRIVE_STRINGIFY(KDRIVE_VERSION_SUFFIX));
    query.addQueryItem(QLatin1String("versionsuffix"), suffix);

    return query;
}


QString UpdaterServer::getSystemInfo() {
#ifdef Q_OS_LINUX
    QProcess process;
    process.start(QLatin1String("lsb_release -a"), QStringList());
    process.waitForFinished();
    QByteArray output = process.readAllStandardOutput();
    LOG_DEBUG(Log::instance()->getLogger(), "Sys Info size: " << output.length());
    if (output.length() > 1024) output.clear(); // don't send too much.

    return QString::fromLocal8Bit(output.toBase64());
#else
    return QString();
#endif
}

// To test, cmake with -DAPPLICATION_UPDATE_URL="http://127.0.0.1:8080/test.rss"
UpdaterServer *UpdaterServer::create() {
    auto url = updateUrl();
    LOG_DEBUG(Log::instance()->getLogger(), url.toString().toStdString().c_str());
    if (url.isEmpty()) {
        LOG_WARN(Log::instance()->getLogger(), "Not a valid updater URL, will not do update check");
        return nullptr;
    }

#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    return new SparkleUpdater(url);
#elif defined(Q_OS_WIN32)
    // Also for MSI
    return new NSISUpdater(url);
#else
    // the best we can do is notify about updates
    return new PassiveUpdateNotifier(url);
#endif
}


qint64 UpdaterServer::Helper::versionToInt(qint64 major, qint64 minor, qint64 patch, qint64 build) {
    return major << 56 | minor << 48 | patch << 40 | build;
}

qint64 UpdaterServer::Helper::currentVersionToInt() {
    return versionToInt(KDRIVE_VERSION_MAJOR, KDRIVE_VERSION_MINOR, KDRIVE_VERSION_PATCH, KDRIVE_VERSION_BUILD);
}

qint64 UpdaterServer::Helper::stringVersionToInt(const QString &version) {
    if (version.isEmpty()) return 0;
    QByteArray baVersion = version.toLatin1();
    int major = 0, minor = 0, patch = 0, build = 0;
    sscanf(baVersion, "%d.%d.%d.%d", &major, &minor, &patch, &build);
    return versionToInt(major, minor, patch, build);
}

QString UpdaterServer::clientVersion() {
    return QString::fromLatin1(KDRIVE_STRINGIFY(KDRIVE_VERSION_FULL));
}

} // namespace KDC
