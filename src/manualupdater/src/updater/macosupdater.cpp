#include "macosupdater.h"
#include "httpdownloader.h"

#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"

#include <QProcess>
#include <QXmlStreamReader>
#include <QFile>
#include <filesystem>
#include <cstdlib>

namespace KDUpdater {

bool MacOSUpdater::install(const KDC::VersionInfo &versionInfo, const std::string &desiredVersion,
                           std::function<void(int, QString)> progressCallback, QString &outMessage) {
    (void) desiredVersion; // the XML URL already encodes the version

    const auto &appcastUrl = versionInfo.downloadUrl;
    if (appcastUrl.empty()) {
        outMessage = QObject::tr("Download URL is empty.");
        return false;
    }

    progressCallback(10, QObject::tr("Downloading appcast..."));

    QString pkgUrl;
    if (!downloadAndParseAppcast(appcastUrl, pkgUrl, outMessage)) {
        return false;
    }

    KDC::SyncPath tmpDir;
    if (const auto exitInfo = KDC::CommonUtility::deviceTempDirectoryPath(tmpDir); !exitInfo) {
        outMessage = QObject::tr("Failed to get temp directory.");
        return false;
    }

    const auto pkgFilename = pkgUrl.mid(pkgUrl.lastIndexOf('/') + 1);
    if (pkgFilename.isEmpty()) {
        outMessage = QObject::tr("Could not determine package filename.");
        return false;
    }

    const QString pkgPath = QString::fromStdString(tmpDir.string()) + QStringLiteral("/") + pkgFilename;
    std::filesystem::remove(KDC::SyncPath(pkgPath.toStdString()));

    progressCallback(40, QObject::tr("Downloading package..."));

    const auto result = HttpDownloader::downloadFile(pkgUrl.toStdString(), KDC::SyncPath(pkgPath.toStdString()));
    if (!result.success) {
        if (result.statusCode == 404) {
            outMessage = QObject::tr("The specified version does not exist or the download failed.");
        } else {
            outMessage = QObject::tr("Failed to download package: %1").arg(QString::fromStdString(result.error));
        }
        return false;
    }

    if (!std::filesystem::exists(KDC::SyncPath(pkgPath.toStdString()))) {
        outMessage = QObject::tr("Package file not found after download.");
        return false;
    }

    progressCallback(70, QObject::tr("Removing old application..."));
    std::system(
            "osascript -e 'tell application \"Finder\" to delete POSIX file "
            "\"/Applications/kDrive/kDrive Uninstaller.app\"'");
    std::system(
            "osascript -e 'tell application \"Finder\" to delete POSIX file "
            "\"/Applications/kDrive/kDrive.app\"'");
    std::system(
            "osascript -e 'tell application \"Finder\" to delete POSIX file "
            "\"/Applications/kDrive\"'");

    progressCallback(90, QObject::tr("Opening installer..."));
    if (!QProcess::startDetached(QStringLiteral("open"), QStringList{pkgPath})) {
        outMessage = QObject::tr("Failed to open installer. Please install manually: %1").arg(pkgPath);
        return false;
    }

    outMessage = QObject::tr("Installer opened: %1").arg(pkgPath);
    progressCallback(100, QObject::tr("Done."));
    return true;
}

bool MacOSUpdater::downloadAndParseAppcast(const std::string &appcastUrl, QString &outPkgUrl, QString &outMessage) {
    KDC::SyncPath tmpDir;
    if (const auto exitInfo = KDC::CommonUtility::deviceTempDirectoryPath(tmpDir); !exitInfo) {
        outMessage = QObject::tr("Failed to get temp directory.");
        return false;
    }

    const KDC::SyncPath appcastXmlPath = tmpDir / "appcast.xml";
    std::filesystem::remove(appcastXmlPath);

    const auto result = HttpDownloader::downloadFile(appcastUrl, appcastXmlPath);
    if (!result.success) {
        if (result.statusCode == 404) {
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
        reader.readNext();
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

} // namespace KDUpdater
