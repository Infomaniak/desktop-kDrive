#pragma once

#include "updaterdata.h"

#include "libcommon/utility/types.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <atomic>
#include <string>
#include <thread>

namespace KDC {

class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(const UpdaterData &updaterData, QWidget *parent = nullptr);
        ~MainWindow() override;

    private slots:
        void onInstallClicked();
        void onVersionTextChanged(const QString &text) const;
        void onInstallFinished(bool success, const QString &message);
        void onInstallProgress(int percent, const QString &message);

    private:
        void setupUi();
        void updateCurrentVersionLabel() const;
        void fetchAndSetDefaultVersion();
        bool validateInputVersion(const std::string &inputVersion, std::string &errorMsg) const;

        QLabel *_currentVersionLabel = nullptr;
        QLabel *_desiredVersionLabel = nullptr;
        QLineEdit *_versionInput = nullptr;
        QPushButton *_installButton = nullptr;
        QProgressBar *_progressBar = nullptr;
        QTextEdit *_statusLog = nullptr;
        QLabel *_validationHint = nullptr;

        const UpdaterData &_updaterData;
        std::string _installedVersion;
        KDC::VersionInfo _fetchedVersionInfo;

        std::thread _workerThread;
        std::atomic<bool> _installInProgress{false};
};

} // namespace KDC
