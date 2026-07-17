#include "macosupdater.h"
#include "httpdownloader.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"

#include <QProcess>
#include <QXmlStreamReader>
#include <QFile>
#include <filesystem>

namespace KDC {

static bool runOsascriptDelete(const QString &posixPath, QString &outMessage) {
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
        LOGW_WARN(Log::instance()->getLogger(), L"osascript delete failed for: "
                                                             << CommonUtility::s2ws(posixPath.toStdString()) << L" — "
                                                             << err.toStdWString());
    }
    return true;
}

bool MacOSUpdater::install(const VersionInfo &versionInfo, const std::string &desiredVersion,
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

    SyncPath tmpDir;
    if (const auto exitInfo = CommonUtility::deviceTempDirectoryPath(tmpDir); !exitInfo) {
        outMessage = QObject::tr("Failed to get temp directory.");
        return false;
    }

    const auto pkgFilename = pkgUrl.mid(pkgUrl.lastIndexOf('/') + 1);
    if (pkgFilename.isEmpty()) {
        outMessage = QObject::tr("Could not determine package filename.");
        return false;
    }

    const QString pkgPath = QString::fromStdString(tmpDir.string()) + QStringLiteral("/") + pkgFilename;
    std::filesystem::remove(SyncPath(pkgPath.toStdString()));

    progressCallback(40, QObject::tr("Downloading package..."));

    const auto result = HttpDownloader::downloadFile(pkgUrl.toStdString(), SyncPath(pkgPath.toStdString()));
    if (!result.success) {
        if (result.statusCode == 404) {
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
    if (!versionInfo.checksum.empty()) {
        if (!verifyFileChecksum(versionInfo, SyncPath(pkgPath.toStdString()), outMessage)) {
            return false;
        }
    }

    progressCallback(70, QObject::tr("Verifying digital signature..."));
    if (!verifyPackageSignature(SyncPath(pkgPath.toStdString()), outMessage)) {
        return false;
    }
    progressCallback(85, QObject::tr("Removing old application..."));
    if (std::filesystem::exists("/Applications/kDrive/kDrive Uninstaller.app")) {
        (void) runOsascriptDelete(QStringLiteral("/Applications/kDrive/kDrive Uninstaller.app"), outMessage);
    }
    if (std::filesystem::exists("/Applications/kDrive/kDrive.app")) {
        (void) runOsascriptDelete(QStringLiteral("/Applications/kDrive/kDrive.app"), outMessage);
    }
    if (std::filesystem::exists("/Applications/kDrive")) {
        (void) runOsascriptDelete(QStringLiteral("/Applications/kDrive"), outMessage);
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

bool MacOSUpdater::downloadAndParseAppcast(const std::string &appcastUrl, QString &outPkgUrl, QString &outMessage) {
    SyncPath tmpDir;
    if (const auto exitInfo = CommonUtility::deviceTempDirectoryPath(tmpDir); !exitInfo) {
        outMessage = QObject::tr("Failed to get temp directory.");
        return false;
    }

    const SyncPath appcastXmlPath = tmpDir / "appcast.xml";
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

bool MacOSUpdater::verifyPackageSignature(const SyncPath &pkgPath, QString &outMessage) {
    QProcess process;
    process.setProgram(QStringLiteral("pkgutil"));
    process.setArguments(QStringList{QStringLiteral("--check-signature"), QString::fromStdString(pkgPath.string())});
    process.start();
    if (!process.waitForFinished(30000)) {
        LOGW_WARN(Log::instance()->getLogger(), L"pkgutil --check-signature timed out.");
        outMessage = QObject::tr("Signature verification timed out.");
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(pkgPath, ioError);
        return false;
    }

    const int exitCode = process.exitCode();
    if (exitCode != 0) {
        LOGW_ERROR(Log::instance()->getLogger(), L"pkgutil --check-signature failed with exit code " << exitCode);
        outMessage = QObject::tr("Digital signature verification failed.");
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(pkgPath, ioError);
        return false;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (!output.contains(QStringLiteral("Status: signed by"))) {
        LOGW_ERROR(Log::instance()->getLogger(),
                   L"Package is not signed. Output: " << CommonUtility::s2ws(output.toStdString()));
        outMessage = QObject::tr("Digital signature verification failed.");
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(pkgPath, ioError);
        return false;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Package signature verification passed.");
    return true;
}

} // namespace KDC
