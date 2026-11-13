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
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "requests/parameterscache.h"

#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <unistd.h>

#include <fstream>
#include <regex>
#include <string>

namespace KDC {

namespace {

constexpr auto installedFileName = "kDrive.AppImage";
constexpr auto partialFileName = ".kDrive.AppImage.part";

// Waits for the old server to exit, then starts the new AppImage on the Settings window, where the update was requested.
// The single-instance check of the new version would hand over to a server still running, so the script waits for the
// complete shutdown (synchronizations included); 5 minutes is only a safeguard against a stuck process.
// Arguments: $1 = PID, $2 = AppImage.
constexpr auto relauncherScript = R"(pid="$1"
target="$2"
i=0
while kill -0 "$pid" 2>/dev/null && [ "$i" -lt 1500 ]; do
    sleep 0.2
    i=$((i + 1))
done
exec "$target" --settings)";

log4cplus::Logger logger() {
    return Log::instance()->getLogger();
}

} // namespace

LinuxUpdater::LinuxUpdater() :
    _appImagePath(CommonUtility::appImagePath()) {}

void LinuxUpdater::onUpdateFound() {
    const auto version = versionInfo().fullVersion();
    bool alreadyInstalled = false;
    {
        const std::scoped_lock lock(_stateMutex);
        alreadyInstalled = _installedVersion == version;
    }
    if (alreadyInstalled) {
        // Periodic check of the version already installed: it only waits for the application to restart.
        setState(UpdateState::Ready);
        return;
    }

    if (!canUpdateInPlace()) {
        setState(UpdateState::ManualUpdateAvailable);
        return;
    }

    if (_downloadInProgress) {
        // A periodic check found an update while the previous one is still being prepared.
        setState(UpdateState::Downloading);
        return;
    }

    if (!ParametersCache::instance()->parameters().autoUpdates()) {
        // The update stays Available until the user starts it from Settings.
        return;
    }

    prepareAnnouncedUpdate();
}

void LinuxUpdater::onAutoUpdatesEnabled() {
    if (state() == UpdateState::Available) {
        onUpdateFound();
    }
}

/**
 * Prepares the announced update when it is not installed yet (checksum, download, install on disk), restarts the
 * application on it otherwise.
 */
void LinuxUpdater::startInstaller() {
    const auto version = versionInfo().fullVersion();
    SyncPath installedPath;
    {
        const std::scoped_lock lock(_stateMutex);
        if (_installedVersion == version) {
            installedPath = _installedPath;
        }
    }

    if (installedPath.empty()) {
        prepareAnnouncedUpdate();
        return;
    }

    if (!launchRelauncher(installedPath)) {
        // The new version is in place and starts at the next launch.
        setState(UpdateState::UpdateError);
        return;
    }

    if (_quitCallback) {
        _quitCallback();
    }
}

void LinuxUpdater::prepareAnnouncedUpdate() {
    if (state() != UpdateState::Available && state() != UpdateState::DownloadError && state() != UpdateState::UpdateError) {
        LOG_INFO(logger(), "No AppImage update to prepare in state " << state());
        return;
    }

    if (!canUpdateInPlace()) {
        setState(UpdateState::ManualUpdateAvailable);
        return;
    }

    // The user's request (GUI job thread) and an automatic preparation (main thread) can race: only one starts.
    if (_downloadInProgress.exchange(true)) {
        LOG_INFO(logger(), "AppImage update already being prepared");
        return;
    }

    // The announced version and its URL are captured together: a check or a channel change during the preparation
    // replaces versionInfo().
    const auto info = versionInfo();
    LOG_INFO(logger(), "Preparing the AppImage update to version " << info.fullVersion());
    if (expectedChecksum(info.fullVersion()).empty()) {
        fetchChecksum(info);
        return;
    }

    prepareUpdate(info);
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

bool LinuxUpdater::hasEnoughSpace(const std::streamsize expectedSize, const std::uintmax_t availableSpace) {
    return expectedSize < 0 || availableSpace >= static_cast<std::uintmax_t>(expectedSize);
}

/**
 * Checks that the running AppImage can be replaced: launched from an AppImage, in a writable directory. The free space is
 * checked against the actual size of the new AppImage, before its download.
 */
bool LinuxUpdater::canUpdateInPlace() const {
    if (_appImagePath.empty()) {
        LOG_INFO(logger(), "Not running from an AppImage, in-place update unavailable");
        return false;
    }

    if (!CommonUtility::isAppImageUpdatable()) {
        LOGW_INFO(logger(), L"AppImage directory is not writable, in-place update unavailable: "
                                    << Utility::formatSyncPath(_appImagePath.parent_path()));
        return false;
    }

    return true;
}

/**
 * Fetches the `.sha256` sidecar before the AppImage: without it the update can never be verified, so the large download
 * is skipped when the sidecar is missing.
 */
void LinuxUpdater::fetchChecksum(const VersionInfo &info) {
    _downloadInProgress = true;
    setState(UpdateState::Downloading);

    const auto sha256Path = SyncPath(partialDownloadPath(_appImagePath).string() + ".sha256");
    const auto job = std::make_shared<DirectDownloadJob>(sha256Path, ChecksumVerifier::buildSha256Url(info.downloadUrl));
    const std::function<void(UniqueId)> callback = [this, info](const UniqueId jobId) { checksumFetched(jobId, info); };
    job->setAdditionalCallback(callback);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
}

void LinuxUpdater::checksumFetched(const UniqueId jobId, const VersionInfo &info) {
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
                                   << ChecksumVerifier::buildSha256Url(info.downloadUrl));
        sentry::Handler::captureMessage(sentry::Level::Error, "LinuxUpdater::checksumFetched",
                                        "Checksum verification failed: sha256FileUnavailable");
        finishPreparation(UpdateState::DownloadError);
        return;
    }

    {
        const std::scoped_lock lock(_stateMutex);
        _checksumVersion = info.fullVersion();
        _expectedChecksum = CommonUtility::trim(CommonUtility::toLower(checksum));
    }

    prepareUpdate(info);
}

// Reuses a download left by a previous session when it matches the announced version, downloads the AppImage otherwise.
void LinuxUpdater::prepareUpdate(const VersionInfo &info) {
    const auto version = info.fullVersion();
    const auto partPath = partialDownloadPath(_appImagePath);
    if (std::error_code ec; std::filesystem::exists(partPath, ec)) {
        const auto checksum = ChecksumVerifier::computeFileChecksum(partPath);
        if (!checksum.empty() && checksum == expectedChecksum(version)) {
            LOGW_INFO(logger(), L"AppImage already downloaded at " << Utility::formatSyncPath(partPath)
                                                                   << L". Update is ready to be installed.");
            installUpdate(info);
            return;
        }
        LOGW_INFO(logger(), L"Discarding outdated or incomplete AppImage download at " << Utility::formatSyncPath(partPath));
    }

    bool sizeKnown = false;
    {
        const std::scoped_lock lock(_stateMutex);
        sizeKnown = _sizeVersion == version;
    }
    if (!sizeKnown) {
        fetchSize(info);
        return;
    }

    downloadUpdate(info);
}

// Asks the size of the new AppImage with a HEAD request, to check the free space before the download.
void LinuxUpdater::fetchSize(const VersionInfo &info) {
    setState(UpdateState::Downloading);

    const auto job = std::make_shared<DirectDownloadJob>(info.downloadUrl);
    const std::function<void(UniqueId)> callback = [this, info](const UniqueId jobId) { sizeFetched(jobId, info); };
    job->setAdditionalCallback(callback);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
}

void LinuxUpdater::sizeFetched(const UniqueId jobId, const VersionInfo &info) {
    const auto job = std::dynamic_pointer_cast<DirectDownloadJob>(SyncJobManagerSingleton::instance()->getJob(jobId));
    if (!job) {
        LOG_ERROR(logger(), "Could not cast job pointer.");
        finishPreparation(UpdateState::DownloadError);
        assert(false);
        return;
    }

    // An unknown size does not block the update: a full disk then fails the download itself.
    std::streamsize size = -1;
    if (!job->hasErrorApi() && job->exitInfo() && !job->isAborted()) {
        size = job->httpResponse().getContentLength();
    }
    if (size < 0) {
        LOG_INFO(logger(), "AppImage size unknown, downloading without a free space check");
    }

    {
        const std::scoped_lock lock(_stateMutex);
        _sizeVersion = info.fullVersion();
        _expectedSize = size;
    }

    downloadUpdate(info);
}

void LinuxUpdater::downloadUpdate(const VersionInfo &info) {
    const auto partPath = partialDownloadPath(_appImagePath);
    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(partPath, ioError);

    std::streamsize expectedSize = -1;
    {
        const std::scoped_lock lock(_stateMutex);
        if (_sizeVersion == info.fullVersion()) {
            expectedSize = _expectedSize;
        }
    }
    const auto directoryPath = partPath.parent_path();
    std::error_code ec;
    const auto spaceInfo = std::filesystem::space(directoryPath, ec);
    if (!ec && !hasEnoughSpace(expectedSize, spaceInfo.available)) {
        LOGW_INFO(logger(), L"Not enough free space for the new AppImage in " << Utility::formatSyncPath(directoryPath) << L": "
                                                                              << spaceInfo.available << L" < " << expectedSize);
        finishPreparation(UpdateState::ManualUpdateAvailable);
        return;
    }

    _downloadInProgress = true;
    const auto job = std::make_shared<DirectDownloadJob>(partPath, info.downloadUrl);
    const std::function<void(UniqueId)> callback = [this, info](const UniqueId jobId) { downloadFinished(jobId, info); };
    job->setAdditionalCallback(callback);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
    setState(UpdateState::Downloading);
}

void LinuxUpdater::downloadFinished(const UniqueId jobId, const VersionInfo &info) {
    const auto downloadJob = std::dynamic_pointer_cast<DirectDownloadJob>(SyncJobManagerSingleton::instance()->getJob(jobId));
    if (!downloadJob) {
        LOG_ERROR(logger(), "Could not cast job pointer.");
        finishPreparation(UpdateState::DownloadError);
        assert(false);
        return;
    }

    // A failed download is removed: it can be large, and the disk may already be nearly full.
    const auto partPath = partialDownloadPath(_appImagePath);
    const auto failDownload = [this, &partPath] {
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(partPath, ioError);
        finishPreparation(UpdateState::DownloadError);
    };

    if (downloadJob->hasErrorApi() || !downloadJob->exitInfo() || downloadJob->isAborted()) {
        LOGW_WARN(logger(),
                  L"Unable to download the AppImage to " << Utility::formatSyncPath(downloadJob->getDestinationFilePath()));
        failDownload();
        return;
    }

    if (!verifyChecksum(partPath, info)) {
        // The sidecar itself may be wrong: fetch it again at the next attempt.
        {
            const std::scoped_lock lock(_stateMutex);
            _checksumVersion.clear();
            _expectedChecksum.clear();
        }
        failDownload();
        return;
    }

    LOGW_INFO(logger(), L"AppImage downloaded at " << Utility::formatSyncPath(partPath));
    installUpdate(info);
}

// Ends the sidecar and AppImage downloads: a periodic check may start a new preparation from now on.
void LinuxUpdater::finishPreparation(const UpdateState state) {
    _downloadInProgress = false;
    setState(state);
}

/**
 * Replaces the AppImage on disk with the verified download. The running process keeps its mount of the old file, so the
 * new version only starts at the next launch or when the user restarts from Settings.
 */
void LinuxUpdater::installUpdate(const VersionInfo &info) {
    const auto version = info.fullVersion();
    const auto partPath = partialDownloadPath(_appImagePath);

    // A version no longer announced is not installed: back to Available, the update manager prepares the announced one.
    if (versionInfo().fullVersion() != version) {
        LOG_INFO(logger(), "Announced version changed during the preparation of version " << version << ", download discarded");
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(partPath, ioError);
        finishPreparation(UpdateState::Available);
        return;
    }

    // Set here rather than after the download, so that a download recovered from a previous session is executable too.
    std::error_code ec;
    std::filesystem::permissions(partPath,
                                 std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
                                         std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
                                         std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) {
        LOGW_WARN(logger(), L"Failed to make the AppImage executable: " << Utility::formatStdError(partPath, ec));
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(partPath, ioError);
        finishPreparation(UpdateState::UpdateError);
        return;
    }

    const auto targetPath = installTargetPath(_appImagePath);
    std::filesystem::rename(partPath, targetPath, ec);
    if (ec) {
        LOGW_WARN(logger(), L"Failed to install the new AppImage: " << Utility::formatStdError(targetPath, ec));
        finishPreparation(UpdateState::UpdateError);
        return;
    }
    LOGW_INFO(logger(), L"New AppImage installed at " << Utility::formatSyncPath(targetPath)
                                                      << L". Update is active once the application restarts.");

    if (targetPath != _appImagePath) {
        // Launchers pointing to the old name (symlinks, user desktop entries) break. Ours are rewritten right away: the
        // application may never be restarted before the next login.
        if (std::filesystem::remove(_appImagePath, ec)) {
            LOGW_INFO(logger(), L"Previous AppImage removed: " << Utility::formatSyncPath(_appImagePath));
        } else if (ec) {
            LOGW_WARN(logger(), L"Failed to remove the previous AppImage: " << Utility::formatStdError(_appImagePath, ec));
        }
        // `$APPIMAGE` still names the removed file: later writes of the autostart entry use the installed one.
        CommonUtility::setAppImagePath(targetPath);
        updateDesktopEntries(targetPath);
    }

    {
        const std::scoped_lock lock(_stateMutex);
        _installedVersion = version;
        _installedPath = targetPath;
    }
    finishPreparation(UpdateState::Ready);
}

// Points the autostart entry (when enabled) and the login URL scheme to the installed AppImage.
void LinuxUpdater::updateDesktopEntries(const SyncPath &targetPath) {
    if (ParametersCache::instance()->parameters().autoStart()) {
        const auto &appName = Theme::instance()->appName();
        if (!Utility::setLaunchOnStartup(appName, appName, targetPath)) {
            LOG_WARN(logger(), "Failed to point the autostart entry to the new AppImage");
        }
    }

    if (!Utility::registerLoginRedirection(targetPath)) {
        LOG_WARN(logger(), "Failed to point the login URL scheme to the new AppImage");
    }
}

std::string LinuxUpdater::expectedChecksum(const std::string &version) const {
    const std::scoped_lock lock(_stateMutex);
    return _checksumVersion == version ? _expectedChecksum : std::string{};
}

/**
 * Verifies the file against the checksum fetched for this version. The file is deleted on mismatch; it is kept when no
 * checksum is known, since nothing proves it is wrong.
 */
bool LinuxUpdater::verifyChecksum(const SyncPath &filepath, const VersionInfo &info) {
    const auto checksum = expectedChecksum(info.fullVersion());
    if (checksum.empty()) {
        LOG_WARN(logger(), "No SHA-256 checksum known for version " << info.fullVersion() << ", the AppImage cannot be verified");
        return false;
    }

    return ChecksumVerifier::verifyFileChecksum(filepath, info.downloadUrl,
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

    QStringList arguments{
            QStringLiteral("/bin/sh"),           QStringLiteral("-c"),        QString::fromLatin1(relauncherScript),
            QStringLiteral("kdrive-relauncher"), QString::number(::getpid()), QString::fromStdString(targetPath.string())};

    // Started at login, kDrive runs in the systemd service generated from its autostart entry. Before systemd 250
    // (Ubuntu 22.04), the end of the main process stops that service and kills the rest of its cgroup, relauncher
    // included: a transient scope of the user manager takes the relauncher out of it.
    const auto systemdRun = QStandardPaths::findExecutable(QStringLiteral("systemd-run"));
    const auto userManagerSocket = environment.value(QStringLiteral("XDG_RUNTIME_DIR")) + QStringLiteral("/systemd/private");
    if (!systemdRun.isEmpty() && QFileInfo::exists(userManagerSocket)) {
        arguments = QStringList{systemdRun, QStringLiteral("--user"), QStringLiteral("--scope"), QStringLiteral("--quiet"),
                                QStringLiteral("--collect")} +
                    arguments;
    } else {
        LOG_INFO(logger(), "No systemd user manager, the AppImage relauncher stays in the current cgroup");
    }

    QProcess process;
    process.setProgram(arguments.takeFirst());
    process.setArguments(arguments);
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QString::fromStdString(targetPath.parent_path().string()));
    // Inherited descriptors include the keepalive pipe of the old AppImage runtime: kept open by the new version, it would
    // keep the old runtime and its mount alive until the next restart.
    process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);

    if (qint64 pid = 0; !process.startDetached(&pid)) {
        LOG_WARN(logger(), "Failed to start the AppImage relauncher: " << process.errorString().toStdString());
        return false;
    } else {
        LOGW_INFO(logger(), L"AppImage relauncher started (PID "
                                    << pid << L", " << CommonUtility::s2ws(process.program().toStdString()) << L") for "
                                    << Utility::formatSyncPath(targetPath));
    }

    return true;
}

} // namespace KDC
