#include "linuxupdater.h"
#include "httpdownloader.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"

#include <QProcess>
#include <cstdlib>
#include <filesystem>

namespace KDUpdater {

bool LinuxUpdater::install(const KDC::VersionInfo &versionInfo, const std::string &desiredVersion,
                           std::function<void(int, QString)> progressCallback, QString &outMessage) {
    (void) desiredVersion;

    const auto &urlStr = versionInfo.downloadUrl;
    if (urlStr.empty()) {
        LOG_ERROR(KDC::Log::instance()->getLogger(), "Download URL is empty.");
        outMessage = QObject::tr("Download URL is empty.");
        return false;
    }

    const char *homeDir = std::getenv("HOME");
    if (!homeDir) {
        LOG_ERROR(KDC::Log::instance()->getLogger(), "HOME environment variable not set.");
        outMessage = QObject::tr("HOME environment variable not set.");
        return false;
    }

    const KDC::SyncPath destDir = std::filesystem::path(homeDir) / "Applications";
    std::filesystem::create_directories(destDir);

    const auto pos = urlStr.find_last_of('/');
    if (pos == std::string::npos) {
        LOG_ERROR(KDC::Log::instance()->getLogger(), "Invalid download URL.");
        outMessage = QObject::tr("Invalid download URL.");
        return false;
    }
    const auto filename = urlStr.substr(pos + 1);
    const KDC::SyncPath destPath = destDir / filename;

    auto ioError = KDC::IoError::Success;
    (void) KDC::IoHelper::deleteItem(destPath, ioError);

    progressCallback(30, QObject::tr(""));

    const auto result = HttpDownloader::downloadFile(urlStr, destPath);
    if (!result.success) {
        if (result.statusCode == 404) {
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"Version not found (404).");
            outMessage = QObject::tr("The specified version does not exist or the download failed.");
        } else {
            LOGW_WARN(KDC::Log::instance()->getLogger(), L"Download failed: " << KDC::CommonUtility::s2ws(result.error));
            outMessage = QObject::tr("Download failed: %1").arg(QString::fromStdString(result.error));
        }
        return false;
    }

    if (std::error_code ec; !std::filesystem::exists(destPath, ec)) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(), L"Downloaded file not found.");
        outMessage = QObject::tr("Downloaded file not found.");
        return false;
    }

    progressCallback(55, QObject::tr("Verifying file integrity..."));
    if (!versionInfo.checksum.empty()) {
        if (!verifyFileChecksum(versionInfo, destPath, outMessage)) {
            return false;
        }
    }

    progressCallback(70, QObject::tr("Making AppImage executable..."));
    try {
        std::filesystem::permissions(
                destPath,
                std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
                std::filesystem::perm_options::add);
    } catch (const std::filesystem::filesystem_error &e) {
        LOGW_WARN(KDC::Log::instance()->getLogger(),
                  L"Failed to make AppImage executable: " << KDC::CommonUtility::s2ws(e.what()));
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

} // namespace KDUpdater
