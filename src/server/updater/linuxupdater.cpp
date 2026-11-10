/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "linuxupdater.h"

#include "log/log.h"
#include "jobs/network/directdownloadjob.h"
#include "jobs/syncjobmanager.h"
#include "io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/utility/checksumverifier.h"
#include "libcommon/utility/utility.h"

#include <QProcess>
#include <QProcessEnvironment>

#include <unistd.h>

#include <fstream>
#include <regex>
#include <string>

namespace KDC {

namespace {

constexpr auto installedFileName = "kDrive.AppImage";
constexpr auto partialFileName = ".kDrive.AppImage.part";

// Waits for the old server to exit (30 s at most), then starts the new AppImage on the Settings window, where the update
// was requested. Arguments: $1 = PID, $2 = AppImage.
constexpr auto relauncherScript = R"(pid="$1"
target="$2"
i=0
while kill -0 "$pid" 2>/dev/null && [ "$i" -lt 150 ]; do
    sleep 0.2
    i=$((i + 1))
done
exec "$target" --settings)";

log4cplus::Logger logger() {
    return Log::instance()->getLogger();
}

} // namespace

LinuxUpdater::LinuxUpdater() :
    _appImagePath(CommonUtility::envVarValue("APPIMAGE")) {}

void LinuxUpdater::onUpdateFound() {
    if (!canUpdateInPlace()) {
        setState(UpdateState::ManualUpdateAvailable);
        return;
    }

    if (_downloadInProgress) {
        // A periodic check found an update while the previous one is still being prepared.
        setState(UpdateState::Downloading);
        return;
    }

    const auto version = versionInfo().fullVersion();
    bool alreadyVerified = false;
    if (std::error_code ec; std::filesystem::exists(partialDownloadPath(_appImagePath), ec)) {
        const std::scoped_lock lock(_checksumMutex);
        alreadyVerified = _verifiedVersion == version;
    }
    if (alreadyVerified) {
        // Periodic check of the version already downloaded: the file is verified again at install time only.
        setState(UpdateState::Ready);
        return;
    }

    if (expectedChecksum(version).empty()) {
        fetchChecksum(version);
        return;
    }

    prepareUpdate(version);
}

void LinuxUpdater::startInstaller() {
    const auto partPath = partialDownloadPath(_appImagePath);
    if (std::error_code ec; _appImagePath.empty() || !std::filesystem::exists(partPath, ec)) {
        LOGW_WARN(logger(), L"No downloaded AppImage to install at " << Utility::formatSyncPath(partPath));
        setState(UpdateState::UpdateError);
        return;
    }

    if (!verifyChecksum(partPath, versionInfo().fullVersion())) {
        setState(UpdateState::UpdateError);
        return;
    }

    const auto targetPath = installTargetPath(_appImagePath);
    std::error_code renameError;
    std::filesystem::rename(partPath, targetPath, renameError);
    if (renameError) {
        LOGW_WARN(logger(), L"Failed to install the new AppImage: " << Utility::formatStdError(targetPath, renameError));
        setState(UpdateState::UpdateError);
        return;
    }
    LOGW_INFO(logger(), L"New AppImage installed at " << Utility::formatSyncPath(targetPath));

    if (targetPath != _appImagePath) {
        // The running process keeps its mount of the old file, so removing it is safe. Launchers pointing to the old
        // name (symlinks, user desktop entries) break; ours are rewritten at the next start.
        if (std::error_code ec; !std::filesystem::remove(_appImagePath, ec) && ec) {
            LOGW_WARN(logger(), L"Failed to remove the previous AppImage: " << Utility::formatStdError(_appImagePath, ec));
        } else {
            LOGW_INFO(logger(), L"Previous AppImage removed: " << Utility::formatSyncPath(_appImagePath));
        }
    }

    if (!launchRelauncher(targetPath)) {
        // The new version is in place and starts at the next launch.
        setState(UpdateState::UpdateError);
        return;
    }

    if (_quitCallback) {
        _quitCallback();
    }
}

bool LinuxUpdater::checkMinOsVersion(const std::string &minOsVersion) const {
    if (CommonUtility::distributionName() != "Ubuntu") return true; // Do not check OS version for distributions other than Ubuntu
    return AbstractUpdater::checkMinOsVersion(minOsVersion);
}

SyncPath LinuxUpdater::installTargetPath(const SyncPath &appImagePath) {
    static const std::regex publishedName(R"(kDrive-[0-9]+(\.[0-9]+)*-(amd64|arm64)\.AppImage)");
    if (!std::regex_match(appImagePath.filename().string(), publishedName)) {
        return appImagePath;
    }

    return appImagePath.parent_path() / installedFileName;
}

SyncPath LinuxUpdater::partialDownloadPath(const SyncPath &appImagePath) {
    return appImagePath.parent_path() / partialFileName;
}

/**
 * Checks that the running AppImage can be replaced: launched from an AppImage, in a writable directory with enough
 * free space for the new file.
 */
bool LinuxUpdater::canUpdateInPlace() const {
    if (_appImagePath.empty()) {
        LOG_INFO(logger(), "Not running from an AppImage, in-place update unavailable");
        return false;
    }

    const auto directoryPath = _appImagePath.parent_path();
    if (::access(directoryPath.c_str(), W_OK) != 0) {
        LOGW_INFO(logger(),
                  L"AppImage directory is not writable, in-place update unavailable: " << Utility::formatSyncPath(directoryPath));
        return false;
    }

    // The new AppImage is assumed to be at most 20% larger than the running one.
    std::error_code ec;
    const auto currentSize = std::filesystem::file_size(_appImagePath, ec);
    constexpr std::uintmax_t defaultRequiredSize = 300 * 1024 * 1024;
    const auto requiredSize = ec ? defaultRequiredSize : currentSize + currentSize / 5;
    const auto spaceInfo = std::filesystem::space(directoryPath, ec);
    if (!ec && spaceInfo.available < requiredSize) {
        LOGW_INFO(logger(), L"Not enough free space for the new AppImage in " << Utility::formatSyncPath(directoryPath) << L": "
                                                                              << spaceInfo.available << L" < " << requiredSize);
        return false;
    }

    return true;
}

/**
 * Fetches the `.sha256` sidecar before the AppImage: without it the update can never be verified, so the large download
 * is skipped when the sidecar is missing.
 */
void LinuxUpdater::fetchChecksum(const std::string &version) {
    _downloadInProgress = true;
    setState(UpdateState::Downloading);

    const auto sha256Path = SyncPath(partialDownloadPath(_appImagePath).string() + ".sha256");
    const auto job = std::make_shared<DirectDownloadJob>(sha256Path, ChecksumVerifier::buildSha256Url(versionInfo().downloadUrl));
    const std::function<void(UniqueId)> callback = [this, version](const UniqueId jobId) { checksumFetched(jobId, version); };
    job->setAdditionalCallback(callback);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
}

void LinuxUpdater::checksumFetched(const UniqueId jobId, const std::string &version) {
    const auto job = std::dynamic_pointer_cast<DirectDownloadJob>(SyncJobManagerSingleton::instance()->getJob(jobId));
    if (!job) {
        LOG_ERROR(logger(), "Could not cast job pointer.");
        finishPreparation(UpdateState::DownloadError);
        assert(false);
        return;
    }

    std::string checksum;
    if (!job->hasErrorApi() && job->exitInfo() && !job->isAborted()) {
        std::ifstream file(job->getDestinationFilePath());
        file >> checksum;
    }
    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(job->getDestinationFilePath(), ioError);

    if (checksum.empty()) {
        LOG_WARN(logger(), "SHA-256 sidecar unavailable or empty, the AppImage is not downloaded: "
                                   << ChecksumVerifier::buildSha256Url(versionInfo().downloadUrl));
        sentry::Handler::captureMessage(sentry::Level::Error, "LinuxUpdater::checksumFetched",
                                        "Checksum verification failed: sha256FileUnavailable");
        finishPreparation(UpdateState::DownloadError);
        return;
    }

    {
        const std::scoped_lock lock(_checksumMutex);
        _checksumVersion = version;
        _expectedChecksum = CommonUtility::trim(CommonUtility::toLower(checksum));
    }

    prepareUpdate(version);
}

// Reuses a download left by a previous session when it matches the announced version, downloads the AppImage otherwise.
void LinuxUpdater::prepareUpdate(const std::string &version) {
    const auto partPath = partialDownloadPath(_appImagePath);
    if (std::error_code ec; std::filesystem::exists(partPath, ec)) {
        const auto checksum = ChecksumVerifier::computeFileChecksum(partPath);
        if (!checksum.empty() && checksum == expectedChecksum(version)) {
            LOGW_INFO(logger(), L"AppImage already downloaded at " << Utility::formatSyncPath(partPath)
                                                                   << L". Update is ready to be installed.");
            setReady(version);
            return;
        }
        LOGW_INFO(logger(), L"Discarding outdated or incomplete AppImage download at " << Utility::formatSyncPath(partPath));
    }

    downloadUpdate(version);
}

void LinuxUpdater::downloadUpdate(const std::string &version) {
    const auto partPath = partialDownloadPath(_appImagePath);
    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(partPath, ioError);
    {
        const std::scoped_lock lock(_checksumMutex);
        _verifiedVersion.clear();
    }

    _downloadInProgress = true;
    const auto job = std::make_shared<DirectDownloadJob>(partPath, versionInfo().downloadUrl);
    const std::function<void(UniqueId)> callback = [this, version](const UniqueId jobId) { downloadFinished(jobId, version); };
    job->setAdditionalCallback(callback);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
    setState(UpdateState::Downloading);
}

void LinuxUpdater::downloadFinished(const UniqueId jobId, const std::string &version) {
    const auto downloadJob = std::dynamic_pointer_cast<DirectDownloadJob>(SyncJobManagerSingleton::instance()->getJob(jobId));
    if (!downloadJob) {
        LOG_ERROR(logger(), "Could not cast job pointer.");
        finishPreparation(UpdateState::DownloadError);
        assert(false);
        return;
    }

    if (downloadJob->hasErrorApi() || !downloadJob->exitInfo() || downloadJob->isAborted()) {
        LOGW_WARN(logger(),
                  L"Unable to download the AppImage to " << Utility::formatSyncPath(downloadJob->getDestinationFilePath()));
        finishPreparation(UpdateState::DownloadError);
        return;
    }

    const auto partPath = partialDownloadPath(_appImagePath);
    if (!verifyChecksum(partPath, version)) {
        // The sidecar itself may be wrong: fetch it again at the next attempt.
        {
            const std::scoped_lock lock(_checksumMutex);
            _checksumVersion.clear();
            _expectedChecksum.clear();
        }
        finishPreparation(UpdateState::DownloadError);
        return;
    }

    std::error_code ec;
    std::filesystem::permissions(partPath,
                                 std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
                                         std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                                         std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) {
        LOGW_WARN(logger(), L"Failed to make the AppImage executable: " << Utility::formatStdError(partPath, ec));
        finishPreparation(UpdateState::DownloadError);
        return;
    }

    LOGW_INFO(logger(), L"AppImage downloaded at " << Utility::formatSyncPath(partPath) << L". Update is ready to be installed.");
    setReady(version);
}

// Ends the sidecar and AppImage downloads: a periodic check may start a new preparation from now on.
void LinuxUpdater::finishPreparation(const UpdateState state) {
    _downloadInProgress = false;
    setState(state);
}

void LinuxUpdater::setReady(const std::string &version) {
    {
        const std::scoped_lock lock(_checksumMutex);
        _verifiedVersion = version;
    }
    finishPreparation(UpdateState::Ready);
}

std::string LinuxUpdater::expectedChecksum(const std::string &version) const {
    const std::scoped_lock lock(_checksumMutex);
    return _checksumVersion == version ? _expectedChecksum : std::string{};
}

/**
 * Verifies the file against the checksum fetched for this version. The file is deleted on mismatch; it is kept when no
 * checksum is known, since nothing proves it is wrong.
 */
bool LinuxUpdater::verifyChecksum(const SyncPath &filepath, const std::string &version) {
    const auto checksum = expectedChecksum(version);
    if (checksum.empty()) {
        LOG_WARN(logger(), "No SHA-256 checksum known for version " << version << ", the AppImage cannot be verified");
        return false;
    }

    return ChecksumVerifier::verifyFileChecksum(filepath, versionInfo().downloadUrl,
                                                [&checksum](const std::string &) { return checksum; });
}

/**
 * Starts a detached shell that waits for this server to exit, then launches the new AppImage. Variables set by the
 * AppImage runtime or pointing into its mount are removed so that the new AppImage starts with a clean environment.
 */
bool LinuxUpdater::launchRelauncher(const SyncPath &targetPath) const {
    auto environment = QProcessEnvironment::systemEnvironment();
    const auto appDir = environment.value(QStringLiteral("APPDIR"));
    for (const auto &name: environment.keys()) {
        if (!appDir.isEmpty() && environment.value(name).contains(appDir)) {
            environment.remove(name);
        }
    }
    for (const auto *name: {"APPIMAGE", "APPDIR", "ARGV0", "OWD"}) {
        environment.remove(QString::fromLatin1(name));
    }

    QProcess process;
    process.setProgram(QStringLiteral("/bin/sh"));
    process.setArguments({QStringLiteral("-c"), QString::fromLatin1(relauncherScript), QStringLiteral("kdrive-relauncher"),
                          QString::number(::getpid()), QString::fromStdString(targetPath.string())});
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QString::fromStdString(targetPath.parent_path().string()));

    if (qint64 pid = 0; !process.startDetached(&pid)) {
        LOG_WARN(logger(), "Failed to start the AppImage relauncher: " << process.errorString().toStdString());
        return false;
    } else {
        LOGW_INFO(logger(), L"AppImage relauncher started (PID " << pid << L") for " << Utility::formatSyncPath(targetPath));
    }

    return true;
}

} // namespace KDC
