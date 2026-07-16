#include "windowsupdater.h"
#include "httpdownloader.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/utility/digitalsignaturechecker_win.h"
#include "libcommon/utility/utility.h"

#include <QProcess>

#include <fstream>
#include <array>
#include <Poco/SHA2Engine.h>
#include <Poco/DigestStream.h>

namespace KDUpdater {

bool WindowsUpdater::install(const KDC::VersionInfo &versionInfo, const std::string &desiredVersion,
                               std::function<void(int, QString)> progressCallback, QString &outMessage) {
    (void) desiredVersion;

    KDC::SyncPath filepath;
    if (!getInstallerPath(versionInfo, filepath)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Failed to get installer path.");
        outMessage = QObject::tr("Failed to get installer path.");
        return false;
    }

    auto ioError = KDC::IoError::Success;
    (void) KDC::IoHelper::deleteItem(filepath, ioError);
    if (ioError != KDC::IoError::Success && ioError != KDC::IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Failed to remove existing installer " << KDC::Utility::formatSyncPath(filepath));
    }

    progressCallback(30, QObject::tr("Downloading installer..."));

    const auto result = HttpDownloader::downloadFile(versionInfo.downloadUrl, filepath);
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

    if (std::error_code ec; !std::filesystem::exists(filepath, ec)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Installer file not found after download.");
        outMessage = QObject::tr("Installer file not found after download.");
        return false;
    }

    progressCallback(60, QObject::tr("Verifying file integrity..."));
    if (!versionInfo.checksum.empty()) {
        if (!verifyFileChecksum(versionInfo, filepath, outMessage)) {
            return false;
        }
    }

    progressCallback(80, QObject::tr("Verifying digital signature..."));
    if (!verifyDigitalSignature(filepath, outMessage)) {
        return false;
    }

    progressCallback(90, QObject::tr("Starting installer..."));
    LOGW_INFO(KDC::Log::instance()->getLogger(), L"Starting installer " << KDC::Utility::formatSyncPath(filepath));

    const QString program = QString::fromStdWString(filepath.wstring());
    if (!QProcess::startDetached(program, QStringList{QStringLiteral("/S"), QStringLiteral("/launch")})) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(), L"Failed to launch installer.");
        outMessage = QObject::tr("Failed to launch installer.");
        return false;
    }

    outMessage = QObject::tr("Installer launched successfully.");
    progressCallback(100, QObject::tr("Done."));
    return true;
}

bool WindowsUpdater::getInstallerPath(const KDC::VersionInfo &versionInfo, KDC::SyncPath &path) const {
    const auto &url = versionInfo.downloadUrl;
    const auto pos = url.find_last_of('/');
    if (pos == std::string::npos) {
        return false;
    }
    const auto installerName = url.substr(pos + 1);

    KDC::SyncPath tmpDirPath;
    if (const auto exitInfo = KDC::CommonUtility::deviceTempDirectoryPath(tmpDirPath); !exitInfo) {
        return false;
    }

    path = tmpDirPath / installerName;
    return true;
}

bool WindowsUpdater::verifyFileChecksum(const KDC::VersionInfo &versionInfo, const KDC::SyncPath &filepath,
                                          QString &outMessage) const {
    const std::string expectedChecksum = KDC::CommonUtility::trim(KDC::CommonUtility::toLower(versionInfo.checksum));
    if (expectedChecksum.empty()) {
        return true;
    }

    const std::string actualChecksum = KDC::CommonUtility::trim(KDC::CommonUtility::toLower(computeFileChecksum(filepath)));
    if (actualChecksum.empty()) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(), L"Failed to compute file checksum.");
        outMessage = QObject::tr("Failed to compute file checksum.");
        auto ioError = KDC::IoError::Success;
        (void) KDC::IoHelper::deleteItem(filepath, ioError);
        return false;
    }

    if (actualChecksum != expectedChecksum) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(),
                  L"Checksum mismatch! Expected: " << KDC::CommonUtility::s2ws(expectedChecksum)
                                                  << L", Got: " << KDC::CommonUtility::s2ws(actualChecksum));
        outMessage = QObject::tr("Checksum verification failed.");
        auto ioError = KDC::IoError::Success;
        (void) KDC::IoHelper::deleteItem(filepath, ioError);
        return false;
    }

    LOGW_INFO(KDC::Log::instance()->getLogger(), L"Checksum verification passed.");
    return true;
}

std::string WindowsUpdater::computeFileChecksum(const KDC::SyncPath &filepath) const {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    Poco::SHA2Engine sha256(Poco::SHA2Engine::ALGORITHM::SHA_256);
    std::array<char, 8192> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        sha256.update(buffer.data(), static_cast<std::size_t>(file.gcount()));
    }

    return Poco::DigestEngine::digestToHex(sha256.digest());
}

bool WindowsUpdater::verifyDigitalSignature(const KDC::SyncPath &filepath, QString &outMessage) const {
    if (!KDC::DigitalSignatureChecker_win(filepath).isSignatureValid()) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(),
                  L"The digital signature of installer " << KDC::Utility::formatSyncPath(filepath)
                                                           << L" is invalid. Aborting update.");
        outMessage = QObject::tr("Digital signature verification failed.");
        auto ioError = KDC::IoError::Success;
        (void) KDC::IoHelper::deleteItem(filepath, ioError);
        return false;
    }
    return true;
}

} // namespace KDUpdater
