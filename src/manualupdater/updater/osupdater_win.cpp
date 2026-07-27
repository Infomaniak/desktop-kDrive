#include "osupdater_win.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/utility/utility.h"
#include "manualupdater/httpdownloader.h"

#include <QProcess>

namespace KDC {
bool OSUpdater::install(const VersionInfo &versionInfo, const std::function<void(int32_t, QString)> &progressCallback,
                        QString &outMessage) {
    SyncPath filepath;
    if (!getInstallerPath(versionInfo, filepath)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to get installer path.");
        outMessage = QObject::tr("Failed to get installer path.");
        return false;
    }

    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(filepath, ioError);
    if (ioError != IoError::Success && ioError != IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to remove existing installer " << Utility::formatSyncPath(filepath));
    }

    progressCallback(30, QObject::tr("Downloading installer..."));

    if (const auto result = HttpDownloader::downloadFile(versionInfo.downloadUrl, filepath); !result.success) {
        if (result.statusCode == 404) {
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

    progressCallback(60, QObject::tr("Verifying file integrity..."));
    if (!versionInfo.checksum.empty() && !verifyFileChecksum(versionInfo, filepath, outMessage)) {
        return false;
    }


    progressCallback(90, QObject::tr("Starting installer..."));
    LOGW_INFO(Log::instance()->getLogger(), L"Starting installer " << Utility::formatSyncPath(filepath));

    if (const QString program = QString::fromStdWString(filepath.wstring());
        !QProcess::startDetached(program, QStringList{QStringLiteral("/S"), QStringLiteral("/launch")})) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Failed to launch installer.");
        outMessage = QObject::tr("Failed to launch installer.");
        return false;
    }

    outMessage = QObject::tr("Installer launched successfully.");
    progressCallback(100, QObject::tr("Done."));
    return true;
}

bool OSUpdater::getInstallerPath(const VersionInfo &versionInfo, SyncPath &path) {
    const auto &url = versionInfo.downloadUrl;
    const auto pos = url.find_last_of('/');
    if (pos == std::string::npos) {
        return false;
    }
    const auto installerName = url.substr(pos + 1);

    SyncPath tmpDirPath;
    if (const auto exitInfo = CommonUtility::deviceTempDirectoryPath(tmpDirPath); !exitInfo) {
        return false;
    }

    path = tmpDirPath / installerName;
    return true;
}


} // namespace KDC
