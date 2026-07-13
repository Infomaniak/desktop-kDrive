#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <string>

namespace KDUpdater {

class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);

    private slots:
        void onInstallClicked();
        void onVersionTextChanged(const QString &text) const;

    private:
        void setupUi();
        void updateCurrentVersionLabel();
        bool validateInputVersion(const std::string &inputVersion, std::string &errorMsg) const;

        QLabel *_currentVersionLabel = nullptr;
        QLabel *_desiredVersionLabel = nullptr;
        QLineEdit *_versionInput = nullptr;
        QPushButton *_installButton = nullptr;
        QProgressBar *_progressBar = nullptr;
        QTextEdit *_statusLog = nullptr;
        QLabel *_validationHint = nullptr;

        std::string _installedVersion;
};

} // namespace KDUpdater
