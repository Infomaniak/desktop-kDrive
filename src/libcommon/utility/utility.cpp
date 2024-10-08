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

#include "utility.h"
#include "libcommon/log/sentry/sentryhandler.h"
#include "config.h"
#include "version.h"

#include <system_error>
#include <sys/types.h>

#ifdef Q_OS_UNIX
#include <sys/statvfs.h>
#endif

#include <random>
#include <fstream>
#include <sstream>
#include <signal.h>

#ifdef _WIN32
#include <Poco/Util/WinRegistryKey.h>
#endif

#include <sentry.h>

#ifdef ZLIB_FOUND
#include <zlib.h>
#endif

#include <QDir>
#include <QFileInfo>
#include <QTranslator>
#include <QLibraryInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStorageInfo>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QOperatingSystemVersion>

#if defined(Q_OS_WIN)
#include "utility_win.cpp"
#elif defined(Q_OS_MAC)
#include "utility_mac.cpp"
#else
#include "utility_linux.cpp"
#endif

#define MAX_PATH_LENGTH_WIN_LONG 32767
#define MAX_PATH_LENGTH_WIN_SHORT 259
#define MAX_PATH_LENGTH_MAC 1023
#define MAX_PATH_LENGTH_LINUX 4096

#define LITE_SYNC_EXT_BUNDLE_ID "com.infomaniak.drive.desktopclient.LiteSyncExt"

namespace KDC {
const int CommonUtility::logsPurgeRate = 7; // days
const int CommonUtility::logMaxSize = 500 * 1024 * 1024; // MB

SyncPath CommonUtility::_workingDirPath = "";

static const QString englishCode = "en";
static const QString frenchCode = "fr";
static const QString germanCode = "de";
static const QString spanishCode = "es";
static const QString italianCode = "it";

static std::random_device rd;
static std::default_random_engine gen(rd());

std::string CommonUtility::generateRandomStringAlphaNum(const int length /*= 10*/) {
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

    static std::uniform_int_distribution<int> distrib(
            0, sizeof(alphanum) - 2); // -2 in order to avoid the null terminating character

    std::string tmp;
    tmp.reserve(length);
    for (int i = 0; i < length; ++i) {
        tmp += alphanum[distrib(gen)];
    }

    return tmp;
}

std::string CommonUtility::generateRandomStringPKCE(const int length /*= 10*/) {
    static const char charArray[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "-._~";

    static std::uniform_int_distribution<int> distrib(
            0, sizeof(charArray) - 2); // -2 in order to avoid the null terminating character

    std::string tmp;
    tmp.reserve(length);
    for (int i = 0; i < length; ++i) {
        tmp += charArray[distrib(gen)];
    }

    return tmp;
}

void CommonUtility::crash() {
    volatile int *a = (int *) (NULL);
    *a = 1;
}

QString CommonUtility::platformName() {
    return QSysInfo::prettyProductName();
}

Platform CommonUtility::platform() {
    const QString name = platformName();
    if (name.contains("macos", Qt::CaseInsensitive)) return Platform::MacOS;
    if (name.contains("windows", Qt::CaseInsensitive)) return Platform::Windows;
    // Otherwise we consider the OS to be Linux based
    if (platformArch().contains("arm", Qt::CaseInsensitive)) return Platform::LinuxARM;

    return Platform::LinuxAMD;
}

QString CommonUtility::platformArch() {
    return QSysInfo::currentCpuArchitecture();
}

const std::string &CommonUtility::userAgentString() {
    static std::string str;
    if (str.empty()) {
        std::stringstream ss;
        ss << APPLICATION_SHORTNAME << " / " << KDRIVE_VERSION_STRING << " (" << platformName().toStdString() << ")";
        str = ss.str();
    }
    return str;
}

const std::string &CommonUtility::currentVersion() {
    static std::string str;
    if (str.empty()) {
        std::stringstream ss;
        ss << KDRIVE_VERSION_MAJOR << "." << KDRIVE_VERSION_MINOR << "." << KDRIVE_VERSION_PATCH << "." << KDRIVE_VERSION_BUILD;
        str = ss.str();
    }
    return str;
}

QString CommonUtility::fileSystemName(const QString &dirPath) {
    QDir dir(dirPath);
    if (dir.exists()) {
        QStorageInfo info(dirPath);
        if (info.isValid()) {
            return info.fileSystemType();
        }
    } else {
        dir.cdUp();
        return fileSystemName(dir.path());
    }

    return QString();
}

QString CommonUtility::getIconPath(IconType iconType) {
    switch (iconType) {
        case KDC::CommonUtility::MAIN_FOLDER_ICON:
            return "../Resources/kdrive-mac.icns"; // TODO : To be changed to a specific incs file
            break;
        case KDC::CommonUtility::COMMON_DOCUMENT_ICON:
            // return path to common_document_folder.icns;   // Not implemented yet
            break;
        case KDC::CommonUtility::DROP_BOX_ICON:
            // return path to drop_box_folder.icns;          // Not implemented yet
            break;
        case KDC::CommonUtility::NORMAL_FOLDER_ICON:
            // return path to normal_folder.icns;            // Not implemented yet
            break;
        default:
            break;
    }

    return QString();
}

bool CommonUtility::setFolderCustomIcon(const QString &folderPath, IconType iconType) {
#ifdef Q_OS_MAC
    if (!setFolderCustomIcon_private(folderPath, getIconPath(iconType))) {
        // qCWarning(lcUtility) << "Error setting custom icon" << getIconPath(iconType) << "for folder" << folderPath;
        return false;
    }
    return true;
#else
    Q_UNUSED(folderPath)
    Q_UNUSED(iconType)

    return true;
#endif
}

qint64 CommonUtility::freeDiskSpace(const QString &path) {
#if defined(Q_OS_MAC) || defined(Q_OS_FREEBSD) || defined(Q_OS_FREEBSD_KERNEL) || defined(Q_OS_NETBSD) || defined(Q_OS_OPENBSD)
    struct statvfs stat;
    if (statvfs(path.toLocal8Bit().data(), &stat) == 0) {
        return (qint64) stat.f_bavail * stat.f_frsize;
    }
#elif defined(Q_OS_UNIX)
    struct statvfs64 stat;
    if (statvfs64(path.toLocal8Bit().data(), &stat) == 0) {
        return (qint64) stat.f_bavail * stat.f_frsize;
    }
#elif defined(Q_OS_WIN)
    ULARGE_INTEGER freeBytes;
    freeBytes.QuadPart = 0L;
    if (GetDiskFreeSpaceEx(reinterpret_cast<const wchar_t *>(path.utf16()), &freeBytes, NULL, NULL)) {
        return freeBytes.QuadPart;
    }
#endif
    return -1;
}

QByteArray CommonUtility::toQByteArray(const qint32 source) {
    // Avoid use of cast, this is the Qt way to serialize objects
    QByteArray temp;
    QDataStream data(&temp, QIODevice::ReadWrite);
    data << source;
    return temp;
}

int CommonUtility::toInt(QByteArray source) {
    int temp;
    QDataStream data(&source, QIODevice::ReadWrite);
    data >> temp;
    return temp;
}

QString CommonUtility::escape(const QString &in) {
    return in.toHtmlEscaped();
}

bool CommonUtility::stringToAppStateValue(const std::string &stringFrom, AppStateValue &appStateValueTo) {
    bool res = true;
    std::string appStateValueType = "Unknown";
    if (std::holds_alternative<std::string>(appStateValueTo)) {
        appStateValueTo = stringFrom;
        appStateValueType = "std::string";
    } else if (std::holds_alternative<int>(appStateValueTo)) {
        appStateValueType = "int";
        try {
            appStateValueTo = std::stoi(stringFrom);
        } catch (const std::invalid_argument &) {
            res = false;
        }
    } else if (std::holds_alternative<LogUploadState>(appStateValueTo)) {
        appStateValueType = "LogUploadState";
        try {
            appStateValueTo = static_cast<LogUploadState>(std::stoi(stringFrom));
        } catch (const std::invalid_argument &) {
            res = false;
        }
    } else if (std::holds_alternative<int64_t>(appStateValueTo)) {
        appStateValueType = "int64_t";
        try {
            std::get<int64_t>(appStateValueTo) = std::stoll(stringFrom);
        } catch (const std::invalid_argument &) {
            res = false;
        }
    } else {
        res = false;
    }

    if (!res) {
        std::string message = "Failed to convert string (" + stringFrom + ") to AppStateValue of type " + appStateValueType + ".";
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "CommonUtility::stringToAppStateValue", message);
    }

    return res;
}

bool CommonUtility::appStateValueToString(const AppStateValue &appStateValueFrom, std::string &stringTo) {
    if (std::holds_alternative<std::string>(appStateValueFrom)) {
        stringTo = std::get<std::string>(appStateValueFrom);
    } else if (std::holds_alternative<int>(appStateValueFrom)) {
        stringTo = std::to_string(std::get<int>(appStateValueFrom));
    } else if (std::holds_alternative<LogUploadState>(appStateValueFrom)) {
        stringTo = std::to_string(static_cast<int>(std::get<LogUploadState>(appStateValueFrom)));
    } else if (std::holds_alternative<int64_t>(appStateValueFrom)) {
        stringTo = std::to_string(std::get<int64_t>(appStateValueFrom));
    } else {
        return false;
    }
    return true;
}

std::string CommonUtility::appStateKeyToString(const AppStateKey &appStateValue) noexcept {
    switch (appStateValue) {
        case AppStateKey::LastServerSelfRestartDate:
            return "LastServerSelfRestartDate";
        case AppStateKey::LastClientSelfRestartDate:
            return "LastClientSelfRestartDate";
        case AppStateKey::LastSuccessfulLogUploadDate:
            return "LastSuccessfulLogUploadDate";
        case AppStateKey::LastLogUploadArchivePath:
            return "LastLogUploadArchivePath";
        case AppStateKey::LogUploadState:
            return "LogUploadState";
        case AppStateKey::LogUploadPercent:
            return "LogUploadPercent";
        case AppStateKey::Unknown:
            return "Unknown";
        default:
            return "AppStateKey not found (" + std::to_string(static_cast<int>(appStateValue)) + ")";
    }
}

bool CommonUtility::compressFile(const std::wstring &originalName, const std::wstring &targetName,
                                 const std::function<bool(int)> &progressCallback) {
    return compressFile(QString::fromStdWString(originalName), QString::fromStdWString(targetName), progressCallback);
}

bool CommonUtility::compressFile(const std::string &originalName, const std::string &targetName,
                                 const std::function<bool(int)> &progressCallback) {
    return compressFile(QString::fromStdString(originalName), QString::fromStdString(targetName), progressCallback);
}

bool CommonUtility::compressFile(const QString &originalName, const QString &targetName,
                                 const std::function<bool(int)> &progressCallback) {
#ifdef ZLIB_FOUND
    const std::function<bool(int)> safeProgressCallback = progressCallback ? progressCallback : [](int) { return true; };

    QFile original(originalName);
    if (!original.open(QIODevice::ReadOnly)) return false;

    auto compressed = gzopen(targetName.toUtf8(), "wb");
    if (!compressed) {
        return false;
    }

    original.seek(0);
    qint64 compressedSize = 0;
    while (!original.atEnd()) {
        auto data = original.read(1024 * 1024);
        if (auto written = gzwrite(compressed, data.data(), data.size()); written != data.size()) {
            gzclose(compressed);
            return false;
        }
        compressedSize += data.size();
        if (!safeProgressCallback(static_cast<int>((100 * compressedSize) / original.size()))) {
            gzclose(compressed);
            return true; // User cancelled
        }
    }
    gzclose(compressed);

    return true;
#else
    return false;
#endif
}

QString applicationTrPath() {
#ifdef __APPLE__
    QString devTrPath = QCoreApplication::applicationDirPath() + QString::fromLatin1("/../../../../src/gui/");
#else
    QString devTrPath = QCoreApplication::applicationDirPath() + QString::fromLatin1("/../src/gui/");
#endif
    if (QDir(devTrPath).exists()) {
        // might miss Qt etc.
        qWarning() << "Running from build location! Translations may be incomplete!";
        return devTrPath;
    }
#if defined(_WIN32)
    return QCoreApplication::applicationDirPath() + QLatin1String("/i18n/");
#elif defined(__APPLE__)

#ifdef QT_NO_DEBUG
    return QCoreApplication::applicationDirPath() + QLatin1String("/../Resources/Translations"); // path defaults to app dir.
#else
    return QString("%1/kDrive.app/Contents/Resources/Translations").arg(CMAKE_INSTALL_PREFIX);
#endif

#else
    return QCoreApplication::applicationDirPath() +
           QString::fromLatin1("/../" SHAREDIR "/" APPLICATION_CLIENT_EXECUTABLE "/i18n/");
#endif
}

QString substLang(const QString &lang) {
    // Map the more appropriate script codes
    // to country codes as used by Qt and
    // transifex translation conventions.

    // Simplified Chinese
    if (lang == QLatin1String("zh_Hans")) return QLatin1String("zh_CN");
    // Traditional Chinese
    if (lang == QLatin1String("zh_Hant")) return QLatin1String("zh_TW");
    return lang;
}

void CommonUtility::setupTranslations(QCoreApplication *app, KDC::Language enforcedLocale) {
    QStringList uiLanguages = languageCodeList(enforcedLocale);

    static QTranslator *translator = nullptr;
    static QTranslator *qtTranslator = nullptr;

    if (translator) {
        app->removeTranslator(translator);
        translator->deleteLater();
        translator = nullptr;
    }
    translator = new QTranslator(app);

    if (qtTranslator) {
        app->removeTranslator(qtTranslator);
        qtTranslator->deleteLater();
        qtTranslator = nullptr;
    }
    qtTranslator = new QTranslator(app);

    foreach (QString lang, uiLanguages) {
        lang.replace(QLatin1Char('-'), QLatin1Char('_')); // work around QTBUG-25973
        lang = substLang(lang);
        const QString trPath = applicationTrPath();
        const QString trFile = QLatin1String("client_") + lang;
        if (translator->load(trFile, trPath) || lang.startsWith(QLatin1String("en"))) {
            // Permissive approach: Qt translations
            // may be missing, but Qt translations must be there in order
            // for us to accept the language. Otherwise, we try with the next.
            // "en" is an exception as it is the default language and may not
            // have a translation file provided.
            app->setProperty("ui_lang", lang);
            const QString qtTrPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
            const QString qtTrFile = QLatin1String("qt_") + lang;
            const QString qtBaseTrFile = QLatin1String("qtbase_") + lang;
            if (!qtTranslator->load(qtTrFile, qtTrPath)) {
                if (!qtTranslator->load(qtTrFile, trPath)) {
                    if (!qtTranslator->load(qtBaseTrFile, qtTrPath)) {
                        static_cast<void>(qtTranslator->load(qtBaseTrFile, trPath));
                        // static_cast<void>() explicitly discard warning on
                        // function declared with 'nodiscard' attribute
                    }
                }
            }
            if (!translator->isEmpty()) app->installTranslator(translator);
            if (!qtTranslator->isEmpty()) app->installTranslator(qtTranslator);
            break;
        }
        if (app->property("ui_lang").isNull()) app->setProperty("ui_lang", "C");
    }
}

bool CommonUtility::colorThresholdCheck(int red, int green, int blue) {
    return 1.0 - (0.299 * red + 0.587 * green + 0.114 * blue) / 255.0 > 0.5;
}

SyncPath CommonUtility::relativePath(const SyncPath &rootPath, const SyncPath &path) {
    std::filesystem::path rootPathNormal(rootPath.lexically_normal());
    std::filesystem::path pathNormal(path.lexically_normal());

    if (rootPathNormal == pathNormal) {
        return std::filesystem::path();
    }

    std::vector<SyncName> rootPathElts;
    for (const auto &dir: rootPathNormal) {
        rootPathElts.push_back(dir.native());
    }

    size_t index = 0;
    std::filesystem::path relativePath;
    for (const auto &dir: pathNormal) {
        if (index >= rootPathElts.size()) {
            relativePath /= dir;
        } else if (dir != rootPathElts[index]) {
            // path is not a sub path of rootPath
            break;
        }
        index++;
    }

    return relativePath;
}

QStringList CommonUtility::languageCodeList(KDC::Language enforcedLocale) {
    QStringList uiLanguages = QLocale::system().uiLanguages();
    uiLanguages.prepend(languageCode(enforcedLocale));

    return uiLanguages;
}

bool CommonUtility::languageCodeIsEnglish(const QString &languageCode) {
    return languageCode.compare(englishCode) == 0;
}

QString CommonUtility::languageCode(KDC::Language enforcedLocale) {
    switch (enforcedLocale) {
        case KDC::Language::Default: {
            return QLocale::system().uiLanguages().isEmpty() ? QString() : QLocale::system().uiLanguages().first().left(2);
            break;
        }
        case KDC::Language::English:
            return englishCode;
            break;
        case KDC::Language::French:
            return frenchCode;
            break;
        case KDC::Language::German:
            return germanCode;
            break;
        case KDC::Language::Italian:
            return italianCode;
            break;
        case KDC::Language::Spanish:
            return spanishCode;
            break;
    }

    return QString();
}

SyncPath CommonUtility::getAppDir() {
    const KDC::SyncPath dirPath(KDC::getAppDir_private());
    return dirPath;
}

bool CommonUtility::hasDarkSystray() {
    return KDC::hasDarkSystray_private();
}

SyncPath CommonUtility::getAppSupportDir() {
    SyncPath dirPath(getAppSupportDir_private());

    dirPath.append(APPLICATION_NAME);
    std::error_code ec;
    if (!std::filesystem::is_directory(dirPath, ec)) {
        bool exists;
#ifdef _WIN32
        exists = (ec.value() != ERROR_FILE_NOT_FOUND && ec.value() != ERROR_PATH_NOT_FOUND && ec.value() != ERROR_INVALID_DRIVE);
#else
        exists = (ec.value() != static_cast<int>(std::errc::no_such_file_or_directory));
#endif

        if (exists) {
            return SyncPath();
        }

        if (!std::filesystem::create_directory(dirPath, ec)) {
            if (ec.value() != 0) {
                return SyncPath();
            }

            return SyncPath();
        }
    }

    return dirPath;
}

SyncPath CommonUtility::getAppWorkingDir() {
    return _workingDirPath;
}

QString CommonUtility::getFileIconPathFromFileName(const QString &fileName, NodeType type) {
    if (type == NodeType::Directory) {
        return QString(":/client/resources/icons/document types/folder.svg");
    } else if (type == NodeType::File) {
        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);
        if (mime.name().startsWith("image/")) {
            return QString(":/client/resources/icons/document types/file-image.svg");
        } else if (mime.name().startsWith("audio/")) {
            return QString(":/client/resources/icons/document types/file-audio.svg");
        } else if (mime.name().startsWith("video/")) {
            return QString(":/client/resources/icons/document types/file-video.svg");
        } else if (mime.inherits("application/pdf")) {
            return QString(":/client/resources/icons/document types/file-pdf.svg");
        } else if (mime.name().startsWith("application/vnd.ms-powerpoint") ||
                   mime.name().startsWith("application/vnd.openxmlformats-officedocument.presentationml") ||
                   mime.inherits("application/vnd.oasis.opendocument.presentation")) {
            return QString(":/client/resources/icons/document types/file-presentation.svg");
        } else if (mime.name().startsWith("application/vnd.ms-excel") ||
                   mime.name().startsWith("application/vnd.openxmlformats-officedocument.spreadsheetml") ||
                   mime.inherits("application/vnd.oasis.opendocument.spreadsheet")) {
            return QString(":/client/resources/icons/document types/file-sheets.svg");
        } else if (mime.inherits("application/zip") || mime.inherits("application/gzip") || mime.inherits("application/tar") ||
                   mime.inherits("application/rar") || mime.inherits("application/x-bzip2")) {
            return QString(":/client/resources/icons/document types/file-zip.svg");
        } else if (mime.inherits("text/x-csrc") || mime.inherits("text/x-c++src") || mime.inherits("text/x-java") ||
                   mime.inherits("text/x-objcsrc") || mime.inherits("text/x-python") || mime.inherits("text/asp") ||
                   mime.inherits("text/html") || mime.inherits("text/javascript") || mime.inherits("application/x-php") ||
                   mime.inherits("application/x-perl")) {
            return QString(":/client/resources/icons/document types/file-code.svg");
        } else if (mime.inherits("text/plain") || mime.inherits("text/xml")) {
            return QString(":/client/resources/icons/document types/file-text.svg");
        } else if (mime.inherits("application/x-msdos-program")) {
            return QString(":/client/resources/icons/document types/file-application.svg");
        }

        return QString(":/client/resources/icons/document types/file-default.svg");
    } else {
        return QString(":/client/resources/icons/document types/file-default.svg");
    }
}

QString CommonUtility::getRelativePathFromHome(const QString &dirPath) {
    QString home = QDir::homePath();
    if (!home.endsWith('/')) {
        home.append('/');
    }
    QString name(dirPath);
    if (name.startsWith(home)) {
        name = name.mid(home.length());
    }
    if (name.length() > 1 && name.endsWith('/')) {
        name.chop(1);
    }
    return QDir::toNativeSeparators(name);
}

bool CommonUtility::isFileSizeMismatchDetectionEnabled() {
    static const bool enableFileSizeMismatchDetection = !envVarValue("KDRIVE_ENABLE_FILE_SIZE_MISMATCH_DETECTION").empty();
    return enableFileSizeMismatchDetection;
}

size_t CommonUtility::maxPathLength() {
#if defined(_WIN32)
    static size_t _maxPathWin = 0;
    if (_maxPathWin == 0) {
        Poco::Util::WinRegistryKey key(R"(HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\FileSystem)", true);
        bool exist = key.exists("LongPathsEnabled");
        if (exist && key.getInt("LongPathsEnabled")) {
            _maxPathWin = MAX_PATH_LENGTH_WIN_LONG;
        } else {
            _maxPathWin = MAX_PATH_LENGTH_WIN_SHORT;
        }
    }
    // For folders in short path mode, it is MAX_PATH - 12
    // (https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry)
    // We decided to apply this rule for files too. We could else encounter issues.
    // If the path length of a folder is > MAX_PATH - 12 and the path length of a file in this folder is between MAX_PATH - 12 and MAX_PATH.
    // It would lead to a synced file in a folder that is not synced (hence excluded because of its path length).
    return (_maxPathWin == MAX_PATH_LENGTH_WIN_LONG) ? MAX_PATH_LENGTH_WIN_LONG : MAX_PATH_LENGTH_WIN_SHORT - 12;
#elif defined(__APPLE__)
    return MAX_PATH_LENGTH_MAC;
#else
    return MAX_PATH_LENGTH_LINUX;
#endif
}

bool CommonUtility::isSubDir(const SyncPath &path1, const SyncPath &path2) {
    if (path1.compare(path2) == 0) {
        return true;
    }

    auto it1 = path1.begin();
    auto it2 = path2.begin();
    while (it1 != path1.end() && it2 != path2.end() && *it1 == *it2) {
        it1++;
        it2++;
    }
    return (it1 == path1.end());
}

const std::string CommonUtility::dbVersionNumber(const std::string &dbVersion) {
#if defined(NDEBUG)
    // Release mode
#if defined(__APPLE__) || defined(_WIN32)
    // Version format = "X.Y.Z (build yyyymmdd)"
    size_t sepPosition = dbVersion.find(" ");
    return dbVersion.substr(0, sepPosition);
#else
    // Version format = "X.Y.Zmaster"
    size_t sepPosition = dbVersion.find("master");
    return dbVersion.substr(0, sepPosition);
#endif
#else
    // Debug mode
    // Version format = "X.Y.Z"
    return dbVersion;
#endif
}

void CommonUtility::extractIntFromStrVersion(const std::string &version, std::vector<int> &tabVersion) {
    std::string::size_type prevPos = 0;
    std::string::size_type pos = 0;
    do {
        pos = version.find('.', prevPos);
        tabVersion.push_back(std::stoi(version.substr(prevPos, pos - prevPos)));
        prevPos = pos + 1;
    } while (pos != std::string::npos);
}

SyncPath CommonUtility::signalFilePath(AppType appType, SignalCategory signalCategory) {
    using namespace KDC::event_dump_files;
    auto sigFilePath =
            std::filesystem::temp_directory_path() /
            (appType == AppType::Server ? (signalCategory == SignalCategory::Crash ? serverCrashFileName : serverKillFileName)
                                        : (signalCategory == SignalCategory::Crash ? clientCrashFileName : clientKillFileName));
    return sigFilePath;
}

bool CommonUtility::isVersionLower(const std::string &currentVersion, const std::string &targetVersion) {
    std::vector<int> currTabVersion;
    extractIntFromStrVersion(currentVersion, currTabVersion);

    std::vector<int> targetTabVersion;
    extractIntFromStrVersion(targetVersion, targetTabVersion);

    if (currTabVersion.size() != targetTabVersion.size()) {
        return false; // Should not happen
    }

    return std::ranges::lexicographical_compare(currTabVersion, targetTabVersion);
}

static std::string tmpDirName = "kdrive_" + CommonUtility::generateRandomStringAlphaNum();

// Check if dir name is valid by trying to create a tmp dir
bool CommonUtility::dirNameIsValid(const SyncName &name) {
#ifdef __APPLE__
    std::error_code ec;

    SyncPath tmpDirPath = std::filesystem::temp_directory_path() / tmpDirName;
    if (!std::filesystem::exists(tmpDirPath, ec)) {
        std::filesystem::create_directory(tmpDirPath, ec);
        if (ec.value()) {
            return false;
        }
    }

    SyncPath tmpPath = tmpDirPath / name;
    std::filesystem::create_directory(tmpPath, ec);
    bool illegalByteSequence;
    illegalByteSequence = (ec.value() == static_cast<int>(std::errc::illegal_byte_sequence));
    if (illegalByteSequence) {
        return false;
    }

    std::filesystem::remove_all(tmpPath, ec);
#else
    (void) name;
#endif
    return true;
}

// Check if dir name is valid by trying to create a tmp file
bool CommonUtility::fileNameIsValid(const SyncName &name) {
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::temp_directory_path() / tmpDirName, ec)) {
        std::filesystem::create_directory(std::filesystem::temp_directory_path() / tmpDirName, ec);
        if (ec.value()) {
            return false;
        }
    }
    SyncPath tmpPath = std::filesystem::temp_directory_path() / tmpDirName / name;

    std::ofstream output(tmpPath.native().c_str(), std::ios::binary);
    if (!output) {
        return false;
    }

    output.close();

    std::filesystem::remove_all(tmpPath, ec);
    return true;
}

std::string CommonUtility::envVarValue(const std::string &name) {
    bool isSet = false;
    return envVarValue(name, isSet);
}

std::string CommonUtility::envVarValue(const std::string &name, bool &isSet) {
#ifdef _WIN32
    char *value = nullptr;
    isSet = false;
    if (size_t sz = 0; _dupenv_s(&value, &sz, name.c_str()) == 0 && value != nullptr) {
        std::string valueStr(value);
        free(value);
        isSet = true;
        return valueStr;
    }
#else
    char *value = std::getenv(name.c_str());
    if (value) {
        isSet = true;
        return std::string(value);
        // Don't free "value"
    }
#endif

    return std::string();
}

void CommonUtility::handleSignals(void (*sigHandler)(int)) {
    // Kills
    signal(SIGTERM, sigHandler); // Termination request, sent to the program
    signal(SIGABRT, sigHandler); // Abnormal termination condition, as is e.g. initiated by abort()
    signal(SIGINT, sigHandler); // External interrupt, usually initiated by the user

    // Crashes
    signal(SIGSEGV, sigHandler); // Invalid memory access (segmentation fault)
    signal(SIGFPE, sigHandler); // Erroneous arithmetic operation such as divide by zero
    signal(SIGILL, sigHandler); // Invalid program image, such as invalid instruction
#ifndef Q_OS_WIN
    signal(SIGBUS, sigHandler); // Access to an invalid address

    signal(SIGPIPE, SIG_IGN);
#endif
}

void CommonUtility::writeSignalFile(AppType appType, SignalType signalType) noexcept {
    SignalCategory signalCategory;
    if (signalType == SignalType::Segv || signalType == SignalType::Fpe || signalType == SignalType::Ill
#ifndef Q_OS_WIN
        || signalType == SignalType::Bus
#endif
    ) {
        // Crash
        signalCategory = SignalCategory::Crash;
    } else {
        // Kill
        signalCategory = SignalCategory::Kill;
    }

    SyncPath sigFilePath(signalFilePath(appType, signalCategory));
    std::ofstream sigFile(sigFilePath);
    if (sigFile) {
        sigFile << KDC::toInt(signalType) << std::endl;
        sigFile.close();
    }
}

void CommonUtility::clearSignalFile(AppType appType, SignalCategory signalCategory, SignalType &signalType) noexcept {
    signalType = SignalType::None;

    SyncPath sigFilePath(signalFilePath(appType, signalCategory));
    std::error_code ec;
    if (std::filesystem::exists(sigFilePath, ec)) {
        // Read signal value
        int value;
        std::ifstream sigFile(sigFilePath);
        sigFile >> value;
        sigFile.close();

        signalType = KDC::fromInt<SignalType>(value);

        // Remove file
        std::filesystem::remove(sigFilePath, ec);
    }
}

#ifdef __APPLE__
bool CommonUtility::isLiteSyncExtEnabled() {
    QProcess *process = new QProcess();
    process->start(
            "bash",
            QStringList() << "-c"
                          << QString("systemextensionsctl list | grep %1 | grep enabled | wc -l").arg(LITE_SYNC_EXT_BUNDLE_ID));
    process->waitForStarted();
    process->waitForFinished();
    QByteArray result = process->readAll();

    return result.trimmed().toInt() == 1;
}

bool CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(std::string &errorDescr) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("/Library/Application Support/com.apple.TCC/TCC.db");
    if (db.open()) {
        bool macOs13orLater = QSysInfo::productVersion().toDouble() >= 13.0;
        QString serviceStr = macOs13orLater ? "kTCCServiceEndpointSecurityClient" : "kTCCServiceSystemPolicyAllFiles";
        QSqlQuery query(db);
        if (QOperatingSystemVersion::current() < QOperatingSystemVersion::MacOSBigSur) {
            query.prepare(QString("SELECT allowed FROM access"
                                  " WHERE service = \"%1\""
                                  " and client = \"%2\""
                                  " and client_type = 0")
                                  .arg(serviceStr)
                                  .arg(LITE_SYNC_EXT_BUNDLE_ID));
        } else {
            query.prepare(QString("SELECT auth_value FROM access"
                                  " WHERE service = \"%1\""
                                  " and client = \"%2\""
                                  " and client_type = 0")
                                  .arg(serviceStr)
                                  .arg(LITE_SYNC_EXT_BUNDLE_ID));
        }

        query.exec();

        bool found = false;
        int value = 0;
        if (query.first()) {
            found = true;
            value = query.value(0).toInt();
        }

        db.close();

        if (found) {
            if (QOperatingSystemVersion::current() < QOperatingSystemVersion::MacOSBigSur) {
                return value == 1;
            } else {
                return value == 2;
            }
        } else {
            errorDescr = "Unable to find Full Disk Access auth for Lite Sync Extension!";
        }
    } else {
        errorDescr = "Cannot open TCC database!";
    }

    return false;
}
#endif
} // namespace KDC
