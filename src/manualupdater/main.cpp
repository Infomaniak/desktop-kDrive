#include "mainwindow.h"
#include "updaterdata.h"
#include "appkiller.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QApplication::setApplicationName(QStringLiteral("kDrive Recovery Updater"));
    QApplication::setOrganizationName(QStringLiteral("Infomaniak"));

    KDC::SyncPath logDirPath;
    if (const auto exitInfo = KDC::CommonUtility::logDirectoryPath(logDirPath); !exitInfo) {
        return 1;
    }

    if (const auto logFilePath = logDirPath / "kDriverUpdater.log"; !KDC::Log::instance(Path2WStr(logFilePath))) {
        return 1;
    }
    {
        using namespace KDC; // Required: in Release mode the LOG macro calls a KDC method without a ::KDC qualifier.
        LOG_INFO(Log::instance()->getLogger(), "kDrive Recovery Updater started");
    }

    {
        using namespace KDC;
        if (QString killMsg; !killRunningApp(killMsg)) {
            LOGW_ERROR(Log::instance()->getLogger(), L"Failed to stop kDrive: " << killMsg.toStdWString());
            (void) QMessageBox::critical(nullptr, QStringLiteral("Cannot stop kDrive"), killMsg);
            return 1;
        }
    }

    KDC::UpdaterData updaterData;
    if (!updaterData.initialize()) {
        {
            using namespace KDC; // same as above
            LOG_ERROR(Log::instance()->getLogger(), "Failed to initialize updater data");
        }

        switch (updaterData.initError()) {
            case KDC::InitError::DbNotFound:
                (void) QMessageBox::information(
                        nullptr, QStringLiteral("No kDrive installation found"),
                        QStringLiteral("No kDrive installation was found. Please use the regular kDrive installer instead."));
                break;
            case KDC::InitError::DbOpenFailed:
                (void) QMessageBox::critical(
                        nullptr, QStringLiteral("Cannot open database"),
                        QStringLiteral("Cannot open the kDrive database. kDrive might still be running — try force "
                                       "quitting it, then relaunch this tool."));
                break;
            case KDC::InitError::VersionReadFailed:
                (void) QMessageBox::critical(
                        nullptr, QStringLiteral("Database read error"),
                        QStringLiteral("Error reading from the kDrive database (could not retrieve the installed version)."));
                break;
            case KDC::InitError::AppUidReadFailed:
                (void) QMessageBox::critical(
                        nullptr, QStringLiteral("Database read error"),
                        QStringLiteral("Error reading from the kDrive database (could not retrieve the app UID)."));
                break;
            default:
                break;
        }

        return 1;
    }

    {
        using namespace KDC; // same as above
        LOG_INFO(Log::instance()->getLogger(), "Distribution Channel: " << updaterData.distributionChannel());
    }

    KDC::MainWindow window(updaterData);
    window.show();

    return QApplication::exec();
}
