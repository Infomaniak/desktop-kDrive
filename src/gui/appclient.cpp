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

#include "appclient.h"
#include "guiutility.h"
#include "config.h"
#include "libcommon/asserts.h"
#include "customproxystyle.h"
#include "config.h"
#include "guirequests.h"
#include "parameterscache.h"
#include "custommessagebox.h"
#include "libcommongui/logger.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"

#if defined(WITH_CRASHREPORTER)
#include <libcrashreporter-handler/Handler.h>
#endif

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

#define LITE_SYNC_EXT_BUNDLE_ID "com.infomaniak.drive.desktopclient.LiteSyncExt"

#define CONNECTION_TRIALS 3
#define CHECKCOMMSTATUS_TRIALS 5

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

#ifdef Q_OS_WIN
static void displayHelpText(const QString &t)  // No console on Windows.
{
    QString spaces(80, ' ');  // Add a line of non-wrapped space to make the messagebox wide enough.
    QString text = QLatin1String("<qt><pre style='white-space:pre-wrap'>") + t.toHtmlEscaped() + QLatin1String("</pre><pre>") +
                   spaces + QLatin1String("</pre></qt>");
    QMessageBox::information(0, Theme::instance()->appClientName() + "_HelpText", text);
}

#else

static void displayHelpText(const QString &t) {
    std::cout << qUtf8Printable(t);
}
#endif

AppClient::AppClient(int &argc, char **argv)
    : SharedTools::QtSingleApplication(Theme::instance()->appClientName(), argc, argv),
      _gui(nullptr),
      _theme(Theme::instance()),
      _logExpire(0),
      _logFlush(false),
      _logDebug(false),
      _debugMode(false) {
#ifdef NDEBUG
    sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_INFO, "AppClient", "Start"));
#endif

    _startedAt.start();

    setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));
    setApplicationName(_theme->appClientName());
    setWindowIcon(_theme->applicationIcon());
    setApplicationVersion(_theme->version());
    setStyle(new KDC::CustomProxyStyle);

    // Load fonts
    for (const QString &fontFile : fontFiles) {
        if (QFontDatabase::addApplicationFont(fontFile) < 0) {
            std::cout << "Error adding font file!" << std::endl;
        }
    }

/* #ifdef QT_DEBUG
    // Read comm port value from .comm file
    std::filesystem::path commPath(QStr2Path(QDir::homePath()));
    commPath.append(".comm");

    quint16 commPort;
    std::ifstream commFile(commPath);
    commFile >> commPort;
    commFile.close();
    _commPort = static_cast<unsigned short>(commPort);*/
//#else
    // Read comm port value from argument list
    if (!parseOptions(arguments())) {
        QMessageBox msgBox;
        msgBox.setText(tr("Bad arguments! Clean restart in progress."));
        msgBox.exec();
        startServer(true /*Close the client and let the server restart it*/);
        return;
    }
//#endif

    if (isRunning()) {
        QMessageBox msgBox;
        msgBox.setText(tr("kDrive client is already running!"));
        msgBox.exec();
        QTimer::singleShot(0, qApp, SLOT(quit()));
        return;
    }

    connectToServer();

    // Init ParametersCache
    ParametersCache::instance();

    // Setup logging
    setupLogging();

    // Set style
    KDC::GuiUtility::setStyle(qApp);

    CommonUtility::setupTranslations(QApplication::instance(), ParametersCache::instance()->parametersInfo().language());

#if defined(WITH_CRASHREPORTER)
    _crashHandler.reset(new CrashReporter::Handler(QDir::tempPath(), true, CRASHREPORTER_EXECUTABLE));
#endif

    _updaterClient.reset(new UpdaterClient);

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

    connect(this, &SharedTools::QtSingleApplication::messageReceived, this, &AppClient::onParseMessage);

    setQuitOnLastWindowClosed(false);

    // Setup translations
    CommonUtility::setupTranslations(this, ParametersCache::instance()->parametersInfo().language());

    // Setup Gui
    _gui = std::shared_ptr<ClientGui>(new ClientGui(this));
    _gui->init();

    _theme->setSystrayUseMonoIcons(ParametersCache::instance()->parametersInfo().monoIcons());
    connect(_theme, &Theme::systrayUseMonoIconsChanged, this, &AppClient::onUseMonoIconsChanged);

    // Cleanup at Quit
    connect(this, &QCoreApplication::aboutToQuit, this, &AppClient::onCleanup);

#ifdef Q_OS_WIN
    ExitCode exitCode =
        GuiRequests::setShowInExplorerNavigationPane(ParametersCache::instance()->parametersInfo().showShortcuts());
    if (exitCode != ExitCodeOk) {
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
        for (auto const &userInfoClient : _gui->userInfoMap()) {
            if (!userInfoClient.second.connected()) {
                askUserToLoginAgain(userInfoClient.second.dbId(), userInfoClient.second.email(), false);
            }
        }
    }
}

AppClient::~AppClient() {}

void AppClient::showSynthesisDialog() {
    _gui->showSynthesisDialog();
}

void AppClient::onSignalReceived(int id, /*SignalNum*/ int num, const QByteArray &params) {
    QDataStream paramsStream(params);

    qCDebug(lcAppClient) << "Sgnl rcvd" << id << num;

    switch (num) {
        case SIGNAL_NUM_USER_ADDED: {
            UserInfo userInfo;
            paramsStream >> userInfo;

            emit userAdded(userInfo);
            break;
        }
        case SIGNAL_NUM_USER_UPDATED: {
            UserInfo userInfo;
            paramsStream >> userInfo;

            emit userUpdated(userInfo);
            break;
        }
        case SIGNAL_NUM_USER_STATUSCHANGED: {
            int userDbId;
            bool connected;
            QString connexionError;
            paramsStream >> userDbId;
            paramsStream >> connected;
            paramsStream >> connexionError;

            emit userStatusChanged(userDbId, connected, connexionError);
            break;
        }
        case SIGNAL_NUM_USER_REMOVED: {
            int userDbId;
            paramsStream >> userDbId;

            emit userRemoved(userDbId);
            break;
        }
        case SIGNAL_NUM_ACCOUNT_ADDED: {
            AccountInfo accountInfo;
            paramsStream >> accountInfo;

            emit accountAdded(accountInfo);
            break;
        }
        case SIGNAL_NUM_ACCOUNT_UPDATED: {
            AccountInfo accountInfo;
            paramsStream >> accountInfo;

            emit accountUpdated(accountInfo);
            break;
        }
        case SIGNAL_NUM_ACCOUNT_REMOVED: {
            int accountDbId;
            paramsStream >> accountDbId;

            emit accountRemoved(accountDbId);
            break;
        }
        case SIGNAL_NUM_DRIVE_ADDED: {
            DriveInfo driveInfo;
            paramsStream >> driveInfo;

            emit driveAdded(driveInfo);
            break;
        }
        case SIGNAL_NUM_DRIVE_UPDATED: {
            DriveInfo driveInfo;
            paramsStream >> driveInfo;

            emit driveUpdated(driveInfo);
            break;
        }
        case SIGNAL_NUM_DRIVE_QUOTAUPDATED: {
            int driveDbId;
            qint64 total;
            qint64 used;
            paramsStream >> driveDbId;
            paramsStream >> total;
            paramsStream >> used;

            emit driveQuotaUpdated(driveDbId, total, used);
            break;
        }
        case SIGNAL_NUM_DRIVE_REMOVED: {
            int driveDbId;
            paramsStream >> driveDbId;

            emit driveRemoved(driveDbId);
            break;
        }
        case SIGNAL_NUM_DRIVE_DELETE_FAILED: {
            int driveDbId;
            paramsStream >> driveDbId;

            emit driveDeletionFailed(driveDbId);
            break;
        }
        case SIGNAL_NUM_SYNC_ADDED: {
            SyncInfo syncInfo;
            paramsStream >> syncInfo;

            emit syncAdded(syncInfo);
            break;
        }
        case SIGNAL_NUM_SYNC_UPDATED: {
            SyncInfo syncInfo;
            paramsStream >> syncInfo;

            emit syncUpdated(syncInfo);
            break;
        }
        case SIGNAL_NUM_SYNC_REMOVED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit syncRemoved(syncDbId);
            break;
        }
        case SIGNAL_NUM_SYNC_PROGRESSINFO: {
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
        case SIGNAL_NUM_SYNC_COMPLETEDITEM: {
            int syncDbId;
            SyncFileItemInfo itemInfo;
            paramsStream >> syncDbId;
            paramsStream >> itemInfo;

            emit itemCompleted(syncDbId, itemInfo);
            break;
        }
        case SIGNAL_NUM_SYNC_VFS_CONVERSION_COMPLETED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit vfsConversionCompleted(syncDbId);
            break;
        }
        case SIGNAL_NUM_SYNC_DELETE_FAILED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit syncDeletionFailed(syncDbId);
            break;
        }
        case SIGNAL_NUM_NODE_FOLDER_SIZE_COMPLETED: {
            QString nodeId;
            qint64 size;
            paramsStream >> nodeId;
            paramsStream >> size;

            emit folderSizeCompleted(nodeId, size);
            break;
        }
        case SIGNAL_NUM_NODE_FIX_CONFLICTED_FILES_COMPLETED: {
            int syncDbId = 0;
            QVariant var;
            paramsStream >> syncDbId;
            paramsStream >> var;

            uint64_t nbErrors = var.toULongLong();

            emit fixConflictingFilesCompleted(syncDbId, nbErrors);
            break;
        }
        case SIGNAL_NUM_UTILITY_SHOW_NOTIFICATION: {
            QString title;
            QString message;
            paramsStream >> title;
            paramsStream >> message;

            emit showNotification(title, message);
            break;
        }
        case SIGNAL_NUM_UTILITY_NEW_BIG_FOLDER: {
            int syncDbId;
            QString path;
            paramsStream >> syncDbId;
            paramsStream >> path;

            emit newBigFolder(syncDbId, path);
            break;
        }
        case SIGNAL_NUM_UTILITY_ERROR_ADDED: {
            bool serverLevel;
            ExitCode exitCode;
            int syncDbId;
            paramsStream >> serverLevel;
            paramsStream >> exitCode;
            paramsStream >> syncDbId;

            emit errorAdded(serverLevel, exitCode, syncDbId);
            break;
        }
        case SIGNAL_NUM_UTILITY_ERRORS_CLEARED: {
            int syncDbId;
            paramsStream >> syncDbId;

            emit errorsCleared(syncDbId);
            break;
        }
        case SIGNAL_NUM_UPDATER_SHOW_DIALOG: {
            QString targetVersion;
            QString targetVersionString;
            QString clientVersion;
            paramsStream >> targetVersion;
            paramsStream >> targetVersionString;
            paramsStream >> clientVersion;

            UpdaterClient::instance()->showWindowsUpdaterDialog(targetVersion, targetVersionString, clientVersion);
            break;
        }
        case SIGNAL_NUM_UTILITY_SHOW_SETTINGS: {
            emit showParametersDialog();
            break;
        }
        case SIGNAL_NUM_UTILITY_SHOW_SYNTHESIS: {
            emit showSynthesisDialog();
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
    if (CommClient::isConnected()) {
        QByteArray results;
        if (!CommClient::instance()->execute(REQUEST_NUM_UTILITY_QUIT, QByteArray(), results)) {
            // Do nothing
        }
    }

    quit();
}

void AppClient::onCleanup() {
    _gui->onShutdown();
}

void AppClient::onCrash() {
    KDC::CommonUtility::crash();
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
        if (exitCode != ExitCodeOk) {
            qCWarning(lcAppClient) << "Error in Requests::getUserDbIdList";
        }

        bool shouldSetAutoStart = (userDbIdList.size() == 1);

#ifdef Q_OS_MAC
        // Don't auto start when not being 'installed'
        shouldSetAutoStart = shouldSetAutoStart && QCoreApplication::applicationDirPath().startsWith("/Applications/");
#endif
        if (shouldSetAutoStart) {
            ExitCode exitCode = GuiRequests::setLaunchOnStartup(true);
            if (exitCode != ExitCodeOk) {
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

    logger->setMinLogLevel(ParametersCache::instance()->parametersInfo().logLevel());

    if (ParametersCache::instance()->parametersInfo().useLog()) {
        // Don't override other configured logging
        if (logger->isLoggingToFile()) return;

        logger->setupTemporaryFolderLogDir();
        if (ParametersCache::instance()->parametersInfo().purgeOldLogs()) {
            logger->setLogExpire(std::chrono::hours(ClientGui::logsPurgeRate * 24));  // C++20 offers std::chrono::day.
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
                               .arg(_theme->appClientName())
                               .arg(QLocale::system().name())
                               .arg(property("ui_lang").toString())
                               .arg(_theme->version())
                               .arg(KDC::CommonUtility::platformName());
}

void AppClient::onUseMonoIconsChanged(bool) {
    _gui->computeOverallSyncStatus();
}

void AppClient::onParseMessage(const QString &msg, QObject *) {
    if (msg.startsWith(QLatin1String("MSG_SHOWSETTINGS"))) {
        qCInfo(lcAppClient) << "Running for" << _startedAt.elapsed() / 1000.0 << "sec";
        if (_startedAt.elapsed() < 10 * 1000) {
            // This call is mirrored with the one in int main()
            qCWarning(lcAppClient)
                << "Ignoring MSG_SHOWSETTINGS, possibly double-invocation of client via session restore and auto start";
            return;
        }
        showParametersDialog();
    } else if (msg.startsWith(QLatin1String("MSG_SHOWSYNTHESIS"))) {
        qCInfo(lcAppClient) << "Running for" << _startedAt.elapsed() / 1000.0 << "sec";
        if (_startedAt.elapsed() < 10 * 1000) {
            // This call is mirrored with the one in int main()
            qCWarning(lcAppClient)
                << "Ignoring MSG_SHOWSETTINGS, possibly double-invocation of client via session restore and auto start";
            return;
        }
        showSynthesisDialog();
    }
}

bool AppClient::parseOptions(const QStringList &options) {
    if (options.size() != 2) {
        _commPort = 0;
        return false;
    }

    QStringListIterator it(options);

    // skip file name;
    it.next();

    // Read comm port
    _commPort = it.next().toUInt();
    displayHelpText(tr("Comm port for client: %1").arg(_commPort)); //Temporary debug, to remove before pull request

    return true;
}

bool AppClient::debugMode() {
    return _debugMode;
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

bool AppClient::startServer(bool restartClient) {
    // Start the client
    QString pathToExecutable = QCoreApplication::applicationDirPath();

#if defined(Q_OS_WIN)
    pathToExecutable += QString("/%1.exe").arg(APPLICATION_EXECUTABLE);
#else
    pathToExecutable += QString("/%1").arg(APPLICATION_EXECUTABLE);
#endif

    QStringList arguments;
    if (!restartClient) {
        arguments << QString("--commPort") << QString::number(_commPort) << QString("--noStartClient");
    }

    QProcess *clientProcess = new QProcess(this);
    clientProcess->setProgram(pathToExecutable);
    clientProcess->setArguments(arguments);
    
    clientProcess->startDetached();

    if (restartClient) {
        QTimer::singleShot(0, qApp, SLOT(quit()));
        return true;
    } 
    return true;
}

bool AppClient::connectToServer() {

    // Check if a commPort is provided
    if (_commPort == 0) {
		QMessageBox msgBox;
		msgBox.setText(tr("No comm port provided! Clean restart in progress..."));
		msgBox.exec();
        displayHelpText(tr("No comm port provided! Clean restart in progress..."));
        startServer(true /*Close the client and let the server restart it*/);
		return false;
	}

    // Connect to server
    int count = 0;
    while (!CommClient::instance()->connectToServer(_commPort) && count < CONNECTION_TRIALS) {
        std::cout << "Connection to server failed!" << std::endl;
        count++;
    }

    if (count == CONNECTION_TRIALS) {
        QMessageBox msgBox;
        msgBox.setText(tr("Unable to connect to application server!"));
        msgBox.exec();
        //startServer(true /*Close the client and let the server restart it*/);
        QTimer::singleShot(0, qApp, SLOT(quit())); //Will be replace by startServer when the mecanisme to prevent loop is implemented
        return false;
    }
    disconnect(CommClient::instance().get(), &CommClient::disconnected, this, &AppClient::onQuit);
    disconnect(CommClient::instance().get(), &CommClient::signalReceived, this, &AppClient::onSignalReceived);

    connect(CommClient::instance().get(), &CommClient::disconnected, this, &AppClient::onQuit);
    connect(CommClient::instance().get(), &CommClient::signalReceived, this, &AppClient::onSignalReceived);

    // Wait for server startup
    count = 0;
    while (count < CHECKCOMMSTATUS_TRIALS) {
        ExitCode exitCode = GuiRequests::checkCommStatus();
        if (exitCode != ExitCodeOk) {
            std::cout << "Check of comm status failed" << std::endl;
            count++;
        } else {
            break;
        }
    }

    if (count == CHECKCOMMSTATUS_TRIALS) {
        QMessageBox msgBox;
        msgBox.setText(tr("The application server did not respond on time!"));
        msgBox.exec();
        //startServer(true /*Close the client and let the server restart it*/);
        QTimer::singleShot(0, qApp, SLOT(quit())); //Will be replace by startServer when the mecanisme to prevent loop is implemented
        return false;
    }
    return true;
}

void AppClient::onTryTrayAgain() {
    qCInfo(lcAppClient) << "Trying tray icon, tray available:" << QSystemTrayIcon::isSystemTrayAvailable();
    _gui->hideAndShowTray();
}

}  // namespace KDC
