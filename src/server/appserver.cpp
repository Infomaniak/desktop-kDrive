/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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
#include "libcommon/asserts.h"
#include "common/utility.h"
#include "updater/kdcupdater.h"
#include "libcommonserver/vfs.h"
#include "migration/migrationparams.h"
#include "socketapi.h"
#include "logarchiver.h"
#include "keychainmanager/keychainmanager.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommon/info/driveinfo.h"
#include "libcommon/info/driveavailableinfo.h"
#include "libcommon/info/userinfo.h"
#include "libcommon/info/exclusiontemplateinfo.h"
#include "libcommon/log/sentry/sentryhandler.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/network/proxy.h"
#include "libsyncengine/requests/serverrequests.h"
#include "libsyncengine/requests/parameterscache.h"
#include "libsyncengine/requests/exclusiontemplatecache.h"
#include "libsyncengine/jobs/jobmanager.h"
#include "libsyncengine/jobs/network/API_v2/upload_session/uploadsessioncanceljob.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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
#define START_SYNCPALS_TRIALS 12
#define START_SYNCPALS_RETRY_INTERVAL 5000 // ms
#define START_SYNCPALS_TIME_GAP 5000 // ms

namespace KDC {

std::unordered_map<int, std::shared_ptr<SyncPal>> AppServer::_syncPalMap;
std::unordered_map<int, std::shared_ptr<KDC::Vfs>> AppServer::_vfsMap;
std::vector<AppServer::Notification> AppServer::_notifications;
std::chrono::time_point<std::chrono::steady_clock> AppServer::_lastSyncPalStart = std::chrono::steady_clock::now();

namespace {

static const char optionsC[] =
        "Options:\n"
        "  -h --help            : show this help screen.\n"
        "  -v --version         : show the application version.\n"
        "  --settings           : show the Settings window (if the application is running).\n"
        "  --synthesis          : show the Synthesis window (if the application is running).\n";
}

static const QString showSynthesisMsg = "showSynthesis";
static const QString showSettingsMsg = "showSettings";
static const QString crashMsg = SharedTools::QtSingleApplication::tr("kDrive application will close due to a fatal error.");

// Helpers for displaying messages. Note that there is no console on Windows.
#ifdef Q_OS_WIN
static void displayHelpText(const QString &t) // No console on Windows.
{
    QString spaces(80, ' '); // Add a line of non-wrapped space to make the messagebox wide enough.
    QString text = QLatin1String("<qt><pre style='white-space:pre-wrap'>") + t.toHtmlEscaped() + QLatin1String("</pre><pre>") +
                   spaces + QLatin1String("</pre></qt>");
    QMessageBox::information(0, Theme::instance()->appNameGUI(), text);
}

#else

static void displayHelpText(const QString &t) {
    std::cout << qUtf8Printable(t);
}
#endif

AppServer::AppServer(int &argc, char **argv) :
    SharedTools::QtSingleApplication(Theme::instance()->appName(), argc, argv), _theme(Theme::instance()) {
    _startedAt.start();
    setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));
    setApplicationName(_theme->appName());
    setWindowIcon(_theme->applicationIcon());
    setApplicationVersion(_theme->version());

    // Setup logging with default parameters
    initLogging();

    parseOptions(arguments());
    if (_helpAsked || _versionAsked || _clearSyncNodesAsked || _clearKeychainKeysAsked || isRunning()) {
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
    bool alreadyExist = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExist);
    if (parmsDbPath.empty()) {
        LOG_WARN(_logger, "Error in Db::makeDbName");
        throw std::runtime_error("Unable to create parameters database.");
        return;
    }

    bool newDbExists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(parmsDbPath, newDbExists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(parmsDbPath, ioError).c_str());
        throw std::runtime_error("Unable to check if parmsdb exists.");
        return;
    }

    std::filesystem::path pre334ConfigFilePath =
            std::filesystem::path(QStr2SyncName(MigrationParams::configDir().filePath(MigrationParams::configFileName())));
    bool oldConfigExists;
    if (!IoHelper::checkIfPathExists(pre334ConfigFilePath, oldConfigExists, ioError)) {
        LOGW_WARN(_logger,
                  L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(pre334ConfigFilePath, ioError).c_str());
        throw std::runtime_error("Unable to check if pre 3.3.4 config exists.");
        return;
    }

    LOGW_INFO(_logger, L"New DB exists : " << Path2WStr(parmsDbPath).c_str() << L" => " << newDbExists);
    LOGW_INFO(_logger, L"Old config exists : " << Path2WStr(pre334ConfigFilePath).c_str() << L" => " << oldConfigExists);

    try {
        ParmsDb::instance(parmsDbPath, _theme->version().toStdString());
    } catch (const std::exception &) {
        throw std::runtime_error("Unable to open parameters database.");
        return;
    }

    // Clear old server errors
    clearErrors(0);

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

#if defined(__unix__) && !defined(__APPLE__)
    // For access to keyring in order to promt authentication popup
    KeyChainManager::instance()->writeDummyTest();
    KeyChainManager::instance()->clearDummyTest();
#endif

    // Init ParametersCache instance
    try {
        ParametersCache::instance();
    } catch (std::exception const &) {
        LOG_WARN(_logger, "Error in ParametersCache::instance");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
        throw std::runtime_error("Unable to initialize parameters cache.");
        return;
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

    // Init ExclusionTemplateCache instance
    try {
        ExclusionTemplateCache::instance();
    } catch (std::exception const &) {
        LOG_WARN(_logger, "Error in ExclusionTemplateCache::instance");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
        throw std::runtime_error("Unable to initialize exclusion template cache.");
        return;
    }

#ifdef Q_OS_WIN
    // Update shortcuts
    _navigationPaneHelper = std::unique_ptr<NavigationPaneHelper>(new NavigationPaneHelper(_vfsMap));
    _navigationPaneHelper->setShowInExplorerNavigationPane(false);
    if (ParametersCache::instance()->parameters().showShortcuts()) {
        _navigationPaneHelper->setShowInExplorerNavigationPane(true);
    }
#endif

    // Setup proxy
    setupProxy();

    // Setup auto start
#ifdef NDEBUG
    if (ParametersCache::instance()->parameters().autoStart() && !OldUtility::hasLaunchOnStartup(_theme->appName(), _logger)) {
        OldUtility::setLaunchOnStartup(_theme->appName(), _theme->appClientName(), true, _logger);
    }
#endif

#ifdef PLUGINDIR
    // Setup extra plugin search path
    QString extraPluginPath = QStringLiteral(PLUGINDIR);
    if (!extraPluginPath.isEmpty()) {
        if (QDir::isRelativePath(extraPluginPath))
            extraPluginPath = QDir(QApplication::applicationDirPath()).filePath(extraPluginPath);
        LOGW_INFO(_logger, L"Adding extra plugin search path:" << Path2WStr(QStr2Path(extraPluginPath)).c_str());
        addLibraryPath(extraPluginPath);
    }
#endif

    // Check vfs plugins
    QString error;
#ifdef Q_OS_WIN
    if (KDC::isVfsPluginAvailable(VirtualFileMode::Win, error)) LOG_INFO(_logger, "VFS windows plugin is available");
#elif defined(Q_OS_MAC)
    if (KDC::isVfsPluginAvailable(VirtualFileMode::Mac, error)) LOG_INFO(_logger, "VFS mac plugin is available");
#endif
    if (KDC::isVfsPluginAvailable(VirtualFileMode::Suffix, error)) LOG_INFO(_logger, "VFS suffix plugin is available");

#ifdef Q_OS_MACOS
    // Init Updater
    const QuitCallback quitCallback = std::bind_front(&AppServer::sendQuit, this);
    UpdaterServer::instance()->setQuitCallback(quitCallback);
#endif

    // Update checks
    UpdaterScheduler *updaterScheduler = new UpdaterScheduler(this);
    connect(updaterScheduler, &UpdaterScheduler::requestRestart, this, &AppServer::onScheduleAppRestart);

#ifdef Q_OS_WIN
    connect(updaterScheduler, &UpdaterScheduler::updaterAnnouncement, this, &AppServer::onShowWindowsUpdateErrorDialog);
#endif

    // Init socket api
    _socketApi.reset(new SocketApi(_syncPalMap, _vfsMap));
    _socketApi->setAddErrorCallback(&addError);
    _socketApi->setGetThumbnailCallback(&ServerRequests::getThumbnail);
    _socketApi->setGetPublicLinkUrlCallback(&ServerRequests::getPublicLinkUrl);

    // Start CommServer
    CommServer::instance();
    connect(CommServer::instance().get(), &CommServer::requestReceived, this, &AppServer::onRequestReceived);
    connect(CommServer::instance().get(), &CommServer::restartClient, this, &AppServer::onRestartClientReceived);

    // Update users/accounts/drives info
    ExitCode exitCode = updateAllUsersInfo();
    if (exitCode == ExitCode::InvalidToken) {
        // The user will be asked to enter its credentials later
    } else if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in updateAllUsersInfo : " << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
        if (exitCode != ExitCode::NetworkError && exitCode != ExitCode::UpdateRequired) {
            throw std::runtime_error("Failed to load user data.");
            return;
        }
    }

    // Set sentry user
    updateSentryUser();

    // Check last crash to avoid crash loop
    bool shouldQuit = false;
    handleCrashRecovery(shouldQuit);
    if (shouldQuit) {
        QTimer::singleShot(0, this, &AppServer::quit);
        return;
    }

    // Start syncs
    QTimer::singleShot(0, [=]() { startSyncPals(); });

    // Check if a log Upload has been interrupted
    AppStateValue appStateValue = LogUploadState::None;
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadState, appStateValue, found) || !found) {
        LOG_ERROR(_logger, "Error in ParmsDb::selectAppState");
    }

    if (auto logUploadState = std::get<LogUploadState>(appStateValue);
        logUploadState == LogUploadState::Archiving || logUploadState == LogUploadState::Uploading) {
        LOG_ERROR(_logger, "App was closed during log upload, resetting upload status.");
        if (bool found = false;
            !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::Failed, found) || !found) {
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        }
    } else if (logUploadState ==
               LogUploadState::CancelRequested) { // If interrupted while cancelling, consider it has been cancelled
        if (bool found = false;
            !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::Canceled, found) || !found) {
            LOG_ERROR(_logger, "Error in ParmsDb::updateAppState");
        }
    }

    appStateValue = "";
    if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadToken, appStateValue, found) || !found) {
        LOG_ERROR(_logger, "Error in ParmsDb::selectAppState");
    }

    if (const auto logUploadToken = std::get<std::string>(appStateValue); !logUploadToken.empty()) {
        UploadSessionCancelJob cancelJob(UploadSessionType::Log, logUploadToken);
        if (const ExitCode exitCode = cancelJob.runSynchronously(); exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in UploadSessionCancelJob::runSynchronously : " << exitCode);
        } else {
            LOG_INFO(_logger, "Previous Log upload api call cancelled");
            if (bool found = false;
                !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadToken, std::string(), found) || !found) {
                LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
            }
        }
    }

    // Start client
    if (!startClient()) {
        LOG_ERROR(_logger, "Error in startClient");
        throw std::runtime_error("Failed to start kDrive client.");
        return;
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

AppServer::~AppServer() {
    LOG_DEBUG(_logger, "~AppServer");
}

void AppServer::onCleanup() {
    LOG_DEBUG(_logger, "AppServer::onCleanup()");
    JobManager::stop();
    // Stop SyncPals
    for (const auto &syncPalMapElt: _syncPalMap) {
        stopSyncPal(syncPalMapElt.first, false, true);
        LOG_DEBUG(_logger, "SyncPal with syncDbId=" << syncPalMapElt.first << " and use_count="
                                                    << _syncPalMap[syncPalMapElt.first].use_count() << " stoped");
    }
    LOG_DEBUG(_logger, "Syncpal(s) stopped");
    // Stop Vfs
    for (const auto &vfsMapElt: _vfsMap) {
        stopVfs(vfsMapElt.first, false);
        LOG_DEBUG(_logger, "Vfs with syncDbId=" << vfsMapElt.first << " and use_count=" << _vfsMap[vfsMapElt.first].use_count()
                                                << " stoped");
    }
    LOG_DEBUG(_logger, "Vfs(s) stopped");
    // Stop JobManager
    JobManager::clear();
    LOG_DEBUG(_logger, "JobManager::clear() done");

    _syncPalMap.clear();
    _vfsMap.clear();

    ParmsDb::instance()->close();
    LOG_DEBUG(_logger, "ParmsDb closed");
}

// This task can be long and block the GUI
void AppServer::stopSyncTask(int syncDbId) {
    // Stop sync and remove it from syncPalMap
    ExitCode exitCode = stopSyncPal(syncDbId, false, true, true);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in stopSyncPal : " << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }

    // Stop Vfs
    exitCode = stopVfs(syncDbId, true);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in stopVfs : " << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }

    ASSERT(_syncPalMap[syncDbId].use_count() == 1)
    _syncPalMap.erase(syncDbId);

    ASSERT(_vfsMap[syncDbId].use_count() <= 1) // `use_count` can be zero when the local drive has been removed.
    _vfsMap.erase(syncDbId);
}

void AppServer::stopAllSyncsTask(const std::vector<int> &syncDbIdList) {
    for (int syncDbId: syncDbIdList) {
        stopSyncTask(syncDbId);
    }
}

void AppServer::deleteAccountIfNeeded(int accountDbId) {
    std::vector<Drive> driveList;
    if (!ParmsDb::instance()->selectAllDrives(accountDbId, driveList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
        addError(Error(errId(), ExitCode::DbError, ExitCause::Unknown));
    } else if (driveList.empty()) {
        const ExitCode exitCode = ServerRequests::deleteAccount(accountDbId);
        if (exitCode == ExitCode::Ok) {
            sendAccountRemoved(accountDbId);
        } else {
            LOG_WARN(_logger, "Error in ServerRequests::deleteAccount: " << exitCode);
            addError(Error(errId(), exitCode, ExitCause::Unknown));
        }
    }
}

void AppServer::deleteDrive(int driveDbId, int accountDbId) {
    const ExitCode exitCode = ServerRequests::deleteDrive(driveDbId);
    if (exitCode == ExitCode::Ok) {
        sendDriveRemoved(driveDbId);
    } else {
        LOG_WARN(_logger, "Error in Requests::deleteDrive : " << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
        sendDriveDeletionFailed(driveDbId);
    }

    deleteAccountIfNeeded(accountDbId);
}

void AppServer::logExtendedLogActivationMessage(bool isExtendedLogEnabled) noexcept {
    const std::string activationStatus = isExtendedLogEnabled ? "enabled" : "disabled";
    const std::string msg = "Extended logging is " + activationStatus + ".";

    LOG_INFO(_logger, msg.c_str());
}

void AppServer::updateSentryUser() const {
    User user;
    bool found = false;
    ParmsDb::instance()->selectLastConnectedUser(user, found);
    std::string userId = "No user in db";
    std::string userName = "No user in db";
    std::string userEmail = "No user in db";
    if (found) {
        userId = std::to_string(user.dbId());
        userName = user.name();
        userEmail = user.email();
    }
    SentryHandler::instance()->setAuthenticatedUser(SentryUser(userEmail, userName, userId));
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
        bool found = false;
        if (!KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastClientSelfRestartDate, 0, found) || !found) {
            addError(Error(errId(), ExitCode::DbError, ExitCause::DbEntryNotFound));
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        }

        quit = true;
    } else {
        // Set client restart date in DB
        const long timestamp =
                std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();
        const std::string timestampStr = std::to_string(timestamp);
        bool found = false;
        if (!KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastClientSelfRestartDate, timestampStr, found) || !found) {
            addError(Error(errId(), ExitCode::DbError, ExitCause::DbEntryNotFound));
            LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
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
                LOG_WARN(_logger, "Error in Requests::requestToken : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getUserDbIdList : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getUserInfoList : " << exitCode);
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
                    LOG_WARN(_logger, "Error in Requests::deleteUser : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getErrorInfoList : " << exitCode);
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
                    LOG_WARN(_logger, "Error in Requests::getConflictErrorInfoList : " << exitCode);
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
                LOG_WARN(_logger, "Error in AppServer::clearErrors : " << exitCode);
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
                LOG_WARN(_logger, "Error in AppServer::clearErrors : " << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::ERROR_DELETE_INVALIDTOKEN: {
            ExitCode exitCode = ServerRequests::deleteInvalidTokenErrors();
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::userLoggedIn : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getUserIdFromUserDbId : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getAccountInfoList : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getDriveInfoList : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getDriveInfo : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getDriveIdFromDriveDbId : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::getDriveIdFromSyncDbId : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::updateDrive : " << exitCode);
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
            int accountDbId = -1;
            std::vector<int> syncDbIdList;
            for (const auto &syncPalMapElt: _syncPalMap) {
                if (syncPalMapElt.second->driveDbId() == driveDbId) {
                    syncDbIdList.push_back(syncPalMapElt.first);
                    accountDbId = syncPalMapElt.second->accountDbId();
                }
            }

            // Stop syncs for this drive and remove them from syncPalMap
            QTimer::singleShot(100, [this, driveDbId, accountDbId, syncDbIdList]() {
                AppServer::stopAllSyncsTask(syncDbIdList);
                AppServer::deleteDrive(driveDbId, accountDbId);
            });

            Utility::restartFinderExtension();

            break;
        }
        case RequestNum::SYNC_INFOLIST: {
            QList<SyncInfo> list;
            ExitCode exitCode;
            exitCode = ServerRequests::getSyncInfoList(list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSyncInfoList : " << exitCode);
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

            ExitCode exitCode = checkIfSyncIsValid(sync);
            ExitCause exitCause = ExitCause::Unknown;
            if (exitCode != ExitCode::Ok) {
                addError(Error(sync.dbId(), errId(), exitCode, exitCause));
                resultStream << toInt(exitCode);
                break;
            }

            exitCode = tryCreateAndStartVfs(sync);
            const bool resumedByUser = exitCode == ExitCode::Ok;

            exitCode = initSyncPal(sync, std::unordered_set<NodeId>(), std::unordered_set<NodeId>(), std::unordered_set<NodeId>(),
                                   true, resumedByUser, false);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " - exitCode=" << exitCode);
                addError(Error(errId(), exitCode, exitCause));
                resultStream << toInt(exitCode);
                break;
            }

            Utility::restartFinderExtension();

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::SYNC_STOP: {
            int syncDbId;
            QDataStream paramsStream(params);
            paramsStream >> syncDbId;

            resultStream << ExitCode::Ok;

            QTimer::singleShot(100, [=]() {
                // Stop SyncPal
                ExitCode exitCode = stopSyncPal(syncDbId, true);
                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger, "Error in stopSyncPal : " << exitCode);
                    addError(Error(errId(), exitCode, ExitCause::Unknown));
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
        case RequestNum::SYNC_ADD: {
            int userDbId = 0;
            int accountId = 0;
            int driveId = 0;
            QString localFolderPath;
            QString serverFolderPath;
            QString serverFolderNodeId;
            bool liteSync = false;
            QSet<QString> blackList;
            QSet<QString> whiteList;
            QDataStream paramsStream(params);
            paramsStream >> userDbId;
            paramsStream >> accountId;
            paramsStream >> driveId;
            paramsStream >> localFolderPath;
            paramsStream >> serverFolderPath;
            paramsStream >> serverFolderNodeId;
            paramsStream >> liteSync;
            paramsStream >> blackList;
            paramsStream >> whiteList;

            // Add sync in DB
            bool showInNavigationPane = false;
#ifdef Q_OS_WIN
            showInNavigationPane = _navigationPaneHelper->showInExplorerNavigationPane();
#endif
            AccountInfo accountInfo;
            DriveInfo driveInfo;
            SyncInfo syncInfo;
            ExitCode exitCode =
                    ServerRequests::addSync(userDbId, accountId, driveId, localFolderPath, serverFolderPath, serverFolderNodeId,
                                            liteSync, showInNavigationPane, accountInfo, driveInfo, syncInfo);
            if (exitCode != ExitCode::Ok) {
                LOGW_WARN(_logger, L"Error in Requests::addSync - userDbId="
                                           << userDbId << L" accountId=" << accountId << L" driveId=" << driveId
                                           << L" localFolderPath=" << QStr2WStr(localFolderPath).c_str() << L" serverFolderPath="
                                           << QStr2WStr(serverFolderPath).c_str() << L" serverFolderNodeId="
                                           << serverFolderNodeId.toStdWString().c_str() << L" liteSync=" << liteSync
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
            sendSyncAdded(syncInfo);

            resultStream << toInt(exitCode);
            if (exitCode == ExitCode::Ok) {
                resultStream << syncInfo.dbId();
            }

            QTimer::singleShot(100, this, [=]() {
                Sync sync;
                ServerRequests::syncInfoToSync(syncInfo, sync);

                ExitCode exitCode = checkIfSyncIsValid(sync);
                ExitCause exitCause = ExitCause::Unknown;
                if (exitCode != ExitCode::Ok) {
                    addError(Error(sync.dbId(), errId(), exitCode, exitCause));
                    return;
                }

                tryCreateAndStartVfs(sync);

                // Create and start SyncPal
                exitCode = initSyncPal(sync, blackList, QSet<QString>(), whiteList, true, false, true);
                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << syncInfo.dbId() << " - exitCode=" << exitCode);
                    addError(Error(errId(), exitCode, exitCause));

                    // Stop sync and remove it from syncPalMap
                    ExitCode exitCode = stopSyncPal(syncInfo.dbId(), false, true, true);
                    if (exitCode != ExitCode::Ok) {
                        // Do nothing
                    }

                    // Stop Vfs
                    exitCode = stopVfs(syncInfo.dbId(), true);
                    if (exitCode != ExitCode::Ok) {
                        // Do nothing
                    }

                    ASSERT(_syncPalMap[syncInfo.dbId()].use_count() == 1)
                    _syncPalMap.erase(syncInfo.dbId());

                    ASSERT(_vfsMap[syncInfo.dbId()].use_count() == 1)
                    _vfsMap.erase(syncInfo.dbId());

                    // Delete sync from DB
                    exitCode = ServerRequests::deleteSync(syncInfo.dbId());
                    if (exitCode != ExitCode::Ok) {
                        LOG_WARN(_logger, "Error in Requests::deleteSync : " << exitCode);
                        addError(Error(errId(), exitCode, ExitCause::Unknown));
                    }

                    sendSyncRemoved(syncInfo.dbId());
                }

                Utility::restartFinderExtension();
            });
            break;
        }
        case RequestNum::SYNC_ADD2: {
            int driveDbId = 0;
            QString localFolderPath;
            QString serverFolderPath;
            QString serverFolderNodeId;
            bool liteSync = false;
            QSet<QString> blackList;
            QSet<QString> whiteList;
            ArgsWriter(params).write(driveDbId, localFolderPath, serverFolderPath, serverFolderNodeId, liteSync, blackList,
                                     whiteList);

            // Add sync in DB
            bool showInNavigationPane = false;
#ifdef Q_OS_WIN
            showInNavigationPane = _navigationPaneHelper->showInExplorerNavigationPane();
#endif
            SyncInfo syncInfo;
            ExitCode exitCode = ServerRequests::addSync(driveDbId, localFolderPath, serverFolderPath, serverFolderNodeId,
                                                        liteSync, showInNavigationPane, syncInfo);
            if (exitCode != ExitCode::Ok) {
                LOGW_WARN(_logger, L"Error in Requests::addSync for driveDbId="
                                           << driveDbId << L" localFolderPath=" << Path2WStr(QStr2Path(localFolderPath)).c_str()
                                           << L" serverFolderPath=" << Path2WStr(QStr2Path(serverFolderPath)).c_str()
                                           << L" liteSync=" << liteSync << L" showInNavigationPane=" << showInNavigationPane);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
                resultStream << toInt(exitCode);
                break;
            }

            sendSyncAdded(syncInfo);

            resultStream << toInt(exitCode);
            if (exitCode == ExitCode::Ok) {
                resultStream << syncInfo.dbId();
            }

            QTimer::singleShot(100, this, [=]() {
                Sync sync;
                ServerRequests::syncInfoToSync(syncInfo, sync);

                // Check if sync is valid
                ExitCode exitCode = checkIfSyncIsValid(sync);
                ExitCause exitCause = ExitCause::Unknown;
                if (exitCode != ExitCode::Ok) {
                    addError(Error(sync.dbId(), errId(), exitCode, exitCause));
                    return;
                }

                tryCreateAndStartVfs(sync);

                // Create and start SyncPal
                exitCode = initSyncPal(sync, blackList, QSet<QString>(), whiteList, true, false, true);
                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " - exitCode=" << exitCode);
                    addError(Error(errId(), exitCode, exitCause));
                }

                Utility::restartFinderExtension();
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

            ExitCause exitCause;
            ExitCode exitCode = startSyncs(user, exitCause);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in startSyncs for userDbId=" << user.dbId());
            }

            resultStream << toInt(exitCode);
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
                const ExitCode exitCode = ServerRequests::deleteSync(syncDbId);

                if (exitCode == ExitCode::Ok) {
                    // Let the client remove the sync-related GUI elements.
                    sendSyncRemoved(syncDbId);
                } else {
                    LOG_WARN(_logger, "Error in Requests::deleteSync : " << exitCode);
                    addError(Error(errId(), exitCode, ExitCause::Unknown));
                    // Let the client unlock the sync-related GUI elements.
                    sendSyncDeletionFailed(syncDbId);
                }

                Utility::restartFinderExtension();
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

            std::unordered_set<NodeId> nodeIdSet;
            ExitCode exitCode = _syncPalMap[syncDbId]->syncIdSet(type, nodeIdSet);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::getSyncIdSet : " << exitCode);
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

            std::unordered_set<NodeId> nodeIdSet2;
            for (const QString &nodeId: nodeIdSet) {
                nodeIdSet2.insert(nodeId.toStdString());
            }

            ExitCode exitCode = _syncPalMap[syncDbId]->setSyncIdSet(type, nodeIdSet2);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet : " << exitCode);
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
                resultStream << QString();
                break;
            }

            QString path;
            ExitCode exitCode = ServerRequests::getPathByNodeId(_syncPalMap[syncDbId]->userDbId(),
                                                                _syncPalMap[syncDbId]->driveId(), nodeId, path);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in AppServer::getPathByNodeId : " << exitCode);
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
            ExitCode exitCode = ServerRequests::getSubFolders(userDbId, driveId, nodeId, subfoldersList, withPath);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSubFolders");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
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
            ExitCode exitCode = ServerRequests::getSubFolders(driveDbId, nodeId, subfoldersList, withPath);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getSubFolders");
                addError(Error(errId(), exitCode, ExitCause::Unknown));
            }

            resultStream << toInt(exitCode);
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
                std::chrono::time_point<std::chrono::system_clock> pauseTime;
                if (syncPalMapElt.second->driveDbId() == driveDbId && !syncPalMapElt.second->isPaused(pauseTime)) {
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
                                                                                        << parentNodeId.toStdString().c_str());
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
                if (syncPalMapElt.second->driveDbId() == driveDbId) {
                    // Get blacklist
                    std::unordered_set<NodeId> nodeIdSet;
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
            resultStream << ExclusionTemplateCache::instance()->isExcludedByTemplate(name.toStdString(), isWarning);
            break;
        }
        case RequestNum::EXCLTEMPL_GETLIST: {
            bool def;
            QDataStream paramsStream(params);
            paramsStream >> def;

            QList<ExclusionTemplateInfo> list;
            ExitCode exitCode = ServerRequests::getExclusionTemplateList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getExclusionTemplateList : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::setExclusionTemplateList : " << exitCode);
                addError(Error(errId(), exitCode, ExitCause::Unknown));
                resultStream << toInt(exitCode);
                break;
            }

            resultStream << toInt(exitCode);
            break;
        }
        case RequestNum::EXCLTEMPL_PROPAGATE_CHANGE: {
            resultStream << ExitCode::Ok;

            QTimer::singleShot(100, [=]() {
                for (auto &syncPalMapElt: _syncPalMap) {
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
#ifdef Q_OS_MAC
        case RequestNum::EXCLAPP_GETLIST: {
            bool def;
            QDataStream paramsStream(params);
            paramsStream >> def;

            QList<ExclusionAppInfo> list;
            ExitCode exitCode = ServerRequests::getExclusionAppList(def, list);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in Requests::getExclusionAppList : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::setExclusionAppList : " << exitCode);
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
        case RequestNum::GET_FETCHING_APP_LIST: {
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
                    LOG_DEBUG(_logger, "Proxy pwd not found for keychainKey=" << parameters.proxyConfig().token().c_str());
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
#ifdef Q_OS_WIN
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
            bool enabled = OldUtility::hasSystemLaunchOnStartup(Theme::instance()->appName(), _logger);

            resultStream << ExitCode::Ok;
            resultStream << enabled;
            break;
        }
        case RequestNum::UTILITY_HASLAUNCHONSTARTUP: {
            bool enabled = OldUtility::hasLaunchOnStartup(Theme::instance()->appName(), _logger);

            resultStream << ExitCode::Ok;
            resultStream << enabled;
            break;
        }
        case RequestNum::UTILITY_SETLAUNCHONSTARTUP: {
            bool enabled;
            QDataStream paramsStream(params);
            paramsStream >> enabled;

            Theme *theme = Theme::instance();
            OldUtility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), enabled, _logger);

            resultStream << ExitCode::Ok;
            break;
        }
        case RequestNum::UTILITY_SET_APPSTATE: {
            AppStateKey key = AppStateKey::Unknown;
            QString value;
            QDataStream paramsStream(params);
            paramsStream >> key;
            paramsStream >> value;

            bool found = true;
            if (!ParmsDb::instance()->updateAppState(key, value.toStdString(), found) || !found) {
                LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
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
            if (bool found = false; !ParmsDb::instance()->selectAppState(key, appStateValue, found) || !found) {
                LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
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
            bool res = LogArchiver::getLogDirEstimatedSize(logSize, ioError);
            if (ioError != IoError::Success) {
                LOG_WARN(_logger,
                         "Error in LogArchiver::getLogDirEstimatedSize: " << IoHelper::ioError2StdString(ioError).c_str());

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

            std::thread uploadLogThread([this, includeArchivedLogs]() { uploadLog(includeArchivedLogs); });
            uploadLogThread.detach();
            break;
        }
        case RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT: {
            resultStream << ExitCode::Ok; // Return immediately, progress and error will be report via addError and signal
            QTimer::singleShot(100, [this]() { cancelLogUpload(); });
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

            if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
                LOG_WARN(_logger, "Vfs not found in vfsMap for syncDbId=" << syncDbId);
                resultStream << ExitCode::DataError;
                break;
            }

            if (!_vfsMap[syncDbId]->setPinState(QString(), state)) {
                LOG_WARN(_logger, "Error in Vfs::setPinState for root directory");
                resultStream << ExitCode::SystemError;
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
        case RequestNum::UPDATER_VERSION: {
            QString version = UpdaterServer::instance()->version();

            resultStream << version;
            break;
        }
        case RequestNum::UPDATER_ISKDCUPDATER: {
            bool ret = UpdaterServer::instance()->isKDCUpdater();

            resultStream << ret;
            break;
        }
        case RequestNum::UPDATER_ISSPARKLEUPDATER: {
            bool ret = UpdaterServer::instance()->isSparkleUpdater();

            resultStream << ret;
            break;
        }
        case RequestNum::UPDATER_STATUSSTRING: {
            QString status = UpdaterServer::instance()->statusString();

            resultStream << status;
            break;
        }
        case RequestNum::UPDATER_STATUS: {
            UpdateState status = UpdaterServer::instance()->updateState();
            resultStream << status;
            break;
        }
        case RequestNum::UPDATER_DOWNLOADCOMPLETED: {
            bool ret = UpdaterServer::instance()->downloadCompleted();

            resultStream << ret;
            break;
        }
        case RequestNum::UPDATER_UPDATEFOUND: {
            bool ret = UpdaterServer::instance()->updateFound();

            resultStream << ret;
            break;
        }
        case RequestNum::UPDATER_STARTINSTALLER: {
            UpdaterServer::instance()->startInstaller();
            break;
        }
        case RequestNum::UPDATER_UPDATE_DIALOG_RESULT: {
            bool skip;
            QDataStream paramsStream(params);
            paramsStream >> skip;

            NSISUpdater *updater = qobject_cast<NSISUpdater *>(UpdaterServer::instance());
            if (skip) {
                updater->wipeUpdateData();
                updater->slotSetSeenVersion();
            } else {
                updater->slotStartInstaller();
            }
            break;
        }
        case RequestNum::RECONSIDER_SKIPPED_UPDATE: {
            NSISUpdater *updater = qobject_cast<NSISUpdater *>(UpdaterServer::instance());
            updater->slotUnsetSeenVersion();
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
        default: {
            LOG_DEBUG(_logger, "Request not implemented!");
            return;
            break;
        }
    }

    CommServer::instance()->sendReply(id, results);
}

void AppServer::startSyncPals() {
    static int trials = 0;

    if (trials < START_SYNCPALS_TRIALS) {
        trials++;
        LOG_DEBUG(_logger, "Start SyncPals - trials = " << trials);
        ExitCause exitCause = ExitCause::Unknown;
        ExitCode exitCode = startSyncs(exitCause);
        if (exitCode != ExitCode::Ok) {
            if (exitCode == ExitCode::SystemError && exitCause == ExitCause::Unknown) {
                QTimer::singleShot(START_SYNCPALS_RETRY_INTERVAL, this, [=]() { startSyncPals(); });
            }
        }
    }
}

ExitCode AppServer::clearErrors(int syncDbId, bool autoResolved /*= false*/) {
    ExitCode exitCode;
    if (syncDbId == 0) {
        exitCode = ServerRequests::deleteErrorsServer();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in ServerRequests::deleteErrorsServer : " << exitCode);
        }
    } else {
        exitCode = ServerRequests::deleteErrorsForSync(syncDbId, autoResolved);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in ServerRequests::deleteErrorsForSync : " << exitCode);
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

    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, status, found) || !found) {
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState with key=" << static_cast<int>(AppStateKey::LogUploadState));
        // Don't fail because it is not a critical error, especially in this context where we are trying to send logs
    }

    if (bool found = false;
        !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadPercent, std::to_string(percent), found) || !found) {
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState with key=" << static_cast<int>(AppStateKey::LogUploadPercent));
        // Don't fail because it is not a critical error, especially in this context where we are trying to send logs
    }
}

void AppServer::cancelLogUpload() {
    ExitCause exitCause = ExitCause::Unknown;
    ExitCode exitCode = ServerRequests::cancelLogToSupport(exitCause);
    if (exitCause == ExitCause::OperationCanceled) {
        LOG_WARN(_logger, "Operation already canceled");
        sendLogUploadStatusUpdated(LogUploadState::Canceled, 0);
        return;
    }

    if (exitCode == ExitCode::InvalidOperation) {
        LOG_WARN(_logger, "Cannot cancel the log upload operation (not started or already finished)");
        AppStateValue logUploadState = LogUploadState::None;
        if (bool found = false;
            !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadState, logUploadState, found) || !found) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        }

        AppStateValue logUploadPercent = int();
        if (bool found = false;
            !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadPercent, logUploadPercent, found) || !found) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        }
        sendLogUploadStatusUpdated(std::get<LogUploadState>(logUploadState), std::get<int>(logUploadPercent));
        return;
    }

    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::cancelLogUploadToSupport : " << exitCode << " | " << exitCause);
        addError(Error(errId(), ExitCode::LogUploadFailed, exitCause));
        sendLogUploadStatusUpdated(LogUploadState::Failed, 0); // Considered as a failure, in case the operation was not
                                                               // canceled, the gui will receive updated status quickly.
        return;
    }
    sendLogUploadStatusUpdated(LogUploadState::CancelRequested, 0);
}

void AppServer::uploadLog(bool includeArchivedLogs) {
    if (bool found = false; !ParmsDb::instance()->updateAppState(AppStateKey::LogUploadState, LogUploadState::None, found) ||
                            !found) { // Reset status
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
    }
    sendLogUploadStatusUpdated(LogUploadState::Archiving, 0); // Send progress to the client

    /* See AppStateKey::LogUploadState for status values
     * The return value of progressFunc is true if the upload should continue, false if the user canceled the upload
     */
    LogUploadState previousStatus = LogUploadState::None;
    int previousProgress = 0;
    std::function<bool(LogUploadState, int)> progressFunc = [this, &previousStatus, &previousProgress](LogUploadState status,
                                                                                                       int progress) {
        AppStateValue appStateValue = LogUploadState::None;
        if (bool found = false; !ParmsDb::instance()->selectAppState(AppStateKey::LogUploadState, appStateValue, found) ||
                                !found) { // Check if the user canceled the upload
            LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        }
        LogUploadState logUploadState = std::get<LogUploadState>(appStateValue);

        bool canceled = logUploadState == LogUploadState::Canceled || logUploadState == LogUploadState::CancelRequested;
        if (!canceled && (status != previousStatus || progress != previousProgress)) {
            sendLogUploadStatusUpdated(status, progress); // Send progress to the client
            LOG_DEBUG(_logger, "Log transfert progress : " << static_cast<int>(status) << " | " << progress << " %");
        }
        previousProgress = progress;
        previousStatus = status;
        return !canceled;
    };

    ExitCause exitCause = ExitCause::Unknown;
    ExitCode exitCode = ServerRequests::sendLogToSupport(includeArchivedLogs, progressFunc, exitCause);

    if (exitCause == ExitCause::OperationCanceled) {
        LOG_DEBUG(_logger, "Log transfert canceled");
        sendLogUploadStatusUpdated(LogUploadState::Canceled, 0);
        return;
    } else if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::sendLogToSupport : " << exitCode << " | " << exitCause);
        addError(Error(errId(), ExitCode::LogUploadFailed, exitCause));
    }
    sendLogUploadStatusUpdated(exitCode == ExitCode::Ok ? LogUploadState::Success : LogUploadState::Failed, 0);
}

ExitCode AppServer::checkIfSyncIsValid(const Sync &sync) {
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
        return ExitCode::DbError;
    }

    // Check for nested syncs
    for (const auto &sync_: syncList) {
        if (sync_.dbId() == sync.dbId()) {
            continue;
        }
        if (CommonUtility::isSubDir(sync.localPath(), sync_.localPath()) ||
            CommonUtility::isSubDir(sync_.localPath(), sync.localPath())) {
            LOGW_WARN(_logger, L"Nested syncs - (1) dbId=" << sync.dbId() << L", "
                                                           << Utility::formatSyncPath(sync.localPath()).c_str() << L"; (2) dbId="
                                                           << sync_.dbId() << L", "
                                                           << Utility::formatSyncPath(sync_.localPath()).c_str());
            return ExitCode::InvalidSync;
        }
    }

    return ExitCode::Ok;
}

void AppServer::onScheduleAppRestart() {
    LOG_INFO(_logger, "Application restart requested!");
    _appRestartRequired = true;
}

void AppServer::onShowWindowsUpdateErrorDialog() {
    static bool alreadyAsked = false; // Ask only once
    if (!alreadyAsked) {
        NSISUpdater *updater = qobject_cast<NSISUpdater *>(UpdaterServer::instance());
        if (updater) {
            if (updater->autoUpdateAttempted()) { // Try auto update first
                alreadyAsked = true;

                // Notify client
                int id = 0;

                QString targetVersion;
                QString targetVersionString;
                QString clientVersion;
                updater->getVersions(targetVersion, targetVersionString, clientVersion);

                QByteArray params;
                QDataStream paramsStream(&params, QIODevice::WriteOnly);
                paramsStream << targetVersion;
                paramsStream << targetVersionString;
                paramsStream << clientVersion;
                CommServer::instance()->sendSignal(SignalNum::UPDATER_SHOW_DIALOG, params, id);
            }
        }
    }
}

void AppServer::onRestartClientReceived() {
    bool quit = false;
#if NDEBUG
    if (clientHasCrashed()) {
        LOG_ERROR(_logger, "Client disconnected because it has crashed");
        handleClientCrash(quit);
        if (quit) {
            QMessageBox::warning(0, QString(APPLICATION_NAME), crashMsg, QMessageBox::Ok);
        }
    } else {
        LOG_INFO(_logger, "Client disconnected because it was killed");
        quit = true;
    }
#else
    LOG_INFO(_logger, "Client disconnected");
    quit = true;
#endif

    if (quit) {
        QTimer::singleShot(0, this, &AppServer::quit);
    }
}

void AppServer::onMessageReceivedFromAnotherProcess(const QString &message, QObject *) {
    LOG_DEBUG(_logger, "Message received from another kDrive process: '" << message.toStdString().c_str() << "'");

    if (message == showSynthesisMsg) {
        showSynthesis();
    } else if (message == showSettingsMsg) {
        showSettings();
    }
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

bool AppServer::vfsIsExcluded(int syncDbId, const SyncPath &itemPath, bool &isExcluded) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    isExcluded = _vfsMap[syncDbId]->isExcluded(SyncName2QStr(itemPath.native()));

    return true;
}

bool AppServer::vfsExclude(int syncDbId, const SyncPath &itemPath) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    _vfsMap[syncDbId]->exclude(SyncName2QStr(itemPath.native()));

    return true;
}

bool AppServer::vfsPinState(int syncDbId, const SyncPath &absolutePath, PinState &pinState) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    SyncPath relativePath = CommonUtility::relativePath(_syncPalMap[syncDbId]->localPath(), absolutePath);
    PinState tmpPinState = _vfsMap[syncDbId]->pinState(SyncName2QStr(relativePath.native()));
    pinState = (tmpPinState != PinState::Inherited) ? tmpPinState : PinState::Unspecified;
    return true;
}

bool AppServer::vfsSetPinState(int syncDbId, const SyncPath &itemPath, PinState pinState) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    SyncPath relativePath = CommonUtility::relativePath(_syncPalMap[syncDbId]->localPath(), itemPath);
    if (!_vfsMap[syncDbId]->setPinState(SyncName2QStr(relativePath.native()), pinState)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in Vfs::setPinState for syncDbId=" << syncDbId << L" and path=" << Path2WStr(itemPath).c_str());
        return false;
    }

    return true;
}

bool AppServer::vfsStatus(int syncDbId, const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing,
                          int &progress) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    return _vfsMap[syncDbId]->status(SyncName2QStr(itemPath.native()), isPlaceholder, isHydrated, isSyncing, progress);
}

bool AppServer::vfsCreatePlaceholder(int syncDbId, const SyncPath &relativeLocalPath, const SyncFileItem &item) {
    auto vfsIt = _vfsMap.find(syncDbId);
    if (vfsIt == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    if (vfsIt->second && !vfsIt->second->createPlaceholder(relativeLocalPath, item)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::createPlaceholder for syncDbId="
                                                        << syncDbId << L" and path=" << Path2WStr(item.path()).c_str());
        return false;
    }

    return true;
}

bool AppServer::vfsConvertToPlaceholder(int syncDbId, const SyncPath &path, const SyncFileItem &item, bool &needRestart) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    if (!_vfsMap[syncDbId]->convertToPlaceholder(SyncName2QStr(path.native()), item, needRestart)) {
        if (!needRestart) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::convertToPlaceholder for syncDbId="
                                                            << syncDbId << L" and path=" << Path2WStr(item.path()).c_str());
        }

        return false;
    }

    return true;
}

bool AppServer::vfsUpdateMetadata(int syncDbId, const SyncPath &path, const SyncTime &creationTime, const SyncTime &modtime,
                                  const int64_t size, const NodeId &id, std::string &error) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    QByteArray fileId(id.c_str());
    QString *errorStr = nullptr;
    if (!_vfsMap[syncDbId]->updateMetadata(SyncName2QStr(path.native()), creationTime, modtime, size, fileId, errorStr)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in Vfs::updateMetadata for syncDbId=" << syncDbId << L" and path=" << Path2WStr(path).c_str());
        error = errorStr ? errorStr->toStdString() : "";
        return false;
    }

    return true;
}

bool AppServer::vfsUpdateFetchStatus(int syncDbId, const SyncPath &tmpPath, const SyncPath &path, int64_t received,
                                     bool &canceled, bool &finished) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    if (!_vfsMap[syncDbId]->updateFetchStatus(SyncName2QStr(tmpPath), SyncName2QStr(path), received, canceled, finished)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in Vfs::updateFetchStatus for syncDbId=" << syncDbId << L" and path=" << Path2WStr(path).c_str());
        return false;
    }

    return true;
}

bool AppServer::vfsFileStatusChanged(int syncDbId, const SyncPath &path, SyncFileStatus status) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    if (!_vfsMap[syncDbId]->fileStatusChanged(SyncName2QStr(path.native()), status)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in Vfs::fileStatusChanged for syncDbId=" << syncDbId << L" and path=" << Path2WStr(path).c_str());
        return false;
    }

    return true;
}

bool AppServer::vfsForceStatus(int syncDbId, const SyncPath &path, bool isSyncing, int progress, bool isHydrated) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    if (!_vfsMap[syncDbId]->forceStatus(SyncName2QStr(path.native()), isSyncing, progress, isHydrated)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in Vfs::forceStatus for syncDbId=" << syncDbId << L" and path=" << Path2WStr(path).c_str());
        return false;
    }

    return true;
}

bool AppServer::vfsCleanUpStatuses(int syncDbId) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    if (!_vfsMap[syncDbId]->cleanUpStatuses()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in Vfs::cleanUpStatuses for syncDbId=" << syncDbId);
        return false;
    }

    return true;
}

bool AppServer::vfsClearFileAttributes(int syncDbId, const SyncPath &path) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    _vfsMap[syncDbId]->clearFileAttributes(SyncName2QStr(path.native()));

    return true;
}

bool AppServer::vfsCancelHydrate(int syncDbId, const SyncPath &path) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return false;
    }

    _vfsMap[syncDbId]->cancelHydrate(SyncName2QStr(path.native()));

    return true;
}

void AppServer::syncFileStatus(int syncDbId, const SyncPath &path, SyncFileStatus &status) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = _syncPalMap[syncDbId]->fileStatus(ReplicaSide::Local, path, status);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::fileStatus for syncDbId=" << syncDbId);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

void AppServer::syncFileSyncing(int syncDbId, const SyncPath &path, bool &syncing) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = _syncPalMap[syncDbId]->fileSyncing(ReplicaSide::Local, path, syncing);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::fileSyncing for syncDbId=" << syncDbId);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

void AppServer::setSyncFileSyncing(int syncDbId, const SyncPath &path, bool syncing) {
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(Log::instance()->getLogger(), "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        addError(Error(errId(), ExitCode::DataError, ExitCause::Unknown));
        return;
    }

    ExitCode exitCode = _syncPalMap[syncDbId]->setFileSyncing(ReplicaSide::Local, path, syncing);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(Log::instance()->getLogger(), "Error in SyncPal::setFileSyncing for syncDbId=" << syncDbId);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

#ifdef Q_OS_MAC
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
            {&MigrationParams::migrateAppExclusion, "migrateAppExclusion"},
            {&MigrationParams::migrateSelectiveSyncs, "migrateSelectiveSyncs"}};

    for (const auto &migrate: migrateArr) {
        ExitCode functionExitCode = (mp.*migrate.first)();
        if (functionExitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in " << migrate.second.c_str());
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
        LOG_WARN(_logger, "Error in Requests::loadUserInfo : " << exitCode);
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
                LOG_WARN(_logger, "Error in Requests::loadDriveInfo : " << exitCode);
                return exitCode;
            }

            if (drive.accessDenied() || drive.maintenance()) {
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
                    if (drive.maintenance()) {
                        exitCause = drive.notRenew() ? ExitCause::DriveNotRenew : ExitCause::DriveMaintenance;
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
                            LOG_WARN(_logger, "Error in Requests::deleteAccount : " << exitCode);
                            return exitCode;
                        }

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

ExitCode AppServer::startSyncs(ExitCause &exitCause) {
    // Load user list
    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllUsers");
        return ExitCode::DbError;
    }

    for (User &user: userList) {
        ExitCode exitCode = startSyncs(user, exitCause);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in startSyncs for userDbId=" << user.dbId());
            return exitCode;
        }
    }

    Utility::restartFinderExtension();

    return ExitCode::Ok;
}

std::string liteSyncActivationLogMessage(bool enabled, int syncDbId) {
    const std::string activationStatus = enabled ? "enabled" : "disabled";
    std::stringstream ss;

    ss << "LiteSync is " << activationStatus << " for syncDbId=" << syncDbId;

    return ss.str();
}

// This function will pause the synchronization in case of errors.
ExitCode AppServer::tryCreateAndStartVfs(Sync &sync) noexcept {
    const std::string liteSyncMsg = liteSyncActivationLogMessage(sync.virtualFileMode() != VirtualFileMode::Off, sync.dbId());
    LOG_INFO(_logger, liteSyncMsg.c_str());

    ExitCause exitCause = ExitCause::Unknown;
    const ExitCode exitCode = createAndStartVfs(sync, exitCause);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger,
                 "Error in createAndStartVfs for syncDbId=" << sync.dbId() << " - exitCode=" << exitCode << ", pausing.");
        addError(Error(sync.dbId(), errId(), exitCode, exitCause));

        // Set sync's paused flag
        sync.setPaused(true);

        bool found = false;
        if (!ParmsDb::instance()->setSyncPaused(sync.dbId(), true, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::setSyncPaused");
        }
        if (!found) {
            LOG_WARN(_logger, "Sync not found");
        }
    }

    return exitCode;
}

ExitCode AppServer::startSyncs(User &user, ExitCause &exitCause) {
    logExtendedLogActivationMessage(ParametersCache::isExtendedLogEnabled());

    ExitCode mainExitCode = ExitCode::Ok;
    ExitCode exitCode = ExitCode::Ok;
    bool found = false;

    // Load account list
    std::vector<Account> accountList;
    if (!ParmsDb::instance()->selectAllAccounts(user.dbId(), accountList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllAccounts");
        exitCause = ExitCause::DbAccessError;
        return ExitCode::DbError;
    }

    for (Account &account: accountList) {
        // Load drive list
        std::vector<Drive> driveList;
        if (!ParmsDb::instance()->selectAllDrives(account.dbId(), driveList)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAllDrives");
            exitCause = ExitCause::DbAccessError;
            return ExitCode::DbError;
        }

        for (Drive &drive: driveList) {
            // Load sync list
            std::vector<Sync> syncList;
            if (!ParmsDb::instance()->selectAllSyncs(drive.dbId(), syncList)) {
                LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
                exitCause = ExitCause::DbAccessError;
                return ExitCode::DbError;
            }

            for (Sync &sync: syncList) {
                QSet<QString> blackList;
                QSet<QString> undecidedList;

                if (user.toMigrate()) {
                    if (!user.keychainKey().empty()) {
                        // End migration once connected
                        bool syncUpdated = false;
                        exitCode = processMigratedSyncOnceConnected(user.dbId(), drive.driveId(), sync, blackList, undecidedList,
                                                                    syncUpdated);
                        if (exitCode != ExitCode::Ok) {
                            LOG_WARN(_logger, "Error in updateMigratedSyncPalOnceConnected for syncDbId=" << sync.dbId());
                            mainExitCode = exitCode;
                            exitCause = ExitCause::Unknown;
                            continue;
                        }

                        if (syncUpdated) {
                            // Update sync
                            if (!ParmsDb::instance()->updateSync(sync, found)) {
                                LOG_WARN(_logger, "Error in ParmsDb::updateSync");
                                exitCause = ExitCause::DbAccessError;
                                return ExitCode::DbError;
                            }
                            if (!found) {
                                LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << sync.dbId());
                                exitCause = ExitCause::DbEntryNotFound;
                                return ExitCode::DataError;
                            }

                            SyncInfo syncInfo;
                            ServerRequests::syncToSyncInfo(sync, syncInfo);
                            sendSyncUpdated(syncInfo);
                        }
                    }
                }

                exitCode = checkIfSyncIsValid(sync);
                exitCause = ExitCause::Unknown;
                if (exitCode != ExitCode::Ok) {
                    addError(Error(sync.dbId(), errId(), exitCode, exitCause));
                    continue;
                }

                tryCreateAndStartVfs(sync);

                // Create and start SyncPal
                exitCode =
                        initSyncPal(sync, blackList, undecidedList, QSet<QString>(), !user.keychainKey().empty(), false, false);
                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger, "Error in initSyncPal for syncDbId=" << sync.dbId() << " - exitCode=" << exitCode);
                    addError(Error(sync.dbId(), errId(), exitCode, ExitCause::Unknown));
                    mainExitCode = exitCode;
                }
            }
        }
    }

    if (user.toMigrate()) {
        // Migration done
        user.setToMigrate(false);

        // Update user
        if (!ParmsDb::instance()->updateUser(user, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateUser");
            exitCause = ExitCause::DbAccessError;
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "User not found in user table for userDbId=" << user.dbId());
            exitCause = ExitCause::DbEntryNotFound;
            return ExitCode::DataError;
        }
    }

    return mainExitCode;
}

ExitCode AppServer::processMigratedSyncOnceConnected(int userDbId, int driveId, Sync &sync, QSet<QString> &blackList,
                                                     QSet<QString> &undecidedList, bool &syncUpdated) {
    LOG_DEBUG(_logger, "Update migrated SyncPal for syncDbId=" << sync.dbId());

    // Set sync target nodeId for advanced sync
    if (!sync.targetPath().empty()) {
        // Split path
        std::vector<SyncName> names;
        SyncPath pathTmp(sync.targetPath());
        while (pathTmp != pathTmp.root_path()) {
            names.push_back(pathTmp.filename().native());
            pathTmp = pathTmp.parent_path();
        }

        // Get root subfolders
        QList<NodeInfo> list;
        ExitCode exitCode = ServerRequests::getSubFolders(sync.driveDbId(), QString(), list);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in Requests::getSubFolders with driveDbId =" << sync.driveDbId());
            return exitCode;
        }

        NodeId nodeId;
        while (!list.empty() && !names.empty()) {
            NodeInfo info = list.back();
            list.pop_back();
            if (QStr2SyncName(info.name()) == names.back()) {
                names.pop_back();
                if (names.empty()) {
                    nodeId = info.nodeId().toStdString();
                    break;
                }

                exitCode = ServerRequests::getSubFolders(sync.driveDbId(), info.nodeId(), list);
                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger, "Error in Requests::getSubFolders with driveDbId =" << sync.driveDbId() << " nodeId = "
                                                                                          << info.nodeId().toStdString().c_str());
                    return exitCode;
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
        return ExitCode::DbError;
    }

    // Generate blacklist & undecidedList
    if (migrationSelectiveSyncList.size()) {
        QString nodeId;
        for (const auto &migrationSelectiveSync: migrationSelectiveSyncList) {
            if (migrationSelectiveSync.syncDbId() != sync.dbId()) {
                continue;
            }

            ExitCode exitCode = ServerRequests::getNodeIdByPath(userDbId, driveId, migrationSelectiveSync.path(), nodeId);
            if (exitCode != ExitCode::Ok) {
                // The folder could have been deleted in the drive
                LOGW_DEBUG(_logger, L"Error in Requests::getNodeIdByPath for userDbId="
                                            << userDbId << L" driveId=" << driveId << L" path="
                                            << Path2WStr(migrationSelectiveSync.path()).c_str());
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

void AppServer::initLogging() {
    // Setup log4cplus
    IoError ioError = IoError::Success;
    SyncPath logDirPath;
    if (!IoHelper::logDirectoryPath(logDirPath, ioError)) {
        throw std::runtime_error("Error in initLogging: failed to get the log directory path.");
    }

    const std::filesystem::path logFilePath = logDirPath / Utility::logFileNameWithTime();
    _logger = Log::instance(Path2WStr(logFilePath))->getLogger();

    LOGW_INFO(_logger, Utility::s2ws(QString::fromLatin1("%1 locale:[%2] version:[%4] os:[%5]")
                                             .arg(_theme->appName(), QLocale::system().name(), _theme->version(),
                                                  KDC::CommonUtility::platformName())
                                             .toStdString())
                               .c_str());
}

void AppServer::setupProxy() {
    Proxy::instance(ParametersCache::instance()->parameters().proxyConfig());
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
            LOG_ERROR(_logger, "Error in ParmsDb::updateAppState");
        }
        return;
    }

    std::string timestampStr;
    if (_crashRecovered) {
        LOG_WARN(_logger, "Server auto restart after a crash.");

        if (serverCrashedRecently()) {
            LOG_FATAL(_logger, "Server crashed twice in a short time, exiting");
            QMessageBox::warning(nullptr, QString(APPLICATION_NAME), crashMsg, QMessageBox::Ok);
            if (!KDC::ParmsDb::instance()->updateAppState(AppStateKey::LastServerSelfRestartDate, 0, found) || !found) {
                LOG_ERROR(_logger, "Error in ParmsDb::updateAppState");
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
        LOG_ERROR(_logger, "Error in ParmsDb::updateAppState");
    }
}

bool AppServer::serverCrashedRecently(int seconds) {
    const int64_t nowSeconds =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

    AppStateValue appStateValue = int64_t(0);
    if (bool found = false;
        !KDC::ParmsDb::instance()->selectAppState(AppStateKey::LastServerSelfRestartDate, appStateValue, found) || !found) {
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbEntryNotFound));
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
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
        !KDC::ParmsDb::instance()->selectAppState(AppStateKey ::LastClientSelfRestartDate, appStateValue, found) || !found) {
        addError(Error(errId(), ExitCode::DbError, ExitCause::DbEntryNotFound));
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
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
        } else {
            showHint("Unrecognized option '" + option.toStdString() + "'");
        }
    }
}

void AppServer::showHelp() {
    QString helpText;
    QTextStream stream(&helpText);
    stream << _theme->appName() << QLatin1String(" version ") << _theme->version() << Qt::endl;

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
        throw std::runtime_error("Unable to create parameters database.");
        return;
    }

    try {
        ParmsDb::instance(parmsDbPath, _theme->version().toStdString());
    } catch (const std::exception &) {
        throw std::runtime_error("Unable to open parameters database.");
        return;
    }

    // Get sync list
    std::vector<Sync> syncList;
    if (!ParmsDb::instance()->selectAllSyncs(syncList)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAllSyncs");
    }

    // Clear node tables
    for (const Sync &sync: syncList) {
        SyncPath dbPath = sync.dbPath();
        auto syncDbPtr = std::make_shared<SyncDb>(dbPath.string(), _theme->version().toStdString());
        syncDbPtr->clearNodes();
    }
}

void AppServer::sendShowSettingsMsg() {
    sendMessage(showSettingsMsg);
}

void AppServer::sendShowSynthesisMsg() {
    sendMessage(showSynthesisMsg);
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
        throw std::runtime_error("Unable to create parameters database.");
        return;
    }

    try {
        ParmsDb::instance(parmsDbPath, _theme->version().toStdString());
    } catch (const std::exception &) {
        throw std::runtime_error("Unable to open parameters database.");
        return;
    }

    std::vector<User> userList;
    if (!ParmsDb::instance()->selectAllUsers(userList)) {
        LOG_WARN(_logger, "Error in  ParmsDb::selectAllUsers");
        throw std::runtime_error("Error in ParmsDb::selectAllUsers");
        return;
    }
    for (const auto &user: userList) {
        KeyChainManager::instance()->deleteToken(user.keychainKey());
    }
}

void AppServer::showAlreadyRunning() {
    QMessageBox::warning(0, QString(APPLICATION_NAME), tr("kDrive application is already running!"), QMessageBox::Ok);
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

#if defined(Q_OS_WIN)
        pathToExecutable += QString("/%1.exe").arg(APPLICATION_CLIENT_EXECUTABLE);
#else
        pathToExecutable += QString("/%1").arg(APPLICATION_CLIENT_EXECUTABLE);
#endif

        QStringList arguments;
        arguments << QString::number(CommServer::instance()->commPort());

        LOGW_INFO(_logger, L"Starting kDrive client - path=" << Path2WStr(QStr2Path(pathToExecutable)).c_str() << L" args="
                                                             << arguments[0].toStdWString().c_str());

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
        if (user.keychainKey().empty()) {
            LOG_DEBUG(_logger, "User " << user.dbId() << " is not connected");
            continue;
        }

        ExitCode exitCode = updateUserInfo(user);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in updateUserInfo : " << exitCode);
            return exitCode;
        }
    }

    return ExitCode::Ok;
}

ExitCode AppServer::initSyncPal(const Sync &sync, const std::unordered_set<NodeId> &blackList,
                                const std::unordered_set<NodeId> &undecidedList, const std::unordered_set<NodeId> &whiteList,
                                bool start, bool resumedByUser, bool firstInit) {
    ExitCode exitCode;

    std::chrono::duration<double, std::milli> elapsed_ms = std::chrono::steady_clock::now() - _lastSyncPalStart;
    if (elapsed_ms.count() < START_SYNCPALS_TIME_GAP) {
        // Shifts the start of the next sync
        Utility::msleep(START_SYNCPALS_TIME_GAP - elapsed_ms.count());
    }
    _lastSyncPalStart = std::chrono::steady_clock::now();

    if (_syncPalMap.find(sync.dbId()) == _syncPalMap.end()) {
        // Create SyncPal
        try {
            _syncPalMap[sync.dbId()] = std::shared_ptr<SyncPal>(new SyncPal(sync.dbId(), _theme->version().toStdString()));
        } catch (std::exception const &) {
            LOG_WARN(_logger, "Error in SyncPal::SyncPal for syncDbId=" << sync.dbId());
            return ExitCode::DbError;
        }

        // Set callbacks
        _syncPalMap[sync.dbId()]->setAddErrorCallback(&addError);
        _syncPalMap[sync.dbId()]->setAddCompletedItemCallback(&addCompletedItem);
        _syncPalMap[sync.dbId()]->setSendSignalCallback(&sendSignal);

        _syncPalMap[sync.dbId()]->setVfsIsExcludedCallback(&vfsIsExcluded);
        _syncPalMap[sync.dbId()]->setVfsExcludeCallback(&vfsExclude);
        _syncPalMap[sync.dbId()]->setVfsPinStateCallback(&vfsPinState);
        _syncPalMap[sync.dbId()]->setVfsSetPinStateCallback(&vfsSetPinState);
        _syncPalMap[sync.dbId()]->setVfsStatusCallback(&vfsStatus);
        _syncPalMap[sync.dbId()]->setVfsCreatePlaceholderCallback(&vfsCreatePlaceholder);
        _syncPalMap[sync.dbId()]->setVfsConvertToPlaceholderCallback(&vfsConvertToPlaceholder);
        _syncPalMap[sync.dbId()]->setVfsUpdateMetadataCallback(&vfsUpdateMetadata);
        _syncPalMap[sync.dbId()]->setVfsUpdateFetchStatusCallback(&vfsUpdateFetchStatus);
        _syncPalMap[sync.dbId()]->setVfsFileStatusChangedCallback(&vfsFileStatusChanged);
        _syncPalMap[sync.dbId()]->setVfsForceStatusCallback(&vfsForceStatus);
        _syncPalMap[sync.dbId()]->setVfsCleanUpStatusesCallback(&vfsCleanUpStatuses);
        _syncPalMap[sync.dbId()]->setVfsClearFileAttributesCallback(&vfsClearFileAttributes);
        _syncPalMap[sync.dbId()]->setVfsCancelHydrateCallback(&vfsCancelHydrate);

        if (blackList != std::unordered_set<NodeId>()) {
            // Set blackList (create or overwrite the possible existing list in DB)
            exitCode = _syncPalMap[sync.dbId()]->setSyncIdSet(SyncNodeType::BlackList, blackList);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet");
                return exitCode;
            }
        }

        if (undecidedList != std::unordered_set<NodeId>()) {
            // Set undecidedList (create or overwrite the possible existing list in DB)
            exitCode = _syncPalMap[sync.dbId()]->setSyncIdSet(SyncNodeType::UndecidedList, undecidedList);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet");
                return exitCode;
            }
        }

        if (whiteList != std::unordered_set<NodeId>()) {
            // Set undecidedList (create or overwrite the possible existing list in DB)
            exitCode = _syncPalMap[sync.dbId()]->setSyncIdSet(SyncNodeType::WhiteList, whiteList);
            if (exitCode != ExitCode::Ok) {
                LOG_WARN(_logger, "Error in SyncPal::setSyncIdSet");
                return exitCode;
            }
        }
    }

#if defined(_WIN32) || defined(__APPLE__)
    if (firstInit) {
        if (!_syncPalMap[sync.dbId()]->wipeOldPlaceholders()) {
            LOG_WARN(_logger, "Error in SyncPal::wipeOldPlaceholders");
        }
    }
#else
    (void) firstInit;
#endif

    if (start && (resumedByUser || !sync.paused())) {
        std::chrono::time_point<std::chrono::system_clock> pauseTime;
        if (_syncPalMap[sync.dbId()]->isPaused(pauseTime)) {
            // Unpause SyncPal
            _syncPalMap[sync.dbId()]->unpause();
        } else if (!_syncPalMap[sync.dbId()]->isRunning()) {
            // Start SyncPal
            _syncPalMap[sync.dbId()]->start();
        }
    }

    // Register the folder with the socket API
    if (_socketApi) {
        _socketApi->registerSync(sync.dbId());
    }

    // Clear old errors for this sync
    clearErrors(sync.dbId(), false);
    clearErrors(sync.dbId(), true);

    return ExitCode::Ok;
}

ExitCode AppServer::initSyncPal(const Sync &sync, const QSet<QString> &blackList, const QSet<QString> &undecidedList,
                                const QSet<QString> &whiteList, bool start, bool resumedByUser, bool firstInit) {
    ExitCode exitCode;

    std::unordered_set<NodeId> blackList2;
    for (const QString &nodeId: blackList) {
        blackList2.insert(nodeId.toStdString());
    }

    std::unordered_set<NodeId> undecidedList2;
    for (const QString &nodeId: undecidedList) {
        undecidedList2.insert(nodeId.toStdString());
    }

    std::unordered_set<NodeId> whiteList2;
    for (const QString &nodeId: whiteList) {
        whiteList2.insert(nodeId.toStdString());
    }

    exitCode = initSyncPal(sync, blackList2, undecidedList2, whiteList2, start, resumedByUser, firstInit);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in initSyncPal");
        return exitCode;
    }

    return ExitCode::Ok;
}

ExitCode AppServer::stopSyncPal(int syncDbId, bool pausedByUser, bool quit, bool clear) {
    // Unregister the folder with the socket API
    if (_socketApi) {
        _socketApi->unregisterSync(syncDbId);
    }

    // Stop SyncPal
    if (_syncPalMap.find(syncDbId) == _syncPalMap.end()) {
        LOG_WARN(_logger, "SyncPal not found in syncPalMap for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    if (_syncPalMap[syncDbId]) {
        _syncPalMap[syncDbId]->stop(pausedByUser, quit, clear);
    }

    return ExitCode::Ok;
}

ExitCode AppServer::createAndStartVfs(const Sync &sync, ExitCause &exitCause) noexcept {
    // Check that the sync folder exists.
    bool exists = false;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(sync.localPath(), exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists " << Utility::formatIoError(sync.localPath(), ioError).c_str());
        exitCause = ExitCause::Unknown;
        return ExitCode::SystemError;
    }

    if (!exists) {
        LOGW_WARN(_logger, L"Sync localpath " << Utility::formatSyncPath(sync.localPath()).c_str() << L" doesn't exist.");
        exitCause = ExitCause::SyncDirDoesntExist;
        return ExitCode::SystemError;
    }

    if (_vfsMap.find(sync.dbId()) == _vfsMap.end()) {
#ifdef Q_OS_WIN
        Drive drive;
        bool found;
        if (!ParmsDb::instance()->selectDrive(sync.driveDbId(), drive, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectDrive");
            exitCause = ExitCause::DbAccessError;
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "Drive not found in drive table for driveDbId=" << sync.driveDbId());
            exitCause = ExitCause::DbEntryNotFound;
            return ExitCode::DataError;
        }

        Account account;
        if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectAccount");
            exitCause = ExitCause::DbAccessError;
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "Account not found in account table for accountDbId=" << drive.accountDbId());
            exitCause = ExitCause::DbEntryNotFound;
            return ExitCode::DataError;
        }

        User user;
        if (!ParmsDb::instance()->selectUser(account.userDbId(), user, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::selectUser");
            exitCause = ExitCause::DbAccessError;
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "User not found in user table for userDbId=" << account.userDbId());
            exitCause = ExitCause::DbEntryNotFound;
            return ExitCode::DataError;
        }
#endif

        // Create VFS instance
        VfsSetupParams vfsSetupParams;
        vfsSetupParams._syncDbId = sync.dbId();
#ifdef Q_OS_WIN
        vfsSetupParams._driveId = drive.driveId();
        vfsSetupParams._userId = user.userId();
#endif
        vfsSetupParams._localPath = sync.localPath();
        vfsSetupParams._targetPath = sync.targetPath();
        connect(this, &AppServer::socketApiExecuteCommandDirect, _socketApi.data(), &SocketApi::executeCommandDirect);
        vfsSetupParams._executeCommand = [this](const char *command) { emit socketApiExecuteCommandDirect(QString(command)); };
        vfsSetupParams._logger = _logger;
        QString error;
        _vfsMap[sync.dbId()] = KDC::createVfsFromPlugin(sync.virtualFileMode(), vfsSetupParams, error);
        if (!_vfsMap[sync.dbId()]) {
            LOG_WARN(_logger, "Error in Vfs::createVfsFromPlugin for mode " << sync.virtualFileMode() << " : "                                                                     << error.toStdString().c_str());
            _vfsMap.erase(sync.dbId());
            exitCause = ExitCause::UnableToCreateVfs;
            return ExitCode::SystemError;
        }
        _vfsMap[sync.dbId()]->setExtendedLog(ParametersCache::isExtendedLogEnabled());

        // Set callbacks
        _vfsMap[sync.dbId()]->setSyncFileStatusCallback(&syncFileStatus);
        _vfsMap[sync.dbId()]->setSyncFileSyncingCallback(&syncFileSyncing);
        _vfsMap[sync.dbId()]->setSetSyncFileSyncingCallback(&setSyncFileSyncing);
#ifdef Q_OS_MAC
        _vfsMap[sync.dbId()]->setExclusionAppListCallback(&exclusionAppList);
#endif
    }

    // Start VFS
    if (!_vfsMap[sync.dbId()]->start(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone)) {
#ifdef Q_OS_MAC
        if (sync.virtualFileMode() == VirtualFileMode::Mac) {
            if (_vfsInstallationDone && !_vfsActivationDone) {
                // Check LiteSync ext authorizations
                std::string liteSyncExtErrorDescr;
                bool liteSyncExtOk = CommonUtility::isLiteSyncExtEnabled() &&
                                     CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
                if (!liteSyncExtOk) {
                    if (liteSyncExtErrorDescr.empty()) {
                        LOG_WARN(_logger, "LiteSync extension is not enabled or doesn't have full disk access");
                    } else {
                        LOG_WARN(_logger, "LiteSync extension is not enabled or doesn't have full disk access: "
                                                  << liteSyncExtErrorDescr.c_str());
                    }
                    exitCause = ExitCause::LiteSyncNotAllowed;
                    return ExitCode::SystemError;
                }
            }
        }
#endif

        LOG_WARN(_logger, "Error in Vfs::start");
        exitCause = ExitCause::UnableToCreateVfs;
        return ExitCode::SystemError;
    }

#ifdef Q_OS_WIN
    // Save sync
    Sync tmpSync(sync);
    tmpSync.setNavigationPaneClsid(_vfsMap[sync.dbId()]->namespaceCLSID());

    if (tmpSync.virtualFileMode() == KDC::VirtualFileMode::Win) {
        OldUtility::setFolderPinState(QUuid(QString::fromStdString(tmpSync.navigationPaneClsid())),
                                      _navigationPaneHelper->showInExplorerNavigationPane());
    } else {
        if (tmpSync.navigationPaneClsid().empty()) {
            tmpSync.setNavigationPaneClsid(QUuid::createUuid().toString().toStdString());
            _vfsMap[sync.dbId()]->setNamespaceCLSID(tmpSync.navigationPaneClsid());
        }
        OldUtility::addLegacySyncRootKeys(QUuid(QString::fromStdString(sync.navigationPaneClsid())),
                                          SyncName2QStr(sync.localPath().native()),
                                          _navigationPaneHelper->showInExplorerNavigationPane());
    }

    bool found = false;
    if (!ParmsDb::instance()->updateSync(tmpSync, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateSync");
        exitCause = ExitCause::DbAccessError;
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << tmpSync.dbId());
        exitCause = ExitCause::DbEntryNotFound;
        return ExitCode::DataError;
    }
#endif

    return ExitCode::Ok;
}

ExitCode AppServer::stopVfs(int syncDbId, bool unregister) {
    LOG_DEBUG(_logger, "Stop VFS for syncDbId=" << syncDbId);

    // Stop Vfs
    if (_vfsMap.find(syncDbId) == _vfsMap.end()) {
        LOG_WARN(_logger, "Vfs not found in vfsMap for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    if (_vfsMap[syncDbId]) {
        _vfsMap[syncDbId]->stop(unregister);
    }

    LOG_DEBUG(_logger, "Stop VFS for syncDbId=" << syncDbId << " done");

    return ExitCode::Ok;
}

ExitCode AppServer::setSupportsVirtualFiles(int syncDbId, bool value) {
    Sync sync;
    bool found;
    if (!ParmsDb::instance()->selectSync(syncDbId, sync, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectSync");
        return ExitCode::DbError;
    }
    if (!found) {
        LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << syncDbId);
        return ExitCode::DataError;
    }

    // Check if sync is valid
    ExitCode exitCode = checkIfSyncIsValid(sync);
    ExitCause exitCause = ExitCause::Unknown;
    if (exitCode != ExitCode::Ok) {
        addError(Error(sync.dbId(), errId(), exitCode, exitCause));
        return exitCode;
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
        exitCode = stopVfs(syncDbId, true);
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in stopVfs for syncDbId=" << sync.dbId());
            return exitCode;
        }

        if (newMode == VirtualFileMode::Off) {
#ifdef Q_OS_WIN
            LOG_INFO(_logger, "Clearing node DB");
            _syncPalMap[syncDbId]->clearNodes();
#else
            // Clear file system
            _syncPalMap[sync.dbId()]->wipeVirtualFiles();
#endif
        }

#ifdef Q_OS_WIN
        if (newMode == VirtualFileMode::Win) {
            // Remove legacy sync root keys
            OldUtility::removeLegacySyncRootKeys(QUuid(QString::fromStdString(sync.navigationPaneClsid())));
            sync.setNavigationPaneClsid(std::string());
        } else if (sync.virtualFileMode() == VirtualFileMode::Win) {
            // Add legacy sync root keys
            bool show = _navigationPaneHelper->showInExplorerNavigationPane();
            if (sync.navigationPaneClsid().empty()) {
                sync.setNavigationPaneClsid(QUuid::createUuid().toString().toStdString());
            }
            OldUtility::addLegacySyncRootKeys(QUuid(QString::fromStdString(sync.navigationPaneClsid())),
                                              SyncName2QStr(sync.localPath().native()), show);
        }
#endif

        // Update Vfs mode in sync
        sync.setVirtualFileMode(newMode);
        if (!ParmsDb::instance()->updateSync(sync, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::updateSync");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "Sync not found in sync table for syncDbId=" << syncDbId);
            return ExitCode::DataError;
        }

        // Delete previous vfs
        _vfsMap.erase(syncDbId);

        tryCreateAndStartVfs(sync);

        QTimer::singleShot(100, this, [=]() {
            bool ok = true;

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

            if (ok) {
                // Re-start sync
                _syncPalMap[syncDbId]->start();
            }
        });
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

        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AppServer::addError", "Sockets defuncted error");

        // Decrease upload session max parallel jobs
        ParametersCache::instance()->decreaseUploadSessionParallelThreads();

        // Decrease JobManager pool capacity
        JobManager::instance()->decreasePoolCapacity();
    }

    if (!ServerRequests::isAutoResolvedError(error)) {
        // Send error to sentry only for technical errors
        SentryUser sentryUser(user.email().c_str(), user.name().c_str(), std::to_string(user.userId()).c_str());
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "AppServer::addError", error.errorString().c_str(),
                                                  sentryUser);
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

void AppServer::sendSyncProgressInfo(int syncDbId, SyncStatus status, SyncStep step, int64_t currentFile, int64_t totalFiles,
                                     int64_t completedSize, int64_t totalSize, int64_t estimatedRemainingTime) {
    int id = 0;

    QByteArray params;
    QDataStream paramsStream(&params, QIODevice::WriteOnly);
    paramsStream << syncDbId;
    paramsStream << status;
    paramsStream << step;
    paramsStream << static_cast<qint64>(currentFile);
    paramsStream << static_cast<qint64>(totalFiles);
    paramsStream << static_cast<qint64>(completedSize);
    paramsStream << static_cast<qint64>(totalSize);
    paramsStream << static_cast<qint64>(estimatedRemainingTime);
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
        LOG_WARN(_logger, "Error in updateAllUsersInfo : " << exitCode);
        addError(Error(errId(), exitCode, ExitCause::Unknown));
    }
}

void AppServer::onUpdateSyncsProgress() {
    SyncStatus status;
    SyncStep step;
    int64_t currentFile;
    int64_t totalFiles;
    int64_t completedSize;
    int64_t totalSize;
    int64_t estimatedRemainingTime;

    for (const auto &[syncDbId, syncPal]: _syncPalMap) {
        // Get progress
        status = syncPal->status();
        step = syncPal->step();
        if (status == SyncStatus::Running && step == SyncStep::Propagation2) {
            syncPal->loadProgress(currentFile, totalFiles, completedSize, totalSize, estimatedRemainingTime);
        } else {
            currentFile = 0;
            totalFiles = 0;
            completedSize = 0;
            totalSize = 0;
            estimatedRemainingTime = 0;
        }

        if (_syncCacheMap.find(syncDbId) == _syncCacheMap.end() || _syncCacheMap[syncDbId]._status != status ||
            _syncCacheMap[syncDbId]._step != step || _syncCacheMap[syncDbId]._currentFile != currentFile ||
            _syncCacheMap[syncDbId]._totalFiles != totalFiles || _syncCacheMap[syncDbId]._completedSize != completedSize ||
            _syncCacheMap[syncDbId]._totalSize != totalSize ||
            _syncCacheMap[syncDbId]._estimatedRemainingTime != estimatedRemainingTime) {
            _syncCacheMap[syncDbId] =
                    SyncCache({status, step, currentFile, totalFiles, completedSize, totalSize, estimatedRemainingTime});
            sendSyncProgressInfo(syncDbId, status, step, currentFile, totalFiles, completedSize, totalSize,
                                 estimatedRemainingTime);
        }

        // New big folders detection
        std::unordered_set<NodeId> undecidedSet;
        ExitCode exitCode = syncPal->syncIdSet(SyncNodeType::UndecidedList, undecidedSet);
        if (exitCode != ExitCode::Ok) {
            addError(Error(syncDbId, errId(), exitCode, ExitCause::Unknown));
            LOG_WARN(_logger, "Error in SyncPal::syncIdSet : " << exitCode);
            return;
        }

        bool undecidedSetUpdated = false;
        for (const NodeId &nodeId: undecidedSet) {
            if (_undecidedListCacheMap[syncDbId].find(nodeId) == _undecidedListCacheMap[syncDbId].end()) {
                undecidedSetUpdated = true;

                QString path;
                exitCode = ServerRequests::getPathByNodeId(syncPal->userDbId(), syncPal->driveId(),
                                                           QString::fromStdString(nodeId), path);
                if (exitCode != ExitCode::Ok) {
                    LOG_WARN(_logger, "Error in Requests::getPathByNodeId : " << exitCode);
                    continue;
                }

                // Send newBigFolder signal to client
                sendNewBigFolder(syncDbId, path);

                // Ask client to display notification
                sendShowNotification(Theme::instance()->appNameGUI(),
                                     tr("A new folder larger than %1 MB has been added in the drive %2, you must validate its "
                                        "synchronization: %3.\n")
                                             .arg(ParametersCache::instance()->parameters().bigFolderSizeLimit())
                                             .arg(QString::fromStdString(syncPal->driveName()), path));
            }
        }

        if (undecidedSetUpdated || _undecidedListCacheMap[syncDbId].size() != undecidedSet.size()) {
            _undecidedListCacheMap[syncDbId] = undecidedSet;
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
#ifdef Q_OS_MAC
    // Check if at least one LiteSync sync exists
    bool vfsSync = false;
    for (const auto &vfsMapElt: _vfsMap) {
        if (vfsMapElt.second->mode() == VirtualFileMode::Mac) {
            vfsSync = true;
            break;
        }
    }

    if (vfsSync && _vfsInstallationDone && !_vfsActivationDone) {
        // Check LiteSync ext authorizations
        std::string liteSyncExtErrorDescr;
        bool liteSyncExtOk =
                CommonUtility::isLiteSyncExtEnabled() && CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
        if (!liteSyncExtOk) {
            if (liteSyncExtErrorDescr.empty()) {
                LOG_WARN(_logger, "LiteSync extension is not enabled or doesn't have full disk access");
            } else {
                LOG_WARN(_logger,
                         "LiteSync extension is not enabled or doesn't have full disk access: " << liteSyncExtErrorDescr.c_str());
            }
        } else {
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
                    if (!_vfsMap[syncPalMapElt.first]->start(_vfsInstallationDone, _vfsActivationDone, _vfsConnectionDone)) {
                        LOG_WARN(Log::instance()->getLogger(), "Error in Vfs::start");
                        continue;
                    }

                    // Start sync
                    syncPalMapElt.second->start();
                }
            }
        }
    }
#endif

    for (const auto &syncPalMapElt: _syncPalMap) {
        std::chrono::time_point<std::chrono::system_clock> pauseTime;
        if (syncPalMapElt.second->isPaused(pauseTime) && pauseTime + std::chrono::minutes(1) < std::chrono::system_clock::now()) {
            LOG_INFO(_logger, "Try to resume SyncPal with syncDbId=" << syncPalMapElt.first);
            syncPalMapElt.second->unpause();
        }
    }
}

} // namespace KDC
