#include "mainwindow.h"
#include "updaterdata.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "utility/utility.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QApplication::setApplicationName(QStringLiteral("kDrive Recovery Updater"));
    QApplication::setOrganizationName(QStringLiteral("Infomaniak"));

    KDC::SyncPath logDirPath;
    if (const auto exitInfo = KDC::CommonUtility::logDirectoryPath(logDirPath); !exitInfo) {
        return 1;
    }

    const auto logFilePath = logDirPath / KDC::Utility::logFileNameWithTime();
    if (!KDC::Log::instance(Path2WStr(logFilePath))) {
        return 1;
    }

    LOG_INFO(KDC::Log::instance()->getLogger(), "kDrive Recovery Updater started");

    KDUpdater::UpdaterData updaterData;
    if (!updaterData.initialize()) {
        LOG_ERROR(KDC::Log::instance()->getLogger(), "Failed to initialize updater data");
        return 1;
    }

    // log the distributionChannel
    LOG_INFO(KDC::Log::instance()->getLogger(), "Distribution Channel: " << updaterData.distributionChannel());

    KDUpdater::MainWindow window(updaterData);
    window.show();

    return QApplication::exec();
}
