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

#include "appclient.h"
#include "guiutility.h"
#include "config.h"
#include "libcommon/utility/qlogiffail.h"
#include "customproxystyle.h"
#include "config.h"
#include "guirequests.h"
#include "parameterscache.h"
#include "custommessagebox.h"
#include "libcommongui/logger.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"
#include "gui/updater/updatedialog.h"
#include "log/sentry/handler.h"

#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QProcess>

#include <iostream>
#include <sstream>
#include <filesystem>
#include <time.h>
#include <fstream>
#include <stdlib.h>

#include <sentry.h>

#define CONNECTION_TRIALS 3
#define CHECKCOMMSTATUS_TRIALS 3

namespace KDC {

Q_LOGGING_CATEGORY(lcAppClient, "gui.appclient", QtInfoMsg)

static const QList<QString> fontFiles = QList<QString>() << QString(":/client/resources/fonts/SuisseIntl-Thin.otf")
                                                         << QString(":/client/resources/fonts/SuisseIntl-UltraLight.otf")
                                                         << QString(":/client/resources/fonts/SuisseIntl-Light.otf")
                                                         << QString(":/client/resources/fonts/SuisseIntl-Regular.otf")
                                                         << QString(":/client/resources/fonts/SuisseIntl-Medium.otf")
                                                         << QString(":/client/resources/fonts/SuisseIntl-SemiBold.otf")
                                                         << QString(":/client/resources/fonts/SuisseIntl-Bold.otf")
                                                         << QString(":/client/resources/fonts/SuisseIntl-Black.otf");
// static const QString defaultFontFamily("Suisse Int'l");

AppClient::AppClient(int &argc, char **argv) :
    SharedTools::QtSingleApplication(QString::fromStdString(Theme::instance()->appClientName()), argc, argv) {
    _startedAt.start();

    setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));
    setApplicationName(QString::fromStdString(_theme->appClientName()));
    setWindowIcon(_theme->applicationIcon());
    setApplicationVersion(QString::fromStdString(_theme->version()));
    setStyle(new KDC::CustomProxyStyle);

    // Load fonts
    for (const QString &fontFile: fontFiles) {
        if (QFontDatabase::addApplicationFont(fontFile) < 0) {
            std::cout << "Error adding font file!" << std::endl;
        }
    }

#ifdef QT_DEBUG
    // Read comm port value from .comm file
    std::filesystem::path commPath(QStr2Path(QDir::homePath()));
    commPath.append(".comm");

    quint16 commPort;
    std::ifstream commFile(commPath);
    commFile >> commPort;
    commFile.close();
    _commPort = static_cast<unsigned short>(commPort);
#else
    // Read comm port value from argument list
    if (!parseOptions(arguments())) {
        QMessageBox msgBox;
        msgBox.setText(tr("kDrive client is run with bad parameters!"));
        msgBox.exec();
        QTimer::singleShot(0, qApp, SLOT(quit()));
        return;
    }
#endif

    if (isRunning()) {
        QMessageBox msgBox;
        msgBox.setText(tr("kDrive client is already running!"));
        msgBox.exec();
        QTimer::singleShot(0, qApp, SLOT(quit()));
        return;
    }

    // Connect to server
    if (connectToServer()) {
        qCInfo(lcAppClient) << "Connected to server";
    } else {
        qCCritical(lcAppClient) << "Failed to connect to server";
        return;
    }

    // Init ParametersCache instance
    if (!ParametersCache::instance()) {
        qCWarning(lcAppClient) << "Error in ParametersCache::instance";
        throw std::runtime_error("Unable to initialize parameters cache.");
    }

    // Setup logging
    setupLogging();

    // Set style
    KDC::GuiUtility::setStyle(qApp);

    CommonUtility::setupTranslations(QApplication::instance(), ParametersCache::instance()->parametersInfo().language());

#ifdef PLUGINDIR
    // Setup extra plugin search path
    QString extraPluginPath = QStringLiteral(PLUGINDIR);
    if (!extraPluginPath.isEmpty()) {
        if (QDir::isRelativePath(extraPluginPath))
            extraPluginPath = QDir(QApplication::applicationDirPath()).filePath(extraPluginPath);
        qCInfo(lcAppClient) << "Adding extra plugin search path:" << extraPluginPath;
        addLibraryPath(extraPluginPath);
    }
#endif

    setQuitOnLastWindowClosed(false);

    // Setup translations
    CommonUtility::setupTranslations(this, ParametersCache::instance()->parametersInfo().language());

    // Remove the files that keep a record of former crash or kill events
    SignalType signalType = SignalType::None;
    CommonUtility::clearSignalFile(AppType::Client, SignalCategory::Crash, signalType);
    if (signalType != SignalType::None) {
        qCInfo(lcAppClient) << "Restarting after a" << SignalCategory::Crash << "with signal" << signalType;
    }

    CommonUtility::clearSignalFile(AppType::Client, SignalCategory::Kill, signalType);
    if (signalType != SignalType::None) {
        qCInfo(lcAppClient) << "Restarting after a" << SignalCategory::Kill << "with signal" << signalType;
    }

    // Setup debug crash mode
    bool isSet = false;
    if (CommonUtility::envVarValue("KDRIVE_DEBUG_CRASH", isSet); isSet) {
        _debugCrash = true;
    }

    // Setup Gui
    _gui = std::shared_ptr<ClientGui>(new ClientGui(this));
    _gui->init();
    GuiRequests::reportClientDisplayed();

    _theme->setSystrayUseMonoIcons(ParametersCache::instance()->parametersInfo().monoIcons());
    connect(_theme, &Theme::systrayUseMonoIconsChanged, this, &AppClient::onUseMonoIconsChanged);

    // Cleanup at Quit
    connect(this, &QCoreApplication::aboutToQuit, this, &AppClient::onCleanup);

#ifdef Q_OS_WIN
    ExitCode exitCode =
            GuiRequests::setShowInExplorerNavigationPane(ParametersCache::instance()->parametersInfo().showShortcuts());
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcAppClient) << "Error in Requests::setShowInExplorerNavigationPane";
    }
#endif

    // Refresh status
    _gui->computeOverallSyncStatus();

    if (_gui->userInfoMap().empty()) {
        // Open the setup wizard
        _gui->onNewDriveWizard();
    } else {
        // Ask user to log in if needed
        for (auto const &[userDbId, userInfoClient]: _gui->userInfoMap()) {
            if (!userInfoClient.connected() && _gui->isUserUsed(userDbId)) {
                askUserToLoginAgain(userInfoClient.dbId(), userInfoClient.email(), false);
            }
        }
    }

    // Update Sentry user
    updateSentryUser();
    connect(_gui.get(), &ClientGui::userListRefreshed, this, &AppClient::updateSentryUser);
}

AppClient::~AppClient() {
    CommClient::instance()->stop();
}

void AppClient::showSynthesisDialog(const QRect &geometry) {
    _gui->showSynthesisDialog(geometry);
}

void AppClient::onSignalReceived(int id, SignalNum num, const QByteArray &params) {
    QDataStream paramsStream(params);

    qCDebug(lcAppClient) << "Sgnl rcvd" << id << num;

    switch (num) {
        case SignalNum::USER_ADDED: {
            UserInfo userInfo;
            paramsStream >> userInfo;

            emit userAdded(userInfo);
            break;
        }
        case SignalNum::USER_UPDATED: {
            UserInfo userInfo;
            paramsStream >> userInfo;

            emit userUpdated(userInfo);
            break;
        }
        case SignalNum::USER_STATUSCHANGED: {
            int userDbId;
            bool connected;
            QString connexionError;
            paramsStream >> userDbId;
            paramsStream >> connected;
            paramsStream >> connexionError;

            emit userStatusChanged(userDbId, connected, connexionError);
            break;
        }
        case SignalNum::USER_REMOVED: {
            int userDbId;
            paramsStream >> userDbId;

            emit userRemoved(userDbId);
            break;
        }
        case SignalNum::ACCOUNT_ADDED: {
            AccountInfo accountInfo;
            paramsStream >> accountInfo;

            emit accountAdded(accountInfo);
            break;
        }
        case SignalNum::ACCOUNT_UPDATED: {
            AccountInfo accountInfo;
            paramsStream >> accountInfo;

            emit accountUpdated(accountInfo);
            break;
        }
        case SignalNum::ACCOUNT_REMOVED: {
            int accountDbId;
            paramsStream >> accountDbId;

            emit accountRemoved(accountDbId);
            break;
        }
        case SignalNum::DRIVE_ADDED: {
            DriveInfo driveInfo;
            paramsStream >> driveInfo;

            emit driveAdded(driveInfo);
            break;
        }
        case SignalNum::DRIVE_UPDATED: {
            DriveInfo driveInfo;
            paramsStream >> driveInfo;

            emit driveUpdated(driveInfo);
            break;
        }
        case SignalNum::DRIVE_QUOTAUPDATED: {
            int driveDbId;
            qint64 total;
            qint64 used;
            paramsStream >> driveDbId;
            paramsStream >> total;
            paramsStream >> used;

            emit driveQuotaUpdated(driveDbId, total, used);
            break;
        }
        case SignalNum::DRIVE_REMOVED: {
            int driveDbId;
            paramsStream >> driveDbId;

            emit driveRemoved(driveDbId);
            break;
        }
        case SignalNum::DRIVE_DELETE_FAILED: {
            int driveDbId;
            paramsStream >> driveDbId;

            emit driveDeletionFailed(driveDbId);
            break;
        }
        case SignalNum::SYNC_ADDED: {
            SyncInfo syncInfo;
            paramsStream >> syncInfo;

            emit syncAdded(syncInfo);
            break;
        }
        case SignalNum::SYNC_UPDATED: {
            SyncInfo syncInfo;
            paramsStream >> syncInfo;

            emit syncUpdated(syncInfo);
            break;
        }
        case SignalNum::SYNC_REMOVED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit syncRemoved(syncDbId);
            break;
        }
        case SignalNum::SYNC_PROGRESSINFO: {
            int syncDbId;
            SyncStatus status;
            SyncStep step;
            qint64 currentFile;
            qint64 totalFiles;
            qint64 completedSize;
            qint64 totalSize;
            qint64 estimatedRemainingTime;
            paramsStream >> syncDbId;
            paramsStream >> status;
            paramsStream >> step;
            paramsStream >> currentFile;
            paramsStream >> totalFiles;
            paramsStream >> completedSize;
            paramsStream >> totalSize;
            paramsStream >> estimatedRemainingTime;

            emit syncProgressInfo(syncDbId, status, step, currentFile, totalFiles, completedSize, totalSize,
                                  estimatedRemainingTime);
            break;
        }
        case SignalNum::SYNC_COMPLETEDITEM: {
            int syncDbId;
            SyncFileItemInfo itemInfo;
            paramsStream >> syncDbId;
            paramsStream >> itemInfo;

            emit itemCompleted(syncDbId, itemInfo);
            break;
        }
        case SignalNum::SYNC_VFS_CONVERSION_COMPLETED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit vfsConversionCompleted(syncDbId);
            break;
        }
        case SignalNum::SYNC_DELETE_FAILED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit syncDeletionFailed(syncDbId);
            break;
        }
        case SignalNum::NODE_FOLDER_SIZE_COMPLETED: {
            QString nodeId;
            qint64 size;
            paramsStream >> nodeId;
            paramsStream >> size;

            emit folderSizeCompleted(nodeId, size);
            break;
        }
        case SignalNum::NODE_FIX_CONFLICTED_FILES_COMPLETED: {
            int syncDbId = 0;
            QVariant var;
            paramsStream >> syncDbId;
            paramsStream >> var;

            uint64_t nbErrors = var.toULongLong();

            emit fixConflictingFilesCompleted(syncDbId, nbErrors);
            break;
        }
        case SignalNum::UTILITY_SHOW_NOTIFICATION: {
            QString title;
            QString message;
            paramsStream >> title;
            paramsStream >> message;

            emit showNotification(title, message);
            break;
        }
        case SignalNum::UTILITY_NEW_BIG_FOLDER: {
            int syncDbId;
            QString path;
            paramsStream >> syncDbId;
            paramsStream >> path;

            emit newBigFolder(syncDbId, path);
            break;
        }
        case SignalNum::UTILITY_ERROR_ADDED: {
            bool serverLevel;
            ExitCode exitCode;
            int syncDbId;
            paramsStream >> serverLevel;
            paramsStream >> exitCode;
            paramsStream >> syncDbId;

            emit errorAdded(serverLevel, exitCode, syncDbId);
            break;
        }
        case SignalNum::UTILITY_ERRORS_CLEARED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit errorsCleared(syncDbId);
            break;
        }
        case SignalNum::UPDATER_SHOW_DIALOG: {
            VersionInfo versionInfo;
            paramsStream >> versionInfo;
            emit showWindowsUpdateDialog(versionInfo);
            break;
        }
        case SignalNum::UPDATER_STATE_CHANGED: {
            auto state = UpdateState::Unknown;
            paramsStream >> state;
            emit updateStateChanged(state);
            break;
        }
        case SignalNum::UTILITY_SHOW_SETTINGS: {
            showParametersDialog();
            break;
        }
        case SignalNum::UTILITY_SHOW_SYNTHESIS: {
            QRect geometry;
            paramsStream >> geometry;
            showSynthesisDialog(geometry);
            break;
        }
        case SignalNum::UTILITY_LOG_UPLOAD_STATUS_UPDATED: {
            LogUploadState status;
            int progress; // Progress in percentage
            paramsStream >> status;
            paramsStream >> progress;
            emit logUploadStatusUpdated(status, progress);
            break;
        }
        case SignalNum::UTILITY_QUIT: {
            qCInfo(lcAppClient) << "Quit app client at the request of the app server";
            _quitInProcess = true;
            quit();
            break;
        }
        default: {
            qCDebug(lcAppClient) << "Signal not implemented!";
            break;
        }
    }
}

void AppClient::onLogTooBig() {
    auto logger = KDC::Logger::instance();
    qCDebug(lcAppClient()) << "Log too big, archiving current log and creating a new one.";
    logger->enterNextLogFile();
}

void AppClient::onQuit() {
    qCInfo(lcAppClient) << "Quit app client at the request of the user";
    _quitInProcess = true;
    if (CommClient::isConnected()) {
        QByteArray results;
        if (!CommClient::instance()->execute(RequestNum::UTILITY_QUIT, QByteArray(), results)) {
            // Do nothing
        }
    }

    quit();
}

void AppClient::onServerDisconnected() {
    if (!_quitInProcess) {
#if NDEBUG
        if (serverHasCrashed()) {
            qCCritical(lcAppClient) << "Server disconnected because it has crashed.";
            // Restart server and die
            startServerAndDie();
        } else {
            qCInfo(lcAppClient) << "Server disconnected because it was killed.";
            QTimer::singleShot(0, qApp, SLOT(quit()));
        }
#else
        qCInfo(lcAppClient) << "Server disconnected";
        QTimer::singleShot(0, qApp, SLOT(quit()));
#endif
    } else {
        qCInfo(lcAppClient) << "Server disconnected because the user quitted.";
    }
}

void AppClient::onCleanup() {
    _gui->onShutdown();
}

void AppClient::onCrash() {
    // SIGSEGV crash
    KDC::CommonUtility::crash();
}

void AppClient::onCrashServer() {
    if (const auto exitCode = GuiRequests::crash(); exitCode != ExitCode::Ok) {
        qCWarning(lcAppClient) << "Error in Requests::crash";
    }
}

void AppClient::onCrashEnforce() {
    ENFORCE(1 == 0);
}

void AppClient::onCrashFatal() {
    qFatal("la Qt fatale");
}

void AppClient::onWizardDone(int res) {
    if (res == QDialog::Accepted) {
        // If one account is configured: enable autostart
        ExitCode exitCode;
        QList<int> userDbIdList;
        exitCode = GuiRequests::getUserDbIdList(userDbIdList);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcAppClient) << "Error in Requests::getUserDbIdList";
        }

        bool shouldSetAutoStart = (userDbIdList.size() == 1);

#ifdef Q_OS_MAC
        // Don't auto start when not being 'installed'
        shouldSetAutoStart = shouldSetAutoStart && QCoreApplication::applicationDirPath().startsWith("/Applications/");
#endif
        if (shouldSetAutoStart) {
            ExitCode exitCode = GuiRequests::setLaunchOnStartup(true);
            if (exitCode != ExitCode::Ok) {
                qCWarning(lcAppClient()) << "Error in GuiRequests::setLaunchOnStartup";
            }
        }
    }
}

void AppClient::setupLogging() {
    // might be called from second instance
    auto logger = KDC::Logger::instance();
    logger->setIsCLientLog(true);
    logger->setLogFile(_logFile);
    logger->setLogDir(_logDir);
    logger->setLogExpire(_logExpire);
    logger->setLogFlush(_logFlush);
    logger->setLogDebug(_logDebug);
    logger->enterNextLogFile();

    logger->setMinLogLevel(toInt(ParametersCache::instance()->parametersInfo().logLevel()));

    if (ParametersCache::instance()->parametersInfo().useLog()) {
        // Don't override other configured logging
        if (logger->isLoggingToFile()) return;

        logger->setupTemporaryFolderLogDir();
        if (ParametersCache::instance()->parametersInfo().purgeOldLogs()) {
            logger->setLogExpire(std::chrono::hours(CommonUtility::logsPurgeRate * 24)); // C++20 offers std::chrono::day.
        } else {
            logger->setLogExpire(std::chrono::hours(0));
        }
        logger->enterNextLogFile();
    } else {
        logger->disableTemporaryFolderLogDir();
    }

    connect(logger, &KDC::Logger::logTooBig, this, &AppClient::onLogTooBig);
    connect(logger, &KDC::Logger::showNotification, this, &AppClient::showNotification);

    qCInfo(lcAppClient) << QString::fromLatin1("################## %1 locale:[%2] ui_lang:[%3] version:[%4] os:[%5]")
                                   .arg(QString::fromStdString(_theme->appClientName()))
                                   .arg(QLocale::system().name())
                                   .arg(property("ui_lang").toString())
                                   .arg(QString::fromStdString(_theme->version()))
                                   .arg(KDC::CommonUtility::platformName());
}

bool AppClient::serverHasCrashed() {
    // Check if a crash file exists
    const auto sigFilePath(CommonUtility::signalFilePath(AppType::Server, SignalCategory::Crash));
    std::error_code ec;
    return std::filesystem::exists(sigFilePath, ec);
}

void AppClient::startServerAndDie() {
    // Start the client
    QString pathToExecutable = QCoreApplication::applicationDirPath();

#if defined(Q_OS_WIN)
    pathToExecutable += QString("/%1.exe").arg(APPLICATION_EXECUTABLE);
#else
    pathToExecutable += QString("/%1").arg(APPLICATION_EXECUTABLE);
#endif

    auto serverProcess = new QProcess(this);
    QStringList arguments;
    arguments << QStringLiteral("--crashRecovered");
    serverProcess->setProgram(pathToExecutable);
    serverProcess->setArguments(arguments);
    serverProcess->setProgram(pathToExecutable);
    serverProcess->startDetached();

    QTimer::singleShot(0, qApp, SLOT(quit()));
}

bool AppClient::connectToServer() {
    // Check if a commPort is provided
    if (_commPort == 0) {
        // Should not happen (checked before)
        qCCritical(lcAppClient()) << "Comm port is not set";
        return false;
    }

    // Connect to server
    int count = 0;
    while (!CommClient::instance()->connectToServer(_commPort) && count < CONNECTION_TRIALS) {
        qCCritical(lcAppClient()) << "Connection to server failed!";
        count++;
    }

    if (count == CONNECTION_TRIALS) {
        qCCritical(lcAppClient()) << "Unable to connect to application server on port" << _commPort;
        return false;
    }

    connect(CommClient::instance().get(), &CommClient::disconnected, this, &AppClient::onServerDisconnected);
    connect(CommClient::instance().get(), &CommClient::signalReceived, this, &AppClient::onSignalReceived);

    // Wait for server startup
    count = 0;
    while (count < CHECKCOMMSTATUS_TRIALS) {
        const ExitCode exitCode = GuiRequests::checkCommStatus();
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcAppClient()) << "Check of comm status failed";
            count++;
        } else {
            break;
        }
    }

    if (count == CHECKCOMMSTATUS_TRIALS) {
        qCCritical(lcAppClient) << "The application server did not respond on time!";
        return false;
    }
    return true;
}

void AppClient::updateSentryUser() const {
    auto userInfo = _gui->userInfoMap().find(_gui->currentUserDbId());
    if (userInfo == _gui->userInfoMap().end()) {
        qCWarning(lcAppClient) << "No user found in updateSentryUser()";
        SentryUser user("No user logged", "No user logged", "No user logged");
        sentry::Handler::instance()->setAuthenticatedUser(user);
        return;
    }

    SentryUser user(userInfo->second.email().toStdString(), userInfo->second.name().toStdString(),
                    std::to_string(userInfo->second.userId()));
    sentry::Handler::instance()->setAuthenticatedUser(user);
}

void AppClient::onUseMonoIconsChanged(bool) {
    _gui->computeOverallSyncStatus();
}

bool AppClient::parseOptions(const QStringList &options) {
    if (options.size() != 2) {
        return false;
    }

    QStringListIterator it(options);

    // skip file name;
    it.next();

    // Read comm port
    _commPort = static_cast<unsigned short>(it.next().toUInt());

    return true;
}

bool AppClient::debugCrash() const {
    return _debugCrash;
}

void AppClient::showParametersDialog() {
    _gui->onShowParametersDialog();
}

void AppClient::updateSystrayIcon() {
    if (_theme && _theme->systrayUseMonoIcons()) {
        onUseMonoIconsChanged(_theme->systrayUseMonoIcons());
    }
}

void AppClient::askUserToLoginAgain(int userDbId, QString userEmail, bool invalidTokenError) {
    CustomMessageBox msgBox(QMessageBox::Information, tr("The user %1 is not connected. Please log in again.").arg(userEmail),
                            QMessageBox::Ok);
    msgBox.exec();

    _gui->openLoginDialog(userDbId, invalidTokenError);
}

void AppClient::onTryTrayAgain() {
    // qCInfo(lcAppClient) << "Trying tray icon, tray available:" << QSystemTrayIcon::isSystemTrayAvailable();
    _gui->hideAndShowTray();
}

} // namespace KDC
