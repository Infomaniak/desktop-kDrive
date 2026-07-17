#include "mainwindow.h"
#include "updaterdata.h"
#include "libcommon/utility/utility.h"
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

    if (const auto logFilePath = logDirPath / "kDriverUpdater.log"; !KDC::Log::instance(Path2WStr(logFilePath))) {
        return 1;
    }
    {
        using namespace KDC;
        LOG_INFO(Log::instance()->getLogger(), "kDrive Recovery Updater started");
    }

    KDC::UpdaterData updaterData;
    if (!updaterData.initialize()) {
        {
            using namespace KDC;
            LOG_ERROR(Log::instance()->getLogger(), "Failed to initialize updater data");
        }
        return 1;
    }

    // log the distributionChannel
    {
        using namespace KDC;
        LOG_INFO(Log::instance()->getLogger(), "Distribution Channel: " << updaterData.distributionChannel());
    }

    KDC::MainWindow window(updaterData);
    window.show();

    return QApplication::exec();
}
