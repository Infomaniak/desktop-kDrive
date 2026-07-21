#include "mainwindow.h"
#include "updaterdata.h"
#include "processchecker.h"

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

    // Preemptive check: kDrive must not be running so its DB isn't locked.
    if (KDUpdater::ProcessChecker::isKDriveRunning()) {
        {
            using namespace KDC; // same as above
            LOG_INFO(Log::instance()->getLogger(), "kDrive is running");
        }

        const auto reply = QMessageBox::question(nullptr, QObject::tr("kDrive is running"),
                                                 QObject::tr("kDrive must be closed before using the Recovery Updater. "
                                                             "Do you want to close it now?"),
                                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::Yes) {
            if (QString errorMsg; !KDUpdater::ProcessChecker::terminateKDrive(errorMsg)) {
                {
                    using namespace KDC; // same as above
                    LOGW_ERROR(Log::instance()->getLogger(), L"Failed to terminate kDrive: " << errorMsg.toStdWString());
                }
                return 1;
            }
        } else {
            return 0;
        }
    }

    KDC::UpdaterData updaterData;
    if (!updaterData.initialize()) {
        {
            using namespace KDC; // same as above
            LOG_ERROR(Log::instance()->getLogger(), "Failed to initialize updater data");
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
