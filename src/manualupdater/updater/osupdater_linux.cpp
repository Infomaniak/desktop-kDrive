#include "osupdater_linux.h"
#include "httpdownloader.h"

#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"

#include <QProcess>
#include <filesystem>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/Path.h>

namespace KDC {

bool OSUpdater::install(const VersionInfo &versionInfo, const std::function<void(int32_t, QString)> &progressCallback,
                        QString &outMessage) {
    const auto &urlStr = versionInfo.downloadUrl;
    if (urlStr.empty()) {
        LOG_ERROR(Log::instance()->getLogger(), "Download URL is empty.");
        outMessage = QObject::tr("Download URL is empty.");
        return false;
    }

    SyncPath destDir;
    if (!createDownloadDirectory(destDir)) {
        LOG_ERROR(Log::instance()->getLogger(), "Failed to create download directory.");
        outMessage = QObject::tr("Failed to create download directory.");
        return false;
    }

    std::string filename;
    try {
        const Poco::URI uri(urlStr);
        const Poco::Path path(uri.getPath());
        filename = path.getFileName();
    } catch (const Poco::Exception &e) {
        LOG_ERROR(Log::instance()->getLogger(), "Invalid download URL.");
        outMessage = QObject::tr("Invalid download URL.");
        return false;
    }
    const SyncPath destPath = destDir / filename;

    progressCallback(30, QObject::tr("Downloading installer..."));

    if (const auto result = HttpDownloader::downloadFile(urlStr, destPath); !result.success) {
        if (result.statusCode == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
            LOGW_WARN(Log::instance()->getLogger(), L"Version not found (404).");
            outMessage = QObject::tr("The specified version does not exist or the download failed.");
        } else {
            LOGW_WARN(Log::instance()->getLogger(), L"Download failed: " << CommonUtility::s2ws(result.error));
            outMessage = QObject::tr("Download failed: %1").arg(QString::fromStdString(result.error));
        }
        return false;
    }

    if (std::error_code ec; !std::filesystem::exists(destPath, ec)) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Downloaded file not found.");
        outMessage = QObject::tr("Downloaded file not found.");
        return false;
    }

    progressCallback(55, QObject::tr("Verifying file integrity..."));
    if (!versionInfo.checksum.empty() && !verifyFileChecksum(versionInfo, destPath, outMessage)) {
        return false;
    }

    progressCallback(70, QObject::tr("Making AppImage executable..."));
    try {
        std::filesystem::permissions(
                destPath,
                std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
                std::filesystem::perm_options::add);
    } catch (const std::filesystem::filesystem_error &e) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to make AppImage executable: " << CommonUtility::s2ws(e.what()));
        outMessage = QObject::tr("Failed to make AppImage executable: %1").arg(QString::fromUtf8(e.what()));
        return false;
    }

    progressCallback(90, QObject::tr("Opening download folder..."));
    (void) QProcess::startDetached(QStringLiteral("xdg-open"), QStringList{QString::fromStdString(destDir.string())});
    outMessage = QObject::tr("AppImage saved to %1.").arg(QString::fromStdString(destDir.string()));


    progressCallback(100, QObject::tr("Done."));
    return true;
}

} // namespace KDC
