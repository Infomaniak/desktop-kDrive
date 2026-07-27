#include "osupdater_mac.h"

#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"
#include "manualupdater/httpdownloader.h"

#include <QProcess>
#include <QXmlStreamReader>
#include <QFile>
#include <filesystem>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/Path.h>

namespace KDC {

static bool runOsascriptDelete(const QString &posixPath) {
    QProcess p;
    p.setProgram(QStringLiteral("/usr/bin/osascript"));
    p.setArguments(QStringList{QStringLiteral("-e"),
                               QStringLiteral("tell application \"Finder\" to delete POSIX file \"%1\"").arg(posixPath)});
    p.start();
    if (!p.waitForFinished(10000)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"osascript timed out deleting: " << CommonUtility::s2ws(posixPath.toStdString()));
        return false;
    }
    if (p.exitCode() != 0) {
        const auto err = QString::fromUtf8(p.readAllStandardError());
        LOGW_WARN(Log::instance()->getLogger(), L"osascript delete failed for: " << CommonUtility::s2ws(posixPath.toStdString())
                                                                                 << L" — " << err.toStdWString());
    }
    return true;
}

bool OSUpdater::install(const VersionInfo &versionInfo, const std::function<void(int32_t, QString)> &progressCallback,
                        QString &outMessage) {
    const auto &appcastUrl = versionInfo.downloadUrl;
    if (appcastUrl.empty()) {
        outMessage = QObject::tr("Download URL is empty.");
        return false;
    }

    // Create a shared unique download directory for appcast + pkg.
    SyncPath downloadDir;
    if (!createDownloadDirectory(downloadDir)) {
        outMessage = QObject::tr("Failed to create download directory.");
        return false;
    }

    progressCallback(10, QObject::tr("Downloading appcast..."));

    QString pkgUrl;
    if (!downloadAndParseAppcast(appcastUrl, downloadDir, pkgUrl, outMessage)) {
        return false;
    }

    QString pkgFilename;
    try {
        const Poco::URI uri(pkgUrl.toStdString());
        const Poco::Path path(uri.getPath());
        pkgFilename = QString::fromStdString(path.getFileName());
    } catch (const Poco::Exception &e) {
        outMessage = QObject::tr("Invalid package URL: %1").arg(QString::fromStdString(e.displayText()));
        return false;
    }
    if (pkgFilename.isEmpty()) {
        outMessage = QObject::tr("Could not determine package filename.");
        return false;
    }

    const QString pkgPath = QString::fromStdString(downloadDir.string()) + QStringLiteral("/") + pkgFilename;

    progressCallback(40, QObject::tr("Downloading package..."));

    const auto result = HttpDownloader::downloadFile(pkgUrl.toStdString(), SyncPath(pkgPath.toStdString()));
    if (!result.success) {
        if (result.statusCode == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            outMessage = QObject::tr("The specified version does not exist or the download failed.");
        } else {
            outMessage = QObject::tr("Failed to download package: %1").arg(QString::fromStdString(result.error));
        }
        return false;
    }

    if (!std::filesystem::exists(SyncPath(pkgPath.toStdString()))) {
        outMessage = QObject::tr("Package file not found after download.");
        return false;
    }

    progressCallback(55, QObject::tr("Verifying file integrity..."));
    if (!versionInfo.checksum.empty() && !verifyFileChecksum(versionInfo, SyncPath(pkgPath.toStdString()), outMessage)) {
        return false;
    }

    progressCallback(85, QObject::tr("Removing old application..."));
    if (std::filesystem::exists("/Applications/kDrive/kDrive Uninstaller.app")) {
        (void) runOsascriptDelete(QStringLiteral("/Applications/kDrive/kDrive Uninstaller.app"));
    }
    if (std::filesystem::exists("/Applications/kDrive/kDrive.app")) {
        (void) runOsascriptDelete(QStringLiteral("/Applications/kDrive/kDrive.app"));
    }
    if (std::filesystem::exists("/Applications/kDrive")) {
        (void) runOsascriptDelete(QStringLiteral("/Applications/kDrive"));
    }

    progressCallback(95, QObject::tr("Opening installer..."));
    if (!QProcess::startDetached(QStringLiteral("open"), QStringList{pkgPath})) {
        outMessage = QObject::tr("Failed to open installer. Please install manually: %1").arg(pkgPath);
        return false;
    }

    outMessage = QObject::tr("Installer opened: %1").arg(pkgPath);
    progressCallback(100, QObject::tr("Done."));
    return true;
}

bool OSUpdater::downloadAndParseAppcast(const std::string &appcastUrl, const SyncPath &baseDir, QString &outPkgUrl,
                                        QString &outMessage) {
    const SyncPath appcastXmlPath = baseDir / "appcast.xml";

    if (const auto result = HttpDownloader::downloadFile(appcastUrl, appcastXmlPath); !result.success) {
        if (result.statusCode == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            outMessage = QObject::tr("The specified version does not exist or the download failed.");
        } else {
            outMessage = QObject::tr("Failed to download appcast: %1").arg(QString::fromStdString(result.error));
        }
        return false;
    }

    QFile file(QString::fromStdString(appcastXmlPath.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        outMessage = QObject::tr("Failed to read appcast.");
        return false;
    }

    QXmlStreamReader reader(&file);
    while (!reader.atEnd() && !reader.hasError()) {
        (void) reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("enclosure")) {
            const auto attrs = reader.attributes();
            // Prefer the direct .pkg downloadUrl if present
            QString url = attrs.value(QStringLiteral("downloadUrl")).toString();
            if (url.isEmpty()) {
                url = attrs.value(QStringLiteral("url")).toString();
            }
            if (!url.isEmpty()) {
                outPkgUrl = url;
                break;
            }
        }
    }
    file.close();

    if (outPkgUrl.isEmpty()) {
        outMessage = QObject::tr("Could not find download link in appcast.");
        return false;
    }

    return true;
}

} // namespace KDC
