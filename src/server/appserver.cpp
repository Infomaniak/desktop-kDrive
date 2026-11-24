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

#include "utility/kdexception.h"
#if defined(KD_LINUX)
#include "qtlocalpeer.h"
#include "runningprocessinfo_linux.h"
#endif
#include "version.h"
#include "migration/migrationparams.h"
#include "keychainmanager/keychainmanager.h"
#include "requests/offlinefilessizeestimator.h"
#include "comm/guijobmanager.h"
#if defined(KD_MACOS) || defined(KD_WINDOWS)
#include "comm/extjobmanager.h"
#endif
#include "comm/guijobs/signalsyncnotifymanydeletesjob.h"
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
#include "server/comm/guijobs/signalloginsendauthorizationcodejob.h"
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

#include "libcommon/data/driveavailable.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommon/utility/logiffail.h"
#include "libcommon/comm.h"
#include "libcommon/data/user.h"
#include "libcommon/data/exclusiontemplate.h"
#include "libcommon/log/sentry/handler.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/vfs/vfs.h"
#include "libcommonserver/utility/utility.h"

#include "libsyncengine/requests/parameterscache.h"
#include "libsyncengine/requests/exclusiontemplatecache.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "utility/timerutility.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdint>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

#if defined(KD_WINDOWS)
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QIcon>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#define LOAD_PROGRESS_INTERVAL 1000 // ms
#define SEND_NOTIFICATIONS_INTERVAL 15000 // ms
#define RESTART_SYNCS_INTERVAL 15000 // ms
#define START_SYNCPALS_RETRY_INTERVAL 60000 // ms
#define START_SYNCPALS_TIME_GAP 5 // sec

namespace KDC {

namespace {
// Runs a token check through AppServer::_loadUserInfo so that it can be mocked in tests
class TokenCheckJob : public AbstractJob {
    public:
        explicit TokenCheckJob(const std::function<ExitInfo()> &check) :
            _check(check) {}
        ExitInfo runJob() override { return _check(); }

    private:
        std::function<ExitInfo()> _check;
};
} // namespace

std::vector<AppServer::Notification> AppServer::_notifications;
std::unique_ptr<UpdateManager> AppServer::_updateManager;

namespace {

static const char optionsC[] =
        "Options:\n"
        "  -h --help            : show this help screen.\n"
        "  -v --version         : show the application version.\n"
        "  --settings           : show the Settings window (if the application is running).\n"
        "  --synthesis          : show the Synthesis window (if the application is running).\n";

} // namespace

static const QString showSynthesisMsg("showSynthesis");
static const QString showSettingsMsg("showSettings");
static const QString restartClientMsg("restartClient");
static const QString authorizationCodeMsg("redirectLogin");
static const QString separatorMsg("$$$");
static constexpr std::int32_t singleApplicationMessageTimeoutMs = 5000;

static const QString crashMsg = SharedTools::QtSingleApplication::tr("kDrive application will close due to a fatal error.");

constexpr int32_t delayedActionMs = 1000;

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

    // Only the main server initializes logging: doing so archives and purges the log files of the folder, including the
    // active ones of a running server and client. A secondary process (OAuth callback, help or version, request forwarded
    // to the running server) stops before, and reports on the console only.
    parseOptions(_arguments);
    if (_helpAsked || _versionAsked) {
        return;
    }

    if (isRunning()) {
        return;
    }
#if defined(KD_LINUX)
    if (const auto processPid = runningProcessPid(APPLICATION_EXECUTABLE); processPid.has_value()) {
        _runningServerPid = *processPid;
        return;
    }
#endif

    // OAuth callbacks start a short-lived helper process: without a running server, there is nothing to forward to.
    if (!_authorizationCodeStr.isEmpty()) {
        return;
    }

    // Setup logging with default parameters
    if (!initLogging()) {
        throw std::runtime_error("Unable to init logging.");
    }

    if (_clearSyncNodesAsked || _clearKeychainKeysAsked) {
        LOG_INFO(_logger, "Command line options processed");
        return;
    }
    // Cleanup at quit
    connect(this, &QCoreApplication::aboutToQuit, this, &AppServer::onCleanup);

    // Setup single application: show the Settings or Synthesis window if the application is running.
    connect(this, &QtSingleApplication::messageReceived, this, &AppServer::onMessageReceivedFromAnotherProcess);
#if defined(KD_MACOS)
    // Qt emits ApplicationActive when macOS asks an already running application to reopen.
    connect(this, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && (!_updateManager || !_updateManager->isUpdateSessionInProgress())) showSynthesis();
    });
#endif

#if defined(KD_LINUX)
    // This adds a bit of Qt to the server, which is deliberate: it temporarily makes the OAuth callback reliable, and the
    // planned removal of QtSingleApplication will de facto remove this small Qt addition along with it.
    // The removal of QtSingleApplication will be done before Qt is fully removed from the server.
    _fallbackLocalPeer = std::make_unique<SharedTools::QtLocalPeer>(
            this, applicationId() + QLatin1Char('-') + QString::number(QCoreApplication::applicationPid()));
    (void) connect(_fallbackLocalPeer.get(), &SharedTools::QtLocalPeer::messageReceived, this,
                   &AppServer::onMessageReceivedFromAnotherProcess);
    // isClient() starts the fallback server when possible; true means this process is only a client.
    if (_fallbackLocalPeer->isClient()) {
        _fallbackLocalPeer.reset();
    } else {
        LOG_INFO(_logger, "Started Linux fallback single-application peer");
    }
#endif

    if (!Utility::registerLoginRedirection()) {
        std::string errorMsg = "Failed to register login redirection";
        LOG_ERROR(_logger, errorMsg);
        KDC::sentry::Handler::captureMessage(KDC::sentry::Level::Error, "Login redirection registration error", errorMsg);
    }

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
    sentry::Handler::instance()->setIsSentryActivated(ParametersCache::instance()->parameters().sentryEnabled());

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
        ServerRequests::fixProxyConfig();
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

#if defined(KD_MACOS)
    // Check LoginItemAgent process
    if (Utility::isLoginItemAgentRunning()) {
        // Unregister LoginItemAgent, it will be registered by the CommManager when it starts
        LOG_DEBUG(_logger, "Unregister LoginItemAgent");
        if (!Utility::registerLoginItemAgent(false)) {
            LOG_WARN(_logger, "Error in Utility::registerLoginItemAgent");
        }
    }
#endif

    // Init CommManager
    LOG_DEBUG(_logger, "Init CommManager");
    _commManager = std::make_shared<CommManager>(*this);
    _commManager->start();

    // Set sentry user
    updateSentryUser();
    QTimer::singleShot(0, this, [this]() {
        // Update users,accounts and drives info.
        if (const auto exitInfo = updateAllUsersInfo(UpdateFollowUpAction::CleanUserDbEntry);
            exitInfo.code() == ExitCode::InvalidToken) {
            // The user will be asked to enter its credentials later.
        } else if (!exitInfo) {
            LOG_WARN(_logger, "Error in updateAllUsersInfo: " << exitInfo);
            addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
        }

        // refresh sentry user
        updateSentryUser();
    });

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
#elif defined(KD_LINUX)
        // The Linux client only quits on UTILITY_QUIT, so the server schedules its own quit once the client is told.
        const std::function<void()> quitCallback = [this] {
            sendQuit();
            quitLater(delayedActionMs);
        };
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
        quitLater();
        return;
    }

    // Start syncs
    LOG_DEBUG(_logger, "Start syncs");
    QTimer::singleShot(0, this, [this]() { startSyncsAndRetryOnError(); });

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

    // Start client
    if (!startClient()) {
        LOG_FATAL(_logger, "Failed to start kDrive client during application startup");
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

    // Stop timers: their slots access the SyncPals and ParmsDb, which are released below
    _loadSyncsProgressTimer.stop();
    _sendFilesNotificationsTimer.stop();
    _restartSyncsTimer.stop();

    // Stop CommManager
    if (_commManager) {
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
    if (_commManager) {
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

    // Destroy the update manager while the Qt event loop is still alive and after the SyncJobManager running its network
    // jobs is cleared. Otherwise it is destroyed during static destruction after ~AppServer, crashing at shutdown.
    reset();
    LOG_DEBUG(_logger, "UpdateManager destroyed");

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

// Schedule the quit on the Qt application thread. Most callers only need the default zero-delay queued execution, while
// asynchronous operations can request enough time to finish before aboutToQuit starts cleanup.
// Note: this function is intentionally thread-safe — it may be called from non-Qt threads (e.g. Poco thread pool).
// QTimer::singleShot called directly from a non-Qt thread cannot reliably transfer the timer to the main thread
// in release builds. Instead, we use QMetaObject::invokeMethod (Qt::QueuedConnection) to first post the
// timer creation to the main thread's event loop, then start the timer there.
void AppServer::quitLater(const int32_t delayMs) {
    auto *app = QCoreApplication::instance();
    if (!app) return;
    QMetaObject::invokeMethod(
            app, [delayMs] { QTimer::singleShot(delayMs, [] { QCoreApplication::quit(); }); }, Qt::QueuedConnection);
}

// This task can be long and block the GUI
ExitInfo AppServer::stopSyncTask(const SyncDbId syncDbId,
                                 const SyncPal::DbBehaviorAfterStop behavior /*= SyncPal::DbBehaviorAfterStop::Keep*/) {
    if (behavior == SyncPal::DbBehaviorAfterStop::Remove) {
        // Mark the sync for deletion in the parameters DB.
        if (bool found = false; !ParmsDb::instance()->setSyncToDelete(syncDbId, true, found)) {
            LOG_WARN(_logger, "Error in setSyncToDelete for syncDbId=" << syncDbId);
            return ExitCode::DbError; // We cannot continue if we cannot mark the sync for deletion in the DB as stopVfs (on
                                      // Windows only) could delete dehydrated placeholders whereas the sync is still in the DB.
        } else if (!found) {
            LOG_WARN(_logger, "Sync not found in DB for syncDbId=" << syncDbId);
        }
    }

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
        // The SyncPal count can be 2 if its Remote File System Observer Worker still exists as it has a reference to the
        // SyncPal. It can be 1 if the SyncPal is scheduled for destruction since it has a reference to itself.
        LOG_IF_FAIL(!syncPalMap[syncDbId] || syncPalMap[syncDbId].use_count() <= 2)
        (void) syncPalMap.erase(syncDbId);
    }

    {
        const std::scoped_lock lock(vfsMapMutex);
        LOG_IF_FAIL(!vfsMap[syncDbId] ||
                    vfsMap[syncDbId].use_count() <= 2) // `use_count` can be zero when the local drive has been removed.
        (void) vfsMap.erase(syncDbId);
    }
    return ExitCode::Ok;
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

ExitInfo AppServer::stopAllSyncsTask(const std::vector<SyncDbId> &syncDbIdList,
                                     const SyncPal::DbBehaviorAfterStop behavior /*= SyncPal::DbBehaviorAfterStop::Keep*/) {
    for (const auto syncDbId: syncDbIdList) {
        if (ExitInfo exitInfo = stopSyncTask(syncDbId, behavior); !exitInfo) {
            LOG_WARN(_logger, "Error in stopSyncTask for syncDbId=" << syncDbId << " : " << exitInfo);
            return exitInfo;
        }
    }
    return ExitCode::Ok;
}

ExitInfo AppServer::deleteAccount(const AccountDbId accountDbId) {
    // Delete the account
    const ExitInfo exitInfo = ServerRequests::deleteAccount(accountDbId);
    if (exitInfo) {
        sendAccountRemoved(accountDbId);
    } else {
        LOG_WARN(_logger, "Error in Requests::deleteAccount: " << exitInfo);
        addError(Error(ERR_ID, exitInfo, ExitCause::Unknown));
        return exitInfo;
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::deleteDrive(const DriveDbId driveDbId) {
    // Get the drive in DB
    bool found = false;
    Drive drive;
    if (!ParmsDb::instance()->selectDrive(driveDbId, drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectDrive");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found for driveDbId=" << driveDbId);
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // Delete the drive
    if (const ExitInfo exitInfo = ServerRequests::deleteDrive(driveDbId); !exitInfo) {
        LOG_WARN(_logger, "Error in Requests::deleteDrive: code=" << exitInfo);
        return exitInfo;
    }

    // Try to delete the account if there is no remaining drive, any failure in this step is not critical, the account will be
    // deleted later.
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(drive.accountDbId(), driveList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
        sendDriveRemoved(driveDbId);
    } else if (!driveList.empty() || !deleteAccount(drive.accountDbId())) {
        sendDriveRemoved(driveDbId); // Useless if deleteAccount succeeds, sendAccountRemoved already updates the drive list.
                                     // May even cause a crash if both are processed concurrently on the GUI side.
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::deleteSync(const SyncDbId syncDbId) {
    // Get the sync in DB
    bool found = false;
    Sync sync;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Sync not found for syncDbId=" << syncDbId);
        return {ExitCode::DataError, ExitCause::DbEntryNotFound};
    }

    // Delete the sync
    const ExitInfo exitInfo = ServerRequests::deleteSync(syncDbId);
    if (!exitInfo) {
        LOG_WARN(_logger, "Error in Requests::deleteSync: " << exitInfo);
        return exitInfo;
    }

    // Delete the drive if there is no remaining sync, any failure in this step is not critical, the drive will be deleted later.
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(sync.driveDbId(), syncList)) {
        LOG_WARN(_logger,
                 "Error in ParmsDb::selectAllSyncs, unable to check if the drive has remaining syncs, keep the drive for safety");
        sendSyncRemoved(syncDbId);
    } else if (!syncList.empty() || !deleteDrive(sync.driveDbId())) {
        sendSyncRemoved(syncDbId); // Useless if deleteDrive is called, sendDriveRemoved already updates the sync list.
                                   // May even cause a crash if both sendDriveRemoved and sendSyncRemoved are processed
                                   // concurrently on the GUI side.
    }
    return ExitCode::Ok;
}

void AppServer::deleteSyncAsBackgroundTask(const SyncDbId syncDbId) {
    QTimer::singleShot(100, [this, syncDbId]() {
        if (ExitInfo exitInfo = AppServer::stopSyncTask(syncDbId,
                                                        SyncPal::DbBehaviorAfterStop::Remove);
            !exitInfo) // This task can be long, hence blocking, on Windows.
        {
            LOG_WARN(_logger, "Error in stopSyncTask for syncDbId=" << syncDbId << " : " << exitInfo);
            addError(Error(ERR_ID, exitInfo));
            return;
        }

        // Delete sync from DB
        if (ExitInfo exitInfo = deleteSync(syncDbId); !exitInfo) {
            LOG_WARN(_logger, "Error in deleteSync for syncDbId=" << syncDbId << " : " << exitInfo);
            addError(Error(ERR_ID, exitInfo));
        }
#if defined(KD_MACOS)
        Utility::restartFinderExtension();
#endif
    });
}

void AppServer::logExtendedLogActivationMessage(const bool isExtendedLogEnabled) noexcept {
    const std::string activationStatus = isExtendedLogEnabled ? "enabled" : "disabled";
    const std::string msg = "Extended logging is " + activationStatus + ".";

    LOG_INFO(_logger, msg);
}

ExitInfo AppServer::updateParametersAndPropagateChanges(const Parameters &newParameters) {
    auto updatedParameters = newParameters;

    // Retrieve current settings
    const auto previousParameters = ParametersCache::instance()->parameters();

    // Proxy parameters change propagation. Must be executed before "updateParameters" in order to save the new keychain
    // key in DB.
    if (updatedParameters.proxyConfig().needsAuth()) {
        std::string keychainKey;
        if (!previousParameters.proxyConfig().needsAuth()) {
            // Proxy needs authentification now, generate keychain key and save password to the keychain.
            keychainKey = Utility::computeMd5Hash(std::to_string(std::time(nullptr)));
        } else {
            // Proxy already needed authentification, keep the same keychain key and update the password in the keychain.
            keychainKey = previousParameters.proxyConfig().keychainKey();
        }
        if (!KeyChainManager::instance()->writeData(keychainKey, updatedParameters.proxyConfig().pwd())) {
            LOG_WARN(_logger, "Failed to write password token into keychain");
            return ExitInfo(ExitCode::SystemError);
        }

        auto proxyConfig = updatedParameters.proxyConfig();
        proxyConfig.setKeychainKey(keychainKey);
        updatedParameters.setProxyConfig(proxyConfig);
    } else if (previousParameters.proxyConfig().needsAuth()) {
        // Proxy does not need authentification anymore, remove the entry from the keychain key.
        if (!KeyChainManager::instance()->deleteData(previousParameters.proxyConfig().keychainKey())) {
            LOG_WARN(_logger, "Failed to remove proxy password from keychain");
        }
    }

    // Update parameters
    const ExitCode exitCode = ServerRequests::updateParameters(updatedParameters);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::updateParameters");
        addError(Error(ERR_ID, exitCode));
    }

    // Propagate extendedLog change
    if (previousParameters.extendedLog() != updatedParameters.extendedLog()) {
        logExtendedLogActivationMessage(updatedParameters.extendedLog());
        const std::scoped_lock lock(vfsMapMutex);
        for (const auto &[_, vfs]: vfsMap) {
            vfs->setExtendedLog(updatedParameters.extendedLog());
        }
    }

    // Propagate language change
    if (previousParameters.language() != updatedParameters.language()) {
        const auto language = updatedParameters.language();
        QTimer::singleShot(100, this, [this, language]() { CommonUtility::setupTranslations(this, language); });
    }

    // Propagate autostart change
    if (previousParameters.autoStart() != updatedParameters.autoStart()) {
        auto *theme = Theme::instance();
        (void) Utility::setLaunchOnStartup(theme->appName(), theme->appName(), updatedParameters.autoStart());
    }

    // Propagate Sentry activation change
    if (previousParameters.sentryEnabled() != updatedParameters.sentryEnabled()) {
        sentry::Handler::instance()->setIsSentryActivated(updatedParameters.sentryEnabled());
    }

    // Propagate distribution channel change. Enabling automatic updates installs nothing at once: the next scheduled check
    // does it.
    setDistributionChannel(updatedParameters.distributionChannel());

    return exitCode;
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
        if (_commManager) {
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
        if (_commManager) {
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

void AppServer::setDistributionChannel(const DistributionChannel versionChannel) {
    if (_noUpdate) return;

    if (!_updateManager) {
        LOG_WARN(_logger, "The update manager is not set.");
        return;
    }

    _updateManager->setDistributionChannel(versionChannel);
}

VersionInfo AppServer::getVersionInfo() const {
    if (_noUpdate) {
        return VersionInfo::current();
    }

    if (!_updateManager) {
        LOG_WARN(_logger, "The update manager is not set.");
        return VersionInfo::current();
    }

    return _updateManager->versionInfo();
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

void AppServer::startSyncsAndRetryOnError(const std::unordered_set<SyncDbId> &toIgnoreSyncDbIds) {
    LOG_DEBUG(_logger, "Start syncs");
    std::unordered_set<SyncDbId> startedSyncDbIds;
    if (const auto exitInfo = startSyncs(toIgnoreSyncDbIds, startedSyncDbIds); !exitInfo) {
        LOG_WARN(_logger, "Error in startSyncsAndRetryOnError: " << exitInfo);
        if (exitInfo.code() == ExitCode::SystemError &&
            (exitInfo.cause() == ExitCause::SyncDirAccessError || exitInfo.cause() == ExitCause::SyncDirDiskMissing)) {
            LOG_DEBUG(_logger, "Retry to start syncs in " << START_SYNCPALS_RETRY_INTERVAL << " ms");
            startedSyncDbIds.insert(toIgnoreSyncDbIds.begin(), toIgnoreSyncDbIds.end());
            QTimer::singleShot(START_SYNCPALS_RETRY_INTERVAL, this,
                               [startedSyncDbIds, this]() { startSyncsAndRetryOnError(startedSyncDbIds); });
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

    return exitCode;
}

void AppServer::sendQuit() const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalUtilityQuitJob>());
    }
}

void AppServer::sendLogUploadStatusUpdated(LogUploadState status, int progressPercent) const {
    if (_commManager) {
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

ExitInfo AppServer::checkIfSyncIsValid(const BaseSync &sync) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    // Check for nested syncs
    for (const auto &sync_: syncList) {
        if (sync_.dbId() == sync.dbId()) {
            if (sync_.toDelete()) return {ExitCode::SystemError, ExitCause::SyncDeletionFailed};
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
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalUpdaterShowDialogJob>(_updateManager->versionInfo()));
    }
}

void AppServer::onUpdateRequired() {
    addError(Error(ERR_ID, ExitCode::UpdateRequired));
    stopAllSyncPals();
    stopAllVfs();
}

void AppServer::onUpdateStateChanged(const UpdateState state) {
    if (_commManager) {
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
        quitLater();
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
        if (_commManager && _commManager->hasActiveGuiConnection()) {
            LOG_INFO(_logger, "An active connexion with a client already exists, showing synthesis!");
            showSynthesis();
            return;
        }
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
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalLoginSendAuthorizationCodeJob>(CommonUtility::qStr2CommString(code),
                                                                                          CommonUtility::qStr2CommString(state)));
    }
}

void AppServer::sendShowNotification(const QString &title, const QString &message) const {
    // Notify client
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalUtilityShowNotificationJob>(CommonUtility::qStr2CommString(title),
                                                                                       CommonUtility::qStr2CommString(message)));
    }
}

void AppServer::sendErrorAdded(const Error &error) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalErrorAddedJob>(error));
    }
}

void AppServer::sendErrorRemoved(int64_t dbId) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalErrorRemovedJob>(dbId));
    }
}

void AppServer::addCompletedItem(const SyncDbId syncDbId, const SyncFileItem &item, const bool notify) {
    // Send completedItem signal to client
    sendSyncCompletedItem(syncDbId, item);

    if (item.status() != SyncFileStatus::Success) {
        return;
    }

    resolveItemErrors(syncDbId, item);

    if (notify) {
        // Store notification
        Notification notification;
        notification._syncDbId = syncDbId;
        notification._filename = Path2QStr(item.path());
        notification._renameTarget = item.newPath().has_value() ? Path2QStr(item.newPath().value()) : QString();
        notification._status = item.instruction();
        _notifications.push_back(notification);
    }
}

void AppServer::sendGuiSignal(std::shared_ptr<AbstractGuiJob> signal) const {
    if (_commManager) {
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
        std::vector<Drive> drives;
        if (!ParmsDb::instance()->selectAllDrives(account.dbId(), drives)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
            return ExitCode::DbError;
        }

        if (drives.empty()) {
            if (const ExitInfo exitInfo = deleteAccount(account.dbId()); !exitInfo) {
                LOG_WARN(_logger, "Error in deleteAccount for accountDbId=" << account.dbId() << ": " << exitInfo);
            }
            continue;
        }

        if (const auto exitInfo = updateAccount(account); !exitInfo) {
            LOG_WARN(_logger, "Error in updateAccount for accountDbId=" << account.dbId() << ": " << exitInfo);
        }

        for (auto &drive: drives) {
            if (const auto exitInfo = updateDrive(user, account, drive); !exitInfo) return exitInfo;
        }
    }

    return ExitCode::Ok;
}

ExitInfo AppServer::updateUser(User &user) {
    bool updated = false;
    const auto exitInfo = _loadUserInfo(user, updated);

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

        sendUserUpdated(user);
    }

    if (!exitInfo) {
        LOG_WARN(_logger, "Error in Requests::loadUserInfo: " << exitInfo);
        return exitInfo;
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
    sendAccountAdded(newAccount);

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
        sendAccountUpdated(account);

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

    if (drive.accessDenied() || drive.maintenanceInfo().inMaintenance()) {
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

    if (const auto exitInfo = manageDriveMovedToAnotherAccount(user, account, newAccountId, drive, driveUpdated); !exitInfo)
        return exitInfo;

    if (driveUpdated || quotaUpdated) {
        sendDriveUpdated(drive);
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
        if (drive.maintenanceInfo().inMaintenance()) {
            if (drive.maintenanceInfo().notRenew())
                exitCause = ExitCause::DriveNotRenew;
            else if (drive.maintenanceInfo().asleep())
                exitCause = ExitCause::DriveAsleep;
            else if (drive.maintenanceInfo().wakingUp())
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
    const std::unordered_set<SyncDbId> emptyList;
    std::unordered_set<SyncDbId> startedSyncDbIds;
    return startSyncs(emptyList, startedSyncDbIds);
}
ExitInfo AppServer::startSyncs(const std::unordered_set<SyncDbId> &toIgnoreSyncDbIds,
                               std::unordered_set<SyncDbId> &startedSyncDbIds) {
    // Load user list
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
        return {ExitCode::DbError, ExitCause::DbAccessError};
    }

    ExitInfo mainExitInfo = ExitCode::Ok;
    for (User &user: userList) {
        std::unordered_set<SyncDbId> userStartedSyncDbIds;
        if (const auto exitInfo = startSyncs(user, toIgnoreSyncDbIds, userStartedSyncDbIds); !exitInfo) {
            LOG_WARN(_logger, "Error in startSyncs for userDbId=" << user.dbId() << " " << exitInfo);
            mainExitInfo.merge(exitInfo, {ExitCode::SystemError});
        }
        startedSyncDbIds.insert(userStartedSyncDbIds.begin(), userStartedSyncDbIds.end());
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
    std::unordered_set<SyncDbId> emptyList;
    return startSyncs(user, emptyList, emptyList);
}

ExitInfo AppServer::startSyncs(User &user, const std::unordered_set<SyncDbId> toIgnoreSyncDbIds,
                               std::unordered_set<SyncDbId> &startedSyncDbIds) {
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
                if (toIgnoreSyncDbIds.contains(sync.dbId())) {
                    LOG_INFO(_logger, "Skipping sync with dbId=" << sync.dbId() << " as it is in the ignore list");
                    continue;
                }

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

                    sendSyncUpdated(sync);
                }

                // Clear old errors for this sync
                clearErrors(sync.dbId(), false);
                clearErrors(sync.dbId(), true);

                if (const auto exitInfo = checkIfSyncIsValid(sync); !exitInfo) {
                    LOG_WARN(_logger, "Error in checkIfSyncIsValid for syncDbId=" << sync.dbId() << " : " << exitInfo);
                    addError(Error(sync.dbId(), ERR_ID, exitInfo));

                    if (exitInfo.cause() == ExitCause::SyncDeletionFailed) deleteSyncAsBackgroundTask(sync.dbId());

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

                (void) startedSyncDbIds.insert(sync.dbId());
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
    SyncPath logDirPath;
    if (const auto exitInfo = CommonUtility::logDirectoryPath(logDirPath); !exitInfo) {
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
#if defined(KD_LINUX)
    LOG_INFO(_logger, "glibc version: " << CommonUtility::glibcVersion());
#endif
    LOG_INFO(_logger, "kernel version : " << QSysInfo::kernelVersion().toStdString());
    LOG_INFO(_logger, "kernel type : " << QSysInfo::kernelType().toStdString());
    LOG_INFO(_logger, "locale: " << QLocale::system().name().toStdString());
    LOG_INFO(_logger, "# of logical CPU core: " << std::thread::hardware_concurrency());

    // Log app ID
    LOG_INFO(Log::instance()->getLogger(), "App ID: " << appUID());

    // Log Sentry activation status
    LOG_INFO(Log::instance()->getLogger(), "Sentry enabled: " << ParametersCache::instance()->parameters().sentryEnabled());

    // Log Matomo activation status
    LOG_INFO(Log::instance()->getLogger(), "Matomo enabled: " << ParametersCache::instance()->parameters().matomoEnabled());

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

void AppServer::sendShowSettingsMsg(qint64 pid) {
    if (!sendMessage(showSettingsMsg, singleApplicationMessageTimeoutMs, pid)) {
        std::cerr << "Failed to forward show-settings request to the running server" << std::endl;
    }
}

void AppServer::sendShowSynthesisMsg(qint64 pid) {
    if (!sendMessage(showSynthesisMsg, singleApplicationMessageTimeoutMs, pid)) {
        std::cerr << "Failed to forward show-synthesis request to the running server" << std::endl;
    }
}

void AppServer::sendRestartClientMsg(qint64 pid) {
    if (!sendMessage(restartClientMsg, singleApplicationMessageTimeoutMs, pid)) {
        std::cerr << "Failed to forward restart-client request to the running server" << std::endl;
    }
}

void AppServer::sendAuthorizationCode(qint64 pid) {
    if (Log::isSet()) {
        LOG_INFO(Log::instance()->getLogger(), "Forwarding login authorization callback to the running server");
    }
    if (!sendMessage(authorizationCodeMsg + separatorMsg + _authorizationCodeStr, singleApplicationMessageTimeoutMs, pid)) {
        if (Log::isSet()) {
            LOG_WARN(Log::instance()->getLogger(), "Failed to forward login authorization callback to the running server");
        }
    }
}

void AppServer::showSettings() {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalUtilityShowSettingsJob>());
    }
}

void AppServer::showSynthesis() {
    if (_commManager) {
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
        KeyChainManager::instance()->deleteData(user.keychainKey());
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
    bool startClient = false;
#ifdef QT_NO_DEBUG
    startClient = true;
#endif

    startClient |= QProcessEnvironment::systemEnvironment().value("KDRIVE_DEBUG_RUN_CLIENT") == "1";
    if (startClient) {
        auto getPathToExecutable = [](const SyncPath &executableName) {
            auto pathToExecutable = QStr2Path(CommonUtility::applicationDirPath()) / executableName;
            auto ioError = IoError::Success;
            if (auto exists = false;
                !IoHelper::checkIfPathExists(pathToExecutable, exists, ioError, IoHelper::PathCheckOption::Insensitive) ||
                !exists || ioError != IoError::Success) {
                return SyncPath();
            }
            return pathToExecutable;
        };

        const auto pathToExecutable = getPathToExecutable(APPLICATION_CLIENTV4_EXECUTABLE);
        if (pathToExecutable.empty()) {
            LOG_FATAL(_logger, "Failed to start kDrive client (executable not found)");
            return false;
        }

        QStringList arguments;
        if (_commManager) {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
            const auto port = _commManager->tryGetGUICommPort();
            if (port <= 0) {
                LOG_ERROR(_logger, "Failed to start kDrive client (comm manager port isn't available)");
                return false;
            }
            arguments << QString::number(port);

#else
            // On macOS the client communicates with the server through XPC and doesn't need the port number as argument.
#endif
        } else {
            LOG_ERROR(_logger, "Failed to start kDrive client (no communication method available)");
            return false;
        }

        if (synthesisAsked()) {
            arguments << "--synthesis";
        } else if (settingsAsked()) {
            arguments << "--settings";
        }

        LOGW_INFO(_logger, L"Starting kDrive client - path=" << Utility::formatSyncPath(pathToExecutable) << L" args="
                                                             << (arguments.size() >= 1 ? arguments[0].toStdWString() : L""));

        _clientProcess = new QProcess(this);
        _clientProcess->setProgram(Path2QStr(pathToExecutable));
        _clientProcess->setArguments(arguments);

        _clientProcess->start();
        if (!_clientProcess->waitForStarted()) {
            LOG_WARN(_logger, "Failed to start kDrive client");
            return false;
        }

        _synthesisAsked = false;
        _settingsAsked = false;
    }

    return true;
}

ExitInfo AppServer::updateAllUsersInfo(const UpdateFollowUpAction action) {
    std::vector<User> users;
    if (!ParmsDb::instance()->selectAllUsers(users)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }

    for (auto &user: users) {
        std::vector<Account> accounts;
        if (!ParmsDb::instance()->selectAllAccounts(user.dbId(), accounts)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllAccounts");
            return ExitCode::DbError;
        }
        if (action == UpdateFollowUpAction::CleanUserDbEntry && accounts.empty()) {
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
            if (exitInfo.code() == ExitCode::InvalidToken) {
                // No sync is involved yet: flag the user as disconnected so that the GUI prompts to log in again
                manageInvalidTokenError(user);
                continue;
            }
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
        } catch (const SyncPalInitException &e) {
            LOG_WARN(_logger, "Error in SyncPal::SyncPal for syncDbId=" << sync.dbId() << " : " << e.what());
            return kdExceptionToExitInfo(e);
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
        syncPalMapIt->second->setSendManyDeletesNotification(std::bind_front(&AppServer::sendManyDeletesNotification, this));

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

#if defined(KD_MACOS)
ExitInfo AppServer::installVfs() {
    VfsSetupParams vfsSetupParams;
    vfsSetupParams.logger = _logger;
    vfsSetupParams.sentryHandler = sentry::Handler::instance();
    vfsSetupParams.executeCommand = []([[maybe_unused]] const CommString &command, [[maybe_unused]] bool broadcast) {
        if (_commManager) {
            _commManager->executeCommandDirect(command, broadcast);
        }
    };

    QString error;
    std::shared_ptr vfs = KDC::createVfsFromPlugin(KDC::VirtualFileMode::Mac, vfsSetupParams, error);
    if (!vfs) {
        LOG_WARN(_logger,
                 "Error in Vfs::createVfsFromPlugin for mode " << KDC::VirtualFileMode::Mac << " : " << error.toStdString());
        return {ExitCode::SystemError, ExitCause::UnableToStartVfs};
    }

    // Set callbacks
    vfs->setExclusionAppListCallback(std::bind_front(&AppServer::exclusionAppList, this));

    // Start VFS
    if (ExitInfo exitInfo = vfs->start(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone); !exitInfo) {
        LOG_WARN(_logger, "Error in Vfs::start: " << exitInfo);
        return exitInfo;
    }

    return ExitCode::Ok;
}
#endif

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
    if (auto ioError = IoError::Unknown;
        !IoHelper::checkIfPathExists(sync.localPath(), exists, ioError, IoHelper::PathCheckOption::Insensitive)) {
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
        if (const auto ioError = IoHelper::getWeakCanonicalPath(sync.localPath(), vfsSetupParams.localPath);
            ioError != IoError::Success) {
            LOGW_WARN(_logger, L"Error in IoHelper::getWeakCanonicalPath: " << Utility::formatIoError(sync.localPath(), ioError));
            return {ExitCode::SystemError, ExitCause::SyncDirAccessError};
        }
        vfsSetupParams.targetPath = sync.targetPath();
        vfsSetupParams.executeCommand = []([[maybe_unused]] const CommString &command, [[maybe_unused]] bool broadcast) {
#if defined(KD_MACOS) || defined(KD_WINDOWS)
            if (_commManager) {
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

ExitInfo AppServer::setSupportsVirtualFiles(const SyncDbId syncDbId, const bool value) {
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
        // Reset the VFS registration time: a new sync root registration is expected and must not trigger a revert of local
        // deletes.
        sync.setVfsRegisteredAt(0);

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
        sendSyncUpdated(sync);

        if (newMode != VirtualFileMode::Off && vfs) {
            // Clear file system
            vfs->convertDirContentToPlaceholder(SyncName2QStr(sync.localPath()), true);
        }

        if (!startPostponed) {
            // Re-start sync
            const std::scoped_lock lock3(syncPalMapMutex);
            auto syncPalMapIt2 = syncPalMap.find(syncDbId);
            if (syncPalMapIt2 != syncPalMap.end()) {
                syncPalMapIt2->second->start();
            }
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

ExitInfo AppServer::acknowledgeManyDeletes(const SyncDbId syncDbId, const TooManyDeletesUserChoice userChoice) {
    const std::scoped_lock lock(syncPalMapMutex);
    auto syncPalMapIt = syncPalMap.find(syncDbId);
    if (syncPalMapIt == syncPalMap.end()) {
        std::stringstream msg;
        msg << "SyncPal not found in syncPalMap for syncDbId=" << syncDbId;
        LOG_WARN(_logger, msg.str());
        sentry::Handler::captureMessage(sentry::Level::Error, "Error in acknowledgeManyDeletes", msg.str());
        return ExitCode::LogicError;
    }

    syncPalMapIt->second->setManyDeleteOpsUserChoice(userChoice);
    LOG_INFO(_logger, "Set acknowledge many deletes to " << userChoice << " for syncDbId=" << syncDbId);

    syncPalMapIt->second->start();

    return ExitCode::Ok;
}

void AppServer::addError(const Error &error) const {
    if (error.isStale()) {
        LOG_WARN(Log::instance()->getLogger(), "Cannot add a stale error");
        sentry::Handler::captureMessage(sentry::Level::Warning, "Cannot add a stale error", "Cannot add a stale error");
        return;
    }

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

    manageError(errorCopy, errorList, errorAlreadyExists);

    if (ServerRequests::isDisplayableError(errorCopy)) {
        // Notify the client
        if (errorAlreadyExists) {
            // First remove the existing error
            sendErrorRemoved(errorCopy.dbId());
        }

        sendErrorAdded(errorCopy);
    }
}

void AppServer::manageError(const Error &error, std::vector<Error> &errorList, bool errorAlreadyExists) const {
    User user;
    Account account;
    Drive drive;
    Sync sync;
    if (error.syncDbId() &&
        ServerRequests::getDbStructsFromSyncDbId(error.syncDbId(), user, account, drive, sync) != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ServerRequests::getDbStructsFromSyncDbId");
        return;
    }

    if (error.exitCode() == ExitCode::InvalidToken) {
        manageInvalidTokenError(user);
    } else if (error.exitCode() == ExitCode::BackError && error.exitCause() == ExitCause::DriveAccessError) {
        manageDriveAccessError(drive);
        checkTokenAfterDriveAccessError(user);
    } else if (error.exitCode() == ExitCode::NetworkError && error.exitCause() == ExitCause::SocketsDefuncted) {
        manageSocketsDefunctedError();
    } else if (error.exitCode() == ExitCode::SystemError && error.exitCause() == ExitCause::FileAccessError) {
        manageFileAccessErrorError(errorList);
    } else if (_updateManager && error.exitCode() == ExitCode::UpdateRequired) {
        manageUpdateRequiredErrorError();
    }

    if (!error.isAutoResolved() && !errorAlreadyExists) {
        // Send error to sentry only for technical errors
        SentryUser sentryUser(user.email(), user.name(), std::to_string(user.userId()));
        sentry::Handler::captureMessage(sentry::Level::Warning, "AppServer::addError", error.errorString(), sentryUser);
    }
}

void AppServer::manageDriveAccessError(Drive &drive) const {
    // Manage drive access error
    LOG_DEBUG(Log::instance()->getLogger(), "Manage drive access error");

    if (!drive.dbId()) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid error");
        return;
    }

    // Update drive
    drive.setAccessDenied(true);
    bool found = false;
    if (!ParmsDb::instance()->updateDrive(drive, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateDrive");
        return;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Drive not found with dbId=" << drive.dbId());
        return;
    }

    sendDriveUpdated(drive);
}

void AppServer::checkTokenAfterDriveAccessError(const User &user) const {
    if (!user.dbId()) return;

    // While a revoked token is still cached, a 401 on the Drive API cannot be told apart from a lost access to the drive and is
    // reported as DriveAccessError. The user profile API answers 401 only if the token itself is invalid.
    // The check runs asynchronously: addError is called synchronously by the sync workers, which must not wait for a request.
    {
        const std::scoped_lock lock(_tokenCheckMutex);
        if (!_tokenCheckInProgress.insert(user.dbId()).second) return; // A check is already running for this user
    }

    const auto userDbId = user.dbId();
    const auto job = std::make_shared<TokenCheckJob>([this, user = User(user)]() mutable {
        bool updated = false;
        return _loadUserInfo(user, updated);
    });
    const std::weak_ptr<TokenCheckJob> weakJob = job;
    job->setAdditionalCallback([this, userDbId, weakJob](const UniqueId) {
        if (const auto checkJob = weakJob.lock(); checkJob && checkJob->exitInfo().code() == ExitCode::InvalidToken) {
            LOG_INFO(Log::instance()->getLogger(), "DriveAccessError caused by an invalid token for userDbId=" << userDbId);
            // Flags the user as disconnected and removes the DriveAccessError errors of their drives
            manageInvalidTokenError(userDbId);
        }

        // Released last, so that the check is reported as finished only once its outcome is applied
        const std::scoped_lock lock(_tokenCheckMutex);
        (void) _tokenCheckInProgress.erase(userDbId);
    });
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_HIGH);
}

void AppServer::manageInvalidTokenError(User &user) const {
    // Manage invalid token error
    LOG_DEBUG(Log::instance()->getLogger(), "Manage invalid token error");

    if (!user.dbId()) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid error");
        return;
    }

    // Update user
    user.setKeychainKey(std::string());
    user.setConnected(false);
    bool found = false;
    if (!ParmsDb::instance()->updateUser(user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateUser");
        return;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User not found with dbId=" << user.dbId());
        return;
    }

    sendUserUpdated(user);
}

void AppServer::manageInvalidTokenError(const UserDbId userDbId) const {
    User user;
    bool found = false;
    if (!ParmsDb::instance()->selectUser(userDbId, user, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectUser");
        return;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "User not found with dbId=" << userDbId);
        return;
    }

    manageInvalidTokenError(user);
}

void AppServer::manageSocketsDefunctedError() const {
    // Manage sockets defuncted error
    LOG_WARN(Log::instance()->getLogger(), "Manage sockets defuncted error");

    sentry::Handler::captureMessage(sentry::Level::Warning, "AppServer::addError", "Sockets defuncted error");

    // Decrease upload session max parallel jobs
    ParametersCache::instance()->decreaseUploadSessionParallelThreads();

    // Decrease JobManager pool capacity
    SyncJobManagerSingleton::instance()->decreasePoolCapacity();
}

void AppServer::manageFileAccessErrorError(const std::vector<Error> &errorList) const {
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
}

void AppServer::manageUpdateRequiredErrorError() const {
    _updateManager->updater()->unskipVersion();
}

void AppServer::resolveItemErrors(const SyncDbId syncDbId, const SyncFileItem &item) const {
    std::vector<Error> errorList;

    bool found = false;
    if (!ParmsDb::instance()->selectErrorByNodeInfo(syncDbId, item.localNodeId(), item.remoteNodeId(), item.path(),
                                                    item.newPath(), errorList, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectErrorByNodeInfo");
        return;
    }

    // Keep cancellation errors visible after the operation is retried
    std::erase_if(errorList, [](const Error &error) {
        switch (error.cancelType()) {
            case CancelType::Create:
            case CancelType::Edit:
            case CancelType::Move:
            case CancelType::Delete:
                return true;
            default:
                return false;
        }
    });

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

void AppServer::sendUserAdded(const User &user) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalUserAddedJob>(user));
    }
}

void AppServer::sendUserUpdated(const User &user) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalUserUpdatedJob>(user));
    }
}

void AppServer::sendUserRemoved(const UserDbId userDbId) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalUserRemovedJob>(userDbId));
    }
}

void AppServer::sendAccountAdded(const Account &account) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalAccountAddedJob>(account));
    }
}

void AppServer::sendAccountUpdated(const Account &account) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalAccountUpdatedJob>(account));
    }
}

void AppServer::sendAccountRemoved(const AccountDbId accountDbId) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalAccountRemovedJob>(accountDbId));
    }
}

void AppServer::sendDriveAdded(const Drive &drive) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalDriveAddedJob>(drive));
    }
}

void AppServer::sendDriveUpdated(const Drive &drive) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalDriveUpdatedJob>(drive));
    }
}

void AppServer::sendDriveRemoved(const DriveDbId driveDbId) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalDriveRemovedJob>(driveDbId));
    }
}

void AppServer::sendSyncUpdated(const Sync &sync) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncUpdatedJob>(sync));
    }
}

void AppServer::sendSyncRemoved(const SyncDbId syncDbId) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncRemovedJob>(syncDbId));
    }
}

void AppServer::sendManyDeletesNotification(const SyncDbId syncDbId, const TooManyDeletesNotificationType notificationType,
                                            const Count nbDeletes, const std::vector<SyncPath> &filesPaths) const {
    if (_commManager) {
        _commManager->sendGuiSignal(
                std::make_shared<SignalSyncNotifyManyDeletesJob>(syncDbId, notificationType, nbDeletes, filesPaths));
    }
}

void AppServer::sendSyncProgressInfo(const SyncDbId syncDbId, const SyncStatus status, const SyncStep step,
                                     const SyncProgress &progress) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncProgressInfoJob>(syncDbId, status, step, progress));
    }
}

void AppServer::sendSyncCompletedItem(const SyncDbId syncDbId, const SyncFileItem &item) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncCompletedItemJob>(syncDbId, item));
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_DEBUG(_logger, L"Send progress for syncDbId=" << syncDbId << L" path=" << Path2WStr(item.path()) << L" size="
                                                               << item.size() << L" progress=" << item.progress()
                                                               << L"% to new gui");
        }
    }
}

void AppServer::sendSyncAdded(const Sync &sync) const {
    if (_commManager) {
        _commManager->sendGuiSignal(std::make_shared<SignalSyncAddedJob>(sync));
    }
}

void AppServer::onLoadInfo() {
    if (const auto exitInfo = updateAllUsersInfo(UpdateFollowUpAction::None); !exitInfo) {
        LOG_WARN(_logger, "Error in updateAllUsersInfo: " << exitInfo);
        addError(Error(ERR_ID, exitInfo.code(), exitInfo.cause()));
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
#if defined(KD_MACOS)
            Utility::restartFinderExtension();
#endif
        }
    }
}
} // namespace KDC
