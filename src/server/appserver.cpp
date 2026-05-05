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

#include "appserver.h"
#include "version.h"
#include "migration/migrationparams.h"
#include "keychainmanager/keychainmanager.h"
#include "requests/syncnodecache.h"
#include "requests/offlinefilessizeestimator.h"
#include "comm/guijobmanager.h"
#if defined(KD_MACOS) || defined(KD_WINDOWS)
#include "comm/extjobmanager.h"
#endif
#include "updater/updatemanager.h"

#include "jobs/network/kDrive_API/searchjob.h"
#include "jobs/network/kDrive_API/upload/loguploadjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/uploadsessioncanceljob.h"

#include "server/comm/guijobs/signalaccountaddedjob.h"
#include "server/comm/guijobs/signalaccountupdatedjob.h"
#include "server/comm/guijobs/signalaccountremovedjob.h"
#include "server/comm/guijobs/signaluseraddedjob.h"
#include "server/comm/guijobs/signaluserupdatedjob.h"
#include "server/comm/guijobs/signaluserremovedjob.h"
#include "server/comm/guijobs/signaldriveaddedjob.h"
#include "server/comm/guijobs/signaldriveupdatedjob.h"
#include "server/comm/guijobs/signaldriveremovedjob.h"
#include "server/comm/guijobs/signalsyncaddedjob.h"
#include "server/comm/guijobs/signalsyncremovedjob.h"
#include "server/comm/guijobs/signalsynccompleteditemjob.h"
#include "server/comm/guijobs/signalsyncupdatedjob.h"
#include "server/comm/guijobs/signalerroraddedjob.h"
#include "server/comm/guijobs/signalerrorremovedjob.h"
#include "server/comm/guijobs/signalsyncprogressinfojob.h"
#include "server/comm/guijobs/signalupdatershowdialogjob.h"
#include "server/comm/guijobs/signalupdaterstatechangedjob.h"
#include "server/comm/guijobs/signalutilityshownotificationjob.h"
#include "server/comm/guijobs/signalutilityshowsettingsjob.h"
#include "server/comm/guijobs/signalutilityshowsynthesisjob.h"
#include "server/comm/guijobs/signalutilityloguploadstatejob.h"
#include "server/comm/guijobs/signalutilityquitjob.h"

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
#include "libsyncengine/jobs/syncjobmanager.h"
#include "utility/timerutility.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

#if defined(KD_WINDOWS)
#include <windows.h>
#endif

#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QIcon>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <comm/guijobs/excltemplsetlistjob.h>
#include <comm/guijobs/excltemplgetlistjob.h>

#define QUIT_DELAY 1000 // ms
#define LOAD_PROGRESS_INTERVAL 1000 // ms
#define SEND_NOTIFICATIONS_INTERVAL 15000 // ms
#define RESTART_SYNCS_INTERVAL 15000 // ms
#define START_SYNCPALS_RETRY_INTERVAL 60000 // ms
#define START_SYNCPALS_TIME_GAP 5 // sec

namespace KDC {

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

static const QString showSynthesisMsg("showSynthesis");
static const QString showSettingsMsg("showSettings");
static const QString restartClientMsg("restartClient");
static const QString authorizationCodeMsg("redirectLogin");
static const QString separatorMsg("$$$");

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

std::shared_ptr<CommManager> AppServer::_commManager = nullptr;


AppServer::AppServer(int &argc, char **argv) :
    SharedTools::QtSingleApplication(QString::fromStdString(Theme::instance()->appName()), argc, argv) {
    _arguments = arguments();
    _theme = Theme::instance();
    installEventFilter(&_eventFilter);
    (void) connect(&_eventFilter, &AuthorizationCodeEventFilter::authorizationCodeReceived, this,
                   &AppServer::onAuthorizationCodeReceived);
}

AppServer::~AppServer() {
    if (_clientProcess && _clientProcess->isOpen()) {
        _clientProcess->kill();
    }

    if (Log::isSet()) {
        LOG_DEBUG(_logger, "~AppServer");
    }
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
    if (!IoHelper::checkIfPathExists(parmsDbPath, newDbExists, ioError, IoHelper::PathCheckOption::Insensitive) ||
        ioError != IoError::Success) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(parmsDbPath, ioError));
        throw std::runtime_error("Unable to check if ParmsDb exists.");
    }

    std::filesystem::path pre334ConfigFilePath =
            std::filesystem::path(QStr2SyncName(MigrationParams::configDir().filePath(MigrationParams::configFileName())));
    bool oldConfigExists;
    if (!IoHelper::checkIfPathExists(pre334ConfigFilePath, oldConfigExists, ioError, IoHelper::PathCheckOption::Insensitive) ||
        ioError != IoError::Success) {
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
            addError(Error(ERR_ID, exitCode, exitCode == ExitCode::SystemError ? ExitCause::MigrationError : ExitCause::Unknown));
        }

        if (proxyNotSupported) {
            addError(Error(ERR_ID, ExitCode::DataError, ExitCause::MigrationProxyNotImplemented));
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

    // Update Sentry configuration
    sentry::Handler::instance()->setAppUUID(appUID());
    if (KDRIVE_VERSION_MAJOR < 4) {
        sentry::Handler::instance()->setIsSentryActivated(true);
    } else {
        sentry::Handler::instance()->setIsSentryActivated(ParametersCache::instance()->parameters().sentryEnabled());
    }

    // Setup translations
    CommonUtility::setupTranslations(this, ParametersCache::instance()->parameters().language());

    // Configure logger
    if (!Log::instance()->configure(ParametersCache::instance()->parameters().useLog(),
                                    ParametersCache::instance()->parameters().logLevel(),
                                    ParametersCache::instance()->parameters().purgeOldLogs())) {
        LOG_WARN(_logger, "Error in Log::configure");
        addError(Error(ERR_ID, ExitCode::SystemError, ExitCause::Unknown));
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
    NavigationPaneHelper::showInExplorerNavigationPane();
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

    // Setup auto start
#ifdef NDEBUG
#if defined(KD_WINDOWS)
    (void) Utility::setLaunchOnStartup(_theme->appName(), _theme->appClientName(),
                                       false); // Disable it first to make sure the path is updated in registry
#endif

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

    if (useCommManager(false)) {
        // Init CommManager
        LOG_DEBUG(_logger, "Init CommManager");
        _commManager = std::make_shared<CommManager>(*this);
        _commManager->start();
    }

    if (useOldCommServer()) {
        // Init OldCommServer instance
        LOG_DEBUG(_logger, "Init OldCommServer");
        if (!OldCommServer::instance()) {
            LOG_WARN(_logger, "Error in OldCommServer::instance");
            throw std::runtime_error("Unable to initialize OldCommServer.");
        }

        connect(OldCommServer::instance().get(), &OldCommServer::requestReceived, this, &AppServer::onRequestReceived);
        connect(OldCommServer::instance().get(), &OldCommServer::clientDisconnected, this,
                &AppServer::onClientDisconnectedReceived);
    }

    // Update users/accounts/drives info
    if (const auto exitInfo = updateAllUsersInfo(); exitInfo.code() == ExitCode::InvalidToken) {
        // The user will be asked to enter its credentials later
    } else if (!exitInfo) {
        LOG_WARN(_logger, "Error in updateAllUsersInfo: " << exitInfo);
        addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
    }

    // Set sentry user
    updateSentryUser();

    // Read Updater activation flag
    AppStateValue noUpdateAppStateValue = 0;
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::NoUpdate, noUpdateAppStateValue, found) || !found) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAppState");
        throw std::runtime_error("Unable to read NoUpdate value in app_state table.");
    }

    const bool noUpdateDb = static_cast<bool>(std::get<int>(noUpdateAppStateValue));

    if (!noUpdateDb) {
        if (!_noUpdate) { // If not already set by command line argument
            // Check if the file "no_update" exists in the application directory to disable updates
            const auto noUpdateFilePath = CommonUtility::applicationFilePath().parent_path() / SyncPath("no_update");
            bool noUpdateFlagFileExists = false;
            ioError = IoError::Success;
            if (!IoHelper::checkIfPathExists(noUpdateFilePath, noUpdateFlagFileExists, ioError,
                                             IoHelper::PathCheckOption::Insensitive) ||
                ioError != IoError::Success) {
                std::string errorMsg = "Error in checkIfPathExists(noUpdateFilePath, ...): ioError=" + toString(ioError);
                LOG_ERROR(_logger, errorMsg);
                KDC::sentry::Handler::captureMessage(KDC::sentry::Level::Error, "noUpdateFilePath lookup error", errorMsg);
            }
            _noUpdate = noUpdateFlagFileExists;
        }

        if (_noUpdate) {
            LOG_INFO(_logger, "Updater disabled by command line argument or no_update file");
            // Update Updater activation flag
            noUpdateAppStateValue = 1;
            if (bool found = false;
                !ParmsDb::instance()->updateAppState(AppStateKey::NoUpdate, noUpdateAppStateValue, found) || !found) {
                LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
                throw std::runtime_error("Unable to update NoUpdate value in app_state table.");
            }
        }
    } else {
        _noUpdate = true;
        LOG_INFO(_logger, "Updater disabled by app_state table");
    }


    if (!_noUpdate) {
        // Update checks
        _updateManager = std::make_unique<UpdateManager>();
        connect(_updateManager.get(), &UpdateManager::requestRestart, this, &AppServer::onScheduleAppRestart);
#if defined(KD_MACOS)
        const std::function<void()> quitCallback = std::bind_front(&AppServer::sendQuit, this);
        _updateManager.get()->setQuitCallback(quitCallback);
#endif

        (void) connect(_updateManager.get(), &UpdateManager::updateStateChanged, this, &AppServer::onUpdateStateChanged,
                       Qt::QueuedConnection);
        (void) connect(_updateManager.get(), &UpdateManager::updateAnnouncement, this, &AppServer::onSendNotifAsked,
                       Qt::QueuedConnection);
        (void) connect(_updateManager.get(), &UpdateManager::showUpdateDialog, this, &AppServer::onShowWindowsUpdateDialog,
                       Qt::QueuedConnection);
        (void) connect(_updateManager.get(), &UpdateManager::requireUpdate, this, &AppServer::onUpdateRequired,
                       Qt::QueuedConnection);
    }

    // Check last crash to avoid crash loop
    bool shouldQuit = false;
    handleCrashRecovery(shouldQuit);
    if (shouldQuit) {
        LOG_WARN(_logger, "Crash loop detected");
        QTimer::singleShot(0, this, &AppServer::quit);
        return;
    }

#if defined(KD_MACOS)
    if (ParmsDb::instance()->versionUpdated()) Utility::restartLoginItemAgent();
#endif

    // Start syncs
    LOG_DEBUG(_logger, "Start syncs");
    QTimer::singleShot(0, [=, this]() { startSyncsAndRetryOnError(); });

    // Init JobManager(s)
    if (!GuiJobManagerSingleton::instance()) {
        LOG_WARN(_logger, "Error in GuiJobManager::instance");
        throw std::runtime_error("Unable to initialize GUI job manager.");
    }
    if (!SyncJobManagerSingleton::instance()) {
        LOG_WARN(_logger, "Error in SyncJobManager::instance");
        throw std::runtime_error("Unable to initialize Sync job manager.");
    }
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    if (!ExtJobManagerSingleton::instance()) {
        LOG_WARN(_logger, "Error in ExtJobManager::instance");
        throw std::runtime_error("Unable to initialize Ext job manager.");
    }
#endif

    // Process possible interrupted logs upload
    processInterruptedLogsUpload();

    if (!Utility::registerLoginRedirection()) {
        std::string errorMsg = "Failed to register login redirection";
        LOG_ERROR(_logger, errorMsg);
        KDC::sentry::Handler::captureMessage(KDC::sentry::Level::Error, "Login redirection registration error", errorMsg);
    }

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

    // Stop CommManager
    if (useCommManager()) {
        _commManager->stop();
        LOG_DEBUG(_logger, "CommManager stopped");
    }

    // Stop JobManager(s)
    GuiJobManagerSingleton::instance()->stop();
    LOG_DEBUG(_logger, "GuiJobManager stopped");

    SyncJobManagerSingleton::instance()->stop();
    LOG_DEBUG(_logger, "SyncJobManager stopped");

#if defined(KD_MACOS) || defined(KD_WINDOWS)
    ExtJobManagerSingleton::instance()->stop();
    LOG_DEBUG(_logger, "ExtJobManager stopped");
#endif

    stopAllSyncPals();
    stopAllVfs();

    // Clear CommManager
    if (useCommManager()) {
        _commManager.reset();
    }

    // Clear JobManager(s)
    GuiJobManagerSingleton::clear();
    LOG_DEBUG(_logger, "GuiJobManager::clear() done");

    SyncJobManagerSingleton::clear();
    LOG_DEBUG(_logger, "SyncJobManager::clear() done");

#if defined(KD_MACOS) || defined(KD_WINDOWS)
    ExtJobManagerSingleton::clear();
    LOG_DEBUG(_logger, "ExtJobManager::clear() done");
#endif

    // Clear maps
    {
        const std::scoped_lock lock(syncPalMapMutex);
        syncPalMap.clear();
    }
    {
        const std::scoped_lock lock(vfsMapMutex);
        vfsMap.clear();
    }

    // Close ParmsDb
    ParmsDb::instance()->close();
    LOG_DEBUG(_logger, "ParmsDb closed");
}

void AppServer::reset() {
    _updateManager.reset();
}

// This task can be long and block the GUI
void AppServer::stopSyncTask(const SyncDbId syncDbId,
                             const SyncPal::DbBehaviorAfterStop behavior /*= SyncPal::DbBehaviorAfterStop::Keep*/) {
    // Stop sync and remove it from syncPalMap
    if (const auto exitInfo = stopSyncPal(syncDbId, SyncPal::PauseCaller::Sync, behavior); !exitInfo) {
        LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncDbId << " : " << exitInfo);
    }

    // Stop Vfs
    if (const auto exitInfo = stopVfs(syncDbId, true); !exitInfo) {
        LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << syncDbId << " : " << exitInfo);
    }

    {
        const std::scoped_lock lock(syncPalMapMutex);
        LOG_IF_FAIL(!syncPalMap[syncDbId] || syncPalMap[syncDbId].use_count() == 1)
        (void) syncPalMap.erase(syncDbId);
    }

    {
        const std::scoped_lock lock(vfsMapMutex);
        LOG_IF_FAIL(!vfsMap[syncDbId] ||
                    vfsMap[syncDbId].use_count() <= 1) // `use_count` can be zero when the local drive has been removed.
        (void) vfsMap.erase(syncDbId);
    }
}

ExitInfo AppServer::setSupportsVirtualFilesAsync(const SyncDbId syncDbId, bool value) {
    return setSupportsVirtualFiles(syncDbId, value, true);
}

ExitInfo AppServer::setSupportsVirtualFiles(const SyncDbId syncDbId, bool value) {
    return setSupportsVirtualFiles(syncDbId, value, false);
}

void AppServer::stopAllSyncPals() {
    const std::scoped_lock lock(syncPalMapMutex);
    for (const auto &[syncDbId, _]: syncPalMap) {
        if (const auto exitInfo = stopSyncPal(syncDbId, SyncPal::PauseCaller::Sync, SyncPal::DbBehaviorAfterStop::Keep);
            !exitInfo) {
            LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncDbId << exitInfo);
        }
    }
    LOG_DEBUG(_logger, "Syncpal(s) stopped");
}

void AppServer::stopAllVfs() {
    const std::scoped_lock lock(vfsMapMutex);
    for (const auto &[syncDbId, _]: vfsMap) {
        if (const auto exitInfo = stopVfs(syncDbId, false); !exitInfo) {
            LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << syncDbId << exitInfo);
        }
    }
    LOG_DEBUG(_logger, "Vfs(s) stopped");
}

void AppServer::stopAllSyncsTask(const std::vector<SyncDbId> &syncDbIdList,
                                 const SyncPal::DbBehaviorAfterStop behavior /*= SyncPal::DbBehaviorAfterStop::Keep*/) {
    for (const auto syncDbId: syncDbIdList) {
        stopSyncTask(syncDbId, behavior);
    }
}

void AppServer::deleteAccount(const AccountDbId accountDbId) {
    // Delete the account
    const ExitCode exitCode = ServerRequests::deleteAccount(accountDbId);
    if (exitCode == ExitCode::Ok) {
        sendAccountRemoved(accountDbId);
    } else {
        LOG_WARN(_logger, "Error in Requests::deleteAccount: code=" << exitCode);
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
        return;
    }
}

void AppServer::deleteDrive(const DriveDbId driveDbId) {
    // Get the drive in DB
    bool found = false;
    Drive drive;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::Unknown));
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found for driveDbId=" << driveDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
    }

    // Delete the drive
    const ExitCode exitCode = ServerRequests::deleteDrive(driveDbId);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::deleteDrive: code=" << exitCode);
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
        sendDriveDeletionFailed(driveDbId);
        return;
    }

    // Delete the account if there is no remaining drive
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(drive.accountDbId(), driveList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::Unknown));
    } else if (driveList.empty()) {
        deleteAccount(drive.accountDbId());
    } else {
        sendDriveRemoved(driveDbId); // Useless if deleteAccount is called, sendAccountRemoved already updates the drive list.
                                     // May even cause a crash if both are processed concurrently on the GUI side.
    }
}

void AppServer::deleteSync(const SyncDbId syncDbId) {
    // Get the sync in DB
    bool found = false;
    Sync sync;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::Unknown));
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
    }

    // Delete the sync
    const ExitCode exitCode = ServerRequests::deleteSync(syncDbId);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::deleteSync: code=" << exitCode);
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
        sendSyncDeletionFailed(syncDbId);
        return;
    }

    // Delete the drive if there is no remaining sync
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(sync.driveDbId(), syncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::Unknown));
    } else if (syncList.empty()) {
        deleteDrive(sync.driveDbId());
    } else {
        sendSyncRemoved(syncDbId); // Useless if deleteDrive is called, sendDriveRemoved already updates the sync list.
                                   // May even cause a crash if both sendDriveRemoved and sendSyncRemoved are processed
                                   // concurrently on the GUI side.
    }
}

void AppServer::logExtendedLogActivationMessage(const bool isExtendedLogEnabled) noexcept {
    const std::string activationStatus = isExtendedLogEnabled ? "enabled" : "disabled";
    const std::string msg = "Extended logging is " + activationStatus + ".";

    LOG_INFO(_logger, msg);
}

ExitInfo AppServer::updateParametersAndPropagateChanges(const ParametersInfo &newParametersInfo) {
    // Retrieve current settings
    const Parameters oldParametersInfo = ParametersCache::instance()->parameters();
    std::string pwd;
    if (oldParametersInfo.proxyConfig().needsAuth()) {
        // Read pwd from keystore
        bool found = false;
        if (!KeyChainManager::instance()->readDataFromKeystore(oldParametersInfo.proxyConfig().token(), pwd, found)) {
            LOG_WARN(_logger, "Failed to read proxy pwd from keychain");
        }
        if (!found) {
            LOG_DEBUG(_logger, "Proxy pwd not found for keychainKey=" << oldParametersInfo.proxyConfig().token());
        }
    }

    // Update parameters
    const ExitCode exitCode = ServerRequests::updateParameters(newParametersInfo);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::updateParameters");
        addError(Error(ERR_ID, exitCode));
    }

    // Propagate extendedLog change
    if (oldParametersInfo.extendedLog() != newParametersInfo.extendedLog()) {
        logExtendedLogActivationMessage(newParametersInfo.extendedLog());
        const std::scoped_lock lock(vfsMapMutex);
        for (const auto &[_, vfs]: vfsMap) {
            vfs->setExtendedLog(newParametersInfo.extendedLog());
        }
    }

    // Propagate language change
    if (oldParametersInfo.language() != newParametersInfo.language()) {
        Language language = newParametersInfo.language();
        QTimer::singleShot(100, this, [this, language]() { CommonUtility::setupTranslations(this, language); });
    }

    // Propagate ProxyConfig change
    if (oldParametersInfo.proxyConfig().type() != newParametersInfo.proxyConfigInfo().type() ||
        oldParametersInfo.proxyConfig().hostName() != newParametersInfo.proxyConfigInfo().hostName().toStdString() ||
        oldParametersInfo.proxyConfig().port() != newParametersInfo.proxyConfigInfo().port() ||
        oldParametersInfo.proxyConfig().needsAuth() != newParametersInfo.proxyConfigInfo().needsAuth() ||
        oldParametersInfo.proxyConfig().user() != newParametersInfo.proxyConfigInfo().user().toStdString() ||
        pwd != newParametersInfo.proxyConfigInfo().pwd().toStdString()) {
        // Note: The parameters cache has been updated with the parameters new values.
        Proxy::instance()->setProxyConfig(ParametersCache::instance()->parameters().proxyConfig());
    }

    // Propagate autostart change
    if (oldParametersInfo.autoStart() != newParametersInfo.autoStart()) {
        auto *theme = Theme::instance();
        (void) Utility::setLaunchOnStartup(theme->appName(), theme->appName(), newParametersInfo.autoStart());
    }

    // Propagate Sentry activation change
    if (KDRIVE_VERSION_MAJOR >= 4) {
        if (oldParametersInfo.sentryEnabled() != newParametersInfo.sentryEnabled()) {
            sentry::Handler::instance()->setIsSentryActivated(newParametersInfo.sentryEnabled());
        }
    }

    // Propagate distribution channel change
    setDistributionChannel(newParametersInfo.distributionChannel());

    return exitCode;
}

ExitInfo AppServer::sendAppStartTrace() {
    if (_clientManuallyRestarted) {
        // If the client initially started by the server never sends the UTILITY_SEND_APP_START_TRACE,
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

    return ExitCode::Ok;
}

void AppServer::updateSentryUser() {
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
            addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
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
            addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
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

void AppServer::registerSync(std::shared_ptr<SyncPal> syncPal) {
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    if (CommonUtility::isMac() || (CommonUtility::isWindows() && syncPal->vfsMode() == VirtualFileMode::Off)) {
        if (useCommManager()) {
            _commManager->registerSync(syncPal->localPath().native());
        }
    }
#else
    (void) syncPal;
#endif
}

void AppServer::unregisterSync(std::shared_ptr<SyncPal> syncPal) {
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    if (CommonUtility::isMac() || (CommonUtility::isWindows() && syncPal->vfsMode() == VirtualFileMode::Off)) {
        if (useCommManager()) {
            _commManager->unregisterSync(syncPal->localPath().native());
        }
    }
#else
    (void) syncPal;
#endif
}

std::string AppServer::appUID() const {
    AppStateValue appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::AppUid, appStateValue, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAppState");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
        return {};
    } else if (!found) {
        LOG_WARN(Log::instance()->getLogger(), AppStateKey::AppUid << " key not found in appstate table");
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::DbEntryNotFound));
        return {};
    }
    return std::get<std::string>(appStateValue);
}

#if defined(KD_MACOS)
bool AppServer::noMacVfsSync() {
    const std::scoped_lock lock(vfsMapMutex);
    for (const auto &[_, vfs]: vfsMap) {
        if (vfs->mode() == VirtualFileMode::Mac) {
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

void AppServer::setDistributionChannel(const VersionChannel versionChannel) {
    if (_noUpdate) return;

    if (!_updateManager) {
        LOG_WARN(_logger, "The update manager is not set.");
        return;
    }

    _updateManager->setDistributionChannel(versionChannel);
}

VersionInfo AppServer::getVersionInfo(const VersionChannel versionChannel) const {
    if (_noUpdate) {
        return VersionInfo::current();
    }

    if (!_updateManager) {
        LOG_WARN(_logger, "The update manager is not set.");
        return VersionInfo::current();
    }

    return _updateManager->versionInfo(versionChannel);
}

UpdateState AppServer::getUpdateState() const {
    if (_noUpdate) return UpdateState::NoUpdate;

    if (!_updateManager) {
        LOG_WARN(_logger, "The update manager is not set.");
        return UpdateState::Unknown;
    }
    return _updateManager->state();
}

void AppServer::refreshUpdateState() {
    if (_noUpdate) return;

    if (!_updateManager) {
        LOG_WARN(_logger, "The update manager is not set.");
        return;
    }
    return _updateManager->forceRefresh();
}

void AppServer::startInstaller() {
    if (!_updateManager) {
        LOG_WARN(_logger, "The update manager is not set.");
        return;
    }
    _updateManager->startInstaller();
}

void AppServer::crash() const {
    // SIGSEGV crash
    CommonUtility::crash();
}

void AppServer::onRequestReceived(int id, RequestNum num, const QByteArray &params) {
    auto results = QByteArray();
    QDataStream resultStream(&results, QIODevice::WriteOnly);

    if (CommonUtility::envVarValue("KDRIVE_COMM_USE_LITTLE_ENDIAN") ==
        "1") { // .NET comm classes use little endian. For dev purpose this var allow to switch endianness.
        resultStream.setByteOrder(QDataStream::LittleEndian);
    }

    switch (num) {
        case RequestNum::LOGIN_REQUESTTOKEN: {
            QString code;
            QString codeVerifier;
            QDataStream paramsStream(params);
            paramsStream >> code;
            paramsStream >> codeVerifier;

            UserInfo userInfo;
            bool userCreated = false;
            std::string error;
            std::string errorDescr;
            ExitCode exitCode = ServerRequests::requestToken(code, codeVerifier, userInfo, userCreated, error, errorDescr);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::requestToken: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }
            updateSentryUser();
            if (userCreated) {
                sendUserAdded(userInfo);
            } else {
                sendUserUpdated(userInfo);
            }

            resultStream << toInt(exitCode);
            if (exitCode == ExitCode::Ok) {
                resultStream << static_cast<qint64>(userInfo.dbId());
            } else {
                resultStream << QString::fromStdString(error);
                resultStream << QString::fromStdString(errorDescr);
            }
            break;
        }
        case RequestNum::USER_DBIDLIST: {
            QList<UserDbId> tmpList;
            ExitCode exitCode = ServerRequests::getUserDbIdList(tmpList);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getUserDbIdList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            QList<qint64> list;
            for (const auto userDbId: tmpList) {
                list << static_cast<qint64>(userDbId);
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
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::USER_DELETE: {
            // As the actual deletion task is postponed via a timer,
            // this request returns immediately with `ExitCode::Ok`.
            // Errors are reported via the addError method.

            resultStream << ExitCode::Ok;

            qint64 tmpUserDbId = 0;
            ArgsWriter(params).write(tmpUserDbId);

            const auto userDbId = static_cast<UserDbId>(tmpUserDbId);

            // Get syncs do delete
            std::vector<SyncDbId> syncDbIdList;
            const std::scoped_lock lock(syncPalMapMutex);
            for (const auto &[syncPalId, syncPal]: syncPalMap) {
                if (!syncPal) continue;
                if (syncPal->userDbId() == userDbId) {
                    syncDbIdList.push_back(syncPalId);
                }
            }

            // Stop syncs for this user and remove them from syncPalMap.
            QTimer::singleShot(100, [this, userDbId, syncDbIdList]() {
                AppServer::stopAllSyncsTask(syncDbIdList, SyncPal::DbBehaviorAfterStop::Remove);

                // Delete user from DB
                const ExitCode exitCode = ServerRequests::deleteUser(userDbId);
                if (exitCode == ExitCode::Ok) {
                    sendUserRemoved(userDbId);
                } else {
                    LOG_WARN(_logger, "Error in Requests::deleteUser: code=" << exitCode);
                    addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
                }
            });

            break;
        }
        case RequestNum::ERROR_INFOLIST_LEGACY: {
            ErrorLevel level{ErrorLevel::Unknown};
            qint64 tmpSyncDbId{0};
            int limit{100};
            ArgsWriter(params).write(level, tmpSyncDbId, limit);

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);

            QList<ErrorInfo> list;
            const auto exitCode = ServerRequests::getErrorInfoList(level, syncDbId, limit, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getErrorInfoList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::ERROR_GET_CONFLICTS_LEGACY: {
            qint64 tmpDriveDbId = 0;
            QList<ConflictType> filter;
            QDataStream paramsStream(params);
            paramsStream >> tmpDriveDbId;
            paramsStream >> filter;

            QList<ErrorInfo> list;

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);

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
                    addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
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
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::ERROR_DELETE_SYNC: {
            qint64 tmpSyncDbId = 0;
            bool autoResolved = false;

            QDataStream paramsStream(params);
            paramsStream >> tmpSyncDbId;
            paramsStream >> autoResolved;
            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);

            ExitCode exitCode = clearErrors(syncDbId, autoResolved);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in AppServer::clearErrors: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::ERROR_DELETE_INVALIDTOKEN: {
            ExitCode exitCode = ServerRequests::deleteInvalidTokenErrors();
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::userLoggedIn: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << ExitCode::Ok;

            break;
        }
        case RequestNum::ERROR_RESOLVE_CONFLICTS_LEGACY: {
            qint64 tmpDriveDbId = 0;
            bool keepLocalVersion = false;

            QDataStream paramsStream(params);
            paramsStream >> tmpDriveDbId;
            paramsStream >> keepLocalVersion;

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);

            // Retrieve all sync related to this drive
            std::vector<Sync> syncs;
            if (!ParmsDb::instance()->selectAllSyncs(driveDbId, syncs)) {
                LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
                resultStream << ExitCode::DbError;
                break;
            }

            ExitCode exitCode = ExitCode::Ok;
            for (auto &sync: syncs) {
                const std::scoped_lock lock(syncPalMapMutex);
                auto syncPalMapIt = syncPalMap.find(sync.dbId());
                if (syncPalMapIt == syncPalMap.end()) {
                    LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << sync.dbId());
                    exitCode = ExitCode::DataError;
                    break;
                }
                if (!syncPalMapIt->second) {
                    LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << sync.dbId());
                    exitCode = ExitCode::DataError;
                    break;
                }

                std::vector<Error> keepLocalErrorList;
                std::vector<Error> keepRemoteErrorList;
                if (keepLocalVersion) {
                    exitCode = ServerRequests::getConflictList(sync.dbId(), conflictsWithLocalRename, keepLocalErrorList);
                } else {
                    exitCode = ServerRequests::getConflictList(sync.dbId(), conflictsWithLocalRename, keepRemoteErrorList);
                }

                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger,
                             "Error in ServerRequests::getConflictList for syncDbId=" << sync.dbId() << " : code=" << exitCode);
                    break;
                }

                if (!keepLocalErrorList.empty() || !keepRemoteErrorList.empty()) {
                    exitCode = syncPalMapIt->second->fixConflictingFilesAsync(keepLocalErrorList, keepRemoteErrorList);
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in SyncPal::setUpConflictingFilesCorrector for syncDbId=" << sync.dbId());
                        break;
                    }
                }
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::ERROR_RESOLVE_UNSUPPORTED_CHAR_LEGACY: {
            // TODO : not implemented yet
            resultStream << ExitCode::LogicError;
            break;
        }
        case RequestNum::USER_AVAILABLEDRIVES: {
            qint64 tmpUserDbId = 0;
            QDataStream paramsStream(params);
            paramsStream >> tmpUserDbId;

            const auto userDbId = static_cast<UserDbId>(tmpUserDbId);

            QList<DriveAvailableInfo> list;
            const auto exitInfo = ServerRequests::getUserAvailableDrives(userDbId, list);
            if (!exitInfo) {
                LOG_WARN(_logger, "Error in Requests::getUserAvailableDrives");
                addError(Error(ERR_ID, exitInfo));
            }

            resultStream << toInt(exitInfo.code());
            resultStream << list;
            break;
        }
        case RequestNum::ACCOUNT_INFOLIST: {
            QList<AccountInfo> list;
            ExitCode exitCode = ServerRequests::getAccountInfoList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getAccountInfoList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
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
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::DRIVE_UPDATE: {
            DriveInfo driveInfo;
            QDataStream paramsStream(params);
            paramsStream >> driveInfo;

            ExitCode exitCode = ServerRequests::updateDrive(driveInfo);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::updateDrive: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::DRIVE_DELETE: {
            // As the actual deletion task is postponed via a timer,
            // this request returns immediately with `ExitCode::Ok`.
            // Errors are reported via the addError method.

            resultStream << ExitCode::Ok;

            qint64 tmpDriveDbId = 0;
            ArgsWriter(params).write(tmpDriveDbId);

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);

            // Get syncs to delete
            std::vector<SyncDbId> syncDbIdList;
            const std::scoped_lock lock(syncPalMapMutex);
            for (const auto &[syncPalId, syncPal]: syncPalMap) {
                if (!syncPal) continue;
                if (syncPal->driveDbId() == driveDbId) {
                    syncDbIdList.push_back(syncPalId);
                }
            }

            // Stop syncs for this drive and remove them from syncPalMap
            QTimer::singleShot(100, [this, driveDbId, syncDbIdList]() {
                AppServer::stopAllSyncsTask(syncDbIdList, SyncPal::DbBehaviorAfterStop::Remove);
                AppServer::deleteDrive(driveDbId);
            });
#if defined(KD_MACOS)
            Utility::restartFinderExtension();
#endif
            break;
        }
        case RequestNum::DRIVE_SEARCH: {
            qint64 tmpDriveDbId = 0;
            QString searchString;
            ArgsWriter(params).write(tmpDriveDbId, searchString);

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);

            // Find drive ID
            Drive drive;
            bool found = false;
            if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectSync");
                resultStream << ExitCode::DbError;
                break;
            }
            if (!found) {
                LOG_WARN(_logger, "Drive not found for ID: " << driveDbId);
                resultStream << ExitCode::DataError;
                break;
            }

            // Send search request (synchronously for now)
            SearchJob searchJob(driveDbId, searchString.toStdString());
            (void) searchJob.runSynchronously();
            QList<SearchInfo> list;
            for (const auto &searchInfo: searchJob.searchResults()) {
                list << searchInfo;
            }

            resultStream << ExitCode::Ok;
            resultStream << list;
            resultStream << searchJob.hasMore();
            resultStream << QString::fromStdString(searchJob.cursor());
            break;
        }
        case RequestNum::SYNC_INFOLIST: {
            QList<SyncInfo> list;
            const auto exitCode = ServerRequests::getSyncInfoList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSyncInfoList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::SYNC_START: {
            qint64 tmpSyncDbId = 0;
            QDataStream paramsStream(params);
            paramsStream >> tmpSyncDbId;

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
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
                addError(Error(sync.dbId(), ERR_ID, exitInfo));
                resultStream << toInt(exitInfo.code());
                break;
            }

            ExitInfo mainExitInfo = ExitCode::Ok;
            bool startPostponed = false;
            if (const auto exitInfo = tryCreateAndStartVfs(sync, startPostponed); !exitInfo) {
                LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
                if (!Utility::isLiteSyncExtError(exitInfo)) {
                    resultStream << toInt(exitInfo.code());
                    break;
                }
                mainExitInfo = exitInfo;
            }

            if (const auto exitInfo = initSyncPal(sync, NodeSet(), !startPostponed, std::chrono::seconds(0), true, false);
                !exitInfo) {
                LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " : " << exitInfo);
                addError(Error(ERR_ID, exitInfo));
                mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
            }
#if defined(KD_MACOS)
            Utility::restartFinderExtension();
#endif
            resultStream << mainExitInfo.code();
            break;
        }
        case RequestNum::SYNC_STOP: {
            qint64 tmpSyncDbId = 0;
            QDataStream paramsStream(params);
            paramsStream >> tmpSyncDbId;

            resultStream << ExitCode::Ok;

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
            QTimer::singleShot(100, [=, this]() {
                // Stop SyncPal
                if (const auto exitInfo = stopSyncPal(syncDbId, SyncPal::PauseCaller::User); !exitInfo) {
                    LOG_WARN(_logger, "Error in stopSyncPal for syncDbId=" << syncDbId << " : " << exitInfo);
                }

                // Note: we do not Stop Vfs in case of a pause
            });

            break;
        }
        case RequestNum::SYNC_STATUS: {
            qint64 tmpSyncDbId = 0;
            QDataStream paramsStream(params);
            paramsStream >> tmpSyncDbId;

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
            const std::scoped_lock lock(syncPalMapMutex);
            auto syncPalMapIt = syncPalMap.find(syncDbId);
            if (syncPalMapIt == syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << SyncStatus::Undefined;
                break;
            }
            if (!syncPalMapIt->second) {
                LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << SyncStatus::Undefined;
                break;
            }

            SyncStatus status = syncPalMapIt->second->status();

            resultStream << ExitCode::Ok;
            resultStream << status;
            break;
        }
        case RequestNum::SYNC_ADD:
        case RequestNum::SYNC_ADD2: {
            qint64 tmpUserDbId = 0;
            qint64 tmpAccountId = 0;
            qint64 tmpDriveId = 0;
            qint64 tmpDriveDbId = 0;
            QString localFolderPath;
            QString serverFolderPath;
            QString serverFolderNodeId;
            bool liteSync = false;
            QSet<QString> blackList;
            if (num == RequestNum::SYNC_ADD) {
                ArgsWriter(params).write(tmpUserDbId, tmpAccountId, tmpDriveId, localFolderPath, serverFolderPath,
                                         serverFolderNodeId, liteSync, blackList);
            } else {
                ArgsWriter(params).write(tmpDriveDbId, localFolderPath, serverFolderPath, serverFolderNodeId, liteSync,
                                         blackList);
            }

            const auto userDbId = static_cast<UserDbId>(tmpUserDbId);
            const auto accountId = static_cast<AccountId>(tmpAccountId);
            const auto driveId = static_cast<DriveId>(tmpDriveId);
            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);

            // Add sync in DB
            ExitCode exitCode = ExitCode::Ok;
            SyncInfo syncInfo;
            if (num == RequestNum::SYNC_ADD) {
                AccountInfo accountInfo;
                DriveInfo driveInfo;

                exitCode = ServerRequests::addSync(userDbId, accountId, driveId, localFolderPath, serverFolderPath,
                                                   serverFolderNodeId, liteSync, accountInfo, driveInfo, syncInfo);

                if (exitCode != ExitCode::Ok) {
                    LOGW_WARN(_logger, L"Error in Requests::addSync - userDbId="
                                               << userDbId << L" accountId=" << accountId << L" driveId=" << driveId
                                               << L" localFolderPath=" << QStr2WStr(localFolderPath) << L" serverFolderPath="
                                               << QStr2WStr(serverFolderPath) << L" serverFolderNodeId="
                                               << serverFolderNodeId.toStdWString() << L" liteSync=" << liteSync);
                    addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
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
                                                   syncInfo);

                if (exitCode != ExitCode::Ok) {
                    LOGW_WARN(_logger, L"Error in Requests::addSync for driveDbId="
                                               << driveDbId << L" localFolderPath=" << Path2WStr(QStr2Path(localFolderPath))
                                               << L" serverFolderPath=" << Path2WStr(QStr2Path(serverFolderPath)) << L" liteSync="
                                               << liteSync);
                    addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
                    resultStream << toInt(exitCode);
                    break;
                }
            }

            sendSyncAdded(syncInfo);

            resultStream << toInt(exitCode);
            if (exitCode == ExitCode::Ok) {
                resultStream << static_cast<qint64>(syncInfo.dbId());
            }

            QTimer::singleShot(100, this, [=, this]() {
                Sync sync;
                ServerRequests::syncInfoToSync(syncInfo, sync);

                // Check if sync is valid
                if (const auto exitInfo = checkIfSyncIsValid(sync); !exitInfo) {
                    LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    addError(Error(sync.dbId(), ERR_ID, exitInfo));
                    return;
                }

                bool startPostponed = false;
                if (const auto exitInfo = tryCreateAndStartVfs(sync, startPostponed); !exitInfo) {
                    LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    if (!Utility::isLiteSyncExtError(exitInfo)) {
                        return;
                    }
                }

                // Create and start SyncPal
                if (const auto exitInfo = initSyncPal(sync, blackList, !startPostponed, std::chrono::seconds(0), false, true);
                    !exitInfo) {
                    stopSyncTask(syncInfo.dbId(), SyncPal::DbBehaviorAfterStop::Remove);

                    // Delete sync from DB
                    if (const ExitInfo exitInfo2 = ServerRequests::deleteSync(syncInfo.dbId()); !exitInfo2) {
                        LOG_WARN(_logger, "Error in Requests::deleteSync for syncDbId=" << syncInfo.dbId() << " : " << exitInfo2);
                        addError(Error(ERR_ID, exitInfo));
                    }

                    sendSyncRemoved(syncInfo.dbId());
                }
#if defined(KD_MACOS)
                Utility::restartFinderExtension();
#endif
            });
            break;
        }
        case RequestNum::SYNC_START_AFTER_LOGIN: {
            qint64 tmpUserDbId = 0;
            QDataStream paramsStream(params);
            paramsStream >> tmpUserDbId;

            const auto userDbId = static_cast<UserDbId>(tmpUserDbId);
            User user;
            bool found = false;
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
            // the postponed task records errors through calls to `addError` and use a dedicated client-server signal
            // for deletion failure.
            resultStream << ExitCode::Ok;

            qint64 tmpSyncDbId = 0;
            ArgsWriter(params).write(tmpSyncDbId);

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
            QTimer::singleShot(100, [this, syncDbId]() {
                AppServer::stopSyncTask(
                        syncDbId, SyncPal::DbBehaviorAfterStop::Remove); // This task can be long, hence blocking, on Windows.

                // Delete sync from DB
                deleteSync(syncDbId);
#if defined(KD_MACOS)
                Utility::restartFinderExtension();
#endif
            });

            break;
        }
        case RequestNum::SYNC_GETPUBLICLINKURL: {
            qint64 tmpDriveDbId = 0;
            QString nodeId;
            QDataStream paramsStream(params);
            paramsStream >> tmpDriveDbId;
            paramsStream >> nodeId;

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);
            std::string linkUrl;
            const auto exitCode = ServerRequests::getPublicLinkUrl(driveDbId, nodeId.toStdString(), linkUrl);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getLinkUrl");
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << QString::fromStdString(linkUrl);

            sendShowNotification(tr("Share link copied to clipboard"), QString::fromStdString(linkUrl));

            break;
        }
        case RequestNum::SYNC_GETPRIVATELINKURL: {
            qint64 tmpDriveDbId = 0;
            QString fileId;
            QDataStream paramsStream(params);
            paramsStream >> tmpDriveDbId;
            paramsStream >> fileId;

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);
            QString linkUrl;
            ExitCode exitCode = ServerRequests::getPrivateLinkUrl(driveDbId, fileId, linkUrl);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getLinkUrl");
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << linkUrl;
            break;
        }
        case RequestNum::SYNC_TRIGGER_PROGRESS_UPDATE: {
            triggerSyncProgressUpdate();

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::BLACKLISTED_NODE_LIST: {
            qint64 tmpSyncDbId = 0;
            QDataStream paramsStream(params);
            paramsStream >> tmpSyncDbId;

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
            const std::scoped_lock lock(syncPalMapMutex);
            auto syncPalMapIt = syncPalMap.find(syncDbId);
            if (syncPalMapIt == syncPalMap.end()) {
                LOG_DEBUG(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << QSet<QString>();
                break;
            }
            if (!syncPalMapIt->second) {
                LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << QSet<QString>();
                break;
            }

            NodeSet nodeIdSet;
            ExitCode exitCode = syncPalMapIt->second->syncIdSet(SyncNodeType::BlackList, nodeIdSet);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::getSyncIdSet: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            QSet<QString> nodeIdSet2;
            for (const NodeId &nodeId: nodeIdSet) {
                nodeIdSet2 << QString::fromStdString(nodeId);
            }

            resultStream << toInt(exitCode);
            resultStream << nodeIdSet2;
            break;
        }
        case RequestNum::BLACKLISTED_NODE_SETLIST: {
            qint64 tmpSyncDbId = 0;
            QSet<QString> nodeIdSet;
            QDataStream paramsStream(params);
            paramsStream >> tmpSyncDbId;
            paramsStream >> nodeIdSet;

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
            const std::scoped_lock lock(syncPalMapMutex);
            const auto syncPalMapIt = syncPalMap.find(syncDbId);
            if (syncPalMapIt == syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                break;
            }
            if (!syncPalMapIt->second) {
                LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                break;
            }

            NodeSet nodeIdSet2;
            for (const QString &nodeId: nodeIdSet) {
                nodeIdSet2.insert(nodeId.toStdString());
            }

            ExitCode exitCode = syncPalMapIt->second->setSyncIdSet(SyncNodeType::BlackList, nodeIdSet2);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }
            exitCode = syncPalMapIt->second->syncListUpdated(true);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::syncListUpdated: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }
            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::NODE_PATH: {
            qint64 tmpSyncDbId = 0;
            QString nodeId;
            QDataStream paramsStream(params);
            paramsStream >> tmpSyncDbId;
            paramsStream >> nodeId;

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
            const std::scoped_lock lock(syncPalMapMutex);
            const auto syncPalMapIt = syncPalMap.find(syncDbId);
            if (syncPalMapIt == syncPalMap.end()) {
                LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << "";
                break;
            }
            if (!syncPalMapIt->second) {
                LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                resultStream << "";
                break;
            }

            QString path;
            ExitInfo exitInfo = ServerRequests::getPathByNodeId(syncPalMapIt->second->userDbId(), syncPalMapIt->second->driveId(),
                                                                nodeId, path);
            if (!exitInfo) {
                if (exitInfo.cause() == ExitCause::NotFound) {
                    (void) SyncNodeCache::instance()->deleteSyncNode(syncDbId, QStr2Str(nodeId));
                } else {
                    LOG_WARN(_logger, "Error in AppServer::getPathByNodeId: " << exitInfo);
                    addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
                }
            }

            resultStream << toInt(exitInfo.code());
            resultStream << path;
            break;
        }
        case RequestNum::NODE_INFO: {
            qint64 userDbId = 0;
            qint64 driveId = 0;
            QString nodeId;
            bool withPath = false;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;
            paramsStream >> driveId;
            paramsStream >> nodeId;
            paramsStream >> withPath;

            NodeInfo nodeInfo;
            const auto exitInfo = ServerRequests::getNodeInfo(userDbId, driveId, nodeId, nodeInfo, withPath);
            if (!exitInfo) {
                LOG_WARN(_logger, "Error in Requests::getNodeInfo");
                addError(Error(ERR_ID, exitInfo));
            }

            resultStream << toInt(exitInfo.code());
            resultStream << nodeInfo;
            break;
        }
        case RequestNum::NODE_SUBFOLDERS: {
            qint64 tmpUserDbId = 0;
            qint64 tmpDriveId = 0;
            QString nodeId;
            bool withPath = false;
            QDataStream paramsStream(params);
            paramsStream >> tmpUserDbId;
            paramsStream >> tmpDriveId;
            paramsStream >> nodeId;
            paramsStream >> withPath;

            const auto userDbId = static_cast<UserDbId>(tmpUserDbId);
            const auto driveId = static_cast<DriveId>(tmpDriveId);
            QList<NodeInfo> subfoldersList;
            const auto exitInfo = ServerRequests::getSubFolders(userDbId, driveId, nodeId, subfoldersList, withPath);
            if (exitInfo.code() != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSubFolders");
                addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
            }

            resultStream << toInt(exitInfo);
            resultStream << subfoldersList;
            break;
        }
        case RequestNum::NODE_SUBFOLDERS2: {
            qint64 tmpDriveDbId = 0;
            QString nodeId;
            bool withPath = false;
            QDataStream paramsStream(params);
            paramsStream >> tmpDriveDbId;
            paramsStream >> nodeId;
            paramsStream >> withPath;

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbId);
            QList<NodeInfo> subfoldersList;
            const auto exitInfo = ServerRequests::getSubFolders(driveDbId, nodeId, subfoldersList, withPath);
            if (exitInfo.code() != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSubFolders");
                addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
            }

            resultStream << toInt(exitInfo);
            resultStream << subfoldersList;
            break;
        }
        case RequestNum::NODE_FOLDER_SIZE: {
            qint64 tmpUserDbId = 0;
            qint64 tmpDriveId = 0;
            QString nodeId;
            QDataStream paramsStream(params);
            paramsStream >> tmpUserDbId;
            paramsStream >> tmpDriveId;
            paramsStream >> nodeId;

            const auto userDbId = static_cast<UserDbId>(tmpUserDbId);
            const auto driveId = static_cast<UserDbId>(tmpDriveId);

            auto getFolderSizeWithCallbackFunc = std::function<void()>([userDbId, driveId, nodeId, this]() {
                (void) ServerRequests::getFolderSizeWithCallback(userDbId, driveId, nodeId.toStdString(),
                                                                 std::bind_front(&AppServer::sendGetFolderSizeCompleted, this));
            });
            StdLoggingThread getFolderSize(getFolderSizeWithCallbackFunc);
            getFolderSize.detach();

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::NODE_CREATEMISSINGFOLDERS_LEGACY: {
            qint64 tmpDriveDbID = 0;
            QList<QPair<QString, QString>> folderList;
            QDataStream paramsStream(params);
            paramsStream >> tmpDriveDbID;
            paramsStream >> folderList;

            const auto driveDbId = static_cast<DriveDbId>(tmpDriveDbID);
            // Pause all syncs of the drive
            QList<SyncDbId> pausedSyncs;
            const std::scoped_lock lock(syncPalMapMutex);
            for (const auto &[syncPalId, syncPal]: syncPalMap) {
                if (!syncPal) continue;
                if (syncPal->driveDbId() == driveDbId && !syncPal->isPaused()) {
                    syncPal->pause();
                    pausedSyncs.push_back(syncPalId);
                }
            }

            // Create missing folders
            QString parentNodeId(QString::fromStdString(SyncDb::driveRootNode().nodeIdRemote().value()));
            QString firstCreatedNodeId;
            for (auto &folderElt: folderList) {
                if (folderElt.second.isEmpty()) {
                    QString newNodeId;
                    const ExitCode exitCode = ServerRequests::createDir(driveDbId, parentNodeId, folderElt.first, newNodeId);
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in Requests::createDir for driveDbId=" << driveDbId << " parentNodeId="
                                                                                        << parentNodeId.toStdString());
                        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
                        resultStream << toInt(exitCode);
                        resultStream << "";
                        break;
                    }
                    if (firstCreatedNodeId.isEmpty()) {
                        firstCreatedNodeId = newNodeId;
                    }
                    parentNodeId = newNodeId;
                } else {
                    parentNodeId = folderElt.second;
                }
            }

            // Add first created node to blacklist of all syncs
            for (const auto &[syncPalId, syncPal]: syncPalMap) {
                if (!syncPal) continue;
                if (syncPal->driveDbId() == driveDbId) {
                    // Get blacklist
                    NodeSet nodeIdSet;
                    ExitCode exitCode = syncPal->syncIdSet(SyncNodeType::BlackList, nodeIdSet);
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in SyncPal::syncIdSet for syncDbId=" << syncPalId);
                        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
                        resultStream << toInt(exitCode);
                        resultStream << "";
                        break;
                    }

                    // Set blacklist
                    nodeIdSet.insert(firstCreatedNodeId.toStdString());
                    exitCode = syncPal->setSyncIdSet(SyncNodeType::BlackList, nodeIdSet);
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << syncPalId);
                        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
                        resultStream << toInt(exitCode);
                        resultStream << "";
                        break;
                    }
                }
            }

            // Resume all paused syncs
            for (const auto syncDbId: pausedSyncs) {
                if (syncPalMap.contains(syncDbId)) {
                    syncPalMap[syncDbId]->unpause();
                }
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
            bool def = false;
            QDataStream paramsStream(params);
            paramsStream >> def;

            std::vector<ExclusionTemplateInfo> exclusionTemplateList;
            if (auto exitCode = ServerRequests::getExclusionTemplateList(def, exclusionTemplateList); exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getExclusionTemplateList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
                resultStream << toInt(exitCode);
                break;
            }

            ExclusionTemplateInfo::normalizeExclusionTemplateInfoList(exclusionTemplateList);

            QList<ExclusionTemplateInfo> list;
            (void) std::for_each(exclusionTemplateList.begin(), exclusionTemplateList.end(),
                                 [&list](const ExclusionTemplateInfo &templateInfo) { list.append(templateInfo); });

            resultStream << toInt(ExitCode::Ok);
            resultStream << list;
            break;
        }
        case RequestNum::EXCLTEMPL_SETUSERLIST: {
            QList<ExclusionTemplateInfo> list;
            QDataStream paramsStream(params);
            paramsStream >> list;

            std::vector<ExclusionTemplateInfo> exclusionTemplateList;
            (void) std::for_each(list.begin(), list.end(), [&exclusionTemplateList](const ExclusionTemplateInfo &templateInfo) {
                exclusionTemplateList.push_back(templateInfo);
            });

            ExclusionTemplateInfo::updateExclusionTemplateInfoList(exclusionTemplateList);


            const auto exitCode = ServerRequests::setUserExclusionTemplateList(exclusionTemplateList);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::setExclusionTemplateList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
                resultStream << toInt(exitCode);
                break;
            }

            resultStream << toInt(exitCode);
            break;
        }
#if defined(KD_MACOS)
        case RequestNum::EXCLAPP_GETLIST: {
            bool def = false;
            QDataStream paramsStream(params);
            paramsStream >> def;

            QList<ExclusionAppInfo> list;
            const auto exitCode = ServerRequests::getExclusionAppList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getExclusionAppList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            resultStream << list;
            break;
        }
        case RequestNum::EXCLAPP_SETLIST: {
            bool def = false;
            QList<ExclusionAppInfo> list;
            QDataStream paramsStream(params);
            paramsStream >> def;
            paramsStream >> list;

            ExitCode exitCode = ServerRequests::setExclusionAppList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::setExclusionAppList: code=" << exitCode);
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            if (exitCode == ExitCode::Ok) {
                const std::scoped_lock lock(vfsMapMutex);
                for (const auto &[_, vfs]: vfsMap) {
                    if (vfs->mode() == VirtualFileMode::Mac) {
                        if (!vfs->setAppExcludeList()) {
                            exitCode = ExitCode::SystemError;
                            LOG_WARN(_logger, "Error in Vfs::setAppExcludeList!");
                            addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
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
            const std::scoped_lock lock(vfsMapMutex);
            for (const auto &[_, vfs]: vfsMap) {
                if (vfs->mode() == VirtualFileMode::Mac) {
                    if (!vfs->getFetchingAppList(appTable)) {
                        exitCode = ExitCode::SystemError;
                        LOG_WARN(_logger, "Error in Vfs::getFetchingAppList!");
                        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
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
            const auto exitCode = ServerRequests::getParameters(parametersInfo);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getParameters");
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
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
                addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
            }

            // extendedLog change propagation
            if (parameters.extendedLog() != parametersInfo.extendedLog()) {
                logExtendedLogActivationMessage(parametersInfo.extendedLog());
                const std::scoped_lock lock(vfsMapMutex);
                for (const auto &[_, vfs]: vfsMap) {
                    vfs->setExtendedLog(parametersInfo.extendedLog());
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
            QString basePath;
            QDataStream paramsStream(params);
            paramsStream >> basePath;

            QString path;
            QString error;
            const auto exitInfo = ServerRequests::findGoodPathForNewSync(basePath, path, error);
            if (!exitInfo) {
                LOG_WARN(_logger, "Error in Requests::findGoodPathForNewSyncFolder");
                addError(Error(ERR_ID, exitInfo));
            }

            resultStream << toInt(exitInfo.code());
            resultStream << path;
            resultStream << error;
            break;
        }
        case RequestNum::UTILITY_BESTVFSAVAILABLEMODE_LEGACY: {
            VirtualFileMode mode = KDC::bestAvailableVfsMode();

            resultStream << ExitCode::Ok;
            resultStream << mode;
            break;
        }
        case RequestNum::UTILITY_ACTIVATELOADINFO: {
            bool value = false;
            QDataStream paramsStream(params);
            paramsStream >> value;

            if (value) {
                QTimer::singleShot(100, this, &AppServer::onLoadInfo);
                triggerSyncProgressUpdate();
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
                addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
                resultStream << ExitCode::DbError;
                break;
            } else if (!found) {
                LOG_WARN(_logger, key << " not found in appState table");
                resultStream << ExitCode::DataError;
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
                addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
                resultStream << ExitCode::DbError;
                break;
            } else if (!found) {
                LOG_WARN(_logger, key << " not found in appState table");
                resultStream << ExitCode::DataError;
                break;
            }
            std::string appStateValueStr = std::get<std::string>(appStateValue);

            resultStream << ExitCode::Ok;
            resultStream << QString::fromStdString(appStateValueStr);
            break;
        }
        case RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE_LEGACY: {
            uint64_t logSize = 0;
            IoError ioError = IoError::Success;
            const bool res = LogUploadJob::getLogDirEstimatedSize(logSize, ioError);
            if (!res || ioError != IoError::Success) {
                LOG_WARN(_logger, "Error in LogArchiver::getLogDirEstimatedSize: " << IoHelper::ioError2StdString(ioError));

                addError(Error(ERR_ID, ExitCode::SystemError, ExitCause::Unknown));
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
            (void) LogUploadJob::cancelUpload();
            break;
        }
        case RequestNum::UTILITY_CRASH: {
            resultStream << ExitCode::Ok;
            QTimer::singleShot(QUIT_DELAY, []() { CommonUtility::crash(); });
            break;
        }
        case RequestNum::UTILITY_QUIT: {
            if (useOldCommServer()) {
                OldCommServer::instance()->setHasQuittedProperly(true);
            }
            QTimer::singleShot(QUIT_DELAY, []() { quit(); });
            break;
        }
        case RequestNum::UTILITY_SEND_APP_START_TRACE: {
            (void) sendAppStartTrace();
            break;
        }
        case RequestNum::SYNC_SETSUPPORTSVIRTUALFILES: {
            qint64 tmpSyncDbId = 0;
            bool value = false;
            ArgsWriter(params).write(tmpSyncDbId, value);

            const auto syncDbId = static_cast<SyncDbId>(tmpSyncDbId);
            const auto exitInfo = setSupportsVirtualFilesAsync(syncDbId, value);
            if (!exitInfo) {
                LOG_WARN(_logger, "Error in setSupportsVirtualFiles for syncDbId=" << syncDbId << " : " << exitInfo);
                resultStream << toInt(exitInfo.code());
                break;
            }

            resultStream << toInt(exitInfo.code());
            break;
        }
        case RequestNum::UPDATER_CHANGE_CHANNEL: {
            if (_noUpdate) {
                assert(false);
            } else {
                auto channel = VersionChannel::Unknown;
                QDataStream paramsStream(params);
                paramsStream >> channel;
                _updateManager.get()->setDistributionChannel(channel);
            }
            break;
        }
        case RequestNum::UPDATER_VERSION_INFO: {
            if (_noUpdate) {
                VersionInfo versionInfo;
                versionInfo.tag = CommonUtility::versionTag();
                versionInfo.buildVersion = CommonUtility::versionBuild();
                resultStream << versionInfo;
            } else {
                auto channel = VersionChannel::Unknown;
                QDataStream paramsStream(params);
                paramsStream >> channel;
                VersionInfo versionInfo = _updateManager.get()->versionInfo(channel);
                resultStream << versionInfo;
            }
            break;
        }
        case RequestNum::UPDATER_STATE: {
            if (_noUpdate) {
                resultStream << UpdateState::NoUpdate;
            } else {
                UpdateState state = _updateManager.get()->state();
                resultStream << state;
            }
            break;
        }
        case RequestNum::UPDATER_START_INSTALLER: {
            if (_noUpdate) {
                assert(false);
            } else {
                _updateManager.get()->startInstaller();
            }
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

    OldCommServer::instance()->sendReply(id, results);
}

void AppServer::startSyncsAndRetryOnError() {
    LOG_DEBUG(_logger, "Start syncs");
    if (const auto exitInfo = startSyncs(); !exitInfo) {
        LOG_WARN(_logger, "Error in startSyncsAndRetryOnError: " << exitInfo);
        if (exitInfo.code() == ExitCode::SystemError &&
            (exitInfo.cause() == ExitCause::SyncDirAccessError || exitInfo.cause() == ExitCause::SyncDirDiskMissing)) {
            LOG_DEBUG(_logger, "Retry to start syncs in " << START_SYNCPALS_RETRY_INTERVAL << " ms");
            QTimer::singleShot(START_SYNCPALS_RETRY_INTERVAL, this, [=, this]() { startSyncsAndRetryOnError(); });
        }
    }
}

ExitCode AppServer::clearErrors(const SyncDbId syncDbId, const bool autoResolved /*= false*/) {
    ExitCode exitCode = ExitCode::Unknown;
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
        QTimer::singleShot(100, [syncDbId, this]() { sendErrorsCleared(syncDbId); });
    }

    return exitCode;
}

void AppServer::sendErrorsCleared(const SyncDbId syncDbId) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(syncDbId);
        (void) OldCommServer::instance()->sendSignal(SignalNum::UTILITY_ERRORS_CLEARED, params, id);
    }
    if (useCommManager()) {
        // N/A - See SignalErrorRemovedJob
    }
}

void AppServer::sendQuit() const {
    if (useOldCommServer()) {
        int id = 0;

        (void) OldCommServer::instance()->sendSignal(SignalNum::UTILITY_QUIT, QByteArray(), id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUtilityQuitJob>());
    }
}

void AppServer::sendLogUploadStatusUpdated(LogUploadState status, int progressPercent) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << status;
        paramsStream << progressPercent;
        (void) OldCommServer::instance()->sendSignal(SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(
                std::make_shared<SignalUtilityLogUploadStateJob>(status, static_cast<std::int32_t>(progressPercent)));
    }

    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, status, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
    } else if (!found) {
        LOG_WARN(Log::instance()->getLogger(), AppStateKey::LogUploadState << " not found in appState table");
    }

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadPercent, std::to_string(progressPercent), found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAppState");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
    } else if (!found) {
        LOG_WARN(Log::instance()->getLogger(), AppStateKey::LogUploadPercent << " not found in appState table");
    }
}

void AppServer::sendNodeFixConflictedFilesCompleted(const SyncDbId syncDbId, const qint64 nbErrors) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(syncDbId);
        paramsStream << nbErrors;
        (void) OldCommServer::instance()->sendSignal(SignalNum::NODE_FIX_CONFLICTED_FILES_COMPLETED, params, id);
    }
}

void AppServer::uploadLog(const bool includeArchivedLogs) {
    /* See AppStateKey::LogUploadState for status values
     * The return value of progressFunc is true if the upload should continue, false if the user canceled the upload
     */
    const std::function<void(LogUploadState, int)> progressStatusCallBack = [this](LogUploadState status, int progressPercent) {
        sendLogUploadStatusUpdated(status, progressPercent); // Send progress to the client
    };
    const auto logUploadJob = std::make_shared<LogUploadJob>(includeArchivedLogs, progressStatusCallBack,
                                                             std::bind_front(&AppServer::addError, this));

    const std::function<void(UniqueId)> jobResultCallback = [logUploadJob]([[maybe_unused]] const UniqueId) {
        if (const ExitInfo exitInfo = logUploadJob->exitInfo(); !exitInfo && exitInfo.code() != ExitCode::OperationCanceled) {
            LOG_WARN(Log::instance()->getLogger(), "Error in LogArchiverHelper::sendLogToSupport: " << exitInfo);
            logUploadJob->addErrorCallback(Error(ERR_ID, ExitCode::LogUploadFailed, exitInfo.cause()));
        }
    };
    logUploadJob->setAdditionalCallback(jobResultCallback);
    SyncJobManagerSingleton::instance()->queueAsyncJob(logUploadJob, Poco::Thread::PRIO_HIGH);
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
    if (useOldCommServer()) {
        if (_updateManager) {
            QByteArray params;
            QDataStream paramsStream(&params, QIODevice::WriteOnly);
            paramsStream << _updateManager.get()->versionInfo();
            (void) OldCommServer::instance()->sendSignal(SignalNum::UPDATER_SHOW_DIALOG, params);
        } else {
            assert(false);
        }
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUpdaterShowDialogJob>(_updateManager->versionInfo()));
    }
}

void AppServer::onUpdateRequired() {
    addError(Error(ERR_ID, ExitCode::UpdateRequired));
    stopAllSyncPals();
    stopAllVfs();
}

void AppServer::onUpdateStateChanged(const UpdateState state) {
    if (useOldCommServer()) {
        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << state;
        (void) OldCommServer::instance()->sendSignal(SignalNum::UPDATER_STATE_CHANGED, params);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUpdaterStateChangedJob>(state));
    }
}

void AppServer::onCleanup() {
    cleanup();
}

void AppServer::onClientDisconnectedReceived() {
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

    if (message.startsWith(authorizationCodeMsg)) {
        const QUrl url = message.split(separatorMsg).back();
        const QUrlQuery query(url);
        const QString code = query.queryItemValue("code");
        const QString state = query.queryItemValue("state");
        onAuthorizationCodeReceived(code, state);
    } else if (message == showSynthesisMsg) {
        showSynthesis();
    } else if (message == showSettingsMsg) {
        showSettings();
    } else if (message == restartClientMsg) {
        const auto oldCommServerHasActiveConnection = useOldCommServer() && OldCommServer::instance()->hasActiveConnexion();
        const auto newCommServerHasActiveConnection = useCommManager() && _commManager->hasActiveClientConnexion();
        if (oldCommServerHasActiveConnection || newCommServerHasActiveConnection) {
            LOG_INFO(_logger, "An active connexion with a client already exist, showing synthesis!");
            showSynthesis();
            return;
        }
        _clientManuallyRestarted = true;
        if (!_clientProcess || _clientProcess->state() == QProcess::ProcessState::NotRunning) {
            if (!startClient()) {
                LOG_ERROR(_logger, "Failed to start the client");
            }
        }
    } else {
        LOG_WARN(_logger, "Unknown message received from another kDrive process: '" << message.toStdString() << "'");
    }
}

void AppServer::onSendNotifAsked(const QString &title, const QString &message) {
    sendShowNotification(title, message);
}

void AppServer::onAuthorizationCodeReceived(const QString &code, const QString &state) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << code;
    paramsStream << state;
    (void) OldCommServer::instance()->sendSignal(SignalNum::LOGIN_SEND_AUTHORIZATION_CODE, params, id);
}

void AppServer::sendShowNotification(const QString &title, const QString &message) const {
    // Notify client
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << title;
        paramsStream << message;
        (void) OldCommServer::instance()->sendSignal(SignalNum::UTILITY_SHOW_NOTIFICATION, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUtilityShowNotificationJob>(CommonUtility::qStr2CommString(title),
                                                                                       CommonUtility::qStr2CommString(message)));
    }
}

void AppServer::sendErrorAdded(const ErrorInfo &errorInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << (errorInfo.level() == ErrorLevel::Server);
        paramsStream << toInt(errorInfo.exitCode());
        paramsStream << static_cast<qint64>(errorInfo.syncDbId());
        (void) OldCommServer::instance()->sendSignal(SignalNum::UTILITY_ERROR_ADDED_LEGACY, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalErrorAddedJob>(errorInfo));
    }
}

void AppServer::sendErrorRemoved(int64_t dbId) const {
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalErrorRemovedJob>(dbId));
    }
}

void AppServer::addCompletedItem(const SyncDbId syncDbId, const SyncFileItem &item, const bool notify) {
    // Send completedItem signal to client
    SyncFileItemInfo itemInfo;
    ServerRequests::syncFileItemToSyncFileItemInfo(item, itemInfo);
    sendSyncCompletedItem(syncDbId, itemInfo);

    if (item.status() != SyncFileStatus::Success) {
        return;
    }

    resolveItemErrors(syncDbId, item);

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


void AppServer::sendGuiSignal(std::shared_ptr<AbstractGuiJob> signal) const {
    if (useCommManager()) {
        _commManager->sendGuiSignal(signal);
    }
}

ExitInfo AppServer::getVfs(const SyncDbId syncDbId, std::shared_ptr<Vfs> &vfs) {
    auto vfsMapIt = vfsMap.find(syncDbId);
    if (vfsMapIt == vfsMap.end()) {
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

void AppServer::syncFileStatus(const SyncDbId syncDbId, const SyncPath &path, SyncFileStatus &status) {
    const std::scoped_lock lock(syncPalMapMutex);
    auto syncPalMapIt = syncPalMap.find(syncDbId);
    if (syncPalMapIt == syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }
    if (!syncPalMapIt->second) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = syncPalMapIt->second->fileStatus(ReplicaSide::Local, path, status);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::fileStatus for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
    }
}

void AppServer::syncFileSyncing(const SyncDbId syncDbId, const SyncPath &path, bool &syncing) {
    const std::scoped_lock lock(syncPalMapMutex);
    auto syncPalMapIt = syncPalMap.find(syncDbId);
    if (syncPalMapIt == syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }
    if (!syncPalMapIt->second) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = syncPalMapIt->second->fileSyncing(ReplicaSide::Local, path, syncing);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::fileSyncing for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
    }
}

void AppServer::setSyncFileSyncing(const SyncDbId syncDbId, const SyncPath &path, const bool syncing) {
    const std::scoped_lock lock(syncPalMapMutex);
    auto syncPalMapIt = syncPalMap.find(syncDbId);
    if (syncPalMapIt == syncPalMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not found in SyncPalMap for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }
    if (!syncPalMapIt->second) {
        LOG_WARN(Log::instance()->getLogger(), "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = syncPalMapIt->second->setFileSyncing(ReplicaSide::Local, path, syncing);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::setFileSyncing for syncDbId=" << syncDbId);
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
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
    };

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

ExitInfo AppServer::updateUserInfo(User &user) {
    if (user.keychainKey().empty()) {
        return ExitCode::Ok;
    }

    if (const auto exitInfo = updateUser(user); !exitInfo) return exitInfo;

    std::vector<Account> accounts;
    if (!ParmsDb::instance()->selectAllAccounts(user.dbId(), accounts)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllAccounts");
        return ExitCode::DbError;
    }

    for (auto &account: accounts) {
        if (const auto exitInfo = updateAccount(account); !exitInfo) return exitInfo;

        std::vector<Drive> drives;
        if (!ParmsDb::instance()->selectAllDrives(account.dbId(), drives)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
            return ExitCode::DbError;
        }

        for (auto &drive: drives) {
            if (const auto exitInfo = updateDrive(user, account, drive); !exitInfo) return exitInfo;
        }
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::updateUser(User &user) {
    bool updated = false;
    if (const auto exitInfo = _loadUserInfo(user, updated); !exitInfo) {
        LOG_WARN(_logger, "Error in Requests::loadUserInfo: " << exitInfo);
        if (exitInfo.code() == ExitCode::InvalidToken) {
            // Notify client app that the user is disconnected
            UserInfo userInfo;
            ServerRequests::userToUserInfo(user, userInfo);
            sendUserUpdated(userInfo);
        }

        return exitInfo;
    }

    if (updated) {
        bool found = false;
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
    return ExitCode::Ok;
}

ExitInfo AppServer::createAccount(Account &newAccount) {
    // Make sure all information are up to date
    bool accountUpdated = false;
    if (const auto exitInfo = _loadAccountInfo(newAccount, accountUpdated); !exitInfo) {
        LOG_WARN(_logger, "Error in Requests::loadDriveInfo: " << exitInfo);
        return exitInfo;
    }

    // Notify the UI
    AccountInfo accountInfo;
    ServerRequests::accountToAccountInfo(newAccount, accountInfo);
    sendAccountAdded(accountInfo);

    // Insert account in DB
    if (!ParmsDb::instance()->insertAccount(newAccount)) {
        LOG_WARN(_logger, "Error in ParmsDb::insertAccount");
        return ExitCode::DbError;
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::updateAccount(Account &account) {
    bool accountUpdated = false;
    if (const auto exitInfo = _loadAccountInfo(account, accountUpdated); !exitInfo) {
        LOG_WARN(_logger, "Error in Requests::loadDriveInfo: " << exitInfo);
        return exitInfo;
    }

    if (accountUpdated) {
        AccountInfo accountInfo;
        ServerRequests::accountToAccountInfo(account, accountInfo);
        sendAccountUpdated(accountInfo);

        bool found = false;
        if (!ParmsDb::instance()->updateAccount(account, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateAccount");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "Account not found for accountDbId=" << account.dbId());
            return ExitCode::DataError;
        }
    }
    return ExitCode::Ok;
}

ExitInfo AppServer::updateDrive(const User &user, const Account &account, Drive &drive) {
    bool driveUpdated = false;
    bool quotaUpdated = false;
    AccountId newAccountId = 0;
    if (const auto exitInfo = _loadDriveInfo(drive, account.accountId(), newAccountId, driveUpdated, quotaUpdated); !exitInfo) {
        LOG_WARN(_logger, "Error in Requests::loadDriveInfo: " << exitInfo);
        return exitInfo;
    }

    if (drive.accessDenied() || drive.maintenanceInfo()._maintenance) {
        if (const auto exitInfo = handleDriveAccessDenied(drive); !exitInfo) return exitInfo;
    }

    if (driveUpdated) {
        bool found = false;
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

    if (const auto exitInfo = manageDriveMovedToAnotherAccount(user, account, newAccountId, drive, driveUpdated); !exitInfo)
        return exitInfo;

    if (driveUpdated || quotaUpdated) {
        DriveInfo driveInfo;
        ServerRequests::driveToDriveInfo(drive, driveInfo);
        sendDriveUpdated(driveInfo);
    }
    return ExitCode::Ok;
}

ExitInfo AppServer::handleDriveAccessDenied(const Drive &drive) {
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
        addError(Error(sync.dbId(), ERR_ID, ExitCode::BackError, exitCause));
    }
    return ExitCode::Ok;
}

ExitInfo AppServer::manageDriveMovedToAnotherAccount(const User &user, const Account &oldAccount, const AccountId newAccountId,
                                                     Drive &drive, bool &driveUpdated) {
    if (newAccountId <= 0) return ExitCode::Ok; // The account has not changed

    // Check if new account already exist in DB for this user
    Account newAccount;
    bool accountIdAlreadyExists = false;
    if (!ParmsDb::instance()->accountFromUserDbIdAndAccountId(user.dbId(), static_cast<int>(newAccountId), newAccount,
                                                              accountIdAlreadyExists)) {
        LOG_WARN(_logger, "Error in ParmsDb::accountDbId");
        return ExitCode::DbError;
    }

    if (!accountIdAlreadyExists) {
        // The account does not exist yet for this user, create it
        AccountDbId accountDbId = 0;
        if (!ParmsDb::instance()->getNewAccountDbId(accountDbId)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::getNewAccountDbId");
            return ExitCode::DbError;
        }
        newAccount.setDbId(accountDbId);
        newAccount.setAccountId(static_cast<int>(newAccountId));
        newAccount.setUserDbId(user.dbId());
        if (const auto exitInfo = createAccount(newAccount); !exitInfo) return exitInfo;
    }

    // Update the account DB ID in drive table
    drive.setAccountDbId(newAccount.dbId());
    bool found = false;
    if (!ParmsDb::instance()->updateDrive(drive, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(_logger, "Drive not found for driveDbId=" << drive.dbId());
        return ExitCode::DataError;
    }
    driveUpdated = true;

    // Delete the old account if not used anymore
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(oldAccount.dbId(), driveList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
        return ExitCode::DbError;
    }

    if (driveList.empty()) {
        if (const auto exitInfo = ServerRequests::deleteAccount(oldAccount.dbId()); !exitInfo) {
            LOG_WARN(_logger, "Error in Requests::deleteAccount: code=" << exitInfo);
            return ExitCode::DataError;
        }
        sendAccountRemoved(oldAccount.dbId());
    }
    return ExitCode::Ok;
}

void AppServer::resolveErrors(std::vector<Error> errorList) const {
    for (const auto &error: errorList) {
        bool keepError = false;

        if (ExitInfo exitInfo = ServerRequests::keepError(error, keepError); !exitInfo) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::keepError: " << exitInfo);
            continue;
        }

        if (keepError) continue;

        bool found = false;
        if (!ParmsDb::instance()->deleteError(error.dbId(), found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::deleteError");
            return;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "Error not found in Error table for dbId=" << error.dbId());
            return;
        }
        sendErrorRemoved(error.dbId());
    }
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
#if defined(KD_MACOS)
    Utility::restartFinderExtension();
#endif
    return mainExitInfo;
}

std::string liteSyncActivationLogMessage(const bool enabled, const SyncDbId syncDbId) {
    const std::string activationStatus = enabled ? "enabled" : "disabled";
    std::stringstream ss;

    ss << "LiteSync is " << activationStatus << " for syncDbId=" << syncDbId;

    return ss.str();
}

// This function will pause the synchronization in case of errors.
ExitInfo AppServer::tryCreateAndStartVfs(const Sync &sync, bool &startPostponed) noexcept {
    startPostponed = false;
    const std::string liteSyncMsg = liteSyncActivationLogMessage(sync.virtualFileMode() != VirtualFileMode::Off, sync.dbId());
    LOG_INFO(_logger, liteSyncMsg);
    if (const auto exitInfo = createAndStartVfs(sync); !exitInfo) {
        LOG_WARN(_logger, "Error in createAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo << ", pausing.");
        addError(Error(sync.dbId(), ERR_ID, exitInfo));
        // Continue. The sync will be initialized but paused.
        // The sync will start when the VFS plugin will be ready (sync folder accessible and permissions granted by the user).
        startPostponed = true;
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
                bool syncUpdated = false;

                // Make sure we have valid CLSID
                if (sync.navigationPaneClsid().empty()) {
                    sync.setNavigationPaneClsid(QUuid::createUuid().toString().toStdString());
                    syncUpdated = true;
                }

                if (user.toMigrate()) {
                    if (!user.keychainKey().empty()) {
                        // End migration once connected
                        if (const auto exitInfo =
                                    processMigratedSyncOnceConnected(user.dbId(), drive.driveId(), sync, blackList, syncUpdated);
                            !exitInfo) {
                            LOG_WARN(_logger, "Error in updateMigratedSyncPalOnceConnected for syncDbId=" << sync.dbId() << " : "
                                                                                                          << exitInfo);
                            addError(Error(sync.dbId(), ERR_ID, exitInfo));
                            mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
                            continue;
                        }
                    }
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

                // Clear old errors for this sync
                clearErrors(sync.dbId(), false);
                clearErrors(sync.dbId(), true);

                if (const auto exitInfo = checkIfSyncIsValid(sync); !exitInfo) {
                    LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    addError(Error(sync.dbId(), ERR_ID, exitInfo));
                    continue;
                }

                bool startPostponed = false;
                if (const auto exitInfo = tryCreateAndStartVfs(sync, startPostponed); !exitInfo) {
                    LOG_WARN(_logger, "Error in tryCreateAndStartVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
                    if (!Utility::isLiteSyncExtError(exitInfo)) {
                        continue;
                    }
                }

                startPostponed |= user.keychainKey().empty();

                // Create and start SyncPal
                startDelay += std::chrono::seconds(START_SYNCPALS_TIME_GAP);
                if (const auto exitInfo = initSyncPal(sync, blackList, !startPostponed, startDelay, false, false); !exitInfo) {
                    LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    addError(Error(sync.dbId(), ERR_ID, exitInfo));
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

ExitInfo AppServer::processMigratedSyncOnceConnected(const UserDbId userDbId, const DriveId driveId, Sync &sync,
                                                     QSet<QString> &blackList, bool &syncUpdated) {
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

            if (!nodeId.isEmpty() && migrationSelectiveSync.type() == SyncNodeType::BlackList) {
                blackList << nodeId;
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

void AppServer::logUsefulInformation() {
    LOG_INFO(_logger, "***** APP INFO *****");

    LOG_INFO(_logger, "version: " << _theme->version());
    LOG_INFO(_logger, "os: " << CommonUtility::platformName().toStdString());
    LOG_INFO(_logger, "os version: " << CommonUtility::osVersion());
    LOG_INFO(_logger, "kernel version : " << QSysInfo::kernelVersion().toStdString());
    LOG_INFO(_logger, "kernel type : " << QSysInfo::kernelType().toStdString());
    LOG_INFO(_logger, "locale: " << QLocale::system().name().toStdString());
    LOG_INFO(_logger, "# of logical CPU core: " << std::thread::hardware_concurrency());

    // Log app ID
    LOG_INFO(Log::instance()->getLogger(), "App ID: " << appUID());

    if (KDRIVE_VERSION_MAJOR >= 4) {
        // Log Sentry activation status
        LOG_INFO(Log::instance()->getLogger(), "Sentry enabled: " << ParametersCache::instance()->parameters().sentryEnabled());

        // Log Matomo activation status
        LOG_INFO(Log::instance()->getLogger(), "Matomo enabled: " << ParametersCache::instance()->parameters().matomoEnabled());
    }

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
    if (CommonUtility::envVarValue("KDRIVE_FORCE_EXTENDED_LOG") == "1") {
        LOG_INFO(Log::instance()->getLogger(), "Extended log forced by environment variable KDRIVE_FORCE_EXTENDED_LOG");
    } else {
        LOG_INFO(Log::instance()->getLogger(),
                 "Extended log activated: " << ParametersCache::instance()->parameters().extendedLog());
    }

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
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
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
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
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
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
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
            addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
        } else if (!found) {
            LOG_WARN(_logger, AppStateKey::LogUploadState << " not found in appstate table.");
        }
    } else if (logUploadState ==
               LogUploadState::CancelRequested) { // If interrupted while cancelling, consider it has been cancelled
        if (bool found = false;
            !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::Canceled, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
            addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
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
        if (option.startsWith(REDIRECT_URI)) {
            const QUrl url(option);

            if (url.scheme() == "kdrive" && url.host() == "auth-desktop") {
                _authorizationCodeStr = option;
                break;
            }
        } else if (option == QLatin1String("--help") || option == QLatin1String("-h")) {
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
        } else if (option == QLatin1String("--noUpdate")) {
            _noUpdate = true;
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
        auto syncDbPtr = std::make_shared<SyncDb>(dbPath.string());
        syncDbPtr->clearNodes();
    }
}

void AppServer::sendShowSettingsMsg() {
    sendMessage(showSettingsMsg);
}

void AppServer::sendShowSynthesisMsg() {
    sendMessage(showSynthesisMsg);
}

void AppServer::sendRestartClientMsg() {
    sendMessage(restartClientMsg);
}

void AppServer::sendAuthorizationCode() {
    sendMessage(authorizationCodeMsg + separatorMsg + _authorizationCodeStr);
}

void AppServer::showSettings() {
    if (useOldCommServer()) {
        int id = 0;
        (void) OldCommServer::instance()->sendSignal(SignalNum::UTILITY_SHOW_SETTINGS, QByteArray(), id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUtilityShowSettingsJob>());
    }
}

void AppServer::showSynthesis() {
    if (useOldCommServer()) {
        int id = 0;
        (void) OldCommServer::instance()->sendSignal(SignalNum::UTILITY_SHOW_SYNTHESIS, QByteArray(), id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUtilityShowSynthesisJob>());
    }
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

ExitCode AppServer::sendShowFileNotification(const SyncDbId syncDbId, const QString &filename, const QString &renameTarget,
                                             const SyncFileInstruction status, const int count) const {
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
    if (useOldCommServer()) {
        OldCommServer::instance()->setHasQuittedProperly(false);

        if (!OldCommServer::instance()->isListening()) {
            LOG_WARN(_logger, "Failed to start kDrive client (comm server isn't started)");
            return false;
        }
    }

    bool startClient = false;
#ifdef QT_NO_DEBUG
    startClient = true;
#endif
    startClient |= QProcessEnvironment::systemEnvironment().value("KDRIVE_DEBUG_RUN_CLIENT") == "1";

    if (startClient) {
        // Start the client
        QString pathToExecutable;

#if defined(KD_WINDOWS)
        pathToExecutable = QCoreApplication::applicationDirPath() + QString("/%1.exe").arg(APPLICATION_CLIENTV4_EXECUTABLE);

        IoError ioError = IoError::Success;
        bool exists = false;
        if (!IoHelper::checkIfPathExists(QStr2Path(pathToExecutable), exists, ioError, IoHelper::PathCheckOption::Insensitive) ||
            !exists || ioError != IoError::Success) {
            pathToExecutable.clear();
        }
        if (pathToExecutable.isEmpty()) {
            pathToExecutable = QCoreApplication::applicationDirPath() + QString("/%1.exe").arg(APPLICATION_CLIENT_EXECUTABLE);
        }
#else
        pathToExecutable = QCoreApplication::applicationDirPath() + QString("/%1").arg(APPLICATION_CLIENT_EXECUTABLE);
#endif

        QStringList arguments;
        if (useOldCommServer()) {
            arguments << QString::number(OldCommServer::instance()->commPort());

            LOGW_INFO(_logger, L"Starting kDrive client - path=" << Path2WStr(QStr2Path(pathToExecutable)) << L" args="
                                                                 << arguments[0].toStdWString());
        }

        _clientProcess = new QProcess(this);
        _clientProcess->setProgram(pathToExecutable);
        _clientProcess->setArguments(arguments);

        _clientProcess->start();
        if (!_clientProcess->waitForStarted()) {
            LOG_WARN(_logger, "Failed to start kDrive client");
            return false;
        }
    }

    return true;
}

ExitInfo AppServer::updateAllUsersInfo() {
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

        if (const auto exitInfo = updateUserInfo(user); !exitInfo) {
            LOG_WARN(_logger, "Error in updateUserInfo: " << exitInfo);
            return exitInfo;
        }
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::initSyncPal(const Sync &sync, const NodeSet &blackList, bool start, const std::chrono::seconds &startDelay,
                                bool resumedByUser, bool firstInit) {
    const std::scoped_lock lock(syncPalMapMutex);
    auto syncPalMapIt = syncPalMap.find(sync.dbId());
    if (syncPalMapIt == syncPalMap.end()) {
        std::shared_ptr<Vfs> vfs;
        const std::scoped_lock lock2(vfsMapMutex);
        if (ExitInfo exitInfo = getVfs(sync.dbId(), vfs); !exitInfo) {
            LOG_WARN(_logger, "Error in getVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
            return exitInfo;
        }

        // Create SyncPal
        try {
            syncPalMap[sync.dbId()] = std::make_shared<SyncPal>(vfs, sync.dbId(), _theme->version());
        } catch (std::exception const &) {
            LOG_WARN(_logger, "Error in SyncPal::SyncPal for syncDbId=" << sync.dbId());
            return {ExitCode::DbError, ExitCause::Unknown};
        }

        // Set callbacks
        syncPalMapIt = syncPalMap.find(sync.dbId());
        syncPalMapIt->second->setAddErrorCallback(std::bind_front(&AppServer::addError, this));
        syncPalMapIt->second->setResolveSyncErrorsByExitCauseCallback(
                std::bind_front(&AppServer::resolveSyncErrorsByExitCause, this));
        syncPalMapIt->second->setAddCompletedItemCallback(std::bind_front(&AppServer::addCompletedItem, this));
        syncPalMapIt->second->setFixConflictedFilesCompletedCallback(
                std::bind_front(&AppServer::sendNodeFixConflictedFilesCompleted, this));

        if (!blackList.empty()) {
            // Set blackList (create or overwrite the possible existing list in DB)
            if (const ExitInfo exitInfo = syncPalMapIt->second->setSyncIdSet(SyncNodeType::BlackList, blackList); !exitInfo) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet for syncDbId=" << sync.dbId() << " : " << exitInfo);
                return exitInfo;
            }
        }
    }

#if defined(KD_WINDOWS) || defined(KD_MACOS)
    if (firstInit) {
        if (!syncPalMapIt->second->wipeOldPlaceholders()) {
            LOG_WARN(_logger, "Error in SyncPal::wipeOldPlaceholders");
        }
    }
#else
    (void) firstInit;
#endif

    if (start && (resumedByUser || !sync.paused())) {
        if (syncPalMapIt->second->isPaused()) {
            // Unpause SyncPal
            syncPalMapIt->second->unpause();
        } else if (!syncPalMapIt->second->isRunning()) {
            // Start SyncPal
            syncPalMapIt->second->start(startDelay);
        }
    }

    registerSync(syncPalMapIt->second);

    return ExitCode::Ok;
}

ExitInfo AppServer::initSyncPal(const Sync &sync, const QSet<QString> &blackList, const bool start,
                                const std::chrono::seconds &startDelay, const bool resumedByUser, const bool firstInit) {
    NodeSet blackList2;
    for (const QString &nodeId: blackList) {
        blackList2.insert(nodeId.toStdString());
    }

    if (const auto exitInfo = initSyncPal(sync, blackList2, start, startDelay, resumedByUser, firstInit); !exitInfo) {
        LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " : " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::stopSyncPal(const SyncDbId syncDbId, const SyncPal::PauseCaller caller,
                                const SyncPal::DbBehaviorAfterStop behavior) {
    LOG_DEBUG(_logger, "Stop SyncPal for syncDbId=" << syncDbId);

    // Stop SyncPal
    const std::scoped_lock lock(syncPalMapMutex);
    const auto syncPalMapIt = syncPalMap.find(syncDbId);
    if (syncPalMapIt == syncPalMap.end()) {
        LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::Unknown};
    }

    if (!syncPalMapIt->second) {
        LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::Unknown};
    }

    unregisterSync(syncPalMapIt->second);
    syncPalMapIt->second->stop(caller, behavior);

    LOG_DEBUG(_logger, "Stop SyncPal for syncDbId=" << syncDbId << " done");

    return ExitCode::Ok;
}

ExitInfo AppServer::createAndStartVfs(const Sync &sync) noexcept {
    // Check that the sync folder exists.
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(sync.localPath(), exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists " << Utility::formatIoError(sync.localPath(), ioError));
        return ExitCode::SystemError;
    }

    if (!exists) {
        LOGW_WARN(_logger, L"Sync localpath " << Utility::formatSyncPath(sync.localPath()) << L" doesn't exist.");
        return {ExitCode::SystemError, Utility::exitCauseFromInaccessibleSyncDirectory(sync.localPath())};
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

    const std::scoped_lock lock(vfsMapMutex);
    auto vfsMapIt = vfsMap.find(sync.dbId());
    if (vfsMapIt == vfsMap.end()) {
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
        vfsSetupParams.executeCommand = []([[maybe_unused]] const CommString &command, [[maybe_unused]] bool broadcast) {
#if defined(KD_MACOS) || defined(KD_WINDOWS)
            if (useCommManager()) {
                _commManager->executeCommandDirect(command, broadcast);
            }
#endif
        };
        vfsSetupParams.logger = _logger;
        vfsSetupParams.sentryHandler = sentry::Handler::instance();
        QString error;
        std::shared_ptr vfs = KDC::createVfsFromPlugin(sync.virtualFileMode(), vfsSetupParams, error);
        if (!vfs) {
            LOG_WARN(_logger,
                     "Error in Vfs::createVfsFromPlugin for mode " << sync.virtualFileMode() << " : " << error.toStdString());
            return {ExitCode::SystemError, ExitCause::UnableToStartVfs};
        }
        vfsMap[sync.dbId()] = vfs;
        vfsMapIt = vfsMap.find(sync.dbId());
        vfsMapIt->second->setExtendedLog(ParametersCache::isExtendedLogEnabled());

        // Set callbacks
        vfsMapIt->second->setSyncFileStatusCallback(std::bind_front(&AppServer::syncFileStatus, this));
        vfsMapIt->second->setSyncFileSyncingCallback(std::bind_front(&AppServer::syncFileSyncing, this));
        vfsMapIt->second->setSetSyncFileSyncingCallback(std::bind_front(&AppServer::setSyncFileSyncing, this));
#if defined(KD_MACOS)
        vfsMapIt->second->setExclusionAppListCallback(std::bind_front(&AppServer::exclusionAppList, this));
#endif
    }

    // Start VFS
    if (ExitInfo exitInfo = vfsMapIt->second->start(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone); !exitInfo) {
#if defined(KD_MACOS)
        if (sync.virtualFileMode() == VirtualFileMode::Mac && _vfsInstallationDone && !_vfsActivationDone) {
            // Check LiteSync ext authorizations
            if (!areMacVfsAuthsOk()) {
                return {ExitCode::SystemError, ExitCause::LiteSyncNotAllowed};
            }
            // Check LiteSync ext process
            if (!Utility::isLiteSyncExtRunning()) {
                return {ExitCode::SystemError, ExitCause::LiteSyncExtNotRunning};
            }
        }
#endif

        LOG_WARN(_logger, "Error in Vfs::start: " << exitInfo);
        return exitInfo;
    }

#if defined(KD_WINDOWS)
    // Save sync
    if (sync.virtualFileMode() == KDC::VirtualFileMode::Win) {
        Utility::setFolderPinState(CommonUtility::s2ws(vfsMapIt->second->namespaceCLSID()), true);
    } else {
        if (vfsMapIt->second->namespaceCLSID().empty()) {
            vfsMapIt->second->setNamespaceCLSID(sync.navigationPaneClsid());
        }
        Utility::addLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()), sync.localPath(), true);
    }

    bool found = false;
    if (!ParmsDb::instance()->updateSync(sync, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateSync");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }
    if (!found) {
        LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << sync.dbId());
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }
#endif
    return ExitCode::Ok;
}

ExitInfo AppServer::stopVfs(const SyncDbId syncDbId, const bool unregister) {
    LOG_DEBUG(_logger, "Stop VFS for syncDbId=" << syncDbId);

    // Stop Vfs
    const std::scoped_lock lock(vfsMapMutex);
    auto vfsMapIt = vfsMap.find(syncDbId);
    if (vfsMapIt == vfsMap.end()) {
        LOG_WARN(_logger, "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::Unknown};
    }

    if (!vfsMapIt->second) {
        LOG_WARN(_logger, "Vfs not set in vfsMap for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::Unknown};
    }

    vfsMapIt->second->stop(unregister);

    LOG_DEBUG(_logger, "Stop VFS for syncDbId=" << syncDbId << " done");

    return ExitCode::Ok;
}

ExitInfo AppServer::setSupportsVirtualFiles(const SyncDbId syncDbId, const bool value, const bool asyncResponse) {
    const std::scoped_lock lock(syncPalMapMutex);
    auto syncPalMapIt = syncPalMap.find(syncDbId);
    if (syncPalMapIt == syncPalMap.end()) {
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
        addError(Error(sync.dbId(), ERR_ID, exitInfo));
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
        syncPalMapIt->second->stop();

        // Stop Vfs
        if (const auto exitInfo = stopVfs(syncDbId, true); !exitInfo) {
            LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << sync.dbId() << " : " << exitInfo);
            return exitInfo;
        }

        if (newMode == VirtualFileMode::Off) {
#if defined(KD_WINDOWS)
            LOG_INFO(_logger, "Clearing node DB");
            if (const auto exitCode = syncPalMapIt->second->clearNodes(); exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::clearNodes for syncDbId=" << sync.dbId() << " : " << exitCode);
            }
#else
            // Clear file system
            syncPalMapIt->second->wipeVirtualFiles();
#endif
        }

#if defined(KD_WINDOWS)
        if (newMode == VirtualFileMode::Win) {
            Utility::removeLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()));
        } else if (sync.virtualFileMode() == VirtualFileMode::Win) {
            Utility::addLegacySyncRootKeys(CommonUtility::s2ws(sync.navigationPaneClsid()), sync.localPath(), true);
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
        const std::scoped_lock lock2(vfsMapMutex);
        (void) vfsMap.erase(syncDbId);

        ExitInfo mainExitInfo = ExitCode::Ok;
        bool startPostponed = false;
        if (const auto exitInfo = tryCreateAndStartVfs(sync, startPostponed); !exitInfo) {
            LOG_WARN(_logger, "Error in tryCreateAndStartVfs: " << exitInfo);
            if (!Utility::isLiteSyncExtError(exitInfo)) {
                return exitInfo;
            }
            mainExitInfo = exitInfo;
        }

        // Update SyncPal
        std::shared_ptr<Vfs> vfs;
        {
            const std::scoped_lock lock3(vfsMapMutex);
            if (const auto exitInfo = getVfs(syncDbId, vfs); !exitInfo) {
                LOG_WARN(_logger, "Error in getVfs for syncDbId=" << syncDbId << " : " << exitInfo);
                return exitInfo;
            }
            syncPalMapIt->second->setVfs(vfs);
        }

        // Update sync info on client side
        SyncInfo syncInfo;
        ServerRequests::syncToSyncInfo(sync, syncInfo);
        sendSyncUpdated(syncInfo);

        auto func = [this, newMode, vfs, sync, asyncResponse, startPostponed, syncDbId]() {
            if (newMode != VirtualFileMode::Off && vfs) {
                // Clear file system
                vfs->convertDirContentToPlaceholder(SyncName2QStr(sync.localPath()), true);
            }
            // Notify conversion completed async
            if (asyncResponse) {
                sendVfsConversionCompleted(sync.dbId());
            }

            if (!startPostponed) {
                // Re-start sync
                const std::scoped_lock lock3(syncPalMapMutex);
                auto syncPalMapIt2 = syncPalMap.find(syncDbId);
                if (syncPalMapIt2 != syncPalMap.end()) {
                    syncPalMapIt2->second->start();
                }
            }
        };

        if (asyncResponse) {
            QTimer::singleShot(100, this, func);
        } else {
            func();
        }
        if (vfs) {
            if (ExitInfo exitInfo = vfs->setPinState("", value ? PinState::Unspecified : PinState::AlwaysLocal); !exitInfo) {
                LOG_WARN(_logger, "Error in vfsSetPinState for syncDbId=" << syncDbId << " : " << exitInfo);
                return exitInfo;
            }
        }
        return mainExitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::getNodePath(const SyncDbId syncDbId, const NodeId &nodeId, CommString &path) {
    const std::scoped_lock lock(syncPalMapMutex);
    auto syncPalMapIt = syncPalMap.find(syncDbId);

    if (syncPalMapIt == syncPalMap.end()) {
        LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);

        return ExitCode::DataError;
    }

    if (!syncPalMapIt->second) {
        LOG_WARN(_logger, "SyncPal not set in syncPalMap for syncDbId=" << syncDbId);

        return ExitCode::DataError;
    }

    if (const auto exitInfo =
                ServerRequests::getPathByNodeId(syncPalMapIt->second->userDbId(), syncPalMapIt->second->driveId(), nodeId, path);
        !exitInfo) {
        if (exitInfo.cause() == ExitCause::NotFound) {
            (void) SyncNodeCache::instance()->deleteSyncNode(syncDbId, nodeId);
        } else {
            LOG_WARN(_logger, "Error in ServerRequests::getPathByNodeId: " << exitInfo);
            AppServer::addError(Error(ERR_ID, exitInfo));
        }

        return exitInfo;
    }

    return ExitCode::Ok;
}

void AppServer::addError(const Error &error) const {
    Error errorCopy = error;
    // Fetch all errors.
    std::vector<Error> errorList;
    if (!ParmsDb::instance()->selectAllErrors(errorCopy.level(), errorCopy.syncDbId(), INT_MAX, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllErrors");
        return;
    }

    // Check if a similar error already exists.
    bool errorAlreadyExists = false;
    for (Error &existingError: errorList) {
        if (!existingError.isSimilarTo(errorCopy)) continue;
        // Update existing error time
        existingError.setTime(errorCopy.time());
        bool found = false;
        if (!ParmsDb::instance()->updateError(existingError, found)) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateError");
            return;
        }
        if (!found) {
            LOG_WARN(Log::instance()->getLogger(), "Error not found in Error table for dbId=" << existingError.dbId());
            return;
        }
        errorCopy.setDbId(existingError.dbId());
        errorAlreadyExists = true;
        break;
    }

    if (!errorAlreadyExists && !ParmsDb::instance()->insertError(errorCopy)) { // Insert new error
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::insertError");
        return;
    }
    if (!errorAlreadyExists) errorList.push_back(errorCopy);


    User user;
    if (errorCopy.syncDbId() && ServerRequests::getUserFromSyncDbId(errorCopy.syncDbId(), user) != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::getUserFromSyncDbId");
        return;
    }

    if (ServerRequests::isDisplayableError(errorCopy)) {
        // Notify the client
        if (errorAlreadyExists) {
            // First remove the existing error
            sendErrorRemoved(errorCopy.dbId());
        }

        ErrorInfo errorInfo;
        ServerRequests::errorToErrorInfo(errorCopy, errorInfo);
        sendErrorAdded(errorInfo);
    }

    if (errorCopy.exitCode() == ExitCode::InvalidToken) {
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
    } else if (errorCopy.exitCode() == ExitCode::NetworkError && errorCopy.exitCause() == ExitCause::SocketsDefuncted) {
        // Manage sockets defuncted error
        LOG_WARN(Log::instance()->getLogger(), "Manage sockets defuncted error");

        sentry::Handler::captureMessage(sentry::Level::Warning, "AppServer::addError", "Sockets defuncted error");

        // Decrease upload session max parallel jobs
        ParametersCache::instance()->decreaseUploadSessionParallelThreads();

        // Decrease JobManager pool capacity
        SyncJobManagerSingleton::instance()->decreasePoolCapacity();
    } else if (errorCopy.exitCode() == ExitCode::SystemError && errorCopy.exitCause() == ExitCause::FileAccessError) {
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
            sendErrorRemoved(errorId);
        }
        if (!toBeRemovedErrorIds.empty()) sendErrorsCleared(errorCopy.syncDbId());
    } else if (_updateManager && errorCopy.exitCode() == ExitCode::UpdateRequired) {
        _updateManager->updater()->unskipVersion();
    }

    if (!ServerRequests::isAutoResolvedError(errorCopy) && !errorAlreadyExists) {
        // Send error to sentry only for technical errors
        SentryUser sentryUser(user.email(), user.name(), std::to_string(user.userId()));
        sentry::Handler::captureMessage(sentry::Level::Warning, "AppServer::addError", error.errorString(), sentryUser);
    }
}

void AppServer::resolveItemErrors(const SyncDbId syncDbId, const SyncFileItem &item) const {
    std::vector<Error> errorList;

    bool found = false;
    if (!ParmsDb::instance()->selectErrorByNodeInfo(syncDbId, item.localNodeId(), item.remoteNodeId(), item.path(),
                                                    item.newPath(), errorList, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectErrorByNodeInfo");
        return;
    }

    if (!found) return;
    resolveErrors(errorList);
}

void AppServer::resolveSyncErrorsByExitCause(SyncDbId syncDbId, ExitCause cause) const {
    std::vector<Error> errorList;

    if (!ParmsDb::instance()->selectSyncErrorsByExitCause(syncDbId, cause, errorList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSyncErrorsByExitCause");
        return;
    }

    resolveErrors(errorList);
}

void AppServer::sendUserAdded(const UserInfo &userInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << userInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::USER_ADDED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUserAddedJob>(userInfo));
    }
}

void AppServer::sendUserUpdated(const UserInfo &userInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << userInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::USER_UPDATED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUserUpdatedJob>(userInfo));
    }
}

void AppServer::sendUserStatusChanged(const UserDbId userDbId, const bool connected, const QString &connexionError) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(userDbId);
        paramsStream << connected;
        paramsStream << connexionError;

        (void) OldCommServer::instance()->sendSignal(SignalNum::USER_STATUSCHANGED, params, id);
    }
    if (useCommManager()) {
        // TODO
    }
}

void AppServer::sendUserRemoved(const UserDbId userDbId) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(userDbId);

        (void) OldCommServer::instance()->sendSignal(SignalNum::USER_REMOVED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalUserRemovedJob>(userDbId));
    }
}

void AppServer::sendAccountAdded(const AccountInfo &accountInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << accountInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::ACCOUNT_ADDED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalAccountAddedJob>(accountInfo));
    }
}

void AppServer::sendAccountUpdated(const AccountInfo &accountInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << accountInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::ACCOUNT_UPDATED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalAccountUpdatedJob>(accountInfo));
    }
}

void AppServer::sendAccountRemoved(const AccountDbId accountDbId) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(accountDbId);

        (void) OldCommServer::instance()->sendSignal(SignalNum::ACCOUNT_REMOVED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalAccountRemovedJob>(accountDbId));
    }
}

void AppServer::sendDriveAdded(const DriveInfo &driveInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << driveInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::DRIVE_ADDED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalDriveAddedJob>(driveInfo));
    }
}

void AppServer::sendDriveUpdated(const DriveInfo &driveInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << driveInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::DRIVE_UPDATED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalDriveUpdatedJob>(driveInfo));
    }
}

void AppServer::sendDriveQuotaUpdated(const DriveDbId driveDbId, const qint64 total, const qint64 used) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(driveDbId);
        paramsStream << total;
        paramsStream << used;

        (void) OldCommServer::instance()->sendSignal(SignalNum::DRIVE_QUOTAUPDATED_LEGACY, params, id);
    }
    if (useCommManager()) {
        // N/A - See SignalDriveUpdatedJob
    }
}

void AppServer::sendDriveRemoved(const DriveDbId driveDbId) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(driveDbId);

        (void) OldCommServer::instance()->sendSignal(SignalNum::DRIVE_REMOVED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalDriveRemovedJob>(driveDbId));
    }
}

void AppServer::sendSyncUpdated(const SyncInfo &syncInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << syncInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::SYNC_UPDATED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncUpdatedJob>(syncInfo));
    }
}

void AppServer::sendSyncRemoved(const SyncDbId syncDbId) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(syncDbId);

        (void) OldCommServer::instance()->sendSignal(SignalNum::SYNC_REMOVED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncRemovedJob>(syncDbId));
    }
}

void AppServer::sendSyncDeletionFailed(const SyncDbId syncDbId) const {
    if (useOldCommServer()) {
        int id = 0;
        const auto params = QByteArray(ArgsReader(static_cast<qint64>(syncDbId)));

        (void) OldCommServer::instance()->sendSignal(SignalNum::SYNC_DELETE_FAILED, params, id);
    }
    if (useCommManager()) {
        // TODO
    }
}


void AppServer::sendDriveDeletionFailed(const DriveDbId driveDbId) const {
    if (useOldCommServer()) {
        int id = 0;
        const auto params = QByteArray(ArgsReader(static_cast<qint64>(driveDbId)));

        (void) OldCommServer::instance()->sendSignal(SignalNum::DRIVE_DELETE_FAILED, params, id);
    }
    if (useCommManager()) {
        // TODO
    }
}


void AppServer::sendGetFolderSizeCompleted(const QString &nodeId, const qint64 size) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << nodeId;
        paramsStream << size;

        (void) OldCommServer::instance()->sendSignal(SignalNum::NODE_FOLDER_SIZE_COMPLETED, params, id);
    }
    if (useCommManager()) {
        // N/A - See NodeFolderSizeJob
    }
}

void AppServer::sendSyncProgressInfo(const SyncDbId syncDbId, const SyncStatus status, const SyncStep step,
                                     const SyncProgress &progress) const {
    if (useOldCommServer()) {
        int id = 0;
        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(syncDbId);
        paramsStream << status;
        paramsStream << step;
        paramsStream << static_cast<qint64>(progress._currentFile);
        paramsStream << static_cast<qint64>(progress._totalFiles);
        paramsStream << static_cast<qint64>(progress._completedSize);
        paramsStream << static_cast<qint64>(progress._totalSize);
        paramsStream << static_cast<qint64>(progress._estimatedRemainingTime);
        (void) OldCommServer::instance()->sendSignal(SignalNum::SYNC_PROGRESSINFO, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncProgressInfoJob>(syncDbId, status, step, progress));
    }
}

void AppServer::sendSyncCompletedItem(const SyncDbId syncDbId, const SyncFileItemInfo &itemInfo) const {
    if (useOldCommServer()) {
        if (itemInfo.progress() == 100) { // 100%
            int id = 0;

            QByteArray params;
            QDataStream paramsStream(&params, QIODevice::WriteOnly);
            paramsStream << static_cast<qint64>(syncDbId);
            paramsStream << itemInfo;
            (void) OldCommServer::instance()->sendSignal(SignalNum::SYNC_COMPLETEDITEM, params, id);
            if (ParametersCache::isExtendedLogEnabled()) {
                LOGW_DEBUG(_logger, L"Send progress for syncDbId="
                                            << syncDbId << L" path=" << Path2WStr(QStr2Path(itemInfo.path())) << L" size="
                                            << itemInfo.size() << L" progress=" << itemInfo.progress() << L"% to gui");
            }
        }
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncCompletedItemJob>(syncDbId, itemInfo));
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_DEBUG(_logger, L"Send progress for syncDbId=" << syncDbId << L" path=" << Path2WStr(QStr2Path(itemInfo.path()))
                                                               << L" size=" << itemInfo.size() << L" progress="
                                                               << itemInfo.progress() << L"% to new gui");
        }
    }
}

void AppServer::sendVfsConversionCompleted(const SyncDbId syncDbId) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << static_cast<qint64>(syncDbId);
        (void) OldCommServer::instance()->sendSignal(SignalNum::SYNC_VFS_CONVERSION_COMPLETED, params, id);
    }
    if (useCommManager()) {
        // N/A - See SyncSetSupportsVirtualFilesJob
    }
}

void AppServer::sendSyncAdded(const SyncInfo &syncInfo) const {
    if (useOldCommServer()) {
        int id = 0;

        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << syncInfo;

        (void) OldCommServer::instance()->sendSignal(SignalNum::SYNC_ADDED, params, id);
    }
    if (useCommManager()) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncAddedJob>(syncInfo));
    }
}

void AppServer::onLoadInfo() {
    ExitCode exitCode = updateAllUsersInfo();
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in updateAllUsersInfo: code=" << exitCode);
        addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
    }
}

void AppServer::onUpdateSyncsProgress() {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllSyncs");
        addError(Error(ERR_ID, ExitCode::DbError, ExitCause::Unknown));
        return;
    }

    for (const auto &sync: syncList) {
        const std::scoped_lock lock(syncPalMapMutex);
        if (const auto syncPalMapIt = syncPalMap.find(sync.dbId()); syncPalMapIt == syncPalMap.end()) {
            // No SyncPal for this sync
            // Show the sync paused anyway. It is most likely that an external drive is not plugged in.
            sendSyncProgressInfo(sync.dbId(), SyncStatus::Paused, SyncStep::None, SyncProgress());
        } else {
            if (!syncPalMapIt->second) {
                assert(false);
                continue;
            }

            const auto syncPal = syncPalMapIt->second;

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
            addError(Error(ERR_ID, exitCode, ExitCause::Unknown));
        }

        _notifications.clear();
    }
}

void AppServer::onRestartSyncs() {
#if defined(KD_MACOS)
    if (!noMacVfsSync() && _vfsInstallationDone && !_vfsActivationDone && areMacVfsAuthsOk()) {
        LOG_INFO(Log::instance()->getLogger(), "LiteSync extension activation done");
        _vfsActivationDone = true;

        // Clear LiteSync errors
        ExitCode exitCode = ServerRequests::deleteLiteSyncErrors();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::deleteLiteSyncErrors: " << toInt(exitCode));
        }

        const std::scoped_lock lock(syncPalMapMutex);
        for (const auto &[syncDbId, syncPal]: syncPalMap) {
            const std::scoped_lock lock2(vfsMapMutex);
            auto vfsMapIt = vfsMap.find(syncDbId);
            if (vfsMapIt == vfsMap.end()) continue;
            if (vfsMapIt->second->mode() == VirtualFileMode::Mac) {
                // Ask client to refresh SyncPal error list
                sendErrorsCleared(syncDbId);

                // Start VFS
                if (ExitInfo exitInfo = vfsMapIt->second->start(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone);
                    !exitInfo) {
                    LOG_WARN(Log::instance()->getLogger(), "Error in Vfs::start: " << exitInfo);
                    continue;
                }

                // Start SyncPal if not paused
                Sync sync;
                bool found = false;
                if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
                    LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
                    continue;
                }

                if (!found) {
                    LOG_WARN(Log::instance()->getLogger(), "Sync not found in sync table for syncDbId=" << syncDbId);
                    continue;
                }

                if (!sync.paused()) {
                    // Start sync
                    syncPal->start();
                }
            }
        }
    }
#endif

    const std::scoped_lock lock(syncPalMapMutex);
    for (const auto &[_, syncPal]: syncPalMap) {
        if (!syncPal) continue;
        if ((syncPal->isPaused() || syncPal->pauseAsked()) &&
            syncPal->pauseTime() + std::chrono::milliseconds(syncPal->pauseDuration()) < std::chrono::steady_clock::now()) {
            syncPal->unpause();
#if defined(__APPLE__)
            Utility::restartFinderExtension();
#endif
        }
    }
}
} // namespace KDC
