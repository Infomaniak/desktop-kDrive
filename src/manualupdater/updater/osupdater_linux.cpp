#include "osupdater_linux.h"
#include "httpdownloader.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"

#include <QProcess>
#include <cstdlib>
#include <filesystem>

namespace KDC {

bool OSUpdater::install(const VersionInfo &versionInfo, const std::function<void(int32_t, QString)> &progressCallback,
                        QString &outMessage) {
    const auto &urlStr = versionInfo.downloadUrl;
    if (urlStr.empty()) {
        LOG_ERROR(Log::instance()->getLogger(), "Download URL is empty.");
        outMessage = QObject::tr("Download URL is empty.");
        return false;
    }

    const char *homeDir = std::getenv("HOME");
    if (!homeDir) {
        LOG_ERROR(Log::instance()->getLogger(), "HOME environment variable not set.");
        outMessage = QObject::tr("HOME environment variable not set.");
        return false;
    }

    const SyncPath destDir = std::filesystem::path(homeDir) / "Applications";
    (void) std::filesystem::create_directories(destDir);

    const auto pos = urlStr.find_last_of('/');
    if (pos == std::string::npos) {
        LOG_ERROR(Log::instance()->getLogger(), "Invalid download URL.");
        outMessage = QObject::tr("Invalid download URL.");
        return false;
    }
    const auto filename = urlStr.substr(pos + 1);
    const SyncPath destPath = destDir / filename;

    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(destPath, ioError);

    progressCallback(30, QObject::tr(""));

    if (const auto result = HttpDownloader::downloadFile(urlStr, destPath); !result.success) {
        if (result.statusCode == 404) {
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
    if (!QProcess::startDetached(QStringLiteral("xdg-open"), QStringList{QString::fromStdString(destDir.string())})) {
        outMessage = QObject::tr("AppImage saved to %1. Please open it manually.").arg(QString::fromStdString(destDir.string()));
    } else {
        outMessage = QObject::tr("AppImage saved to %1.").arg(QString::fromStdString(destDir.string()));
    }

    progressCallback(100, QObject::tr("Done."));
    return true;
}

} // namespace KDC
