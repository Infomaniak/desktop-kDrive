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

#if defined(KD_WINDOWS)
#define _WINSOCKAPI_
#endif

#if defined(KD_MACOS)
#include <Security/Security.h>
#elif defined(KD_LINUX)
#include <libsecret/secret.h>
#include <Poco/Base64Decoder.h>
#elif defined(KD_WINDOWS)
#include <memory>
#include <windows.h>
#include <wincred.h>
#endif

#include "migrationparams.h"
#include "config.h"
#include "keychainmanager/keychainmanager.h"
#include "common/utility.h"
#include "log/log.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/requests/parameterscache.h"
#include "jobs/network/login/gettokenfromapppasswordjob.h"

#include <log4cplus/loggingmacros.h>

#include <QDir>
#include <QStandardPaths>
#include <QStringLiteral>
#include <QFileInfo>

namespace KDC {

static const char accountsC[] = "Accounts";
static const char userIdC[] = "dav_user";
static const char urlC[] = "url";
static const char localPathC[] = "localPath";
static const char targetPathC[] = "targetPath";
static const char journalPathC[] = "journalPath";
static const char pausedC[] = "paused";
static const char notificationsDisabledC[] = "notificationsDisabled";
static const char virtualFilesModeC[] = "virtualFilesMode";
static const char emailC[] = "webflow_user";

static const char oldKeychainKeySeparator[] = ":";
static const char app_password[] = "_app-password";

static const char clientVersionC[] = "clientVersion";
static const char darkThemeC[] = "darkTheme";
static const char languageC[] = "language";
static const char deleteOldLogsAfterHoursC[] = "temporaryLogDirDeleteOldLogsAfterHours";
static const char automaticLogDirC[] = "logToTemporaryLogDir";
static const char minLogLevelC[] = "minLogLevel";
static const char moveToTrashC[] = "moveToTrash";
static const char monoIconsC[] = "monoIcons";
static const char newBigFolderSizeLimitC[] = "newBigFolderSizeLimit";
static const char useNewBigFolderSizeLimitC[] = "useNewBigFolderSizeLimit";
static const int defaultBigFolderSizeLimit = 500;
static const int defaultMinLogLevel = 1;
static const char navigationPaneClsidC[] = "navigationPaneClsid";

static const char noWarningIndicator = ']';

static const char proxyHostC[] = "Proxy/host";
static const char proxyTypeC[] = "Proxy/type";
static const char proxyPortC[] = "Proxy/port";
static const char proxyUserC[] = "Proxy/user";
static const char proxyPassC[] = "Proxy/pass";
static const char proxyNeedsAuthC[] = "Proxy/needsAuth";

static const char geometryC[] = "geometry";
static const std::string geometryDialog[] = {"FileExclusionDialog", "LiteSyncDialog",    "ParametersDialog",
                                             "DebuggingDialog",     "ProxyServerDialog", "Bandwidthdialog"};

static const QString excludedTemplatesFileName("sync-exclude.lst");
static const QString excludedAppsFileName("litesync-exclude.lst");


MigrationParams::MigrationParams() :
    _logger(Log::instance()->getLogger()),
    _proxyNotSupported(false) {}

Language MigrationParams::strToLanguage(QString lang) {
    if (lang == "en") {
        return Language::English;
    } else if (lang == "fr") {
        return Language::French;
    } else if (lang == "de") {
        return Language::German;
    } else if (lang == "es") {
        return Language::Spanish;
    } else if (lang == "it") {
        return Language::Italian;
    } else {
        return Language::Default;
    }
}

LogLevel MigrationParams::intToLogLevel(int log) {
    switch (log) {
        case 0:
            return LogLevel::Debug;
        case 1:
            return LogLevel::Info;
        case 2:
            return LogLevel::Warning;
        case 3:
            return LogLevel::Error;
        case 4:
            return LogLevel::Fatal;
        default:
            return LogLevel::Info;
    }
}

VirtualFileMode MigrationParams::modeFromString(const QString &str) {
    if (str == "suffix") {
        return VirtualFileMode::Suffix;
    } else if (str == "wincfapi") {
        return VirtualFileMode::Win;
    } else if (str == "mac") {
        return VirtualFileMode::Mac;
    } else {
        return VirtualFileMode::Off;
    }
}

ProxyType intToProxyType(int pTypeInt) {
    switch (pTypeInt) {
        case 0:
            return ProxyType::System;
        case 1:
            return ProxyType::Socks5;
        case 3:
            return ProxyType::HTTP;
        default:
            return ProxyType::None;
    }
}

QDir MigrationParams::configDir() {
    return QStandardPaths::writableLocation(OldUtility::isWindows() ? QStandardPaths::AppDataLocation
                                                                    : QStandardPaths::AppConfigLocation);
}

QString MigrationParams::configFileName() {
    return QStringLiteral(APPLICATION_NAME ".cfg");
}

int MigrationParams::extractDriveIdFromUrl(std::string url) {
    if (url.empty()) {
        return 0;
    }
    std::size_t pos = url.find("//");
    url.erase(0, pos + 2);
    pos = url.find(".");
    url.erase(pos);
    return std::stoi(url);
}

QString MigrationParams::excludeTemplatesFileName() const {
    QFileInfo fi;

    fi.setFile(configDir(), excludedTemplatesFileName);

    return fi.isReadable() && fi.isFile() ? fi.absoluteFilePath() : QString();
}

QString MigrationParams::excludeAppsFileName() const {
    QFileInfo fi;

    fi.setFile(configDir(), excludedAppsFileName);

    return fi.isReadable() && fi.isFile() ? fi.absoluteFilePath() : QString();
}

ExitCode MigrationParams::migrateGeneralParams() {
    LOG_INFO(Log::instance()->getLogger(), "Migrate general params");

    QSettings settings(configDir().filePath(configFileName()), QSettings::IniFormat);

    bool darkTheme = settings.value(QString(darkThemeC), false).toBool();
    bool moveToTrash = settings.value(QString(moveToTrashC), true).toBool();
    bool monoIcons = settings.value(QString(monoIconsC), false).toBool();
    int minLogLevel = settings.value(QString(minLogLevelC), defaultMinLogLevel).toInt();
    long newBigFolderSizeLimit = settings.value(QString(newBigFolderSizeLimitC), defaultBigFolderSizeLimit).toLongLong();
    QString clientVersion = settings.value(QString(clientVersionC), QString()).toString();
    QString language = settings.value(QString(languageC), QString()).toString();
    QString deleteOldLogsAfterHours = settings.value(QString(deleteOldLogsAfterHoursC), QString()).toString();
    bool automaticLogDir = settings.value(QString(automaticLogDirC), true).toBool();
    bool useNewBigFolderSizeLimit = settings.value(QString(useNewBigFolderSizeLimitC), true).toBool();

    // Log level Info and Debug are to be switched
    LogLevel logLevel = intToLogLevel(minLogLevel);
    if (logLevel == LogLevel::Info) {
        logLevel = LogLevel::Debug;
    } else if (logLevel == LogLevel::Debug) {
        logLevel = LogLevel::Info;
    }

    ParametersCache::instance()->parameters().setDarkTheme(darkTheme);
    ParametersCache::instance()->parameters().setMoveToTrash(moveToTrash);
    ParametersCache::instance()->parameters().setMonoIcons(monoIcons);
    ParametersCache::instance()->parameters().setUseLog(automaticLogDir);
    ParametersCache::instance()->parameters().setLogLevel(logLevel);
    ParametersCache::instance()->parameters().setUseBigFolderSizeLimit(useNewBigFolderSizeLimit);
    ParametersCache::instance()->parameters().setBigFolderSizeLimit(newBigFolderSizeLimit);
    ParametersCache::instance()->parameters().setLanguage(strToLanguage(language));
    bool purgeOldLogs = false;
    if (deleteOldLogsAfterHours == QString() || deleteOldLogsAfterHours != "-1") {
        purgeOldLogs = true;
    }
    ParametersCache::instance()->parameters().setPurgeOldLogs(purgeOldLogs);

    std::shared_ptr<std::vector<char>> geometry;
    migrateGeometry(geometry);
    ParametersCache::instance()->parameters().setDialogGeometry(geometry);

    ProxyConfig proxyConfig;
    ExitCode exitCode = migrateProxySettings(proxyConfig);
    if (exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in MigrationParams::migrateProxySettings");
        return exitCode;
    }
    ParametersCache::instance()->parameters().setProxyConfig(proxyConfig);
    ParametersCache::instance()->save();

    return ExitCode::Ok;
}

ExitCode MigrationParams::migrateAccountsParams() {
    LOG_INFO(Log::instance()->getLogger(), "Migrate accounts params");

    ExitCode code = ExitCode::Ok;
    QSettings settings(configDir().filePath(configFileName()), QSettings::IniFormat);
    settings.beginGroup(accountsC);
    for (const auto &accountIdStr: settings.childGroups()) {
        LOG_INFO(Log::instance()->getLogger(), "Migrate account " << accountIdStr.toStdString());
        settings.beginGroup(accountIdStr);
        code = loadAccount(settings);
        if (code != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in loadAccount : " << code);
            return code;
        }
        settings.endGroup();
    }
    return code;
}

ExitCode MigrationParams::loadAccount(QSettings &settings) {
    bool found;
    User user;
    int userId = settings.value(QString(userIdC)).toInt();

    // User
    if (!ParmsDb::instance()->selectUserByUserId(userId, user, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectUserByUserId");
        return ExitCode::DbError;
    }

    if (!found) {
        int userDbId;
        if (!ParmsDb::instance()->getNewUserDbId(userDbId)) {
            LOG_WARN(_logger, "Error in ParmsDb::getNewUserDbId");
            return ExitCode::DbError;
        }
        user.setUserId(userId);
        user.setDbId(userDbId);
        user.setName("<" + std::to_string(userId) + ">");
        QString emailStr = settings.value(QString(emailC)).toString();
        user.setEmail(emailStr.toStdString());
        user.setToMigrate(true);
        if (!ParmsDb::instance()->insertUser(user)) {
            LOG_WARN(_logger, "Error in ParmsDb::insertUser");
            return ExitCode::DbError;
        }
    }

    Account account;
    int accountDbId;

    // Drive
    Drive drive;
    int driveDbId;
    QString strDriveUrl = settings.value(QString(urlC)).toString();
    int driveId = extractDriveIdFromUrl(strDriveUrl.toStdString());
    if (!ParmsDb::instance()->selectDriveByDriveId(driveId, drive, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectDriveByDriveId");
        return ExitCode::DbError;
    }

    if (!found) {
        // create account
        if (!ParmsDb::instance()->getNewAccountDbId(accountDbId)) {
            LOG_WARN(_logger, "Error in ParmsDb::getNewAccountDbId");
            return ExitCode::DbError;
        }
        account = Account(accountDbId, 0, user.dbId());
        if (!ParmsDb::instance()->insertAccount(account)) {
            LOG_WARN(_logger, "Error in ParmsDb::insertAccount");
            return ExitCode::DbError;
        }

        if (!ParmsDb::instance()->getNewDriveDbId(driveDbId)) {
            LOG_WARN(_logger, "Error in ParmsDb::getNewDriveDbId");
            return ExitCode::DbError;
        }
        std::string driveTmpName = "<" + std::to_string(driveId) + ">";
        drive = Drive(driveDbId, driveId, accountDbId, driveTmpName);
        if (!ParmsDb::instance()->insertDrive(drive)) {
            LOG_WARN(_logger, "Error in ParmsDb::insertDrive");
            return ExitCode::DbError;
        }
    } else {
        // get existing accound
        if (!ParmsDb::instance()->selectAccount(drive.accountDbId(), account, found)) {
            LOG_WARN(_logger, "Error in ParmsDb::insertAccount");
            return ExitCode::DbError;
        }
        if (!found) {
            LOG_WARN(_logger, "Account not found with accountId=" << drive.accountDbId());
            return ExitCode::DataError;
        }

        driveDbId = drive.dbId();
    }

    if (user.keychainKey().empty()) {
        std::string oldAccountStr = settings.group().toStdString();
        std::string oldAccountId = oldAccountStr.substr(oldAccountStr.size() - 1); // get last char
        std::string urlKey = strDriveUrl.toStdString()[static_cast<unsigned long>(strDriveUrl.size() - 1)] == '/'
                                     ? strDriveUrl.toStdString()
                                     : strDriveUrl.toStdString() + "/"; // add "/" to url if necessary

        std::string keychainKeyAppPassword;
#ifdef Q_OS_WIN
        keychainKeyAppPassword += APPLICATION_SHORTNAME;
        keychainKeyAppPassword += "_";
#endif
        keychainKeyAppPassword +=
                user.email() + app_password + oldKeychainKeySeparator + urlKey + oldKeychainKeySeparator + oldAccountId;


        LOG_DEBUG(_logger, "Old account id : " << oldAccountId << " old keychain app password : " << keychainKeyAppPassword);

        found = false;
        std::string appPassword;
        bool appPasswordReadError = false;
        if (getOldAppPwd(keychainKeyAppPassword, appPassword, found) != ExitCode::Ok) {
#ifdef Q_OS_WIN
            appPasswordReadError = true;
#else
            // Retry without :index
            keychainKeyAppPassword = user.email() + app_password + oldKeychainKeySeparator + urlKey;
            if (getOldAppPwd(keychainKeyAppPassword, appPassword, found) != ExitCode::Ok) {
                appPasswordReadError = true;
            }
#endif
        }

        if (appPasswordReadError) {
            LOG_WARN(_logger, "Error when trying to access keychain, user will be asked to connect again");
        } else {
            LOG_DEBUG(_logger, "Old app password has been read");
            if (found && setToken(user, appPassword) == ExitCode::Ok) {
                LOG_DEBUG(_logger, "New token set");
            } else {
                LOG_DEBUG(_logger, "Error when trying to set token, user will be asked to connect again");
            }
        }
    }

    // list used to analyse .cfg consistency
    std::vector<std::pair<bool, Sync>> syncConsistencyCheckList;
    VirtualFileMode masterVfs = VirtualFileMode::Off;

    // Sync
    bool hasMaster = false;
    foreach (const auto &syncType, settings.childGroups()) {
        // Get sync parameters
        settings.beginGroup(syncType);
        foreach (const auto &syncContent, settings.childGroups()) {
            settings.beginGroup(syncContent);

            // Create sync
            int syncDbId;
            if (!ParmsDb::instance()->getNewSyncDbId(syncDbId)) {
                LOG_WARN(_logger, "Error in ParmsDb::getNewSyncDbId");
                return ExitCode::DbError;
            }
            Sync sync;
            sync.setDbId(syncDbId);
            sync.setDriveDbId(driveDbId);

            // Fill sync with .cfg content
            QString localPathStr = settings.value(localPathC).toString();
            if (localPathStr.back() == '/') {
                localPathStr.chop(1);
            }
            SyncPath localPath = QStr2Path(localPathStr);
            sync.setLocalPath(localPath);

            SyncName syncDbName = QStr2SyncName(settings.value(QLatin1String(journalPathC)).toString());
            _syncToMigrate[syncDbId] = {localPath, syncDbName};

            QString targetPathStr = settings.value(QLatin1String(targetPathC)).toString();
            if (targetPathStr.back() == '/') {
                targetPathStr.chop(1);
            }
            SyncPath targetPath = QStr2Path(targetPathStr);
            sync.setTargetPath(targetPath);

            sync.setPaused(settings.value(QLatin1String(pausedC), false).toBool());
            sync.setNotificationsDisabled(settings.value(QString(notificationsDisabledC), false).toBool());
            QString vfsModeStr = settings.value(QString(virtualFilesModeC), "off").toString();

            // Check vfs support
            QString fsName(KDC::CommonUtility::fileSystemName(SyncName2QStr(localPath.native())));
            bool supportVfs = (fsName == "NTFS" || fsName == "apfs");
            sync.setSupportVfs(supportVfs);
            sync.setVirtualFileMode(modeFromString(vfsModeStr));
            sync.setNavigationPaneClsid(settings.value(QString(navigationPaneClsidC)).toString().toStdString());

            if (!ParmsDb::instance()->insertSync(sync)) {
                LOG_WARN(_logger, "Error in ParmsDb::insertSync");
                return ExitCode::DbError;
            }

            bool isMaster = (targetPath == targetPath.root_path());
            if (isMaster) {
                hasMaster = true;
                masterVfs = modeFromString(vfsModeStr);
            }
            syncConsistencyCheckList.push_back({isMaster, sync});

            settings.endGroup();
        }
        settings.endGroup();
    }

    // prevent corrupted .cfg
    if (hasMaster) {
        for (auto &syncCheckPair: syncConsistencyCheckList) {
            if (!syncCheckPair.first) {
                if (syncCheckPair.second.virtualFileMode() != masterVfs) {
                    // TODO : changeVfs for this sync in database
                    syncCheckPair.second.setVirtualFileMode(masterVfs);
                    bool found;
                    if (!ParmsDb::instance()->updateSync(syncCheckPair.second, found)) {
                        LOG_WARN(_logger, "Error in ParmsDb::updateSync");
                        return ExitCode::DbError;
                    }
                    if (!found) {
                        LOG_WARN(_logger, "Sync not found in ParmsDb :" << syncCheckPair.second.dbId());
                        return ExitCode::DataError;
                    }
                }
            }
        }
    }

    return ExitCode::Ok;
}

ExitCode MigrationParams::migrateTemplateExclusion() {
    LOG_INFO(Log::instance()->getLogger(), "Migrate template exclusion");

    QString excludeFilePath = excludeTemplatesFileName();
    if (excludeFilePath.isEmpty()) {
        return ExitCode::Ok;
    }

    QFile excludeFile(excludeFilePath);
    if (!excludeFile.open(QIODevice::ReadOnly)) {
        LOGW_WARN(_logger, L"Unable to open file " << QStr2WStr(excludeFilePath));
        return ExitCode::SystemError;
    }

    while (!excludeFile.atEnd()) {
        QString line = QString::fromUtf8(excludeFile.readLine());

        // Remove end of line
        if (line.size() > 0 && line[line.size() - 1] == '\n') {
            line.chop(1);
        }
        if (line.size() > 0 && line[line.size() - 1] == '\r') {
            line.chop(1);
        }

        bool warning = true;
        if (line.startsWith(noWarningIndicator)) {
            warning = false;
            line = line.mid(1);
        }

        ExclusionTemplate excltmp(line.toStdString(), warning);
        bool constraintError;
        if (!ParmsDb::instance()->insertExclusionTemplate(excltmp, constraintError)) {
            LOG_WARN(_logger, "Error in ParmsDb::insertExclusionTemplate");
            if (!constraintError) {
                return ExitCode::DbError;
            }
        }
    }

    excludeFile.close();

    return ExitCode::Ok;
}

#if defined(KD_MACOS)
ExitCode MigrationParams::migrateAppExclusion() {
    LOG_INFO(Log::instance()->getLogger(), "Migrate app exclusion");

    QString excludeFilePath = excludeAppsFileName();
    if (excludeFilePath.isEmpty()) {
        return ExitCode::Ok;
    }

    QFile excludeAppFile(excludeFilePath);

    if (!excludeAppFile.open(QIODevice::ReadOnly)) {
        LOGW_WARN(_logger, L"Unable to open file " << QStr2WStr(excludeFilePath));
        return ExitCode::SystemError;
    }

    while (!excludeAppFile.atEnd()) {
        QString line = QString::fromUtf8(excludeAppFile.readLine());

        // Remove end of line
        if (line.size() > 0 && line[line.size() - 1] == '\n') {
            line.chop(1);
        }
        if (line.size() > 0 && line[line.size() - 1] == '\r') {
            line.chop(1);
        }

        QList<QString> appExcl = line.split(";", Qt::SkipEmptyParts);

        ExclusionApp exclApp(appExcl[0].toStdString(), appExcl[1].toStdString());
        bool constraintError;
        if (!ParmsDb::instance()->insertExclusionApp(exclApp, constraintError)) {
            LOG_WARN(_logger, "Error in ParmsDb::insertExclusionApp");
            if (!constraintError) {
                return ExitCode::DbError;
            }
        }
    }

    excludeAppFile.close();

    return ExitCode::Ok;
}
#endif

void MigrationParams::migrateGeometry(std::shared_ptr<std::vector<char>> &geometry) {
    std::string geometryStr;
    QSettings settings(configDir().filePath(configFileName()), QSettings::IniFormat);

    for (auto dialog: geometryDialog) {
        settings.beginGroup(QString::fromStdString(dialog));
        QByteArray dialogGeo = settings.value(geometryC).toByteArray();

        if (!dialogGeo.isEmpty()) {
            geometryStr.append(dialog + ";" + dialogGeo.toStdString() + "\n");
        }
        settings.endGroup();
    }
    geometry = std::shared_ptr<std::vector<char>>(new std::vector<char>(geometryStr.begin(), geometryStr.end()));
}

ExitCode MigrationParams::migrateProxySettings(ProxyConfig &proxyConfig) {
    QSettings settings(configDir().filePath(configFileName()), QSettings::IniFormat);

    QString host = settings.value(QString(proxyHostC)).toString();
    bool needsAuth = settings.value(QString(proxyNeedsAuthC)).toBool();
    int port = settings.value(QString(proxyPortC)).toInt();
    int pTypeInt = settings.value(QString(proxyTypeC)).toInt();
    ProxyType pType = intToProxyType(pTypeInt);

    // SOCKS5 is not supported
    if (pType == ProxyType::Socks5) {
        pType = ProxyType::System;
        setProxyNotSupported(true);
    }
    proxyConfig = ProxyConfig(pType, host.toStdString(), port, needsAuth);

    if (needsAuth) {
        QString user = settings.value(QString(proxyUserC)).toString();

        QByteArray passByteArray = settings.value(QString(proxyPassC)).toByteArray();
        std::string pwd(QString::fromUtf8(QByteArray::fromBase64(passByteArray)).toStdString());
        proxyConfig.setUser(user.toStdString());

        std::string keychainKeyProxyPass(Utility::computeMd5Hash(std::to_string(std::time(nullptr))));
        if (!KeyChainManager::instance()->writeToken(keychainKeyProxyPass, pwd)) {
            LOG_WARN(_logger, "Failed to write password token into keychain");
            return ExitCode::SystemError;
        }
        proxyConfig.setToken(keychainKeyProxyPass);
    }
    return ExitCode::Ok;
}

ExitCode MigrationParams::migrateSelectiveSyncs() {
    LOG_INFO(Log::instance()->getLogger(), "Migrate selective syncs");

    ExitCode ret = ExitCode::Ok;
    for (auto &syncToMigrateElt: _syncToMigrate) {
        ExitCode code = ServerRequests::migrateSelectiveSync(syncToMigrateElt.first, syncToMigrateElt.second);
        if (code != ExitCode::Ok) {
            ret = code;
        }
    }
    return ret;
}

void MigrationParams::deleteUselessConfigFiles() {
    // Remove old configuration files
    QDir cfgDir(configDir().absolutePath());
    QStringList filters;
    filters << "kDrive.cfg*";
    filters << "cookies*.db";
    filters << "sync-exclude.lst";
    filters << "litesync-exclude.lst";
    QStringList cfgFileNameList = cfgDir.entryList(filters, QDir::Files);
    for (const QString &cfgFileName: cfgFileNameList) {
        QFileInfo cfgFileInfo(cfgDir, cfgFileName);
        QFile cfgFile(cfgFileInfo.absoluteFilePath());
        cfgFile.remove();
    }

    // Remove old sync DB files
    for (auto sync: _syncToMigrate) {
        QDir dbDir(SyncName2QStr(sync.second.first.native()));
        QStringList filters;
        filters << SyncName2QStr(sync.second.second);
        filters << SyncName2QStr(sync.second.second) + "-shm";
        filters << SyncName2QStr(sync.second.second) + "-wal";
        filters << ".owncloudsync.log"; // This is a now unused log file from older kDrive versions, we keep this filter here to
                                        // avoid people with older config to synchronize it
        QStringList dbFileNameList = dbDir.entryList(filters, QDir::Files | QDir::Hidden);
        for (const QString &dbFileName: dbFileNameList) {
            QFileInfo dbFileInfo(dbDir, dbFileName);
            QFile dbFile(dbFileInfo.absoluteFilePath());
            dbFile.remove();
        }
    }
}

ExitCode MigrationParams::getOldAppPwd(const std::string &keychainKey, std::string &appPassword, bool &found) {
#if defined(KD_MACOS)
    const std::string serviceName("kDrive");
    void *data;
    uint32 length;

    OSStatus status = SecKeychainFindGenericPassword(nullptr, static_cast<uint32>(serviceName.length()), serviceName.c_str(),
                                                     static_cast<uint32>(keychainKey.length()), keychainKey.c_str(), &length,
                                                     &data, nullptr);

    if (status == errSecNoSuchKeychain) {
        LOG_DEBUG(_logger, "Application password not found");
        found = false;
    } else if (status != errSecSuccess) {
        LOG_WARN(_logger, "Unable to read application password");
        return ExitCode::SystemError;
    } else if (data != nullptr) {
        LOG_DEBUG(_logger, "Application password found");
        appPassword = std::string(reinterpret_cast<const char *>(data), length);
        found = true;
        SecKeychainItemFreeContent(nullptr, data);
    }
#elif defined(KD_LINUX)
    const std::string package = "kDrive";

    const char *userFieldName = "user";
    const char *serverFieldName = "server";
    const char *typeFieldName = "type";

    const std::string server("kDrive");
    const std::string type("base64");

    const auto schema = SecretSchema{package.c_str(),
                                     SECRET_SCHEMA_DONT_MATCH_NAME,
                                     {
                                             {userFieldName, SECRET_SCHEMA_ATTRIBUTE_STRING},
                                             {serverFieldName, SECRET_SCHEMA_ATTRIBUTE_STRING},
                                             {typeFieldName, SECRET_SCHEMA_ATTRIBUTE_STRING},
                                             {nullptr, SecretSchemaAttributeType(0)},
                                     },
                                     0,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr};

    GError *error = nullptr;

    gchar *raw_passwords = secret_password_lookup_sync(&schema,
                                                       nullptr, // not cancellable
                                                       &error, userFieldName, keychainKey.c_str(), serverFieldName,
                                                       server.c_str(), typeFieldName, type.c_str(), nullptr);

    if (error != nullptr) {
        LOG_WARN(_logger, "Unable to read application password");
        return ExitCode::SystemError;
    } else if (raw_passwords == nullptr) {
        LOG_DEBUG(_logger, "Application password not found");
        found = false;
    } else {
        LOG_DEBUG(_logger, "Application password found");
        std::istringstream ss(raw_passwords);
        Poco::Base64Decoder decoder(ss);
        decoder >> appPassword;
        found = true;
        secret_password_free(raw_passwords);
    }
#elif defined(KD_WINDOWS)
    CREDENTIAL *cred;
    bool result = ::CredRead(Utility::s2ws(keychainKey).c_str(), CRED_TYPE_GENERIC, 0, &cred);

    if (result == TRUE) {
        LOGW_DEBUG(_logger, L"Application password found");
        appPassword = std::string(reinterpret_cast<char *>(cred->CredentialBlob), cred->CredentialBlobSize);
        found = true;
        ::CredFree(cred);
    } else {
        const auto code = ::GetLastError();
        if (code == ERROR_NOT_FOUND) {
            LOG_DEBUG(_logger, "Application password not found");
            found = false;
        } else {
            LOG_WARN(_logger, "Unable to read application password");
            return ExitCode::SystemError;
        }
    }
#endif

    return ExitCode::Ok;
}

ExitCode MigrationParams::getTokenFromAppPassword(const std::string &email, const std::string &appPassword, ApiToken &apiToken) {
    try {
        std::string errorDescr, errorCode;
        GetTokenFromAppPasswordJob job(email, appPassword);
        const ExitCode exitCode = job.runSynchronously();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in GetTokenFromAppPasswordJob::runSynchronously: code=" << exitCode);
            errorCode = std::string();
            errorDescr = std::string();
            return exitCode;
        }

        LOG_DEBUG(_logger, "job.runSynchronously() done");
        if (job.hasErrorApi(&errorCode, &errorDescr)) {
            LOGW_WARN(_logger, L"Failed to retrieve authentification token. code=" << KDC::Utility::s2ws(errorCode) << L" descr="
                                                                                   << KDC::Utility::s2ws(errorDescr));
            return ExitCode::BackError;
        }

        LOG_DEBUG(_logger, "job.hasErrorApi done");
        apiToken = job.apiToken();
    } catch (const std::runtime_error &e) {
        LOG_WARN(_logger, "Error in GetTokenFromAppPasswordJob::GetTokenFromAppPasswordJob: error=" << e.what());
        return ExitCode::SystemError;
    }

    return ExitCode::Ok;
}

ExitCode MigrationParams::setToken(User &user, const std::string &appPassword) {
    bool found;
    ApiToken apiToken;
    ExitCode tokenGetExitCode = getTokenFromAppPassword(user.email(), appPassword, apiToken);
    if (tokenGetExitCode == ExitCode::Ok) {
        std::string keychainKey(Utility::computeMd5Hash(std::to_string(std::time(nullptr))));
        if (KeyChainManager::instance()->writeToken(keychainKey, apiToken.rawData())) {
            // Update userInfo
            user.setKeychainKey(keychainKey);
            found = false;
            if (!ParmsDb::instance()->updateUser(user, found)) {
                LOG_WARN(_logger, "Error in ParmsDb::insertUser");
                return ExitCode::DbError;
            }
            if (!found) {
                LOG_WARN(_logger, "Error user with dbId " << user.dbId() << " does not exist");
                return ExitCode::DataError;
            }
        } else {
            LOG_WARN(_logger, "Failed to write token into keychain");
            return ExitCode::SystemError;
        }
    } else {
        LOG_WARN(_logger, "Can't get API Token from old application password, user will be asked to connect again");
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}


} // namespace KDC
