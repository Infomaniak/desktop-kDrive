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

    if (!ParametersCache::instance()->parameters().automaticUpdates()) {
        // The update stays Available until the user starts it from Settings.
        return;
    }

    prepareAnnouncedUpdate(version);
}

void LinuxUpdater::onAutomaticUpdatesEnabled() {
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
        prepareAnnouncedUpdate(version);
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

void LinuxUpdater::prepareAnnouncedUpdate(const std::string &version) {
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

    LOG_INFO(logger(), "Preparing the AppImage update to version " << version);
    if (expectedChecksum(version).empty()) {
        fetchChecksum(version);
        return;
    }

    prepareUpdate(version);
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
    if (!CommonUtility::isAppImageUpdatable()) {
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
        const std::scoped_lock lock(_stateMutex);
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
            installUpdate(version);
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
            const std::scoped_lock lock(_stateMutex);
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

    LOGW_INFO(logger(), L"AppImage downloaded at " << Utility::formatSyncPath(partPath));
    installUpdate(version);
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
void LinuxUpdater::installUpdate(const std::string &version) {
    const auto partPath = partialDownloadPath(_appImagePath);
    const auto targetPath = installTargetPath(_appImagePath);
    std::error_code ec;
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
