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

#include "guiutility.h"
#include "appclient.h"
#include "parameterscache.h"
#include "custommessagebox.h"
#include "libcommon/utility/utility.h"
#include "libcommongui/utility/utility.h"

#include <QApplication>
#include <QBitmap>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGraphicsColorizeEffect>
#include <QGraphicsSvgItem>
#include <QGraphicsScene>
#include <QIcon>
#include <QImage>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QOperatingSystemVersion>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QUrlQuery>

#if defined(KD_WINDOWS)
#include <fileapi.h>
#endif

namespace KDC {

static const QString styleSheetWhiteFile(":/client/resources/styles/stylesheetwhite.qss");
static const QString styleSheetBlackFile(":/client/resources/styles/stylesheetblack.qss");
static const QColor styleSheetWhiteWidgetShadowColor = QColor(200, 200, 200, 180);
#ifdef Q_OS_WIN
static const QColor styleSheetWhiteDialogShadowColor = QColor(150, 150, 150, 180);
#else
static const QColor styleSheetWhiteDialogShadowColor = QColor(0, 0, 0, 180);
#endif
static const QColor styleSheetBlackWidgetShadowColor = QColor(20, 20, 20, 180);
static const QColor styleSheetBlackDialogShadowColor = QColor(20, 20, 20, 180);

static const int fontWeightNormal = 450;
static const int fontWeightMedium = 550;
static const int fontWeightSemibold = 650;
static const int fontWeightBold = 750;
#ifdef Q_OS_WIN
static const int windowsIncrement = 50;
#endif

static bool darkTheme = false;

Q_LOGGING_CATEGORY(lcGuiUtility, "gui.guiutility", QtInfoMsg)

bool GuiUtility::openBrowser(const QUrl &url, QWidget *errorWidgetParent) {
    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(errorWidgetParent, QCoreApplication::translate("utility", "Could not open browser"),
                                 QCoreApplication::translate("utility",
                                                             "There was an error when launching the browser to go to "
                                                             "URL %1. Maybe no default browser is configured?")
                                         .arg(url.toString()));
        }
        qCWarning(lcGuiUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

bool GuiUtility::openEmailComposer(const QString &subject, const QString &body, QWidget *errorWidgetParent) {
    QUrl url(QLatin1String("mailto:"));
    QUrlQuery query;
    query.setQueryItems({{QLatin1String("subject"), subject}, {QLatin1String("body"), body}});
    url.setQuery(query);

    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(errorWidgetParent, QCoreApplication::translate("utility", "Could not open email client"),
                                 QCoreApplication::translate("utility",
                                                             "There was an error when launching the email client to "
                                                             "create a new message. Maybe no default email client is "
                                                             "configured?"));
        }
        qCWarning(lcGuiUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

QIcon GuiUtility::getIconWithColor(const QString &path, const QColor &color) {
    QGraphicsSvgItem *item = new QGraphicsSvgItem(path);

    if (color.isValid()) {
        QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect;
        effect->setColor(color);
        effect->setStrength(1);
        item->setGraphicsEffect(effect);
    }

    QGraphicsScene scene;
    scene.addItem(item);

    int ratio = 3;
    QPixmap pixmap(QSize(static_cast<int>(round(scene.width() * ratio)), static_cast<int>(round(scene.height() * ratio))));
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    scene.render(&painter);

    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

GuiUtility::systrayPosition GuiUtility::getSystrayPosition(const QScreen *const screen) {
    const QRect displayRect = screen->geometry();
    const QRect desktopRect = screen->availableGeometry();
    if (displayRect == desktopRect) {
        // Unable to get systray position
        return systrayPosition::Top;
    } else {
        if (desktopRect.height() < displayRect.height()) {
            if (desktopRect.y() > displayRect.y()) {
                return systrayPosition::Top;
            } else {
                return systrayPosition::Bottom;
            }
        } else {
            if (desktopRect.x() > displayRect.x()) {
                return systrayPosition::Left;
            } else {
                return systrayPosition::Right;
            }
        }
    }
}

bool GuiUtility::isPointInSystray(QScreen *screen, const QPoint &point) {
    QRect displayRect = screen->geometry();
    QRect desktopRect = screen->availableGeometry();
    if (desktopRect.height() < displayRect.height()) {
        if (desktopRect.y() > displayRect.y()) {
            // Systray position = Top
            return point.y() < desktopRect.y();
        } else {
            // Systray position = Bottom
            return point.y() > desktopRect.y() + desktopRect.height();
        }
    } else {
        if (desktopRect.x() > displayRect.x()) {
            // Systray position = Left
            return point.x() < desktopRect.x();
        } else {
            // Systray position = Right
            return point.x() > desktopRect.x() + desktopRect.width();
        }
    }
}

QIcon GuiUtility::getIconMenuWithColor(const QString &path, const QColor &color) {
    QGraphicsSvgItem *item = new QGraphicsSvgItem(path);
    QGraphicsSvgItem *itemMenu = new QGraphicsSvgItem(":/client/resources/icons/actions/chevron-down.svg");

    if (color.isValid()) {
        QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect;
        effect->setColor(color);
        effect->setStrength(1);
        item->setGraphicsEffect(effect);

        QGraphicsColorizeEffect *effectMenu = new QGraphicsColorizeEffect;
        effectMenu->setColor(color);
        effectMenu->setStrength(1);
        itemMenu->setGraphicsEffect(effectMenu);
    }

    QGraphicsScene scene;
    scene.addItem(item);
    item->setPos(QPointF(0, 0));
    int iconWidth = static_cast<int>(round(scene.width()));
    scene.setSceneRect(QRectF(0, 0, iconWidth * 2, iconWidth));

    scene.addItem(itemMenu);
    itemMenu->setPos(QPointF(5.0 / 4.0 * iconWidth, 1.0 / 4.0 * iconWidth));
    itemMenu->setScale(0.5);

    qreal ratio = qApp->primaryScreen()->devicePixelRatio();
    QPixmap pixmap(QSize(static_cast<int>(round(scene.width() * ratio)), static_cast<int>(round(scene.height() * ratio))));
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    scene.render(&painter);

    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

void GuiUtility::setStyle(QApplication *app) {
    setStyle(app, isDarkTheme());
}

void GuiUtility::setStyle(QApplication *app, bool isDarkTheme) {
    // Load style sheet
    darkTheme = isDarkTheme;
    QFile ssFile(darkTheme ? styleSheetBlackFile : styleSheetWhiteFile);
    if (ssFile.exists()) {
        ssFile.open(QFile::ReadOnly);
        QString styleSheet = QLatin1String(ssFile.readAll());

#ifdef Q_OS_WIN
        // Update font weight
        styleSheet.replace("$$bold_font$$", QString::number(fontWeightBold + windowsIncrement));
        styleSheet.replace("$$semibold_font$$", QString::number(fontWeightSemibold + windowsIncrement));
        styleSheet.replace("$$medium_font$$", QString::number(fontWeightMedium + windowsIncrement));
        styleSheet.replace("$$normal_font$$", QString::number(fontWeightNormal + windowsIncrement));
#else
        // Update font weight
        styleSheet.replace("$$bold_font$$", QString::number(fontWeightBold));
        styleSheet.replace("$$semibold_font$$", QString::number(fontWeightSemibold));
        styleSheet.replace("$$medium_font$$", QString::number(fontWeightMedium));
        styleSheet.replace("$$normal_font$$", QString::number(fontWeightNormal));
#endif
        app->setStyleSheet(styleSheet);
        ::KDC::AppClient *kDriveApp = qobject_cast<KDC::AppClient *>(app);
        kDriveApp->updateSystrayIcon();
    } else {
        qCWarning(lcGuiUtility) << "Style sheet file not found!";
    }
}

QString GuiUtility::getFileStatusIconPath(::KDC::SyncFileStatus status) {
    switch (status) {
        case ::KDC::SyncFileStatus::Error:
            return QString(":/client/resources/icons/statuts/error-sync.svg");
            break;
        case ::KDC::SyncFileStatus::Success:
        case ::KDC::SyncFileStatus::Inconsistency:
            return QString(":/client/resources/icons/statuts/success.svg");
            break;
        case ::KDC::SyncFileStatus::Conflict:
        case ::KDC::SyncFileStatus::Ignored:
            return QString(":/client/resources/icons/statuts/warning.svg");
            break;
        case ::KDC::SyncFileStatus::Syncing:
            return QString(":/client/resources/icons/statuts/sync.svg");
            break;
        case ::KDC::SyncFileStatus::Unknown:
            break;
        case ::KDC::SyncFileStatus::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
            break;
        }
    }

    return {};
}

QString GuiUtility::getSyncStatusIconPath(StatusInfo &statusInfo) {
    QString path;

    if (statusInfo._disconnected) {
        path = QString(":/client/resources/icons/statuts/pause.svg");
    } else {
        switch (statusInfo._status) {
            case KDC::SyncStatus::Undefined:
                // Drive with no configured sync
                path = QString("");
                break;
            case KDC::SyncStatus::Starting:
            case KDC::SyncStatus::Running:
                path = QString(":/client/resources/icons/statuts/sync.svg");
                break;
            case KDC::SyncStatus::Idle:
                path = QString(":/client/resources/icons/statuts/success.svg");
                break;
            case KDC::SyncStatus::Error:
                path = QString(":/client/resources/icons/statuts/error-sync.svg");
                break;
            case KDC::SyncStatus::PauseAsked:
            case KDC::SyncStatus::Paused:
            case KDC::SyncStatus::StopAsked:
            case KDC::SyncStatus::Stopped:
                path = QString(":/client/resources/icons/statuts/pause.svg");
                break;
            default:
                break;
        }
    }

    return path;
}

QString GuiUtility::getSyncStatusText(StatusInfo &statusInfo) {
    QString text;
    if (statusInfo._disconnected) {
        text = QCoreApplication::translate("utility", "You are not connected anymore. <a style=\"%1\" href=\"%2\">Log in</a>")
                       .arg(KDC::CommonUtility::linkStyle)
                       .arg(loginLink);
    } else {
        switch (statusInfo._status) {
            case KDC::SyncStatus::Undefined:
                text = QCoreApplication::translate("utility",
                                                   "No folder to synchronize\nYou can add one from the kDrive settings.");
                break;
            case KDC::SyncStatus::Starting:
            case KDC::SyncStatus::Running:
                if (statusInfo._totalFiles > 0) {
                    if (statusInfo._liteSyncActivated) {
                        text = QCoreApplication::translate("utility", "Sync in progress (%1 of %2)")
                                       .arg(statusInfo._syncedFiles)
                                       .arg(statusInfo._totalFiles);
                    } else {
                        text = QCoreApplication::translate("utility", "Sync in progress (%1 of %2)\n%3 left...")
                                       .arg(statusInfo._syncedFiles)
                                       .arg(statusInfo._totalFiles)
                                       .arg(KDC::CommonGuiUtility::durationToDescriptiveString1(
                                               static_cast<quint64>(statusInfo._estimatedRemainingTime)));
                    }
                } else if (statusInfo._oneSyncInPropagationStep) {
                    text = QCoreApplication::translate("utility", "Sync in progress (Step %1/%2).")
                                   .arg(toInt(statusInfo._syncStep))
                                   .arg(toInt(KDC::SyncStep::Done));
                } else if (statusInfo._status == KDC::SyncStatus::Starting) {
                    text = QCoreApplication::translate("utility", "Synchronization starting");
                } else {
                    text = QCoreApplication::translate("utility", "Sync in progress.");
                }
                break;
            case KDC::SyncStatus::Idle:
                if (statusInfo._unresolvedConflicts) {
                    text = QCoreApplication::translate("utility", "You are up to date, unresolved conflicts.");
                } else {
                    text = QCoreApplication::translate("utility", "You are up to date!");
                }
                break;
            case KDC::SyncStatus::Error:
                text = QCoreApplication::translate(
                               "utility", "Some files couldn't be synchronized. <a style=\"%1\" href=\"%2\">Learn more</a>")
                               .arg(KDC::CommonUtility::linkStyle)
                               .arg(learnMoreLink);
                break;
            case KDC::SyncStatus::PauseAsked:
            case KDC::SyncStatus::StopAsked:
                text = QCoreApplication::translate("utility", "Synchronization pausing ...");
                break;
            case KDC::SyncStatus::Paused:
            case KDC::SyncStatus::Stopped:
                text = QCoreApplication::translate("utility", "Synchronization paused.");
                break;
            default:
                break;
        }
    }

    return text;
}

QString GuiUtility::getDriveStatusIconPath(StatusInfo &statusInfo) {
    return getSyncStatusIconPath(statusInfo);
}

bool GuiUtility::getPauseActionAvailable(KDC::SyncStatus status) {
    if (status == KDC::SyncStatus::PauseAsked || status == KDC::SyncStatus::Paused || status == KDC::SyncStatus::StopAsked ||
        status == KDC::SyncStatus::Stopped || status == KDC::SyncStatus::Error) {
        // Pause
        return false;
    } else {
        return true;
    }
}

bool GuiUtility::getResumeActionAvailable(KDC::SyncStatus status) {
    if (status == KDC::SyncStatus::PauseAsked || status == KDC::SyncStatus::Paused || status == KDC::SyncStatus::StopAsked ||
        status == KDC::SyncStatus::Stopped || status == KDC::SyncStatus::Error) {
        // Pause
        return true;
    }

    return false;
}

QPixmap GuiUtility::getAvatarFromImage(const QImage &image) {
    QPixmap originalPixmap = QPixmap::fromImage(image);

    // Draw mask
    QBitmap mask(originalPixmap.size());
    QPainter painter(&mask);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    mask.fill(Qt::white);
    painter.setBrush(Qt::black);
    int rx = static_cast<int>(round(mask.width() / 2.0));
    int ry = static_cast<int>(round(mask.height() / 2.0));
    painter.drawEllipse(QPoint(rx, ry), rx, ry);

    // Draw the final image.
    originalPixmap.setMask(mask);

    return originalPixmap;
}

QColor GuiUtility::getShadowColor(bool dialog) {
    return darkTheme ? (dialog ? styleSheetBlackDialogShadowColor : styleSheetBlackWidgetShadowColor)
                     : (dialog ? styleSheetWhiteDialogShadowColor : styleSheetWhiteWidgetShadowColor);
}

bool GuiUtility::isDarkTheme() {
    bool darkTheme = false;
    if (CommonUtility::isMac()) {
        darkTheme = CommonUtility::hasDarkSystray();
    } else {
        darkTheme = ParametersCache::instance()->parametersInfo().darkTheme();
    }

    return darkTheme;
}

QUrl GuiUtility::getUrlFromLocalPath(const QString &path) {
    QUrl url = QUrl();
    if (!path.isEmpty()) {
#ifndef Q_OS_WIN
        url = QUrl::fromLocalFile(path);
#else
        // work around a bug in QDesktopServices on Win32, see i-net
        if (path.startsWith(QLatin1String("\\\\")) || path.startsWith(QLatin1String("//"))) {
            url = QUrl::fromLocalFile(QDir::toNativeSeparators(path));
        } else {
            url = QUrl::fromLocalFile(path);
        }
#endif
    }
    return url;
}

int GuiUtility::getQFontWeightFromQSSFontWeight(int weight) {
    // QFont::Weight[0, 99] = font-weight[100, 900] / 9
    return weight / 9;
}

qint64 GuiUtility::folderSize(const QString &dirPath) {
    QDirIterator it(dirPath, QDirIterator::Subdirectories);
    qint64 total = 0;
    while (it.hasNext()) {
        QFileInfo fileInfo(it.next());
        total += fileInfo.size();
    }

    return total;
}

qint64 GuiUtility::folderDiskSize(const QString &dirPath) {
    qint64 total = 0;

#if defined(KD_WINDOWS)
    QDirIterator it(dirPath, QDirIterator::Subdirectories);
    DWORD fileSizeLow, fileSizeHigh;
    while (it.hasNext()) {
        fileSizeLow = GetCompressedFileSizeA(it.next().toStdString().c_str(), &fileSizeHigh);
        total += ((ULONGLONG) fileSizeHigh << 32) + fileSizeLow;
    }
#else
    Q_UNUSED(dirPath)
#endif

    return total;
}

QString GuiUtility::getFolderPath(const QString &path, NodeType nodeType) {
    return nodeType == NodeType::Directory ? path : QFileInfo(path).path();
}

bool GuiUtility::openFolder(const QString &dirPath) {
    if (dirPath.isEmpty()) return true;

    if (const auto fileInfo = QFileInfo(dirPath); fileInfo.exists()) {
        const QUrl url = getUrlFromLocalPath(QDir::cleanPath(fileInfo.filePath()));
        if (url.isValid() && !QDesktopServices::openUrl(url)) return false;
    } else if (fileInfo.dir().exists()) {
        const QUrl url = getUrlFromLocalPath(QDir::cleanPath(fileInfo.dir().path()));
        if (!url.isValid()) return false;
        if (!QDesktopServices::openUrl(url)) return false;
    } else {
        return false;
    }

    return true;
}

QWidget *GuiUtility::getTopLevelWidget(QWidget *widget) {
    QWidget *topLevelWidget = widget;
    while (topLevelWidget->parentWidget() != nullptr) {
        topLevelWidget = topLevelWidget->parentWidget();
    }
    return topLevelWidget;
}

void GuiUtility::forceUpdate(QWidget *widget) {
    // Update all child widgets.
    for (int i = 0; i < widget->children().size(); i++) {
        QObject *child = widget->children()[i];
        if (child->isWidgetType()) {
            forceUpdate((QWidget *) child);
        }
    }

    // Invalidate the layout of the widget.
    if (widget->layout()) {
        invalidateLayout(widget->layout());
    }
}

void GuiUtility::invalidateLayout(QLayout *layout) {
    // Recompute the given layout and all its child layouts.
    for (int i = 0; i < layout->count(); i++) {
        QLayoutItem *item = layout->itemAt(i);
        if (item->layout()) {
            invalidateLayout(item->layout());
        } else {
            item->invalidate();
        }
    }
    layout->invalidate();
    layout->activate();
}

void GuiUtility::makePrintablePath(QString &path, const uint64_t maxSize /*= 50*/) {
    if (path.size() > (qsizetype) maxSize) {
        path = path.left(static_cast<int64_t>(maxSize)) + "...";
    }
}

bool GuiUtility::warnOnInvalidSyncFolder(const QString &dirPath, const std::map<int, SyncInfoClient> &syncInfoMap,
                                         QWidget *parent) {
    const QString selectedFolderName = CommonUtility::getRelativePathFromHome(dirPath);
    const SyncPath directoryPath = QStr2Path(dirPath);

    bool warn = false;
    QString warningMsg;
    for (const auto &sync: syncInfoMap) {
        const QString syncFolderName = sync.second.name();
        const SyncPath syncLocalPath = QStr2Path(sync.second.localPath());

        if (syncLocalPath == directoryPath) {
            warn = true;
            warningMsg = QCoreApplication::translate(
                                 "utility", "Folder <b>%1</b> cannot be selected because another sync is using the same folder.")
                                 .arg(selectedFolderName);
            break;
        } else if (CommonUtility::isSubDir(directoryPath, syncLocalPath)) {
            warn = true;
            warningMsg = QCoreApplication::translate(
                                 "utility",
                                 "Folder <b>%1</b> cannot be selected because it contains the synchronized folder <b>%2</b>.")
                                 .arg(selectedFolderName, syncFolderName);
            break;
        } else if (CommonUtility::isSubDir(syncLocalPath, directoryPath)) {
            warn = true;
            warningMsg =
                    QCoreApplication::translate(
                            "utility",
                            "Folder <b>%1</b> cannot be selected because it is contained in the synchronized folder <b>%2</b>.")
                            .arg(selectedFolderName, syncFolderName);
            break;
        }
    }

    if (SyncPath suggestedPath; CommonUtility::isDiskRootFolder(directoryPath, suggestedPath)) {
        warn = true;
        if (suggestedPath.empty()) {
            warningMsg = QCoreApplication::translate(
                                 "utility", "Folder <b>%1</b> cannot be selected as sync folder. Please, select another folder.")
                                 .arg(selectedFolderName);
        } else {
            warningMsg = QCoreApplication::translate("utility",
                                                     "Folder <b>%1</b> cannot be selected as sync folder. Please, select another "
                                                     "folder. Suggested folder: <b>%2</b>")
                                 .arg(selectedFolderName, QString::fromStdString(suggestedPath.lexically_normal().string()));
        }
    }

    if (warn) {
        CustomMessageBox msgBox(QMessageBox::Warning, warningMsg, QMessageBox::Ok, parent);
        msgBox.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(parent));
        return false;
    }

    return true;
}

QLocale GuiUtility::languageToQLocale(Language language) {
    switch (language) {
        case Language::Spanish:
            return QLocale::Spanish;
        case Language::English:
            return QLocale::English;
        case Language::French:
            return QLocale::French;
        case Language::German:
            return QLocale::German;
        case Language::Italian:
            return QLocale::Italian;
        default:
            return QLocale();
    }
}

QString GuiUtility::getDateForCurrentLanguage(const QDateTime &dateTime, const QString &dateFormat) {
    const Language lang = ParametersCache::instance()->parametersInfo().language();
    return languageToQLocale(lang).toString(dateTime, dateFormat);
}

bool GuiUtility::checkBlacklistSize(const qsizetype blacklistSize, QWidget *parent) {
    if (blacklistSize > 1000) {
        (void) CustomMessageBox(
                QMessageBox::Warning,
                QCoreApplication::translate("utility",
                                            "You cannot exclude more than 1000 folders. Please uncheck higher-level folders."),
                QMessageBox::Ok, parent)
                .exec();
        return false;
    }
    return true;
}

#ifdef Q_OS_LINUX
bool GuiUtility::getLinuxDesktopType(QString &type, QString &version) {
    type = QProcessEnvironment::systemEnvironment().value("XDG_CURRENT_DESKTOP");
    if (type.contains("GNOME")) {
        QProcess process;
        process.start("gnome-shell", QStringList() << "--version");
        process.waitForStarted();
        process.waitForFinished();

        QByteArray result = process.readAll();
        if (result.startsWith("GNOME")) {
            QList<QByteArray> resultList = result.split(' ');
            if (resultList.size() == 3) {
                version = resultList[2];
            }
        }
    }

    return true;
}
#endif
} // namespace KDC
