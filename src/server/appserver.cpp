/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "appserver.h"
#include "migration/migrationparams.h"
#include "socketapi.h"
#include "keychainmanager/keychainmanager.h"
#include "requests/serverrequests.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommon/utility/logiffail.h"
#include "libcommon/comm.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/info/exclusiontemplateinfo.h"
#include "libcommon/log/sentry/handler.h"
#include "libcommon/log/sentry/ptraces.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/network/proxy.h"
#include "libcommonserver/vfs/vfs.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/requests/parameterscache.h"
#include "libsyncengine/requests/exclusiontemplatecache.h"
#include "libsyncengine/jobs/jobmanager.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

#if defined(KD_WINDOWS)
#include <windows.h>
#endif

#include "jobs/network/API_v2/upload/loguploadjob.h"
#include "jobs/network/API_v2/upload/upload_session/uploadsessioncanceljob.h"
#include "updater/updatemanager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QIcon>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#define QUIT_DELAY 1000 // ms
#define LOAD_PROGRESS_INTERVAL 1000 // ms
#define LOAD_PROGRESS_MAXITEMS 100 // ms
#define SEND_NOTIFICATIONS_INTERVAL 15000 // ms
#define RESTART_SYNCS_INTERVAL 15000 // ms
#define START_SYNCPALS_RETRY_INTERVAL 60000 // ms
#define START_SYNCPALS_TIME_GAP 5 // sec

namespace KDC {

std::unordered_map<int, std::shared_ptr<SyncPal>> AppServer::_syncPalMap;
std::unordered_map<int, std::shared_ptr<KDC::Vfs>> AppServer::_vfsMap;
std::vector<AppServer::Notification> AppServer::_notifications;

std::unique_ptr<UpdateManager> AppServer::_updateManager;

namespace {

static const char optionsC[] =
        "Options:\n"
        "  -h --help            : show this help screen.\n"
        "  -v --version         : show the application version.\n"
        "  --settings           : show the Settings window (if the application is running).\n"
        "  --synthesis          : show the Synthesis window (if the application is running).\n";
}

static const std::string showSynthesisMsg = "showSynthesis";
static const std::string showSettingsMsg = "showSettings";
static const std::string restartClientMsg = "restartClient";

static const QString crashMsg = SharedTools::QtSingleApplication::tr("kDrive application will close due to a fatal error.");


// Helpers for displaying messages. Note that there is no console on Windows.
#if defined(KD_WINDOWS)
static void displayHelpText(const QString &t) // No console on Windows.
{
    QString spaces(80, ' '); // Add a line of non-wrapped space to make the messagebox wide enough.
    QString text = QLatin1String("<qt><pre style='white-space:pre-wrap'>") + t.toHtmlEscaped() + QLatin1String("</pre><pre>") +
                   spaces + QLatin1String("</pre></qt>");
    QMessageBox::information(0, QString::fromStdString(Theme::instance()->appName()), text);
}

#else

static void displayHelpText(const QString &t) {
    std::cout << qUtf8Printable(t);
}
#endif

AppServer::AppServer(int &argc, char **argv) :
    SharedTools::QtSingleApplication(QString::fromStdString(Theme::instance()->appName()), argc, argv) {
    _arguments = arguments();
    _theme = Theme::instance();
}

AppServer::~AppServer() {
    LOG_DEBUG(_logger, "~AppServer");
}

void AppServer::init() {
    _startedAt.start();
    setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));
    setApplicationName(QString::fromStdString(_theme->appName()));
    setWindowIcon(_theme->applicationIcon());
    setApplicationVersion(QString::fromStdString(_theme->version()));

    // Setup logging with default parameters
    if (!initLogging()) {
        throw std::runtime_error("Unable to init logging.");
    }

    parseOptions(_arguments);
    if (_helpAsked || _versionAsked || _clearSyncNodesAsked || _clearKeychainKeysAsked) {
        LOG_INFO(_logger, "Command line options processed");
        return;
    }

    if (isRunning()) {
        LOG_INFO(_logger, "AppServer already running");
        return;
    }

    // Cleanup at quit
    connect(this, &QCoreApplication::aboutToQuit, this, &AppServer::onCleanup);

    // Setup single application: show the Settings or Synthesis window if the application is running.
    connect(this, &QtSingleApplication::messageReceived, this, &AppServer::onMessageReceivedFromAnotherProcess);

    // Remove the files that keep a record of former crash or kill events
    SignalType signalType = SignalType::None;
    CommonUtility::clearSignalFile(AppType::Server, SignalCategory::Crash, signalType);
    if (signalType != SignalType::None) {
        LOG_INFO(_logger, "Restarting after a " << SignalCategory::Crash << " with signal " << signalType);
    }

    CommonUtility::clearSignalFile(AppType::Server, SignalCategory::Kill, signalType);
    if (signalType != SignalType::None) {
        LOG_INFO(_logger, "Restarting after a " << SignalCategory::Kill << " with signal " << signalType);
    }

    // Init parms DB
    std::filesystem::path parmsDbPath = makeDbName();
    if (parmsDbPath.empty()) {
        LOG_WARN(_logger, "Error in AppServer::makeDbName");
        throw std::runtime_error("Unable to get ParmsDb path.");
    }

    bool newDbExists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(parmsDbPath, newDbExists, ioError) || ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(parmsDbPath, ioError));
        throw std::runtime_error("Unable to check if ParmsDb exists.");
    }

    std::filesystem::path pre334ConfigFilePath =
            std::filesystem::path(QStr2SyncName(MigrationParams::configDir().filePath(MigrationParams::configFileName())));
    bool oldConfigExists;
    if (!IoHelper::checkIfPathExists(pre334ConfigFilePath, oldConfigExists, ioError) || ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(pre334ConfigFilePath, ioError));
        throw std::runtime_error("Unable to check if a pre 3.3.4 config exists.");
    }

    LOGW_INFO(_logger, L"New DB exists : " << Path2WStr(parmsDbPath) << L" => " << newDbExists);
    LOGW_INFO(_logger, L"Old config exists : " << Path2WStr(pre334ConfigFilePath) << L" => " << oldConfigExists);

    // Init ParmsDb instance
    if (!initParmsDB(parmsDbPath, _theme->version())) {
        LOG_WARN(_logger, "Error in AppServer::initParmsDB");
        throw std::runtime_error("Unable to initialize ParmsDb.");
    }

    // Clear old server errors
    if (clearErrors(0) != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in AppServer::clearErrors");
        throw std::runtime_error("Unable to clear old errors.");
    }

    bool migrateFromPre334 = !newDbExists && oldConfigExists;
    if (migrateFromPre334) {
        // Migrate pre v3.4.0 configuration
        LOG_INFO(_logger, "Migrate pre v3.4.0 configuration");
        bool proxyNotSupported = false;
        ExitCode exitCode = migrateConfiguration(proxyNotSupported);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in migrateConfiguration");
            addError(
                    Error(errId(), exitCode, exitCode == ExitCode::SystemError ? ExitCause::MigrationError : ExitCause::Unknown));
        }

        if (proxyNotSupported) {
            addError(Error(errId(), ExitCode::DataError, ExitCause::MigrationProxyNotImplemented));
        }
    }

    // Init KeyChainManager instance
    if (!KeyChainManager::instance()) {
        LOG_WARN(_logger, "Error in KeyChainManager::instance");
        throw std::runtime_error("Unable to initialize key chain manager.");
    }

#if defined(KD_LINUX)
    // For access to keyring in order to promt authentication popup
    KeyChainManager::instance()->writeDummyTest();
    KeyChainManager::instance()->clearDummyTest();
#endif

    // Init ParametersCache instance
    if (!ParametersCache::instance()) {
        LOG_WARN(_logger, "Error in ParametersCache::instance");
        throw std::runtime_error("Unable to initialize parameters cache.");
    }

    // Setup translations
    CommonUtility::setupTranslations(this, ParametersCache::instance()->parameters().language());

    // Configure logger
    if (!Log::instance()->configure(ParametersCache::instance()->parameters().useLog(),
                                    ParametersCache::instance()->parameters().logLevel(),
                                    ParametersCache::instance()->parameters().purgeOldLogs())) {
        LOG_WARN(_logger, "Error in Log::configure");
        addError(Error(errId(), ExitCode::SystemError, ExitCause::Unknown));
    }

    // Log useful information
    logUsefulInformation();

    // Init ExclusionTemplateCache instance
    if (!ExclusionTemplateCache::instance()) {
        LOG_WARN(_logger, "Error in ExclusionTemplateCache::instance");
        throw std::runtime_error("Unable to initialize exclusion template cache.");
    }

#if defined(KD_WINDOWS)
    // Update shortcuts
    _navigationPaneHelper = std::unique_ptr<NavigationPaneHelper>(new NavigationPaneHelper(_vfsMap));
    _navigationPaneHelper->setShowInExplorerNavigationPane(false);
    if (ParametersCache::instance()->parameters().showShortcuts()) {
        _navigationPaneHelper->setShowInExplorerNavigationPane(true);
    }
#endif

    // Setup proxy
    if (ParametersCache::instance()->parameters().proxyConfig().type() == ProxyType::Undefined) {
        // Migration issue?
        LOG_WARN(_logger, "Proxy type is undefined, fix it");
        const ExitCode exitCode = ServerRequests::fixProxyConfig();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in ServerRequests::fixProxyConfig: code=" << exitCode);
            throw std::runtime_error("Unable to fix proxy type.");
        }
    }

    if (!setupProxy()) {
        LOG_WARN(_logger, "Error in AppServer::setupProxy");
        throw std::runtime_error("Unable to initialize proxy.");
    }

    // Init JobManager
    if (!JobManager::instance()) {
        LOG_WARN(_logger, "Error in JobManager::instance");
        throw std::runtime_error("Unable to initialize job manager.");
    }

    // Setup auto start
#ifdef NDEBUG
#if defined(KD_LINUX)
    // On Linux, override the auto startup file on every app launch to make sure it points to the correct executable.
    if (ParametersCache::instance()->parameters().autoStart()) {
        (void) Utility::setLaunchOnStartup(_theme->appName(), _theme->appName(), true);
    }
#else
    if (ParametersCache::instance()->parameters().autoStart() && !Utility::hasLaunchOnStartup(_theme->appName())) {
        (void) Utility::setLaunchOnStartup(_theme->appName(), _theme->appClientName(), true);
    }
#endif
#endif

#ifdef PLUGINDIR
    // Setup extra plugin search path
    QString extraPluginPath = QStringLiteral(PLUGINDIR);
    if (!extraPluginPath.isEmpty()) {
        if (QDir::isRelativePath(extraPluginPath))
            extraPluginPath = QDir(QApplication::applicationDirPath()).filePath(extraPluginPath);
        LOGW_INFO(_logger, L"Adding extra plugin search path:" << Path2WStr(QStr2Path(extraPluginPath)));
        addLibraryPath(extraPluginPath);
    }
#endif

    // Check vfs plugins
    QString error;
#if defined(KD_WINDOWS)
    if (KDC::isVfsPluginAvailable(VirtualFileMode::Win, error)) LOG_INFO(_logger, "VFS windows plugin is available");
#elif defined(KD_MACOS)
    if (KDC::isVfsPluginAvailable(VirtualFileMode::Mac, error)) LOG_INFO(_logger, "VFS mac plugin is available");
#endif
    if (KDC::isVfsPluginAvailable(VirtualFileMode::Suffix, error)) LOG_INFO(_logger, "VFS suffix plugin is available");

    // Init socket api
    _socketApi.reset(new SocketApi(_syncPalMap, _vfsMap));
    _socketApi->setAddErrorCallback(&addError);
    _socketApi->setGetThumbnailCallback(&ServerRequests::getThumbnail);
    _socketApi->setGetPublicLinkUrlCallback(&ServerRequests::getPublicLinkUrl);

    // Init CommServer instance
    if (!CommServer::instance()) {
        LOG_WARN(_logger, "Error in CommServer::instance");
        throw std::runtime_error("Unable to initialize CommServer.");
    }

    connect(CommServer::instance().get(), &CommServer::requestReceived, this, &AppServer::onRequestReceived);
    connect(CommServer::instance().get(), &CommServer::restartClient, this, &AppServer::onRestartClientReceived);

    // Update users/accounts/drives info
    ExitCode exitCode = updateAllUsersInfo();
    if (exitCode == ExitCode::InvalidToken) {
        // The user will be asked to enter its credentials later
    } else if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in updateAllUsersInfo: code=" << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
        if (exitCode != ExitCode::NetworkError && exitCode != ExitCode::UpdateRequired) {
            throw std::runtime_error("Failed to load user data.");
        }
    }

    // Set sentry user
    updateSentryUser();

    // Update checks
    _updateManager = std::make_unique<UpdateManager>();
    connect(_updateManager.get(), &UpdateManager::requestRestart, this, &AppServer::onScheduleAppRestart);
#if defined(KD_MACOS)
    const std::function<void()> quitCallback = std::bind_front(&AppServer::sendQuit, this);
    _updateManager.get()->setQuitCallback(quitCallback);
#endif

    connect(_updateManager.get(), &UpdateManager::updateStateChanged, this, &AppServer::onUpdateStateChanged);
    connect(_updateManager.get(), &UpdateManager::updateAnnouncement, this, &AppServer::onSendNotifAsked);
    connect(_updateManager.get(), &UpdateManager::showUpdateDialog, this, &AppServer::onShowWindowsUpdateDialog);

    // Check last crash to avoid crash loop
    bool shouldQuit = false;
    handleCrashRecovery(shouldQuit);
    if (shouldQuit) {
        LOG_WARN(_logger, "Crash loop detected");
        QTimer::singleShot(0, this, &AppServer::quit);
        return;
    }

    // Start syncs
    QTimer::singleShot(0, [=, this]() { startSyncsAndRetryOnError(); });

    // Process possible interrupted logs upload
    processInterruptedLogsUpload();

    // Start client
    if (!startClient()) {
        LOG_ERROR(_logger, "Error in startClient");
        throw std::runtime_error("Failed to start kDrive client.");
    }

    // Send syncs progress
    connect(&_loadSyncsProgressTimer, &QTimer::timeout, this, &AppServer::onUpdateSyncsProgress);
    _loadSyncsProgressTimer.start(LOAD_PROGRESS_INTERVAL);

    // Send files notifications
    connect(&_sendFilesNotificationsTimer, &QTimer::timeout, this, &AppServer::onSendFilesNotifications);
    _sendFilesNotificationsTimer.start(SEND_NOTIFICATIONS_INTERVAL);

    // Restart paused syncs
    connect(&_restartSyncsTimer, &QTimer::timeout, this, &AppServer::onRestartSyncs);
    _restartSyncsTimer.start(RESTART_SYNCS_INTERVAL);
}

void AppServer::cleanup() {
    LOG_DEBUG(_logger, "AppServer::cleanup");

    // Stop JobManager
    JobManager::instance()->stop();
    LOG_DEBUG(_logger, "JobManager stopped");

    // Stop SyncPals
    for (const auto &syncPalMapElt: _syncPalMap) {
        if (const auto exitInfo = stopSyncPal(syncPalMapElt.first, false, true); !exitInfo) {
            LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncPalMapElt.first << exitInfo);
        }
    }
    LOG_DEBUG(_logger, "Syncpal(s) stopped");

    // Stop Vfs
    for (const auto &vfsMapElt: _vfsMap) {
        if (const auto exitInfo = stopVfs(vfsMapElt.first, false); !exitInfo) {
            LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << vfsMapElt.first << exitInfo);
        }
    }
    LOG_DEBUG(_logger, "Vfs(s) stopped");

    // Clear JobManager
    JobManager::instance()->clear();
    LOG_DEBUG(_logger, "JobManager::clear() done");

    // Clear maps
    _syncPalMap.clear();
    _vfsMap.clear();

    // Close ParmsDb
    ParmsDb::instance()->close();
    LOG_DEBUG(_logger, "ParmsDb closed");
}

void AppServer::reset() {
    _updateManager.reset();
}

// This task can be long and block the GUI
void AppServer::stopSyncTask(int syncDbId) {
    // Stop sync and remove it from syncPalMap
    if (const auto exitInfo = stopSyncPal(syncDbId, false, true, true); !exitInfo) {
        LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncDbId << " : " << exitInfo);
    }

    // Stop Vfs
    if (const auto exitInfo = stopVfs(syncDbId, true); !exitInfo) {
        LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << syncDbId << " : " << exitInfo);
    }

    LOG_IF_FAIL(!_syncPalMap[syncDbId] || _syncPalMap[syncDbId].use_count() == 1)
    _syncPalMap.erase(syncDbId);

    LOG_IF_FAIL(!_vfsMap[syncDbId] ||
                _vfsMap[syncDbId].use_count() <= 1) // `use_count` can be zero when the local drive has been removed.
    _vfsMap.erase(syncDbId);
}

void AppServer::stopAllSyncsTask(const std::vector<int> &syncDbIdList) {
    for (int syncDbId: syncDbIdList) {
        stopSyncTask(syncDbId);
    }
}

void AppServer::deleteAccount(int accountDbId) {
    // Delete the account
    const ExitCode exitCode = ServerRequests::deleteAccount(accountDbId);
    if (exitCode == ExitCode::Ok) {
        sendAccountRemoved(accountDbId);
    } else {
        LOG_WARN(_logger, "Error in Requests::deleteAccount: code=" << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
        return;
    }
}

void AppServer::deleteDrive(int driveDbId) {
    // Get the drive in DB
    bool found = false;
    Drive drive;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found for driveDbId=" << driveDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
    }

    // Delete the drive
    const ExitCode exitCode = ServerRequests::deleteDrive(driveDbId);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::deleteDrive: code=" << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
        sendDriveDeletionFailed(driveDbId);
        return;
    }

    // Delete the account if there is no remaining drive
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(drive.accountDbId(), driveList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
    } else if (driveList.empty()) {
        deleteAccount(drive.accountDbId());
    } else {
        sendDriveRemoved(driveDbId); // Useless if deleteAccount is called, sendAccountRemoved already updates the drive list.
                                     // May even cause a crash if both are processed concurrently on the GUI side.
    }
}

void AppServer::deleteSync(int syncDbId) {
    // Get the sync in DB
    bool found = false;
    Sync sync;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found for syncDbId=" << syncDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
    }

    // Delete the sync
    const ExitCode exitCode = ServerRequests::deleteSync(syncDbId);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::deleteSync: code=" << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
        sendSyncDeletionFailed(syncDbId);
        return;
    }

    // Delete the drive if there is no remaining sync
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(sync.driveDbId(), syncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
    } else if (syncList.empty()) {
        deleteDrive(sync.driveDbId());
    } else {
        sendSyncRemoved(syncDbId); // Useless if deleteDrive is called, sendDriveRemoved already updates the sync list.
                                   // May even cause a crash if both sendDriveRemoved and sendSyncRemoved are processed
                                   // concurrently on the GUI side.
    }
}

void AppServer::logExtendedLogActivationMessage(bool isExtendedLogEnabled) noexcept {
    const std::string activationStatus = isExtendedLogEnabled ? "enabled" : "disabled";
    const std::string msg = "Extended logging is " + activationStatus + ".";

    LOG_INFO(_logger, msg);
}

void AppServer::updateSentryUser() const {
    User user;
    bool found = false;
    ParmsDb::instance()->selectLastConnectedUser(user, found);
    std::string userId = "No user in db";
    std::string userName = "No user in db";
    std::string userEmail = "No user in db";
    if (found) {
        userId = std::to_string(user.userId());
        userName = user.name();
        userEmail = user.email();
    }
    sentry::Handler::instance()->setAuthenticatedUser(SentryUser(userEmail, userName, userId));
}

bool AppServer::clientHasCrashed() const {
    // Check if a crash file exists
    auto sigFilePath(CommonUtility::signalFilePath(AppType::Client, SignalCategory::Crash));
    std::error_code ec;
    return std::filesystem::exists(sigFilePath, ec);
}

void AppServer::handleClientCrash(bool &quit) {
    if (clientCrashedRecently()) {
        LOG_FATAL(_logger, "Client has crashed twice in a short time, exiting");

        // Reset client restart date in DB
        if (bool found = false; !KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastClientSelfRestartDate, 0, found)) {
            addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        } else if (!found) {
            LOG_WARN(_logger, "ParmsDb::updateAppState: missing entry for key " << AppStateKey::LastClientSelfRestartDate);
        }

        quit = true;
    } else {
        // Set client restart date in DB
        const long timestamp =
                std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();
        const std::string timestampStr = std::to_string(timestamp);

        if (bool found = false;
            !KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastClientSelfRestartDate, timestampStr, found)) {
            addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        } else if (!found) {
            LOG_WARN(_logger, "ParmsDb::updateAppState: missing entry for key " << AppStateKey::LastClientSelfRestartDate);
        }

        // Restart client
        if (!startClient()) {
            LOG_WARN(_logger, "Error in AppServer::startClient");
            quit = true;
        } else {
            LOG_INFO(_logger, "Client restarted");
            quit = false;
        }
    }
}

#if defined(KD_MACOS)
bool AppServer::noMacVfsSync() const {
    for (const auto &[_, vfsMapElt]: _vfsMap) {
        if (vfsMapElt->mode() == VirtualFileMode::Mac) {
            return false;
        }
    }
    return true;
}

bool AppServer::areMacVfsAuthsOk() const {
    std::string liteSyncExtErrorDescr;
    bool ret = CommonUtility::isLiteSyncExtEnabled() && CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
    if (!ret) {
        if (liteSyncExtErrorDescr.empty()) {
            LOG_WARN(_logger, "LiteSync extension is not enabled or doesn't have full disk access");
        } else {
            LOG_WARN(_logger, "LiteSync extension is not enabled or doesn't have full disk access: " << liteSyncExtErrorDescr);
        }
    }
    return ret;
}
#endif

void AppServer::crash() const {
    // SIGSEGV crash
    CommonUtility::crash();
}

void AppServer::onRequestReceived(int id, RequestNum num, const QByteArray &params) {
    QByteArray results = QByteArray();
    QDataStream resultStream(&results, QIODevice::WriteOnly);

    switch (num) {
        case RequestNum::LOGIN_REQUESTTOKEN: {
            QString code, codeVerifier;
            QDataStream paramsStream(params);
            paramsStream >> code;
            paramsStream >> codeVerifier;

            UserInfo userInfo;
            bool userCreated;
            std::string error;
            std::string errorDescr;
            ExitCode exitCode = ServerRequests::requestToken(code, codeVerifier, userInfo, userCreated, error, errorDescr);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::requestToken: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }
            updateSentryUser();
            if (userCreated) {
                sendUserAdded(userInfo);
            } else {
                sendUserUpdated(userInfo);
            }

            resultStream << toInt(exitCode);
            if (exitCode == ExitCode::Ok) {
                resultStream << userInfo.dbId();
            } else {
                resultStream << QString::fromStdString(error);
                resultStream << QString::fromStdString(errorDescr);
            }
            break;
        }
        case RequestNum::USER_DBIDLIST: {
            QList<int> list;
            ExitCode exitCode = ServerRequests::getUserDbIdList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getUserDbIdList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::USER_INFOLIST: {
            QList<UserInfo> list;
            ExitCode exitCode = ServerRequests::getUserInfoList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getUserInfoList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::USER_DELETE: {
            // As the actual deletion task is post-poned via a timer,
            // this request returns immediately with `ExitCode::Ok`.
            // Errors are reported via the addError method.

            resultStream << ExitCode::Ok;

            int userDbId = 0;
            ArgsWriter(params).write(userDbId);

            // Get syncs do delete
            std::vector<int> syncDbIdList;
            for (const auto &syncPalMapElt: _syncPalMap) {
                if (!syncPalMapElt.second) continue;
                if (syncPalMapElt.second->userDbId() == userDbId) {
                    syncDbIdList.push_back(syncPalMapElt.first);
                }
            }

            // Stop syncs for this user and remove them from syncPalMap.
            QTimer::singleShot(100, [this, userDbId, syncDbIdList]() {
                AppServer::stopAllSyncsTask(syncDbIdList);

                // Delete user from DB
                const ExitCode exitCode = ServerRequests::deleteUser(userDbId);
                if (exitCode == ExitCode::Ok) {
                    sendUserRemoved(userDbId);
                } else {
                    LOG_WARN(_logger, "Error in Requests::deleteUser: code=" << exitCode);
                    addError(Error(errId(), exitCode, ExitCause::Unknown));
                }
            });

            break;
        }
        case RequestNum::ERROR_INFOLIST: {
            ErrorLevel level{ErrorLevel::Unknown};
            int syncDbId{0};
            int limit{100};
            ArgsWriter(params).write(level, syncDbId, limit);

            QList<ErrorInfo> list;
            ExitCode exitCode = ServerRequests::getErrorInfoList(level, syncDbId, limit, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getErrorInfoList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::ERROR_GET_CONFLICTS: {
            int driveDbId;
            QList<ConflictType> filter;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;
            paramsStream >> filter;

            QList<ErrorInfo> list;

            // Retrieve all sync related to this drive
            std::vector<Sync> syncs;
            if (!ParmsDb::instance()->selectAllSyncs(driveDbId, syncs)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
                resultStream << ExitCode::DbError;
                break;
            }

            std::unordered_set<ConflictType> filter2;
            for (const auto &c: filter) {
                filter2.insert(c);
            }

            ExitCode exitCode = ExitCode::Ok;
            for (auto &sync: syncs) {
                exitCode = ServerRequests::getConflictErrorInfoList(sync.dbId(), filter2, list);
                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger, "Error in Requests::getConflictErrorInfoList: code=" << exitCode);
                    addError(Error(errId(), exitCode, ExitCause::Unknown));
                }
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::ERROR_DELETE_SERVER: {
            ExitCode exitCode = clearErrors(0, false);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in AppServer::clearErrors: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::ERROR_DELETE_SYNC: {
            int syncDbId = 0;
            bool autoResolved;

            QDataStream paramsStream(params);
            paramsStream >> syncDbId;
            paramsStream >> autoResolved;

            ExitCode exitCode = clearErrors(syncDbId, autoResolved);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in AppServer::clearErrors: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::ERROR_DELETE_INVALIDTOKEN: {
            ExitCode exitCode = ServerRequests::deleteInvalidTokenErrors();
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::userLoggedIn: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << ExitCode::Ok;

            break;
        }
        case RequestNum::ERROR_RESOLVE_CONFLICTS: {
            int driveDbId = 0;
            bool keepLocalVersion = false;

            QDataStream paramsStream(params);
            paramsStream >> driveDbId;
            paramsStream >> keepLocalVersion;

            // Retrieve all sync related to this drive
            std::vector<Sync> syncs;
            if (!ParmsDb::instance()->selectAllSyncs(driveDbId, syncs)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
                resultStream << ExitCode::DbError;
                break;
            }

            ExitCode exitCode = ExitCode::Ok;
            for (auto &sync: syncs) {
                if (_syncPalMap.find(sync.dbId()) == _syncPalMap.end()) {
                    LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << sync.dbId());
                    exitCode = ExitCode::DataError;
                    break;
                }

                std::vector<Error> errorList;
                ServerRequests::getConflictList(sync.dbId(), conflictsWithLocalRename, errorList);

                if (!errorList.empty()) {
                    _syncPalMap[sync.dbId()]->fixConflictingFiles(keepLocalVersion, errorList);
                }
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::ERROR_RESOLVE_UNSUPPORTED_CHAR: {
            int driveId = 0;

            QDataStream paramsStream(params);
            paramsStream >> driveId;

            // TODO : not implemented yet

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::USER_AVAILABLEDRIVES: {
            int userDbId;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;

            QHash<int, DriveAvailableInfo> list;
            ExitCode exitCode = ServerRequests::getUserAvailableDrives(userDbId, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getUserAvailableDrives");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::USER_ID_FROM_USERDBID: {
            int userDbId;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;

            int userId;
            ExitCode exitCode = ServerRequests::getUserIdFromUserDbId(userDbId, userId);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getUserIdFromUserDbId: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << userId;
            break;
        }
        case RequestNum::ACCOUNT_INFOLIST: {
            QList<AccountInfo> list;
            ExitCode exitCode = ServerRequests::getAccountInfoList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getAccountInfoList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::DRIVE_INFOLIST: {
            QList<DriveInfo> list;
            ExitCode exitCode = ServerRequests::getDriveInfoList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getDriveInfoList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::DRIVE_INFO: {
            int driveDbId;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;

            DriveInfo driveInfo;
            ExitCode exitCode = ServerRequests::getDriveInfo(driveDbId, driveInfo);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getDriveInfo: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << driveInfo;
            break;
        }
        case RequestNum::DRIVE_ID_FROM_DRIVEDBID: {
            int driveDbId;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;

            int driveId;
            ExitCode exitCode = ServerRequests::getDriveIdFromDriveDbId(driveDbId, driveId);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getDriveIdFromDriveDbId: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << driveId;
            break;
        }
        case RequestNum::DRIVE_ID_FROM_SYNCDBID: {
            int syncDbId;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;

            int driveId;
            ExitCode exitCode = ServerRequests::getDriveIdFromSyncDbId(syncDbId, driveId);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getDriveIdFromSyncDbId: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << driveId;
            break;
        }
        case RequestNum::DRIVE_DEFAULTCOLOR: {
            static const QColor driveDefaultColor(0x9F9F9F);

            resultStream << ExitCode::Ok;
            resultStream << driveDefaultColor;
            break;
        }
        case RequestNum::DRIVE_UPDATE: {
            DriveInfo driveInfo;
            QDataStream paramsStream(params);
            paramsStream >> driveInfo;

            ExitCode exitCode = ServerRequests::updateDrive(driveInfo);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::updateDrive: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::DRIVE_DELETE: {
            // As the actual deletion task is post-poned via a timer,
            // this request returns immediately with `ExitCode::Ok`.
            // Errors are reported via the addError method.

            resultStream << ExitCode::Ok;

            int driveDbId = 0;
            ArgsWriter(params).write(driveDbId);

            // Get syncs do delete
            std::vector<int> syncDbIdList;
            for (const auto &syncPalMapElt: _syncPalMap) {
                if (!syncPalMapElt.second) continue;
                if (syncPalMapElt.second->driveDbId() == driveDbId) {
                    syncDbIdList.push_back(syncPalMapElt.first);
                }
            }

            // Stop syncs for this drive and remove them from syncPalMap
            QTimer::singleShot(100, [this, driveDbId, syncDbIdList]() {
                AppServer::stopAllSyncsTask(syncDbIdList);
                AppServer::deleteDrive(driveDbId);
            });
#if defined(__APPLE__)
            Utility::restartFinderExtension();
#endif
            break;
        }
        case RequestNum::SYNC_INFOLIST: {
            QList<SyncInfo> list;
            ExitCode exitCode;
            exitCode = ServerRequests::getSyncInfoList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSyncInfoList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::SYNC_START: {
            int syncDbId;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;

            Sync sync;
            bool found = false;
            if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectSync");
                resultStream << ExitCode::DbError;
                break;
            }
            if (!found) {
                LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                break;
            }

            // Clear old errors for this sync
            clearErrors(sync.dbId(), false);
            clearErrors(sync.dbId(), true);

            if (const auto exitInfo = checkIfSyncIsValid(sync); !exitInfo) {
                LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
                addError(Error(sync.dbId(), errId(), exitInfo));
                resultStream << toInt(exitInfo.code());
                break;
            }

            ExitInfo mainExitInfo = ExitCode::Ok;
            bool start = true;
            if (const auto exitInfo = tryCreateAndStartVfs(sync); !exitInfo) {
                LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
                if (!(exitInfo.code() == ExitCode::SystemError && exitInfo.cause() == ExitCause::LiteSyncNotAllowed)) {
                    resultStream << toInt(exitInfo.code());
                    break;
                }
                // Continue (ie. Init SyncPal but don't start it)
                mainExitInfo = exitInfo;
                start = false;
            }

            if (const auto exitInfo =
                        initSyncPal(sync, NodeSet(), NodeSet(), NodeSet(), start, std::chrono::seconds(0), true, false);
                !exitInfo) {
                LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " : " << exitInfo);
                addError(Error(errId(), exitInfo));
                mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
            }
#if defined(__APPLE__)
            Utility::restartFinderExtension();
#endif
            resultStream << mainExitInfo.code();
            break;
        }
        case RequestNum::SYNC_STOP: {
            int syncDbId;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;

            resultStream << ExitCode::Ok;

            QTimer::singleShot(100, [=, this]() {
                // Stop SyncPal
                if (const auto exitInfo = stopSyncPal(syncDbId, true); !exitInfo) {
                    LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncDbId << " : " << exitInfo);
                }

                // Note: we do not Stop Vfs in case of a pause
            });

            break;
        }
        case RequestNum::SYNC_STATUS: {
            int syncDbId;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;

            if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << SyncStatus::Undefined;
                break;
            }

            SyncStatus status = _syncPalMap[syncDbId]->status();

            resultStream << ExitCode::Ok;
            resultStream << status;
            break;
        }
        case RequestNum::SYNC_ISRUNNING: {
            int syncDbId;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;

            if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << false;
                break;
            }

            bool isRunning = _syncPalMap[syncDbId]->isRunning();

            resultStream << ExitCode::Ok;
            resultStream << isRunning;
            break;
        }
        case RequestNum::SYNC_ADD:
        case RequestNum::SYNC_ADD2: {
            int userDbId = 0;
            int accountId = 0;
            int driveId = 0;
            int driveDbId = 0;
            QString localFolderPath;
            QString serverFolderPath;
            QString serverFolderNodeId;
            bool liteSync = false;
            QSet<QString> blackList;
            QSet<QString> whiteList;
            if (num == RequestNum::SYNC_ADD) {
                ArgsWriter(params).write(userDbId, accountId, driveId, localFolderPath, serverFolderPath, serverFolderNodeId,
                                         liteSync, blackList, whiteList);
            } else {
                ArgsWriter(params).write(driveDbId, localFolderPath, serverFolderPath, serverFolderNodeId, liteSync, blackList,
                                         whiteList);
            }

            // Add sync in DB
            bool showInNavigationPane = false;
#if defined(KD_WINDOWS)
            showInNavigationPane = _navigationPaneHelper->showInExplorerNavigationPane();
#endif

            ExitCode exitCode = ExitCode::Ok;
            SyncInfo syncInfo;
            if (num == RequestNum::SYNC_ADD) {
                AccountInfo accountInfo;
                DriveInfo driveInfo;

                exitCode = ServerRequests::addSync(userDbId, accountId, driveId, localFolderPath, serverFolderPath,
                                                   serverFolderNodeId, liteSync, showInNavigationPane, accountInfo, driveInfo,
                                                   syncInfo);

                if (exitCode != ExitCode::Ok) {
                    LOGW_WARN(_logger, L"Error in Requests::addSync - userDbId="
                                               << userDbId << L" accountId=" << accountId << L" driveId=" << driveId
                                               << L" localFolderPath=" << QStr2WStr(localFolderPath) << L" serverFolderPath="
                                               << QStr2WStr(serverFolderPath) << L" serverFolderNodeId="
                                               << serverFolderNodeId.toStdWString() << L" liteSync=" << liteSync
                                               << L" showInNavigationPane=" << showInNavigationPane);
                    addError(Error(errId(), exitCode, ExitCause::Unknown));
                    resultStream << toInt(exitCode);
                    break;
                }

                if (accountInfo.dbId() != 0) {
                    sendAccountAdded(accountInfo);
                }

                if (driveInfo.dbId() != 0) {
                    sendDriveAdded(driveInfo);
                }
            } else {
                exitCode = ServerRequests::addSync(driveDbId, localFolderPath, serverFolderPath, serverFolderNodeId, liteSync,
                                                   showInNavigationPane, syncInfo);

                if (exitCode != ExitCode::Ok) {
                    LOGW_WARN(_logger, L"Error in Requests::addSync for driveDbId="
                                               << driveDbId << L" localFolderPath=" << Path2WStr(QStr2Path(localFolderPath))
                                               << L" serverFolderPath=" << Path2WStr(QStr2Path(serverFolderPath)) << L" liteSync="
                                               << liteSync << L" showInNavigationPane=" << showInNavigationPane);
                    addError(Error(errId(), exitCode, ExitCause::Unknown));
                    resultStream << toInt(exitCode);
                    break;
                }
            }

            sendSyncAdded(syncInfo);

            resultStream << toInt(exitCode);
            if (exitCode == ExitCode::Ok) {
                resultStream << syncInfo.dbId();
            }

            QTimer::singleShot(100, this, [=, this]() {
                Sync sync;
                ServerRequests::syncInfoToSync(syncInfo, sync);

                // Check if sync is valid
                if (const auto exitInfo = checkIfSyncIsValid(sync); !exitInfo) {
                    LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    addError(Error(sync.dbId(), errId(), exitInfo));
                    return;
                }

                bool start = true;
                if (const auto exitInfo = tryCreateAndStartVfs(sync); !exitInfo) {
                    LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    if (!(exitInfo.code() == ExitCode::SystemError && exitInfo.cause() == ExitCause::LiteSyncNotAllowed)) {
                        return;
                    }
                    // Continue (ie. Init SyncPal but don't start it)
                    start = false;
                }

                // Create and start SyncPal
                if (const auto exitInfo =
                            initSyncPal(sync, blackList, QSet<QString>(), whiteList, start, std::chrono::seconds(0), false, true);
                    !exitInfo) {
                    LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << syncInfo.dbId() << " : " << exitInfo);
                    addError(Error(errId(), exitInfo));

                    // Stop sync and remove it from syncPalMap
                    if (const auto exitInfo2 = stopSyncPal(syncInfo.dbId(), false, true, true); !exitInfo2) {
                        LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
                    }

                    // Stop Vfs
                    if (const auto exitInfo2 = stopVfs(syncInfo.dbId(), true); !exitInfo2) {
                        LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
                    }

                    LOG_IF_FAIL(!_syncPalMap[syncInfo.dbId()] || _syncPalMap[syncInfo.dbId()].use_count() == 1)
                    _syncPalMap.erase(syncInfo.dbId());

                    LOG_IF_FAIL(!_vfsMap[syncInfo.dbId()] || _vfsMap[syncInfo.dbId()].use_count() == 1)
                    _vfsMap.erase(syncInfo.dbId());

                    // Delete sync from DB
                    if (const ExitInfo exitInfo2 = ServerRequests::deleteSync(syncInfo.dbId()); !exitInfo2) {
                        LOG_WARN(_logger, "Error in Requests::deleteSync for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
                        addError(Error(errId(), exitInfo));
                    }

                    sendSyncRemoved(syncInfo.dbId());
                }
#if defined(__APPLE__)
                Utility::restartFinderExtension();
#endif
            });
            break;
        }
        case RequestNum::SYNC_START_AFTER_LOGIN: {
            int userDbId;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;

            User user;
            bool found;
            if (!ParmsDb::instance()->selectUser(userDbId, user, found)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectUser");
                resultStream << ExitCode::DbError;
                break;
            }
            if (!found) {
                LOG_WARN(_logger, "User not found in user table for userDbId=" << userDbId);
                resultStream << ExitCode::DataError;
                break;
            }

            if (const auto exitInfo = startSyncs(user); !exitInfo) {
                LOG_WARN(_logger, "Error in startSyncs for userDbId=" << user.dbId() << " : " << exitInfo);
                resultStream << toInt(exitInfo.code());
                break;
            }

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::SYNC_DELETE: {
            // Although the return code is always `ExitCode::Ok` because of fake asynchronicity via QTimer,
            // the post-poned task records errors through calls to `addError` and use a dedicated client-server signal
            // for deletion failure.
            resultStream << ExitCode::Ok;

            int syncDbId = 0;
            ArgsWriter(params).write(syncDbId);

            QTimer::singleShot(100, [this, syncDbId]() {
                AppServer::stopSyncTask(syncDbId); // This task can be long, hence blocking, on Windows.

                // Delete sync from DB
                deleteSync(syncDbId);
#if defined(__APPLE__)
                Utility::restartFinderExtension();
#endif
            });

            break;
        }
        case RequestNum::SYNC_GETPUBLICLINKURL: {
            int driveDbId;
            QString nodeId;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;
            paramsStream >> nodeId;

            QString linkUrl;
            ExitCode exitCode = ServerRequests::getPublicLinkUrl(driveDbId, nodeId, linkUrl);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getLinkUrl");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << linkUrl;
            break;
        }
        case RequestNum::SYNC_GETPRIVATELINKURL: {
            int driveDbId;
            QString fileId;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;
            paramsStream >> fileId;

            QString linkUrl;
            ExitCode exitCode = ServerRequests::getPrivateLinkUrl(driveDbId, fileId, linkUrl);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getLinkUrl");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << linkUrl;
            break;
        }
        case RequestNum::SYNC_ASKFORSTATUS: {
            _syncCacheMap.clear();

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::SYNCNODE_LIST: {
            int syncDbId;
            SyncNodeType type;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;
            paramsStream >> type;

            if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
                LOG_DEBUG(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << QSet<QString>();
                break;
            }

            NodeSet nodeIdSet;
            ExitCode exitCode = _syncPalMap[syncDbId]->syncIdSet(type, nodeIdSet);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::getSyncIdSet: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            QSet<QString> nodeIdSet2;
            for (const NodeId &nodeId: nodeIdSet) {
                nodeIdSet2 << QString::fromStdString(nodeId);
            }

            resultStream << toInt(exitCode);
            resultStream << nodeIdSet2;
            break;
        }
        case RequestNum::SYNCNODE_SETLIST: {
            int syncDbId;
            SyncNodeType type;
            QSet<QString> nodeIdSet;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;
            paramsStream >> type;
            paramsStream >> nodeIdSet;

            if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                break;
            }

            NodeSet nodeIdSet2;
            for (const QString &nodeId: nodeIdSet) {
                nodeIdSet2.insert(nodeId.toStdString());
            }

            ExitCode exitCode = _syncPalMap[syncDbId]->setSyncIdSet(type, nodeIdSet2);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::NODE_PATH: {
            int syncDbId;
            QString nodeId;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;
            paramsStream >> nodeId;

            if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << "";
                break;
            }

            QString path;
            ExitCode exitCode = ServerRequests::getPathByNodeId(_syncPalMap[syncDbId]->userDbId(),
                                                                _syncPalMap[syncDbId]->driveId(), nodeId, path);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in AppServer::getPathByNodeId: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << path;
            break;
        }
        case RequestNum::NODE_INFO: {
            int userDbId;
            int driveId;
            QString nodeId;
            bool withPath = false;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;
            paramsStream >> driveId;
            paramsStream >> nodeId;
            paramsStream >> withPath;

            NodeInfo nodeInfo;
            ExitCode exitCode = ServerRequests::getNodeInfo(userDbId, driveId, nodeId, nodeInfo, withPath);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getNodeInfo");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << nodeInfo;
            break;
        }
        case RequestNum::NODE_SUBFOLDERS: {
            int userDbId;
            int driveId;
            QString nodeId;
            bool withPath = false;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;
            paramsStream >> driveId;
            paramsStream >> nodeId;
            paramsStream >> withPath;

            QList<NodeInfo> subfoldersList;
            const auto exitInfo = ServerRequests::getSubFolders(userDbId, driveId, nodeId, subfoldersList, withPath);
            if (exitInfo.code() != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSubFolders");
                addError(Error(errId(), exitInfo.code(), exitInfo.cause()));
            }

            resultStream << toInt(exitInfo);
            resultStream << subfoldersList;
            break;
        }
        case RequestNum::NODE_SUBFOLDERS2: {
            int driveDbId;
            QString nodeId;
            bool withPath = false;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;
            paramsStream >> nodeId;
            paramsStream >> withPath;

            QList<NodeInfo> subfoldersList;
            const auto exitInfo = ServerRequests::getSubFolders(driveDbId, nodeId, subfoldersList, withPath);
            if (exitInfo.code() != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSubFolders");
                addError(Error(errId(), exitInfo.code(), exitInfo.cause()));
            }

            resultStream << toInt(exitInfo);
            resultStream << subfoldersList;
            break;
        }
        case RequestNum::NODE_FOLDER_SIZE: {
            int userDbId;
            int driveId;
            QString nodeId;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;
            paramsStream >> driveId;
            paramsStream >> nodeId;

            std::function<void(const QString &, qint64)> callback =
                    std::bind(&AppServer::sendGetFolderSizeCompleted, this, std::placeholders::_1, std::placeholders::_2);
            std::thread getFolderSize(ServerRequests::getFolderSize, userDbId, driveId, nodeId.toStdString(), callback);
            getFolderSize.detach();

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::NODE_CREATEMISSINGFOLDERS: {
            int driveDbId;
            QList<QPair<QString, QString>> folderList;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;
            paramsStream >> folderList;

            // Pause all syncs of the drive
            QList<int> pausedSyncs;
            for (auto &syncPalMapElt: _syncPalMap) {
                if (!syncPalMapElt.second) continue;
                if (syncPalMapElt.second->driveDbId() == driveDbId && !syncPalMapElt.second->isPaused()) {
                    syncPalMapElt.second->pause();
                    pausedSyncs.push_back(syncPalMapElt.first);
                }
            }

            // Create missing folders
            QString parentNodeId(QString::fromStdString(SyncDb::driveRootNode().nodeIdRemote().value()));
            QString firstCreatedNodeId;
            for (auto &folderElt: folderList) {
                if (folderElt.second.isEmpty()) {
                    ExitCode exitCode = ServerRequests::createDir(driveDbId, parentNodeId, folderElt.first, parentNodeId);
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in Requests::createDir for driveDbId=" << driveDbId << " parentNodeId="
                                                                                        << parentNodeId.toStdString());
                        addError(Error(errId(), exitCode, ExitCause::Unknown));
                        resultStream << toInt(exitCode);
                        resultStream << QString();
                        break;
                    }
                    if (firstCreatedNodeId.isEmpty()) {
                        firstCreatedNodeId = parentNodeId;
                    }
                } else {
                    parentNodeId = folderElt.second;
                }
            }

            // Add first created node to blacklist of all syncs
            for (auto &syncPalMapElt: _syncPalMap) {
                if (!syncPalMapElt.second) continue;
                if (syncPalMapElt.second->driveDbId() == driveDbId) {
                    // Get blacklist
                    NodeSet nodeIdSet;
                    ExitCode exitCode = syncPalMapElt.second->syncIdSet(SyncNodeType::BlackList, nodeIdSet);
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in SyncPal::syncIdSet for syncDbId=" << syncPalMapElt.first);
                        addError(Error(errId(), exitCode, ExitCause::Unknown));
                        resultStream << toInt(exitCode);
                        resultStream << QString();
                        break;
                    }

                    // Set blacklist
                    nodeIdSet.insert(firstCreatedNodeId.toStdString());
                    exitCode = syncPalMapElt.second->setSyncIdSet(SyncNodeType::BlackList, nodeIdSet);
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << syncPalMapElt.first);
                        addError(Error(errId(), exitCode, ExitCause::Unknown));
                        resultStream << toInt(exitCode);
                        resultStream << QString();
                        break;
                    }
                }
            }

            // Resume all paused syncs
            for (int syncDbId: pausedSyncs) {
                _syncPalMap[syncDbId]->unpause();
            }

            resultStream << ExitCode::Ok;
            resultStream << parentNodeId;
            break;
        }
        case RequestNum::EXCLTEMPL_GETEXCLUDED: {
            QString name;
            QDataStream paramsStream(params);
            paramsStream >> name;

            bool isWarning = false;

            resultStream << ExitCode::Ok;
            resultStream << ExclusionTemplateCache::instance()->isExcluded(name.toStdString(), isWarning);
            break;
        }
        case RequestNum::EXCLTEMPL_GETLIST: {
            bool def;
            QDataStream paramsStream(params);
            paramsStream >> def;

            QList<ExclusionTemplateInfo> list;
            ExitCode exitCode = ServerRequests::getExclusionTemplateList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getExclusionTemplateList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::EXCLTEMPL_SETLIST: {
            bool def;
            QList<ExclusionTemplateInfo> list;
            QDataStream paramsStream(params);
            paramsStream >> def;
            paramsStream >> list;

            ExitCode exitCode = ServerRequests::setExclusionTemplateList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::setExclusionTemplateList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
                resultStream << toInt(exitCode);
                break;
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::EXCLTEMPL_PROPAGATE_CHANGE: {
            resultStream << ExitCode::Ok;

            QTimer::singleShot(100, [=, this]() {
                for (auto &syncPalMapElt: _syncPalMap) {
                    if (!syncPalMapElt.second) continue;
                    if (_socketApi) {
                        _socketApi->unregisterSync(syncPalMapElt.second->syncDbId());
                    }

                    _syncPalMap[syncPalMapElt.first]->excludeListUpdated();

                    if (_socketApi) {
                        _socketApi->registerSync(syncPalMapElt.second->syncDbId());
                    }
                }
            });

            break;
        }
#if defined(KD_MACOS)
        case RequestNum::EXCLAPP_GETLIST: {
            bool def;
            QDataStream paramsStream(params);
            paramsStream >> def;

            QList<ExclusionAppInfo> list;
            ExitCode exitCode = ServerRequests::getExclusionAppList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getExclusionAppList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::EXCLAPP_SETLIST: {
            bool def;
            QList<ExclusionAppInfo> list;
            QDataStream paramsStream(params);
            paramsStream >> def;
            paramsStream >> list;

            ExitCode exitCode = ServerRequests::setExclusionAppList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::setExclusionAppList: code=" << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            if (exitCode == ExitCode::Ok) {
                for (const auto &vfsMapElt: _vfsMap) {
                    if (vfsMapElt.second->mode() == VirtualFileMode::Mac) {
                        if (!vfsMapElt.second->setAppExcludeList()) {
                            exitCode = ExitCode::SystemError;
                            LOG_WARN(_logger, "Error in Vfs::setAppExcludeList!");
                            addError(Error(errId(), exitCode, ExitCause::Unknown));
                        }
                        break;
                    }
                }
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::EXCLAPP_GET_FETCHING_APP_LIST: {
            ExitCode exitCode = ExitCode::Ok;
            QHash<QString, QString> appTable;
            for (const auto &vfsMapElt: _vfsMap) {
                if (vfsMapElt.second->mode() == VirtualFileMode::Mac) {
                    if (!vfsMapElt.second->getFetchingAppList(appTable)) {
                        exitCode = ExitCode::SystemError;
                        LOG_WARN(_logger, "Error in Vfs::getFetchingAppList!");
                        addError(Error(errId(), exitCode, ExitCause::Unknown));
                    }
                    break;
                }
            }

            resultStream << toInt(exitCode);
            resultStream << appTable;
            break;
        }
#endif
        case RequestNum::PARAMETERS_INFO: {
            ParametersInfo parametersInfo;
            ExitCode exitCode = ServerRequests::getParameters(parametersInfo);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getParameters");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << parametersInfo;
            break;
        }
        case RequestNum::PARAMETERS_UPDATE: {
            ParametersInfo parametersInfo;
            QDataStream paramsStream(params);
            paramsStream >> parametersInfo;

            // Retrieve current settings
            const Parameters parameters = ParametersCache::instance()->parameters();
            std::string pwd;
            if (parameters.proxyConfig().needsAuth()) {
                // Read pwd from keystore
                bool found;
                if (!KeyChainManager::instance()->readDataFromKeystore(parameters.proxyConfig().token(), pwd, found)) {
                    LOG_WARN(_logger, "Failed to read proxy pwd from keychain");
                }
                if (!found) {
                    LOG_DEBUG(_logger, "Proxy pwd not found for keychainKey=" << parameters.proxyConfig().token());
                }
            }

            // Update parameters
            const ExitCode exitCode = ServerRequests::updateParameters(parametersInfo);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::updateParameters");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            // extendedLog change propagation
            if (parameters.extendedLog() != parametersInfo.extendedLog()) {
                logExtendedLogActivationMessage(parametersInfo.extendedLog());
                for (const auto &[_, vfsMapElt]: _vfsMap) {
                    vfsMapElt->setExtendedLog(parametersInfo.extendedLog());
                }
            }

            // Language change propagation
            if (parameters.language() != parametersInfo.language()) {
                CommonUtility::setupTranslations(this, parametersInfo.language());
            }

            // ProxyConfig change propagation
            if (parameters.proxyConfig().type() != parametersInfo.proxyConfigInfo().type() ||
                parameters.proxyConfig().hostName() != parametersInfo.proxyConfigInfo().hostName().toStdString() ||
                parameters.proxyConfig().port() != parametersInfo.proxyConfigInfo().port() ||
                parameters.proxyConfig().needsAuth() != parametersInfo.proxyConfigInfo().needsAuth() ||
                parameters.proxyConfig().user() != parametersInfo.proxyConfigInfo().user().toStdString() ||
                pwd != parametersInfo.proxyConfigInfo().pwd().toStdString()) {
                Proxy::instance()->setProxyConfig(ParametersCache::instance()->parameters().proxyConfig());
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC: {
            int driveDbId;
            QString basePath;
            QDataStream paramsStream(params);
            paramsStream >> driveDbId;
            paramsStream >> basePath;

            QString path;
            QString error;
            ExitCode exitCode = ServerRequests::findGoodPathForNewSync(driveDbId, basePath, path, error);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::findGoodPathForNewSyncFolder");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << path;
            resultStream << error;
            break;
        }
        case RequestNum::UTILITY_BESTVFSAVAILABLEMODE: {
            VirtualFileMode mode = KDC::bestAvailableVfsMode();

            resultStream << ExitCode::Ok;
            resultStream << mode;
            break;
        }
#if defined(KD_WINDOWS)
        case RequestNum::UTILITY_SHOWSHORTCUT: {
            bool show = _navigationPaneHelper->showInExplorerNavigationPane();

            resultStream << ExitCode::Ok;
            resultStream << show;
            break;
        }
        case RequestNum::UTILITY_SETSHOWSHORTCUT: {
            bool show;
            QDataStream paramsStream(params);
            paramsStream >> show;

            _navigationPaneHelper->setShowInExplorerNavigationPane(show);

            resultStream << ExitCode::Ok;
            break;
        }
#endif
        case RequestNum::UTILITY_ACTIVATELOADINFO: {
            bool value;
            QDataStream paramsStream(params);
            paramsStream >> value;

            if (value) {
                QTimer::singleShot(100, this, &AppServer::onLoadInfo);

                // Clear sync update progress cache
                _syncCacheMap.clear();
            }

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::UTILITY_CHECKCOMMSTATUS: {
            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP: {
            const bool enabled = Utility::hasSystemLaunchOnStartup(Theme::instance()->appName());
            resultStream << ExitCode::Ok;
            resultStream << enabled;
            break;
        }
        case RequestNum::UTILITY_HASLAUNCHONSTARTUP: {
            const bool enabled = Utility::hasLaunchOnStartup(Theme::instance()->appName());
            resultStream << ExitCode::Ok;
            resultStream << enabled;
            break;
        }
        case RequestNum::UTILITY_SETLAUNCHONSTARTUP: {
            bool enabled = false;
            QDataStream paramsStream(params);
            paramsStream >> enabled;

            Theme *theme = Theme::instance();
            (void) Utility::setLaunchOnStartup(theme->appName(), theme->appName(), enabled);

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::UTILITY_SET_APPSTATE: {
            AppStateKey key = AppStateKey::Unknown;
            QString value;
            QDataStream paramsStream(params);
            paramsStream >> key;
            paramsStream >> value;

            if (bool found = true; !ParmsDb::instance()->updateAppState(key, value.toStdString(), found)) {
                LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
                addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
                resultStream << ExitCode::DbError;
                break;
            } else if (!found) {
                LOG_WARN(_logger, key << " not found in appState table");
                resultStream << ExitCode::DbError;
                break;
            }

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::UTILITY_GET_APPSTATE: {
            AppStateKey key = AppStateKey::Unknown;
            QString defaultValue;
            QDataStream paramsStream(params);
            paramsStream >> key;

            AppStateValue appStateValue = std::string();
            if (bool found = false; !ParmsDb::instance()->selectAppState(key, appStateValue, found)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
                addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
                resultStream << ExitCode::DbError;
                break;
            } else if (!found) {
                LOG_WARN(_logger, key << " not found in appState table");
                resultStream << ExitCode::DbError;
                break;
            }
            std::string appStateValueStr = std::get<std::string>(appStateValue);

            resultStream << ExitCode::Ok;
            resultStream << QString::fromStdString(appStateValueStr);
            break;
        }
        case RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE: {
            uint64_t logSize = 0;
            IoError ioError = IoError::Success;
            const bool res = LogUploadJob::getLogDirEstimatedSize(logSize, ioError);
            if (!res || ioError != IoError::Success) {
                LOG_WARN(_logger, "Error in LogArchiver::getLogDirEstimatedSize: " << IoHelper::ioError2StdString(ioError));

                addError(Error(errId(), ExitCode::SystemError, ExitCause::Unknown));
                resultStream << ExitCode::SystemError;
                resultStream << 0;
            } else {
                resultStream << ExitCode::Ok;
                resultStream << static_cast<qint64>(logSize);
            }
            break;
        }
        case RequestNum::UTILITY_SEND_LOG_TO_SUPPORT: {
            bool includeArchivedLogs = false;
            QDataStream paramsStream(params);
            paramsStream >> includeArchivedLogs;
            resultStream << ExitCode::Ok; // Return immediately, progress and error will be report via addError and signal

            uploadLog(includeArchivedLogs);
            break;
        }
        case RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT: {
            resultStream << ExitCode::Ok; // Return immediately, progress and error will be report via addError and signal
            LogUploadJob::cancelUpload();
            break;
        }
        case RequestNum::UTILITY_CRASH: {
            resultStream << ExitCode::Ok;
            QTimer::singleShot(QUIT_DELAY, []() { CommonUtility::crash(); });
            break;
        }
        case RequestNum::UTILITY_QUIT: {
            CommServer::instance()->setHasQuittedProperly(true);
            QTimer::singleShot(QUIT_DELAY, []() { quit(); });
            break;
        }
        case RequestNum::UTILITY_DISPLAY_CLIENT_REPORT: {
            if (_clientManuallyRestarted) {
                // If the client initially started by the server never sends the UTILITY_DISPLAY_CLIENT_REPORT,
                // we consider the client's startup aborted, and the user was forced to manually start the client again.
                if (!_appStartPTraceStopped) {
                    sentry::pTraces::basic::AppStart().stop(sentry::PTraceStatus::Aborted);
                    _appStartPTraceStopped = true;
                }
            }

            if (!_appStartPTraceStopped) {
                _appStartPTraceStopped = true;
                sentry::pTraces::basic::AppStart().stop();
            }
            break;
        }
        case RequestNum::SYNC_SETSUPPORTSVIRTUALFILES: {
            int syncDbId = 0;
            bool value = false;
            ArgsWriter(params).write(syncDbId, value);

            const ExitCode exitCode = setSupportsVirtualFiles(syncDbId, value);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in setSupportsVirtualFiles for syncDbId=" << syncDbId);
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::SYNC_SETROOTPINSTATE: {
            int syncDbId;
            PinState state;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;
            paramsStream >> state;
            std::shared_ptr<Vfs> vfsPtr;
            if (ExitInfo exitInfo = getVfsPtr(syncDbId, vfsPtr); !exitInfo) {
                LOG_WARN(_logger, "Error in getVfsPtr for syncDbId=" << syncDbId << " : " << exitInfo);
                resultStream << toInt(exitInfo.code());
                break;
            }
            if (const ExitInfo exitInfo = vfsPtr->setPinState("", state); !exitInfo) {
                LOG_WARN(_logger, "Error in vfsSetPinState for syncDbId=" << syncDbId << " : " << exitInfo);
                resultStream << toInt(exitInfo.code());
                break;
            }
            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::SYNC_PROPAGATE_SYNCLIST_CHANGE: {
            int syncDbId;
            bool restartSync;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;
            paramsStream >> restartSync;

            if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                break;
            }

            _syncPalMap[syncDbId]->syncListUpdated(restartSync);

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::UPDATER_CHANGE_CHANNEL: {
            auto channel = VersionChannel::Unknown;
            QDataStream paramsStream(params);
            paramsStream >> channel;
            _updateManager.get()->setDistributionChannel(channel);
            break;
        }
        case RequestNum::UPDATER_VERSION_INFO: {
            auto channel = VersionChannel::Unknown;
            QDataStream paramsStream(params);
            paramsStream >> channel;
            VersionInfo versionInfo = _updateManager.get()->versionInfo(channel);
            resultStream << versionInfo;
            break;
        }
        case RequestNum::UPDATER_STATE: {
            UpdateState state = _updateManager.get()->state();
            resultStream << state;
            break;
        }
        case RequestNum::UPDATER_START_INSTALLER: {
            _updateManager.get()->startInstaller();
            break;
        }
        case RequestNum::UPDATER_SKIP_VERSION: {
            QString tmp;
            QDataStream paramsStream(params);
            paramsStream >> tmp;
            AbstractUpdater::skipVersion(tmp.toStdString());
            break;
        }
        default: {
            LOG_DEBUG(_logger, "Request not implemented!");
            return;
            break;
        }
    }

    CommServer::instance()->sendReply(id, results);
}

void AppServer::startSyncsAndRetryOnError() {
    LOG_DEBUG(_logger, "Start syncs");
    if (const auto exitInfo = startSyncs(); !exitInfo) {
        LOG_WARN(_logger, "Error in startSyncsAndRetryOnError: " << exitInfo);
        if (exitInfo.code() == ExitCode::SystemError && exitInfo.cause() == ExitCause::SyncDirAccessError) {
            LOG_DEBUG(_logger, "Retry to start syncs in " << START_SYNCPALS_RETRY_INTERVAL << " ms");
            QTimer::singleShot(START_SYNCPALS_RETRY_INTERVAL, this, [=, this]() { startSyncsAndRetryOnError(); });
        }
    }
}

ExitCode AppServer::clearErrors(int syncDbId, bool autoResolved /*= false*/) {
    ExitCode exitCode;
    if (syncDbId == 0) {
        exitCode = ServerRequests::deleteErrorsServer();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in ServerRequests::deleteErrorsServer: code=" << exitCode);
        }
    } else {
        exitCode = ServerRequests::deleteErrorsForSync(syncDbId, autoResolved);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in ServerRequests::deleteErrorsForSync: code=" << exitCode);
        }
    }

    if (exitCode == ExitCode::Ok) {
        QTimer::singleShot(100, [=]() { sendErrorsCleared(syncDbId); });
    }

    return exitCode;
}

void AppServer::sendErrorsCleared(int syncDbId) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    CommServer::instance()->sendSignal(SignalNum::UTILITY_ERRORS_CLEARED, params, id);
}

void AppServer::sendQuit() {
    int id = 0;

    CommServer::instance()->sendSignal(SignalNum::UTILITY_QUIT, QByteArray(), id);
}

void AppServer::sendLogUploadStatusUpdated(LogUploadState status, int percent) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << status;
    paramsStream << percent;
    CommServer::instance()->sendSignal(SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED, params, id);

    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, status, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
    } else if (!found) {
        LOG_WARN(_logger, AppStateKey::LogUploadState << " not found in appState table");
    }

    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadPercent, std::to_string(percent), found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
    } else if (!found) {
        LOG_WARN(_logger, AppStateKey::LogUploadPercent << " not found in appState table");
    }
}

void AppServer::uploadLog(const bool includeArchivedLogs) {
    /* See AppStateKey::LogUploadState for status values
     * The return value of progressFunc is true if the upload should continue, false if the user canceled the upload
     */
    const std::function<void(LogUploadState, int)> jobProgressCallBack = [this](LogUploadState status, int progress) {
        sendLogUploadStatusUpdated(status, progress); // Send progress to the client
    };
    const auto logUploadJob = std::make_shared<LogUploadJob>(includeArchivedLogs, jobProgressCallBack, &addError);

    const std::function<void(UniqueId)> jobResultCallback = [this, logUploadJob]([[maybe_unused]] const UniqueId id) {
        if (const ExitInfo exitInfo = logUploadJob->exitInfo(); !exitInfo && exitInfo.code() != ExitCode::OperationCanceled) {
            LOG_WARN(_logger, "Error in LogArchiverHelper::sendLogToSupport: " << exitInfo);
            addError(Error(errId(), ExitCode::LogUploadFailed, exitInfo.cause()));
        }
    };
    logUploadJob->setAdditionalCallback(jobResultCallback);
    JobManager::instance()->queueAsyncJob(logUploadJob, Poco::Thread::PRIO_HIGH);
}

ExitInfo AppServer::checkIfSyncIsValid(const Sync &sync) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    // Check for nested syncs
    for (const auto &sync_: syncList) {
        if (sync_.dbId() == sync.dbId()) {
            continue;
        }
        if (CommonUtility::isSubDir(sync.localPath(), sync_.localPath()) ||
            CommonUtility::isSubDir(sync_.localPath(), sync.localPath())) {
            LOGW_WARN(_logger, L"Nested syncs - (1) dbId=" << sync.dbId() << L", " << Utility::formatSyncPath(sync.localPath())
                                                           << L"; (2) dbId=" << sync_.dbId() << L", "
                                                           << Utility::formatSyncPath(sync_.localPath()));
            return {ExitCode::InvalidSync, ExitCause::SyncDirNestingError};
        }
    }

    return ExitCode::Ok;
}

void AppServer::onScheduleAppRestart() {
    LOG_INFO(_logger, "Application restart requested!");
    _appRestartRequired = true;
}

void AppServer::onShowWindowsUpdateDialog() {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << _updateManager.get()->versionInfo();
    CommServer::instance()->sendSignal(SignalNum::UPDATER_SHOW_DIALOG, params);
}

void AppServer::onUpdateStateChanged(const UpdateState state) {
    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << state;
    CommServer::instance()->sendSignal(SignalNum::UPDATER_STATE_CHANGED, params);
}

void AppServer::onCleanup() {
    cleanup();
}

void AppServer::onRestartClientReceived() {
    bool quit = false;
#if NDEBUG
    if (clientHasCrashed()) {
        LOG_ERROR(_logger, "Client disconnected because it has crashed");
        handleClientCrash(quit);
        if (quit) {
            (void) QMessageBox::warning(0, QString(APPLICATION_NAME), crashMsg, QMessageBox::Ok);
        }
    } else {
        LOG_INFO(_logger, "Client disconnected because it was killed");
        quit = true;
    }
#else
    LOG_INFO(_logger, "Client disconnected");
#endif

    if (quit) {
        QTimer::singleShot(0, this, &AppServer::quit);
    }
}

void AppServer::onMessageReceivedFromAnotherProcess(const QString &message, QObject *) {
    LOG_DEBUG(_logger, "Message received from another kDrive process: '" << message.toStdString() << "'");

    if (message.toStdString() == showSynthesisMsg) {
        showSynthesis();
    } else if (message.toStdString() == showSettingsMsg) {
        showSettings();
    } else if (message.toStdString() == restartClientMsg) {
        _clientManuallyRestarted = true;
        if (!startClient()) {
            LOG_ERROR(_logger, "Failed to start the client");
        }
    } else {
        LOG_WARN(_logger, "Unknown message received from another kDrive process: '" << message.toStdString() << "'");
    }
}

void AppServer::onSendNotifAsked(const QString &title, const QString &message) {
    sendShowNotification(title, message);
}

void AppServer::sendShowNotification(const QString &title, const QString &message) {
    // Notify client
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << title;
    paramsStream << message;
    CommServer::instance()->sendSignal(SignalNum::UTILITY_SHOW_NOTIFICATION, params, id);
}

void AppServer::sendNewBigFolder(int syncDbId, const QString &path) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << path;
    CommServer::instance()->sendSignal(SignalNum::UTILITY_NEW_BIG_FOLDER, params, id);
}

void AppServer::sendErrorAdded(bool serverLevel, ExitCode exitCode, int syncDbId) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << serverLevel;
    paramsStream << toInt(exitCode);
    paramsStream << syncDbId;
    CommServer::instance()->sendSignal(SignalNum::UTILITY_ERROR_ADDED, params, id);
}

void AppServer::addCompletedItem(int syncDbId, const SyncFileItem &item, bool notify) {
    // Send completedItem signal to client
    SyncFileItemInfo itemInfo;
    ServerRequests::syncFileItemToSyncFileItemInfo(item, itemInfo);
    sendSyncCompletedItem(syncDbId, itemInfo);

    if (notify) {
        // Store notification
        Notification notification;
        notification._syncDbId = syncDbId;
        notification._filename = itemInfo.path();
        notification._renameTarget = itemInfo.newPath();
        notification._status = itemInfo.instruction();
        _notifications.push_back(notification);
    }
}

void AppServer::sendSignal(SignalNum sigNum, int syncDbId, const SigValueType &val) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << QVariant::fromStdVariant(val);
    CommServer::instance()->sendSignal(sigNum, params, id);
}

ExitInfo AppServer::getVfsPtr(int syncDbId, std::shared_ptr<Vfs> &vfs) {
    auto vfsMapIt = _vfsMap.find(syncDbId);
    if (vfsMapIt == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return ExitCode::LogicError;
    }
    if (!vfsMapIt->second) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs is null for syncDbId=" << syncDbId);
        return ExitCode::LogicError;
    }
    vfs = vfsMapIt->second;
    return ExitCode::Ok;
}

void AppServer::syncFileStatus(int syncDbId, const SyncPath &path, SyncFileStatus &status) {
    auto syncPalMapIt = _syncPalMap.find(syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap for syncDbId=" << syncDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = syncPalMapIt->second->fileStatus(ReplicaSide::Local, path, status);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::fileStatus for syncDbId=" << syncDbId);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

void AppServer::syncFileSyncing(int syncDbId, const SyncPath &path, bool &syncing) {
    auto syncPalMapIt = _syncPalMap.find(syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap for syncDbId=" << syncDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = syncPalMapIt->second->fileSyncing(ReplicaSide::Local, path, syncing);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::fileSyncing for syncDbId=" << syncDbId);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

void AppServer::setSyncFileSyncing(int syncDbId, const SyncPath &path, bool syncing) {
    auto syncPalMapIt = _syncPalMap.find(syncDbId);
    if (syncPalMapIt == _syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap for syncDbId=" << syncDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = syncPalMapIt->second->setFileSyncing(ReplicaSide::Local, path, syncing);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::setFileSyncing for syncDbId=" << syncDbId);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

#if defined(KD_MACOS)
void AppServer::exclusionAppList(QString &appList) {
    for (bool def: {false, true}) {
        std::vector<ExclusionApp> exclusionList;
        if (!ParmsDb::instance()->selectAllExclusionApps(def, exclusionList)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllExclusionApps");
            return;
        }

        for (const ExclusionApp &exclusionApp: exclusionList) {
            if (appList.size() > 0) {
                appList += ";";
            }
            appList += QString::fromStdString(exclusionApp.appId());
        }
    }
}
#endif

ExitCode AppServer::migrateConfiguration(bool &proxyNotSupported) {
    typedef ExitCode (MigrationParams::*migrateptr)();
    ExitCode exitCode(ExitCode::Ok);

    MigrationParams mp = MigrationParams();
    std::vector<std::pair<migrateptr, std::string>> migrateArr = {
            {&MigrationParams::migrateGeneralParams, "migrateGeneralParams"},
            {&MigrationParams::migrateAccountsParams, "migrateAccountsParams"},
            {&MigrationParams::migrateTemplateExclusion, "migrateFileExclusion"},
#if defined(KD_MACOS)
            {&MigrationParams::migrateAppExclusion, "migrateAppExclusion"},
#endif
            {&MigrationParams::migrateSelectiveSyncs, "migrateSelectiveSyncs"}};

    for (const auto &migrate: migrateArr) {
        ExitCode functionExitCode = (mp.*migrate.first)();
        if (functionExitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in " << migrate.second);
            exitCode = functionExitCode;
        }
    }

    // delete old files
    mp.deleteUselessConfigFiles();
    proxyNotSupported = mp.isProxyNotSupported();

    return exitCode;
}

ExitCode AppServer::updateUserInfo(User &user) {
    if (user.keychainKey().empty()) {
        return ExitCode::Ok;
    }

    bool found = false;
    bool updated = false;
    ExitCode exitCode = ServerRequests::loadUserInfo(user, updated);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::loadUserInfo: code=" << exitCode);
        if (exitCode == ExitCode::InvalidToken) {
            // Notify client app that the user is disconnected
            UserInfo userInfo;
            ServerRequests::userToUserInfo(user, userInfo);
            sendUserUpdated(userInfo);
        }

        return exitCode;
    }

    if (updated) {
        if (!ParmsDb::instance()->updateUser(user, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateUser");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "User not found for userDbId=" << user.dbId());
            return ExitCode::DataError;
        }

        UserInfo userInfo;
        ServerRequests::userToUserInfo(user, userInfo);
        sendUserUpdated(userInfo);
    }

    std::vector<Account> accounts;
    if (!ParmsDb::instance()->selectAllAccounts(user.dbId(), accounts)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllAccounts");
        return ExitCode::DbError;
    }

    for (auto &account: accounts) {
        QHash<int, DriveAvailableInfo> userDriveInfoList;
        ServerRequests::getUserAvailableDrives(account.userDbId(), userDriveInfoList);

        std::vector<Drive> drives;
        if (!ParmsDb::instance()->selectAllDrives(account.dbId(), drives)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
            return ExitCode::DbError;
        }

        for (auto &drive: drives) {
            bool quotaUpdated = false;
            bool accountUpdated = false;
            exitCode = ServerRequests::loadDriveInfo(drive, account, updated, quotaUpdated, accountUpdated);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::loadDriveInfo: code=" << exitCode);
                return exitCode;
            }

            if (drive.accessDenied() || drive.maintenanceInfo()._maintenance) {
                LOG_WARN(_logger, "Access denied for driveId=" << drive.driveId());

                std::vector<Sync> syncs;
                if (!ParmsDb::instance()->selectAllSyncs(drive.dbId(), syncs)) {
                    LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
                    return ExitCode::DbError;
                }

                for (auto &sync: syncs) {
                    // Pause sync
                    sync.setPaused(true);
                    ExitCause exitCause = ExitCause::DriveAccessError;
                    if (drive.maintenanceInfo()._maintenance) {
                        if (drive.maintenanceInfo()._notRenew)
                            exitCause = ExitCause::DriveNotRenew;
                        else if (drive.maintenanceInfo()._asleep)
                            exitCause = ExitCause::DriveAsleep;
                        else if (drive.maintenanceInfo()._wakingUp)
                            exitCause = ExitCause::DriveWakingUp;
                        else
                            exitCause = ExitCause::DriveMaintenance;
                    }
                    addError(Error(sync.dbId(), errId(), ExitCode::BackError, exitCause));
                }
            }

            if (userDriveInfoList.contains(drive.driveId())) {
                const DriveAvailableInfo &userDriveInfo = userDriveInfoList[drive.driveId()];
                std::string strColor = userDriveInfo.color().name().toStdString();
                if (strColor != drive.color()) {
                    drive.setColor(strColor);
                    updated = true;
                }
            }

            if (updated) {
                if (!ParmsDb::instance()->updateDrive(drive, found)) {
                    LOG_WARN(_logger, "Error in ParmsDb::updateDrive");
                    return ExitCode::DbError;
                }
                if (!found) {
                    LOG_WARN(_logger, "Drive not found for driveDbId=" << drive.dbId());
                    return ExitCode::DataError;
                }
            }

            if (quotaUpdated) {
                sendDriveQuotaUpdated(drive.dbId(), drive.size(), drive.usedSize());
            }

            bool accountRemoved = false;
            if (accountUpdated) {
                int accountDbId = 0;
                if (!ParmsDb::instance()->accountDbId(user.dbId(), account.accountId(), accountDbId)) {
                    LOG_WARN(_logger, "Error in ParmsDb::accountDbId");
                    return ExitCode::DbError;
                }

                if (accountDbId == 0) {
                    // No existing account with the new accountId, update it
                    if (!ParmsDb::instance()->updateAccount(account, found)) {
                        LOG_WARN(_logger, "Error in ParmsDb::updateAccount");
                        return ExitCode::DbError;
                    }
                    if (!found) {
                        LOG_WARN(_logger, "Account not found for accountDbId=" << account.dbId());
                        return ExitCode::DataError;
                    }

                    AccountInfo accountInfo;
                    ServerRequests::accountToAccountInfo(account, accountInfo);
                    sendAccountUpdated(accountInfo);
                } else {
                    // An account already exists with the new accountId, link the drive to it
                    drive.setAccountDbId(accountDbId);
                    if (!ParmsDb::instance()->updateDrive(drive, found)) {
                        LOG_WARN(_logger, "Error in ParmsDb::updateDrive");
                        return ExitCode::DbError;
                    }
                    if (!found) {
                        LOG_WARN(_logger, "Drive not found for driveDbId=" << drive.dbId());
                        return ExitCode::DataError;
                    }

                    updated = true;

                    // Delete the old account if not used anymore
                    std::vector<Drive> driveList;
                    if (!ParmsDb::instance()->selectAllDrives(account.dbId(), driveList)) {
                        LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
                        return ExitCode::DbError;
                    }

                    if (driveList.size() == 0) {
                        exitCode = ServerRequests::deleteAccount(account.dbId());
                        if (exitCode != ExitCode::Ok) {
                            LOG_WARN(_logger, "Error in Requests::deleteAccount: code=" << exitCode);
                            return exitCode;
                        }
                        sendAccountRemoved(account.accountId());
                        accountRemoved = true;
                    }
                }
            }

            if (updated) {
                DriveInfo driveInfo;
                ServerRequests::driveToDriveInfo(drive, driveInfo);
                sendDriveUpdated(driveInfo);
            }

            if (accountRemoved) {
                sendAccountRemoved(account.dbId());
            }
        }
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::startSyncs() {
    // Load user list
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    ExitInfo mainExitInfo = ExitCode::Ok;
    for (User &user: userList) {
        if (const auto exitInfo = startSyncs(user); !exitInfo) {
            LOG_WARN(_logger, "Error in startSyncs for userDbId=" << user.dbId() << " " << exitInfo);
            mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
        }
    }
#if defined(__APPLE__)
    Utility::restartFinderExtension();
#endif
    return mainExitInfo;
}

std::string liteSyncActivationLogMessage(bool enabled, int syncDbId) {
    const std::string activationStatus = enabled ? "enabled" : "disabled";
    std::stringstream ss;

    ss << "LiteSync is " << activationStatus << " for syncDbId=" << syncDbId;

    return ss.str();
}

// This function will pause the synchronization in case of errors.
ExitInfo AppServer::tryCreateAndStartVfs(const Sync &sync) noexcept {
    const std::string liteSyncMsg = liteSyncActivationLogMessage(sync.virtualFileMode() != VirtualFileMode::Off, sync.dbId());
    LOG_INFO(_logger, liteSyncMsg);
    if (const auto exitInfo = createAndStartVfs(sync); !exitInfo) {
        if (exitInfo != ExitInfo(ExitCode::SystemError, ExitCause::SyncDirAccessError)) {
            LOG_WARN(_logger, "Error in createAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo << ", pausing.");
            addError(Error(sync.dbId(), errId(), exitInfo));
        }
        return exitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::startSyncs(User &user) {
    logExtendedLogActivationMessage(ParametersCache::isExtendedLogEnabled());

    ExitInfo mainExitInfo = ExitCode::Ok;

    // Load account list
    std::vector<Account> accountList;
    if (!ParmsDb::instance()->selectAllAccounts(user.dbId(), accountList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllAccounts");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    std::chrono::seconds startDelay{0};
    for (Account &account: accountList) {
        // Load drive list
        std::vector<Drive> driveList;
        if (!ParmsDb::instance()->selectAllDrives(account.dbId(), driveList)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }

        for (Drive &drive: driveList) {
            // Load sync list
            std::vector<Sync> syncList;
            if (!ParmsDb::instance()->selectAllSyncs(drive.dbId(), syncList)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
                return {ExitCode::DbError, ExitCause::DbAccessError};
            }

            for (Sync &sync: syncList) {
                QSet<QString> blackList;
                QSet<QString> undecidedList;

                if (user.toMigrate()) {
                    if (!user.keychainKey().empty()) {
                        // End migration once connected
                        bool syncUpdated = false;
                        if (const auto exitInfo = processMigratedSyncOnceConnected(user.dbId(), drive.driveId(), sync, blackList,
                                                                                   undecidedList, syncUpdated);
                            !exitInfo) {
                            LOG_WARN(_logger, "Error in updateMigratedSyncPalOnceConnected for syncDbId=" << sync.dbId() << " : "
                                                                                                          << exitInfo);
                            addError(Error(sync.dbId(), errId(), exitInfo));
                            mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
                            continue;
                        }

                        if (syncUpdated) {
                            // Update sync
                            bool found = false;
                            if (!ParmsDb::instance()->updateSync(sync, found)) {
                                LOG_WARN(_logger, "Error in ParmsDb::updateSync");
                                return {ExitCode::DbError, ExitCause::DbAccessError};
                            }
                            if (!found) {
                                LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << sync.dbId());
                                return {ExitCode::DataError, ExitCause::DbEntryNotFound};
                            }

                            SyncInfo syncInfo;
                            ServerRequests::syncToSyncInfo(sync, syncInfo);
                            sendSyncUpdated(syncInfo);
                        }
                    }
                }

                // Clear old errors for this sync
                clearErrors(sync.dbId(), false);
                clearErrors(sync.dbId(), true);

                if (const auto exitInfo = checkIfSyncIsValid(sync); !exitInfo) {
                    LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    addError(Error(sync.dbId(), errId(), exitInfo));
                    continue;
                }

                bool start = !user.keychainKey().empty();

                if (const auto exitInfo = tryCreateAndStartVfs(sync); !exitInfo) {
                    LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
                    if (!(exitInfo.code() == ExitCode::SystemError && exitInfo.cause() == ExitCause::LiteSyncNotAllowed)) {
                        continue;
                    }
                    // Continue (i.e. Init SyncPal but don't start it)
                    start = false;
                }

                // Create and start SyncPal
                startDelay += std::chrono::seconds(START_SYNCPALS_TIME_GAP);
                if (const auto exitInfo =
                            initSyncPal(sync, blackList, undecidedList, QSet<QString>(), start, startDelay, false, false);
                    !exitInfo) {
                    LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    addError(Error(sync.dbId(), errId(), exitInfo));
                    mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
                }
            }
        }
    }

    if (user.toMigrate()) {
        // Migration done
        user.setToMigrate(false);

        // Update user
        bool found = false;
        if (!ParmsDb::instance()->updateUser(user, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateUser");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }
        if (!found) {
            LOG_WARN(_logger, "User not found in user table for userDbId=" << user.dbId());
            return {ExitCode::DataError, ExitCause::DbEntryNotFound};
        }
    }

    return mainExitInfo;
}

ExitInfo AppServer::processMigratedSyncOnceConnected(int userDbId, int driveId, Sync &sync, QSet<QString> &blackList,
                                                     QSet<QString> &undecidedList, bool &syncUpdated) {
    LOG_DEBUG(_logger, "Update migrated SyncPal for syncDbId=" << sync.dbId());

    // Set sync target nodeId for advanced sync
    if (!sync.targetPath().empty()) {
        // Get root subfolders
        QList<NodeInfo> nodeInfoList;
        if (const ExitInfo exitInfo = ServerRequests::getSubFolders(sync.driveDbId(), "", nodeInfoList); !exitInfo) {
            LOG_WARN(_logger, "Error in Requests::getSubFolders with driveDbId =" << sync.driveDbId() << " : " << exitInfo);
            return exitInfo;
        }

        NodeId nodeId;
        auto itemNames = CommonUtility::splitSyncPath(sync.targetPath());
        while (!nodeInfoList.empty() && !itemNames.empty()) {
            NodeInfo info = nodeInfoList.back();
            nodeInfoList.pop_back();
            if (QStr2SyncName(info.name()) == itemNames.front()) {
                itemNames.pop_front();
                if (itemNames.empty()) {
                    nodeId = info.nodeId().toStdString();
                    break;
                }

                if (const ExitInfo exitInfo = ServerRequests::getSubFolders(sync.driveDbId(), info.nodeId(), nodeInfoList);
                    !exitInfo) {
                    LOG_WARN(_logger, "Error in Requests::getSubFolders with driveDbId =" << sync.driveDbId() << " nodeId = "
                                                                                          << info.nodeId().toStdString() << " : "
                                                                                          << exitInfo);
                    return exitInfo;
                }
            }
        }

        if (!nodeId.empty()) {
            sync.setTargetNodeId(nodeId);
            syncUpdated = true;
        }
    }

    // Load migration SelectiveSync list
    std::vector<MigrationSelectiveSync> migrationSelectiveSyncList;
    if (!ParmsDb::instance()->selectAllMigrationSelectiveSync(migrationSelectiveSyncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllMigrationSelectiveSync");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    // Generate blacklist & undecidedList
    if (migrationSelectiveSyncList.size()) {
        QString nodeId;
        for (const auto &migrationSelectiveSync: migrationSelectiveSyncList) {
            if (migrationSelectiveSync.syncDbId() != sync.dbId()) {
                continue;
            }

            if (const ExitInfo exitInfo =
                        ServerRequests::getNodeIdByPath(userDbId, driveId, migrationSelectiveSync.path(), nodeId);
                !exitInfo) {
                // The folder could have been deleted in the drive
                LOGW_DEBUG(_logger, L"Error in Requests::getNodeIdByPath for userDbId="
                                            << userDbId << L" driveId=" << driveId << L" path="
                                            << Path2WStr(migrationSelectiveSync.path()) << L" : " << exitInfo);
                continue;
            }

            if (!nodeId.isEmpty()) {
                if (migrationSelectiveSync.type() == SyncNodeType::BlackList) {
                    blackList << nodeId;
                } else if (migrationSelectiveSync.type() == SyncNodeType::UndecidedList) {
                    undecidedList << nodeId;
                }
            }
        }
    }

    return ExitCode::Ok;
}

bool AppServer::initLogging() noexcept {
    IoError ioError = IoError::Success;
    SyncPath logDirPath;
    if (!IoHelper::logDirectoryPath(logDirPath, ioError)) {
        return false;
    }

    // Setup log4cplus
    const std::filesystem::path logFilePath = logDirPath / Utility::logFileNameWithTime();
    if (!Log::instance(Path2WStr(logFilePath))) {
        assert(false);
        return false;
    }

    _logger = Log::instance()->getLogger();

    return true;
}

void AppServer::logUsefulInformation() const {
    LOG_INFO(_logger, "***** APP INFO *****");

    LOG_INFO(_logger, "version: " << _theme->version());
    LOG_INFO(_logger, "os: " << CommonUtility::platformName().toStdString());
    LOG_INFO(_logger, "kernel version : " << QSysInfo::kernelVersion().toStdString());
    LOG_INFO(_logger, "kernel type : " << QSysInfo::kernelType().toStdString());
    LOG_INFO(_logger, "locale: " << QLocale::system().name().toStdString());
    LOG_INFO(_logger, "# of logical CPU core: " << std::thread::hardware_concurrency());

    // Log cache path
    SyncPath cachePath;
    if (!IoHelper::cacheDirectoryPath(cachePath)) {
        LOGW_WARN(_logger, L"Error getting cache directory");
    }
    LOGW_INFO(_logger, L"cache " << Utility::formatSyncPath(cachePath));
    LOGW_INFO(_logger, L"free space for cache: " << Utility::getFreeDiskSpace(cachePath) << L" bytes");

    // Log app ID
    AppStateValue appStateValue = "";

    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::AppUid, appStateValue, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAppState");
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
    } else if (!found) {
        LOG_WARN(Log::instance()->getLogger(), AppStateKey::AppUid << " key not found in appstate table");
    }
    const auto &appUid = std::get<std::string>(appStateValue);
    LOG_INFO(Log::instance()->getLogger(), "App ID: " << appUid);
    sentry::Handler::instance()->setAppUUID(appUid);
    // Log user IDs
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllUsers");
    }
    for (const auto &user: userList) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"User ID: " << user.userId() << L", email: " << CommonUtility::s2ws(user.email()));
    }

    // Log drive IDs
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(driveList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllDrives");
    }
    for (const auto &drive: driveList) {
        LOG_INFO(Log::instance()->getLogger(), "Drive ID: " << drive.driveId());
    }

    // Log level
    LOG_INFO(Log::instance()->getLogger(), "Log level: " << ParametersCache::instance()->parameters().logLevel());
    LOG_INFO(Log::instance()->getLogger(), "Extended log activated: " << ParametersCache::instance()->parameters().extendedLog());

    LOG_INFO(_logger, "********************");
}

bool AppServer::setupProxy() noexcept {
    return Proxy::instance(ParametersCache::instance()->parameters().proxyConfig()) != nullptr;
}

std::filesystem::path AppServer::makeDbName() {
    bool alreadyExist = false;
    return Db::makeDbName(alreadyExist);
}

std::shared_ptr<ParmsDb> AppServer::initParmsDB(const std::filesystem::path &dbPath, const std::string &version) {
    return ParmsDb::instance(dbPath, version);
}

void AppServer::handleCrashRecovery(bool &shouldQuit) {
    bool found = false;
    if (AppStateValue lastServerRestartDate = int64_t(0);
        !KDC::ParmsDb::instance()->selectAppState(AppStateKey::LastServerSelfRestartDate, lastServerRestartDate, found) ||
        !found) {
        LOG_ERROR(_logger, "Error in ParmsDb::selectAppState");
        shouldQuit = false;
    } else if (std::get<int64_t>(lastServerRestartDate) == selfRestarterDisableValue) {
        LOG_INFO(_logger, "Last session requested to not restart the server.");
        shouldQuit = _crashRecovered;
        if (!KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastServerSelfRestartDate, 0, found) || !found) {
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        }
        return;
    }

    std::string timestampStr;
    if (_crashRecovered) {
        LOG_INFO(_logger, "Server auto restart after a crash.");

        if (serverCrashedRecently()) {
            LOG_INFO(_logger, "Server crashed twice in a short time, exiting");
            QMessageBox::warning(nullptr, QString(APPLICATION_NAME), crashMsg, QMessageBox::Ok);
            if (!KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastServerSelfRestartDate, 0, found) || !found) {
                LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
            }
            shouldQuit = true;
        } else {
            LOG_INFO(_logger, "Server crashed once, restarting");
            shouldQuit = false;
        }

        long long timestamp =
                std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();
        timestampStr = std::to_string(timestamp);
    } else {
        timestampStr = std::to_string(selfRestarterNoCrashDetected);
    }

    KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastServerSelfRestartDate, timestampStr, found);
    if (!KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastServerSelfRestartDate, timestampStr, found) || !found) {
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
    }
}

bool AppServer::serverCrashedRecently(int seconds) {
    const int64_t nowSeconds =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

    AppStateValue appStateValue = int64_t(0);
    if (bool found = false;
        !KDC::ParmsDb::instance()->selectAppState(AppStateKey::LastServerSelfRestartDate, appStateValue, found)) {
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        return false;
    } else if (!found) {
        LOG_WARN(_logger, "ParmsDb::selectAppState: missing entry for key " << AppStateKey::LastServerSelfRestartDate);
        return false;
    }

    int64_t lastServerCrash = std::get<int64_t>(appStateValue);

    const auto diff = nowSeconds - lastServerCrash;
    if (diff > seconds) {
        return false;
    } else {
        LOG_WARN(_logger, "Server crashed recently: " << diff << " seconds ago");
        return true;
    }
}

bool AppServer::clientCrashedRecently(int seconds) {
    const int64_t nowSeconds =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

    AppStateValue appStateValue = int64_t(0);

    if (bool found = false;
        !KDC::ParmsDb::instance()->selectAppState(AppStateKey ::LastClientSelfRestartDate, appStateValue, found)) {
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        return false;
    } else if (!found) {
        LOG_WARN(_logger, "ParmsDb::selectAppState: missing entry for key " << AppStateKey::LastServerSelfRestartDate);
        return false;
    }

    int64_t lastClientCrash = std::get<int64_t>(appStateValue);
    const auto diff = nowSeconds - lastClientCrash;
    if (diff > seconds) {
        return false;
    } else {
        LOG_WARN(_logger, "Client crashed recently: " << diff << " seconds ago");
        return true;
    }
}

void AppServer::processInterruptedLogsUpload() {
    AppStateValue appStateValue = LogUploadState::None;

    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadState, appStateValue, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
    } else if (!found) {
        LOG_WARN(_logger, AppStateKey::LogUploadState << " not found in appstate table. LogUploadSession might be in progress "
                                                         "and will be cancelled by the backend after a timeout.");
        return;
    }

    if (auto logUploadState = std::get<LogUploadState>(appStateValue);
        logUploadState == LogUploadState::Archiving || logUploadState == LogUploadState::Uploading) {
        LOG_DEBUG(_logger, "App was closed during log upload, resetting upload status.");
        if (bool found = false;
            !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::Failed, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
            addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
        } else if (!found) {
            LOG_WARN(_logger, AppStateKey::LogUploadState << " not found in appstate table.");
        }
    } else if (logUploadState ==
               LogUploadState::CancelRequested) { // If interrupted while cancelling, consider it has been cancelled
        if (bool found = false;
            !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::Canceled, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
            addError(Error(errId(), ExitCode::DbError, ExitCause::DbAccessError));
        } else if (!found) {
            LOG_WARN(_logger, AppStateKey::LogUploadState << " not found in appstate table.");
        }
    }

    appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadToken, appStateValue, found) || !found) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
    }

    if (const auto logUploadToken = std::get<std::string>(appStateValue); !logUploadToken.empty()) {
        std::shared_ptr<UploadSessionCancelJob> cancelJob = nullptr;
        try {
            cancelJob = std::make_shared<UploadSessionCancelJob>(UploadSessionType::Log, logUploadToken);
        } catch (const std::exception &e) {
            LOG_WARN(_logger, "Error in UploadSessionCancelJob::UploadSessionCancelJob error=" << e.what());
            return;
        }
        if (const ExitCode exitCode = cancelJob->runSynchronously(); exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in UploadSessionCancelJob::runSynchronously: code=" << exitCode);
        } else {
            LOG_INFO(_logger, "Previous Log upload api call cancelled");
            if (bool found = false;
                !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, std::string(), found) || !found) {
                LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
            }
        }
    }
}

void AppServer::parseOptions(const QStringList &options) {
    // Parse options; if help or bad option exit
    QStringListIterator it(options);
    it.next(); // File name
    while (it.hasNext()) {
        QString option = it.next();
        if (option == QLatin1String("--help") || option == QLatin1String("-h")) {
            _helpAsked = true;
            break;
        } else if (option == QLatin1String("--version") || option == QLatin1String("-v")) {
            _versionAsked = true;
            break;
        } else if (option == QLatin1String("--clearSyncNodes")) {
            _clearSyncNodesAsked = true;
            break;
        } else if (option == QLatin1String("--settings")) {
            _settingsAsked = true;
            break;
        } else if (option == QLatin1String("--synthesis")) {
            _synthesisAsked = true;
            break;
        } else if (option == QLatin1String("--clearKeychainKeys")) {
            _clearKeychainKeysAsked = true;
            break;
        } else if (option == QLatin1String("--crashRecovered")) {
            _crashRecovered = true;
            break;
        } else {
            showHint("Unrecognized option '" + option.toStdString() + "'");
        }
    }
}

void AppServer::showHelp() {
    QString helpText;
    QTextStream stream(&helpText);
    stream << QString::fromStdString(_theme->appName()) << QLatin1String(" version ") << QString::fromStdString(_theme->version())
           << Qt::endl;

    stream << QLatin1String("File synchronisation desktop utility.") << Qt::endl << Qt::endl << QLatin1String(optionsC);

    displayHelpText(helpText);
}

void AppServer::showVersion() {
    displayHelpText(Theme::instance()->versionSwitchOutput());
}

void AppServer::clearSyncNodes() {
    LOG_INFO(_logger, "Clearing node DB table...");

    // Init parms DB
    bool alreadyExist;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExist);
    if (parmsDbPath.empty()) {
        LOG_WARN(_logger, "Error in Db::makeDbName");
        throw std::runtime_error("Unable to get ParmsDb path.");
    }

    if (!ParmsDb::instance(parmsDbPath, _theme->version())) {
        LOG_WARN(_logger, "Error in ParmsDb::instance");
        throw std::runtime_error("Unable to initialize ParmsDb.");
    }

    // Get sync list
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
        throw std::runtime_error("Unable to get syncs list.");
    }

    // Clear node tables
    for (const auto &sync: syncList) {
        SyncPath dbPath = sync.dbPath();
        auto syncDbPtr = std::make_shared<SyncDb>(dbPath.string(), _theme->version());
        syncDbPtr->clearNodes();
    }
}

void AppServer::sendShowSettingsMsg() {
    sendMessage(QString::fromStdString(showSettingsMsg));
}

void AppServer::sendShowSynthesisMsg() {
    sendMessage(QString::fromStdString(showSynthesisMsg));
}

void AppServer::sendRestartClientMsg() {
    sendMessage(QString::fromStdString(restartClientMsg));
}

void AppServer::showSettings() {
    int id = 0;
    CommServer::instance()->sendSignal(SignalNum::UTILITY_SHOW_SETTINGS, QByteArray(), id);
}

void AppServer::showSynthesis() {
    int id = 0;
    CommServer::instance()->sendSignal(SignalNum::UTILITY_SHOW_SYNTHESIS, QByteArray(), id);
}

void AppServer::clearKeychainKeys() {
    bool alreadyExist = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExist);
    if (parmsDbPath.empty()) {
        LOG_WARN(_logger, "Error in Db::makeDbName");
        throw std::runtime_error("Unable to get ParmsDb path.");
    }

    if (!ParmsDb::instance(parmsDbPath, _theme->version())) {
        LOG_WARN(_logger, "Error in ParmsDb::instance");
        throw std::runtime_error("Unable to initialize ParmsDb.");
    }

    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(_logger, "Error in  ParmsDb::selectAllUsers");
        throw std::runtime_error("Unable to select users.");
        return;
    }

    for (const auto &user: userList) {
        KeyChainManager::instance()->deleteToken(user.keychainKey());
    }
}

ExitCode AppServer::sendShowFileNotification(int syncDbId, const QString &filename, const QString &renameTarget,
                                             SyncFileInstruction status, int count) {
    // Check if notifications are disabled globally
    if (ParametersCache::instance()->parameters().notificationsDisabled() == NotificationsDisabled::Always) {
        return ExitCode::Ok;
    }

    // Check if notifications are disabled for this drive
    Sync sync;
    bool found = false;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found in sync table for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    Drive drive;
    if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found in drive table for driveDbId=" << sync.driveDbId());
        return ExitCode::DataError;
    }

    if (!drive.notifications()) {
        return ExitCode::Ok;
    }

    if (count > 0) {
        QString file = QDir::toNativeSeparators(filename);
        QString text;

        switch (status) {
            case SyncFileInstruction::Remove:
                if (count > 1) {
                    text = tr("%1 and %n other file(s) have been removed.", "", count - 1).arg(file);
                } else {
                    text = tr("%1 has been removed.", "%1 names a file.").arg(file);
                }
                break;
            case SyncFileInstruction::Get:
                if (count > 1) {
                    text = tr("%1 and %n other file(s) have been added.", "", count - 1).arg(file);
                } else {
                    text = tr("%1 has been added.", "%1 names a file.").arg(file);
                }
                break;
            case SyncFileInstruction::Update:
                if (count > 1) {
                    text = tr("%1 and %n other file(s) have been updated.", "", count - 1).arg(file);
                } else {
                    text = tr("%1 has been updated.", "%1 names a file.").arg(file);
                }
                break;
            case SyncFileInstruction::Move:
                if (count > 1) {
                    text = tr("%1 has been moved to %2 and %n other file(s) have been moved.", "", count - 1)
                                   .arg(file, renameTarget);
                } else {
                    text = tr("%1 has been moved to %2.").arg(file, renameTarget);
                }
                break;
            default:
                break;
        }

        if (!text.isEmpty()) {
            sendShowNotification(tr("Sync Activity"), text);
        }
    }

    return ExitCode::Ok;
}

void AppServer::showHint(std::string errorHint) {
    static QString binName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    std::cerr << errorHint << std::endl;
    std::cerr << "Try '" << binName.toStdString() << " --help' for more information" << std::endl;
    std::exit(1);
}

bool AppServer::startClient() {
    CommServer::instance()->setHasQuittedProperly(false);

    if (!CommServer::instance()->isListening()) {
        LOG_WARN(_logger, "Failed to start kDrive client (comm server isn't started)");
        return false;
    }

    bool startClient = false;
#ifdef QT_NO_DEBUG
    startClient = true;
#endif
    startClient |= QProcessEnvironment::systemEnvironment().value("KDRIVE_DEBUG_RUN_CLIENT") == "1";

    if (startClient) {
        // Start the client
        QString pathToExecutable = QCoreApplication::applicationDirPath();

#if defined(KD_WINDOWS)
        pathToExecutable += QString("/%1.exe").arg(APPLICATION_CLIENT_EXECUTABLE);
#else
        pathToExecutable += QString("/%1").arg(APPLICATION_CLIENT_EXECUTABLE);
#endif

        QStringList arguments;
        arguments << QString::number(CommServer::instance()->commPort());

        LOGW_INFO(_logger, L"Starting kDrive client - path=" << Path2WStr(QStr2Path(pathToExecutable)) << L" args="
                                                             << arguments[0].toStdWString());

        QProcess *clientProcess = new QProcess(this);
        clientProcess->setProgram(pathToExecutable);
        clientProcess->setArguments(arguments);
        clientProcess->start();
        if (!clientProcess->waitForStarted()) {
            LOG_WARN(_logger, "Failed to start kDrive client");
            return false;
        }
    }

    return true;
}

ExitCode AppServer::updateAllUsersInfo() {
    std::vector<User> users;
    if (!ParmsDb::instance()->selectAllUsers(users)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }

    for (auto &user: users) {
        std::vector<Account> accounts;
        if (!ParmsDb::instance()->selectAllAccounts(user.dbId(), accounts)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
            return ExitCode::DbError;
        }
        if (accounts.empty()) {
            LOG_INFO(_logger,
                     "User: " << user.email() << " (id:" << user.userId() << ") is not used anymore. It will be removed.");
            ServerRequests::deleteUser(user.dbId());
            sendUserRemoved(user.dbId());
            continue;
        }
        if (user.keychainKey().empty()) {
            LOG_DEBUG(_logger, "User " << user.dbId() << " is not connected");
            continue;
        }

        ExitCode exitCode = updateUserInfo(user);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in updateUserInfo: code=" << exitCode);
            return exitCode;
        }
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::initSyncPal(const Sync &sync, const NodeSet &blackList, const NodeSet &undecidedList,
                                const NodeSet &whiteList, bool start, const std::chrono::seconds &startDelay, bool resumedByUser,
                                bool firstInit) {
    if (_syncPalMap.find(sync.dbId()) == _syncPalMap.end()) {
        std::shared_ptr<Vfs> vfsPtr;
        if (ExitInfo exitInfo = getVfsPtr(sync.dbId(), vfsPtr); !exitInfo) {
            LOG_WARN(_logger, "Error in getVfsPtr for syncDbId=" << sync.dbId() << " : " << exitInfo);
            return exitInfo;
        }

        // Create SyncPal
        try {
            _syncPalMap[sync.dbId()] = std::make_shared<SyncPal>(vfsPtr, sync.dbId(), _theme->version());
        } catch (std::exception const &) {
            LOG_WARN(_logger, "Error in SyncPal::SyncPal for syncDbId=" << sync.dbId());
            return {ExitCode::DbError, ExitCause::Unknown};
        }

        // Set callbacks
        _syncPalMap[sync.dbId()]->setAddErrorCallback(&addError);
        _syncPalMap[sync.dbId()]->setAddCompletedItemCallback(&addCompletedItem);
        _syncPalMap[sync.dbId()]->setSendSignalCallback(&sendSignal);

        if (!blackList.empty()) {
            // Set blackList (create or overwrite the possible existing list in DB)
            if (const ExitInfo exitInfo = _syncPalMap[sync.dbId()]->setSyncIdSet(SyncNodeType::BlackList, blackList); !exitInfo) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << sync.dbId() << " : " << exitInfo);
                return exitInfo;
            }
        }

        if (!undecidedList.empty()) {
            // Set undecidedList (create or overwrite the possible existing list in DB)
            if (const ExitInfo exitInfo = _syncPalMap[sync.dbId()]->setSyncIdSet(SyncNodeType::UndecidedList, undecidedList);
                !exitInfo) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << sync.dbId() << " : " << exitInfo);
                return exitInfo;
            }
        }

        if (!whiteList.empty()) {
            // Set whiteList (create or overwrite the possible existing list in DB)
            if (const ExitInfo exitInfo = _syncPalMap[sync.dbId()]->setSyncIdSet(SyncNodeType::WhiteList, whiteList); !exitInfo) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << sync.dbId() << " : " << exitInfo);
                return exitInfo;
            }
        }
    }

#if defined(KD_WINDOWS) || defined(KD_MACOS)
    if (firstInit) {
        if (!_syncPalMap[sync.dbId()]->wipeOldPlaceholders()) {
            LOG_WARN(_logger, "Error in SyncPal::wipeOldPlaceholders");
        }
    }
#else
    (void) firstInit;
#endif

    if (start && (resumedByUser || !sync.paused())) {
        if (_syncPalMap[sync.dbId()]->isPaused()) {
            // Unpause SyncPal
            _syncPalMap[sync.dbId()]->unpause();
        } else if (!_syncPalMap[sync.dbId()]->isRunning()) {
            // Start SyncPal
            _syncPalMap[sync.dbId()]->start(startDelay);
        }
    }

    // Register the folder with the socket API
    if (_socketApi) {
        _socketApi->registerSync(sync.dbId());
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::initSyncPal(const Sync &sync, const QSet<QString> &blackList, const QSet<QString> &undecidedList,
                                const QSet<QString> &whiteList, bool start, const std::chrono::seconds &startDelay,
                                bool resumedByUser, bool firstInit) {
    NodeSet blackList2;
    for (const QString &nodeId: blackList) {
        blackList2.insert(nodeId.toStdString());
    }

    NodeSet undecidedList2;
    for (const QString &nodeId: undecidedList) {
        undecidedList2.insert(nodeId.toStdString());
    }

    NodeSet whiteList2;
    for (const QString &nodeId: whiteList) {
        whiteList2.insert(nodeId.toStdString());
    }

    if (const auto exitInfo =
                initSyncPal(sync, blackList2, undecidedList2, whiteList2, start, startDelay, resumedByUser, firstInit);
        !exitInfo) {
        LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " : " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::stopSyncPal(int syncDbId, bool pausedByUser, bool quit, bool clear) {
    LOG_DEBUG(_logger, "Stop SyncPal for syncDbId=" << syncDbId);

    // Unregister the folder with the socket API
    if (_socketApi) {
        _socketApi->unregisterSync(syncDbId);
    }

    // Stop SyncPal
    if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
        LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    if (_syncPalMap[syncDbId]) {
        _syncPalMap[syncDbId]->stop(pausedByUser, quit, clear);
    }

    LOG_DEBUG(_logger, "Stop SyncPal for syncDbId=" << syncDbId << " done");

    return ExitCode::Ok;
}

ExitInfo AppServer::createAndStartVfs(const Sync &sync) noexcept {
    // Check that the sync folder exists.
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(sync.localPath(), exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists " << Utility::formatIoError(sync.localPath(), ioError));
        return ExitCode::SystemError;
    }

    if (!exists) {
        LOGW_WARN(_logger, L"Sync localpath " << Utility::formatSyncPath(sync.localPath()) << L" doesn't exist.");
        auto tmpSync(sync);
        tmpSync.setPaused(true);
        if (bool found = false; !ParmsDb::instance()->updateSync(tmpSync, found) || !found) {
            LOG_WARN(_logger, "Failed to update sync status!");
        }
        return {ExitCode::SystemError, ExitCause::SyncDirAccessError};
    }

#if defined(KD_MACOS)
    if (sync.virtualFileMode() == VirtualFileMode::Mac) {
        // If the sync is the first with Mac vfs mode, reset installation/activation/connection flags
        if (noMacVfsSync()) {
            _vfsInstallationDone = false;
            _vfsActivationDone = false;
            _vfsConnectionDone = false;
        }
    }
#endif

    if (_vfsMap.find(sync.dbId()) == _vfsMap.end()) {
#if defined(KD_WINDOWS)
        Drive drive;
        bool found;
        if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectDrive");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }
        if (!found) {
            LOG_WARN(_logger, "Drive not found in drive table for driveDbId=" << sync.driveDbId());
            return {ExitCode::DataError, ExitCause::DbEntryNotFound};
        }

        Account account;
        if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAccount");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }
        if (!found) {
            LOG_WARN(_logger, "Account not found in account table for accountDbId=" << drive.accountDbId());
            return {ExitCode::DataError, ExitCause::DbEntryNotFound};
        }

        User user;
        if (!ParmsDb::instance()->selectUser(account.userDbId(), user, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectUser");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }
        if (!found) {
            LOG_WARN(_logger, "User not found in user table for userDbId=" << account.userDbId());
            return {ExitCode::DataError, ExitCause::DbEntryNotFound};
        }
#endif

        // Create VFS instance
        VfsSetupParams vfsSetupParams;
        vfsSetupParams.syncDbId = sync.dbId();
#if defined(KD_WINDOWS)
        vfsSetupParams.driveId = drive.driveId();
        vfsSetupParams.userId = user.userId();
#endif
        vfsSetupParams.localPath = sync.localPath();
        vfsSetupParams.targetPath = sync.targetPath();
        connect(this, &AppServer::socketApiExecuteCommandDirect, _socketApi.data(), &SocketApi::executeCommandDirect);
        vfsSetupParams.executeCommand = [this](const char *command) { emit socketApiExecuteCommandDirect(QString(command)); };
        vfsSetupParams.logger = _logger;
        vfsSetupParams.sentryHandler = sentry::Handler::instance();
        QString error;
        std::shared_ptr vfsPtr = KDC::createVfsFromPlugin(sync.virtualFileMode(), vfsSetupParams, error);
        if (!vfsPtr) {
            LOG_WARN(_logger,
                     "Error in Vfs::createVfsFromPlugin for mode " << sync.virtualFileMode() << " : " << error.toStdString());
            return {ExitCode::SystemError, ExitCause::UnableToCreateVfs};
        }
        _vfsMap[sync.dbId()] = vfsPtr;
        _vfsMap[sync.dbId()]->setExtendedLog(ParametersCache::isExtendedLogEnabled());

        // Set callbacks
        _vfsMap[sync.dbId()]->setSyncFileStatusCallback(&syncFileStatus);
        _vfsMap[sync.dbId()]->setSyncFileSyncingCallback(&syncFileSyncing);
        _vfsMap[sync.dbId()]->setSetSyncFileSyncingCallback(&setSyncFileSyncing);
#if defined(KD_MACOS)
        _vfsMap[sync.dbId()]->setExclusionAppListCallback(&exclusionAppList);
#endif
    }

    // Start VFS
    if (ExitInfo exitInfo = _vfsMap[sync.dbId()]->start(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone);
        !exitInfo) {
#if defined(KD_MACOS)
        if (sync.virtualFileMode() == VirtualFileMode::Mac && _vfsInstallationDone && !_vfsActivationDone) {
            // Check LiteSync ext authorizations
            if (!areMacVfsAuthsOk()) {
                return {ExitCode::SystemError, ExitCause::LiteSyncNotAllowed};
            }
        }
#endif

        LOG_WARN(_logger, "Error in Vfs::start: " << exitInfo);
        return exitInfo;
    }

#if defined(KD_WINDOWS)
    // Save sync
    Sync tmpSync(sync);
    tmpSync.setNavigationPaneClsid(_vfsMap[sync.dbId()]->namespaceCLSID());

    if (tmpSync.virtualFileMode() == KDC::VirtualFileMode::Win) {
        Utility::setFolderPinState(CommonUtility::s2ws(tmpSync.navigationPaneClsid()),
                                   _navigationPaneHelper->showInExplorerNavigationPane());
    } else {
        if (tmpSync.navigationPaneClsid().empty()) {
            tmpSync.setNavigationPaneClsid(QUuid::createUuid().toString().toStdString());
            _vfsMap[sync.dbId()]->setNamespaceCLSID(tmpSync.navigationPaneClsid());
        }
        Utility::addLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()), sync.localPath(),
                                       _navigationPaneHelper->showInExplorerNavigationPane());
    }

    bool found = false;
    if (!ParmsDb::instance()->updateSync(tmpSync, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateSync");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << tmpSync.dbId());
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }
#endif
    return ExitCode::Ok;
}

ExitInfo AppServer::stopVfs(int syncDbId, bool unregister) {
    LOG_DEBUG(_logger, "Stop VFS for syncDbId=" << syncDbId);

    // Stop Vfs
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(_logger, "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    if (_vfsMap[syncDbId]) {
        _vfsMap[syncDbId]->stop(unregister);
    }

    LOG_DEBUG(_logger, "Stop VFS for syncDbId=" << syncDbId << " done");

    return ExitCode::Ok;
}

ExitInfo AppServer::setSupportsVirtualFiles(int syncDbId, bool value) {
    if (!_syncPalMap.contains(syncDbId)) {
        std::stringstream msg;
        msg << "SyncPal not found in syncPalMap for syncDbId=" << syncDbId;
        LOG_WARN(_logger, msg.str());
        sentry::Handler::captureMessage(sentry::Level::Error, "Error in setSupportsVirtualFiles", msg.str());
        return ExitCode::LogicError;
    }

    Sync sync;
    bool found = false;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectSync");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // Check if sync is valid
    if (const auto exitInfo = checkIfSyncIsValid(sync); !exitInfo) {
        LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
        addError(Error(sync.dbId(), errId(), exitInfo));
        return exitInfo;
    }

    VirtualFileMode newMode;
    if (value && sync.virtualFileMode() == VirtualFileMode::Off) {
        newMode = KDC::bestAvailableVfsMode();
    } else {
        newMode = VirtualFileMode::Off;
    }

    if (newMode != sync.virtualFileMode()) {
        // Stop sync
        _syncPalMap[syncDbId]->stop();

        // Stop Vfs
        if (const auto exitInfo = stopVfs(syncDbId, true); !exitInfo) {
            LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
            return exitInfo;
        }

        if (newMode == VirtualFileMode::Off) {
#if defined(KD_WINDOWS)
            LOG_INFO(_logger, "Clearing node DB");
            _syncPalMap[syncDbId]->clearNodes();
#else
            // Clear file system
            _syncPalMap[sync.dbId()]->wipeVirtualFiles();
#endif
        }

#if defined(KD_WINDOWS)
        if (newMode == VirtualFileMode::Win) {
            // Remove legacy sync root keys
            Utility::removeLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()));
            sync.setNavigationPaneClsid(std::string());
        } else if (sync.virtualFileMode() == VirtualFileMode::Win) {
            // Add legacy sync root keys
            bool show = _navigationPaneHelper->showInExplorerNavigationPane();
            if (sync.navigationPaneClsid().empty()) {
                sync.setNavigationPaneClsid(QUuid::createUuid().toString().toStdString());
            }
            Utility::addLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()), sync.localPath(), show);
        }
#endif

        // Update Vfs mode in sync
        sync.setVirtualFileMode(newMode);
        if (!ParmsDb::instance()->updateSync(sync, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateSync");
            return {ExitCode::DbError, ExitCause::DbAccessError};
        }
        if (!found) {
            LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << syncDbId);
            return {ExitCode::DataError, ExitCause::DbEntryNotFound};
        }

        // Delete/create Vfs
        _vfsMap.erase(syncDbId);

        ExitInfo mainExitInfo = ExitCode::Ok;
        bool start = true;
        if (const auto exitInfo = tryCreateAndStartVfs(sync); !exitInfo) {
            LOG_WARN(_logger, "Error in tryCreateAndStartVfs: " << exitInfo);
            if (!(exitInfo.code() == ExitCode::SystemError && exitInfo.cause() == ExitCause::LiteSyncNotAllowed)) {
                return exitInfo;
            }
            // Continue (ie. Update SyncPal and convert the sync dir but don't start SyncPal)
            mainExitInfo = exitInfo;
            start = false;
        }

        // Update SyncPal
        std::shared_ptr<Vfs> vfsPtr;
        if (const auto exitInfo = getVfsPtr(sync.dbId(), vfsPtr); !exitInfo) {
            LOG_WARN(_logger, "Error in getVfsPtr for syncDbId=" << sync.dbId() << " : " << exitInfo);
            return exitInfo;
        }
        _syncPalMap[sync.dbId()]->setVfs(vfsPtr);

        QTimer::singleShot(100, this, [=, this]() {
            if (newMode != VirtualFileMode::Off) {
                // Clear file system
                _vfsMap[sync.dbId()]->convertDirContentToPlaceholder(SyncName2QStr(sync.localPath()), true);
            }

            // Update sync info on client side
            SyncInfo syncInfo;
            ServerRequests::syncToSyncInfo(sync, syncInfo);
            sendSyncUpdated(syncInfo);

            // Notify conversion completed
            sendVfsConversionCompleted(sync.dbId());

            if (start) {
                // Re-start sync
                _syncPalMap[syncDbId]->start();
            }
        });

        return mainExitInfo;
    }

    return ExitCode::Ok;
}

void AppServer::addError(const Error &error) {
    // Fetch all errors.
    std::vector<Error> errorList;
    if (!ParmsDb::instance()->selectAllErrors(error.level(), error.syncDbId(), INT_MAX, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return;
    }

    // Check if a similar error already exists.
    bool errorAlreadyExists = false;
    for (Error &existingError: errorList) {
        if (!existingError.isSimilarTo(error)) continue;
        // Update existing error time
        existingError.setTime(error.time());
        bool found = false;
        if (!ParmsDb::instance()->updateError(existingError, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateError");
            return;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "Error not found in Error table for dbId=" << existingError.dbId());
            return;
        }

        errorAlreadyExists = true;
        break;
    }

    if (!errorAlreadyExists && !ParmsDb::instance()->insertError(error)) { // Insert new error
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertError");
        return;
    }
    if (!errorAlreadyExists) errorList.push_back(error);


    User user;
    if (error.syncDbId() && ServerRequests::getUserFromSyncDbId(error.syncDbId(), user) != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::getUserFromSyncDbId");
        return;
    }

    if (ServerRequests::isDisplayableError(error)) {
        // Notify the client
        sendErrorAdded(error.level() == ErrorLevel::Server, error.exitCode(), error.syncDbId());
    }

    if (error.exitCode() == ExitCode::InvalidToken) {
        // Manage invalid token error
        LOG_DEBUG(Log::instance()->getLogger(), "Manage invalid token error");

        if (!user.dbId()) {
            LOG_WARN(Log::instance()->getLogger(), "Invalid error");
            return;
        }

        // Update user
        user.setKeychainKey(std::string());
        bool found = false;
        if (!ParmsDb::instance()->updateUser(user, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateUser");
            return;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "User not found with dbId=" << user.dbId());
            return;
        }

        UserInfo userInfo;
        ServerRequests::userToUserInfo(user, userInfo);
        sendUserUpdated(userInfo);
    } else if (error.exitCode() == ExitCode::NetworkError && error.exitCause() == ExitCause::SocketsDefuncted) {
        // Manage sockets defuncted error
        LOG_WARN(Log::instance()->getLogger(), "Manage sockets defuncted error");

        sentry::Handler::captureMessage(sentry::Level::Warning, "AppServer::addError", "Sockets defuncted error");

        // Decrease upload session max parallel jobs
        ParametersCache::instance()->decreaseUploadSessionParallelThreads();

        // Decrease JobManager pool capacity
        JobManager::instance()->decreasePoolCapacity();
    } else if (error.exitCode() == ExitCode::SystemError && error.exitCause() == ExitCause::FileAccessError) {
        // Remove child errors
        std::unordered_set<int64_t> toBeRemovedErrorIds;
        for (const Error &parentError: errorList) {
            for (const Error &childError: errorList) {
                if (CommonUtility::isDescendantOrEqual(childError.path(), parentError.path()) &&
                    childError.dbId() != parentError.dbId()) {
                    toBeRemovedErrorIds.insert(childError.dbId());
                }
            }
        }
        for (auto errorId: toBeRemovedErrorIds) {
            bool found = false;
            if (!ParmsDb::instance()->deleteError(errorId, found)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteError");
                return;
            }
            if (!found) {
                LOG_WARN(Log::instance()->getLogger(), "Error not found in Error table for dbId=" << errorId);
                return;
            }
        }
        if (!toBeRemovedErrorIds.empty()) sendErrorsCleared(error.syncDbId());
    } else if (error.exitCode() == ExitCode::UpdateRequired) {
        _updateManager.get()->updater()->unskipVersion();
    }

    if (!ServerRequests::isAutoResolvedError(error) && !errorAlreadyExists) {
        // Send error to sentry only for technical errors
        SentryUser sentryUser(user.email(), user.name(), std::to_string(user.userId()));
        sentry::Handler::captureMessage(sentry::Level::Warning, "AppServer::addError", error.errorString(), sentryUser);
    }
}

void AppServer::sendUserAdded(const UserInfo &userInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userInfo;

    CommServer::instance()->sendSignal(SignalNum::USER_ADDED, params, id);
}

void AppServer::sendUserUpdated(const UserInfo &userInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userInfo;

    CommServer::instance()->sendSignal(SignalNum::USER_UPDATED, params, id);
}

void AppServer::sendUserStatusChanged(int userDbId, bool connected, QString connexionError) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;
    paramsStream << connected;
    paramsStream << connexionError;

    CommServer::instance()->sendSignal(SignalNum::USER_STATUSCHANGED, params, id);
}

void AppServer::sendUserRemoved(int userDbId) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << userDbId;

    CommServer::instance()->sendSignal(SignalNum::USER_REMOVED, params, id);
}

void AppServer::sendAccountAdded(const AccountInfo &accountInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << accountInfo;

    CommServer::instance()->sendSignal(SignalNum::ACCOUNT_ADDED, params, id);
}

void AppServer::sendAccountUpdated(const AccountInfo &accountInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << accountInfo;

    CommServer::instance()->sendSignal(SignalNum::ACCOUNT_UPDATED, params, id);
}

void AppServer::sendAccountRemoved(int accountDbId) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << accountDbId;

    CommServer::instance()->sendSignal(SignalNum::ACCOUNT_REMOVED, params, id);
}

void AppServer::sendDriveAdded(const DriveInfo &driveInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveInfo;

    CommServer::instance()->sendSignal(SignalNum::DRIVE_ADDED, params, id);
}

void AppServer::sendDriveUpdated(const DriveInfo &driveInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveInfo;

    CommServer::instance()->sendSignal(SignalNum::DRIVE_UPDATED, params, id);
}

void AppServer::sendDriveQuotaUpdated(int driveDbId, qint64 total, qint64 used) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;
    paramsStream << total;
    paramsStream << used;

    CommServer::instance()->sendSignal(SignalNum::DRIVE_QUOTAUPDATED, params, id);
}

void AppServer::sendDriveRemoved(int driveDbId) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << driveDbId;

    CommServer::instance()->sendSignal(SignalNum::DRIVE_REMOVED, params, id);
}

void AppServer::sendSyncUpdated(const SyncInfo &syncInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncInfo;

    CommServer::instance()->sendSignal(SignalNum::SYNC_UPDATED, params, id);
}

void AppServer::sendSyncRemoved(int syncDbId) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;

    CommServer::instance()->sendSignal(SignalNum::SYNC_REMOVED, params, id);
}

void AppServer::sendSyncDeletionFailed(int syncDbId) {
    int id = 0;
    const auto params = QByteArray(ArgsReader(syncDbId));

    CommServer::instance()->sendSignal(SignalNum::SYNC_DELETE_FAILED, params, id);
}


void AppServer::sendDriveDeletionFailed(int driveDbId) {
    int id = 0;
    const auto params = QByteArray(ArgsReader(driveDbId));

    CommServer::instance()->sendSignal(SignalNum::DRIVE_DELETE_FAILED, params, id);
}


void AppServer::sendGetFolderSizeCompleted(const QString &nodeId, qint64 size) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << nodeId;
    paramsStream << size;

    CommServer::instance()->sendSignal(SignalNum::NODE_FOLDER_SIZE_COMPLETED, params, id);
}

void AppServer::sendSyncProgressInfo(int syncDbId, SyncStatus status, SyncStep step, const SyncProgress &progress) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << status;
    paramsStream << step;
    paramsStream << static_cast<qint64>(progress._currentFile);
    paramsStream << static_cast<qint64>(progress._totalFiles);
    paramsStream << static_cast<qint64>(progress._completedSize);
    paramsStream << static_cast<qint64>(progress._totalSize);
    paramsStream << static_cast<qint64>(progress._estimatedRemainingTime);
    CommServer::instance()->sendSignal(SignalNum::SYNC_PROGRESSINFO, params, id);
}

void AppServer::sendSyncCompletedItem(int syncDbId, const SyncFileItemInfo &itemInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << itemInfo;
    CommServer::instance()->sendSignal(SignalNum::SYNC_COMPLETEDITEM, params, id);
}

void AppServer::sendVfsConversionCompleted(int syncDbId) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    CommServer::instance()->sendSignal(SignalNum::SYNC_VFS_CONVERSION_COMPLETED, params, id);
}

void AppServer::sendSyncAdded(const SyncInfo &syncInfo) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncInfo;

    CommServer::instance()->sendSignal(SignalNum::SYNC_ADDED, params, id);
}

void AppServer::onLoadInfo() {
    ExitCode exitCode = updateAllUsersInfo();
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in updateAllUsersInfo: code=" << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

void AppServer::onUpdateSyncsProgress() {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
        return;
    }

    for (const auto &sync: syncList) {
        if (const auto syncPalIt = _syncPalMap.find(sync.dbId()); syncPalIt == _syncPalMap.end()) {
            // No SyncPal for this sync
            sendSyncProgressInfo(sync.dbId(), sync.paused() ? SyncStatus::Paused : SyncStatus::Error, SyncStep::None,
                                 SyncProgress());
        } else {
            if (!syncPalIt->second) {
                assert(false);
                continue;
            }

            const auto syncPal = syncPalIt->second;

            // Get progress
            SyncProgress progress;
            if (syncPal->status() == SyncStatus::Running && syncPal->step() == SyncStep::Propagation2) {
                syncPal->loadProgress(progress);
            }

            const SyncCache syncCache{syncPal->status(), syncPal->step(), progress};
            if (const auto syncCacheMapIt = _syncCacheMap.find(sync.dbId());
                syncCacheMapIt == _syncCacheMap.end() || syncCacheMapIt->second != syncCache) {
                // Set/update sync cache
                if (syncCacheMapIt == _syncCacheMap.end())
                    _syncCacheMap[sync.dbId()] = syncCache;
                else
                    syncCacheMapIt->second = syncCache;

                // Send progress to the client
                sendSyncProgressInfo(sync.dbId(), syncCache._status, syncCache._step, progress);
            }

            // New big folders detection
            NodeSet undecidedSet;
            if (const auto exitCode = syncPal->syncIdSet(SyncNodeType::UndecidedList, undecidedSet); exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::syncIdSet: code=" << exitCode);
                addError(Error(sync.dbId(), errId(), exitCode, ExitCause::Unknown));
                return;
            }

            auto [undecidedListCacheMapIt, _] = _undecidedListCacheMap.try_emplace(sync.dbId(), NodeSet());
            bool undecidedSetUpdated = false;
            for (const NodeId &nodeId: undecidedSet) {
                if (!undecidedListCacheMapIt->second.contains(nodeId)) {
                    undecidedSetUpdated = true;

                    QString path;
                    if (const auto exitCode = ServerRequests::getPathByNodeId(syncPal->userDbId(), syncPal->driveId(),
                                                                              QString::fromStdString(nodeId), path);
                        exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in Requests::getPathByNodeId: code=" << exitCode);
                        continue;
                    }

                    // Send newBigFolder signal to client
                    sendNewBigFolder(sync.dbId(), path);

                    // Ask client to display notification
                    sendShowNotification(
                            QString::fromStdString(Theme::instance()->appName()),
                            tr("A new folder larger than %1 MB has been added in the drive %2, you must validate its "
                               "synchronization: %3.\n")
                                    .arg(ParametersCache::instance()->parameters().bigFolderSizeLimit())
                                    .arg(QString::fromStdString(syncPal->driveName()), path));
                }
            }

            if (undecidedSetUpdated || undecidedListCacheMapIt->second.size() != undecidedSet.size()) {
                undecidedListCacheMapIt->second = undecidedSet;
            }
        }
    }
}

void AppServer::onSendFilesNotifications() {
    if (!_notifications.empty()) {
        // Ask client to display notification
        Notification notification = _notifications[0];
        ExitCode exitCode = sendShowFileNotification(notification._syncDbId, notification._filename, notification._renameTarget,
                                                     notification._status, static_cast<int>(_notifications.size()));
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in sendShowFileNotification");
            addError(Error(errId(), exitCode, ExitCause::Unknown));
        }

        _notifications.clear();
    }
}

void AppServer::onRestartSyncs() {
#if defined(KD_MACOS)
    if (!noMacVfsSync() && _vfsInstallationDone && !_vfsActivationDone) {
        // Check LiteSync ext authorizations
        if (areMacVfsAuthsOk()) {
            LOG_INFO(Log::instance()->getLogger(), "LiteSync extension activation done");
            _vfsActivationDone = true;

            // Clear LiteSyncNotAllowed error
            ExitCode exitCode = ServerRequests::deleteLiteSyncNotAllowedErrors();
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(Log::instance()->getLogger(),
                         "Error in ServerRequests::deleteLiteSyncNotAllowedErrors: " << toInt(exitCode));
            }

            for (const auto &syncPalMapElt: _syncPalMap) {
                if (_vfsMap[syncPalMapElt.first]->mode() == VirtualFileMode::Mac) {
                    // Ask client to refresh SyncPal error list
                    sendErrorsCleared(syncPalMapElt.first);

                    // Start VFS
                    if (ExitInfo exitInfo =
                                _vfsMap[syncPalMapElt.first]->start(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone);
                        !exitInfo) {
                        LOG_WARN(Log::instance()->getLogger(), "Error in Vfs::start: " << exitInfo);
                        continue;
                    }

                    // Start SyncPal if not paused
                    Sync sync;
                    bool found = false;
                    if (!ParmsDb::instance()->selectSync(syncPalMapElt.first, sync, found)) {
                        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
                        continue;
                    }

                    if (!found) {
                        LOG_WARN(Log::instance()->getLogger(),
                                 "Sync not found in sync table for syncDbId=" << syncPalMapElt.first);
                        continue;
                    }

                    if (!sync.paused()) {
                        // Start sync
                        syncPalMapElt.second->start();
                    }
                }
            }
        }
    }
#endif

    for (const auto &[syncId, syncPtr]: _syncPalMap) {
        if (!syncPtr) continue;
        if ((syncPtr->isPaused() || syncPtr->pauseAsked()) &&
            syncPtr->pauseTime() + std::chrono::minutes(1) < std::chrono::steady_clock::now()) {
            syncPtr->unpause();
#if defined(__APPLE__)
            Utility::restartFinderExtension();
#endif
        }
    }
}
} // namespace KDC
