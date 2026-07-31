#include "osupdater_win.h"

#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/utility.h"
#include "manualupdater/httpdownloader.h"

#include <QProcess>
#include <filesystem>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/Path.h>

namespace KDC {
bool OSUpdater::install(const VersionInfo &versionInfo, const std::function<void(InstallStep, const QString &)> &progressCallback,
                        QString &outMessage) {
    // Create a unique download directory.
    SyncPath downloadDir;
    if (!createDownloadDirectory(downloadDir)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to create download directory.");
        outMessage = QObject::tr("Failed to create download directory.");
        return false;
    }

    // Derive installer name from URL.
    std::string installerName;
    try {
        const Poco::URI uri(versionInfo.downloadUrl);
        const Poco::Path path(uri.getPath());
        installerName = path.getFileName();
    } catch (const Poco::Exception &e) {
        LOGW_WARN(Log::instance()->getLogger(), L"Invalid download URL.");
        outMessage = QObject::tr("Invalid download URL.");
        return false;
    }
    const SyncPath filepath = downloadDir / installerName;

    progressCallback(InstallStep::Downloading, QObject::tr("Downloading installer..."));

    if (const auto result = HttpDownloader::downloadFile(versionInfo.downloadUrl, filepath); !result.success) {
        if (result.statusCode == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            LOGW_WARN(Log::instance()->getLogger(), L"Version not found (404).");
            outMessage = QObject::tr("The specified version does not exist or the download failed.");
        } else {
            LOGW_WARN(Log::instance()->getLogger(), L"Download failed: " << CommonUtility::s2ws(result.error));
            outMessage = QObject::tr("Download failed: %1").arg(QString::fromStdString(result.error));
        }
        return false;
    }

    if (std::error_code ec; !std::filesystem::exists(filepath, ec)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Installer file not found after download.");
        outMessage = QObject::tr("Installer file not found after download.");
        return false;
    }

    progressCallback(InstallStep::Verifying, QObject::tr("Verifying file integrity..."));
    if (!versionInfo.checksum.empty() && !verifyFileChecksum(versionInfo, filepath, outMessage)) {
        return false;
    }

    progressCallback(InstallStep::Installing, QObject::tr("Starting installer..."));
    LOGW_INFO(Log::instance()->getLogger(), L"Starting installer " << Utility::formatSyncPath(filepath));

    if (const QString program = QString::fromStdWString(filepath.wstring());
        !QProcess::startDetached(program, QStringList{QStringLiteral("/S"), QStringLiteral("/launch")})) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Failed to launch installer.");
        outMessage = QObject::tr("Failed to launch installer.");
        return false;
    }

    outMessage = QObject::tr("Installer launched successfully.");
    progressCallback(InstallStep::Done, QObject::tr("Done."));
    return true;
}


} // namespace KDC
