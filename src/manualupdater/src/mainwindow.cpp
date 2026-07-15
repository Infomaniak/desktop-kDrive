#include "mainwindow.h"
#include "updaterdata.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "libsyncengine/jobs/network/infomaniak_API/getappversionjob.h"
#include "libsyncengine/jobs/network/directdownloadjob.h"
#include "server/updater/abstractupdater.h"

#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QFile>
#include <QXmlStreamReader>
#include <QProcess>
#include <thread>
#include <filesystem>
#include <iostream>
#include <memory>
#include <regex>
#include <string>

namespace KDUpdater {


namespace {
bool isValidVersion(const std::string &version) {
    static const std::regex re(R"(^\d+\.\d+\.\d+\.\d+$)");
    return std::regex_match(version, re);
}
} // namespace

MainWindow::MainWindow(const UpdaterData &updaterData, QWidget *parent) :
    QMainWindow(parent),
    _updaterData(updaterData),
    _installedVersion(updaterData.installedVersion()) {
    setupUi();
    updateCurrentVersionLabel();
    fetchAndSetDefaultVersion();
}

void MainWindow::setupUi() {
    setWindowTitle(tr("kDrive Recovery Updater"));
    resize(500, 350);

    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Info group
    auto *infoGroup = new QGroupBox(tr("Current Installation"), centralWidget);
    auto *infoLayout = new QVBoxLayout(infoGroup);
    _currentVersionLabel = new QLabel(tr("Detecting version..."), infoGroup);
    _currentVersionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(_currentVersionLabel);
    mainLayout->addWidget(infoGroup);

    // Input group
    auto *inputGroup = new QGroupBox(tr("Desired kDrive Version"), centralWidget);
    auto *inputLayout = new QVBoxLayout(inputGroup);

    auto inputRow = std::make_unique<QHBoxLayout>();
    _desiredVersionLabel = new QLabel(tr("Version:"), inputGroup);
    _versionInput = new QLineEdit(inputGroup);
    _versionInput->setPlaceholderText(tr("e.g., 3.6.10"));
    const auto *validator =
            new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[0-9]+(\\.[0-9]+)*$")), _versionInput);
    _versionInput->setValidator(validator);
    inputRow->addWidget(_desiredVersionLabel);
    inputRow->addWidget(_versionInput);
    inputLayout->addLayout(inputRow.release());

    _validationHint = new QLabel(inputGroup);
    _validationHint->setStyleSheet(QStringLiteral("color: red;"));
    inputLayout->addWidget(_validationHint);

    mainLayout->addWidget(inputGroup);

    // Progress
    _progressBar = new QProgressBar(centralWidget);
    _progressBar->setRange(0, 100);
    _progressBar->setValue(0);
    _progressBar->setTextVisible(true);
    mainLayout->addWidget(_progressBar);

    // Install button
    _installButton = new QPushButton(tr("Download && Install"), centralWidget);
    _installButton->setEnabled(false);
    mainLayout->addWidget(_installButton);

    // Status log
    _statusLog = new QTextEdit(centralWidget);
    _statusLog->setReadOnly(true);
    _statusLog->setPlaceholderText(tr("Status log will appear here..."));
    mainLayout->addWidget(_statusLog, 1);

    connect(_installButton, &QPushButton::clicked, this, &MainWindow::onInstallClicked);
    connect(_versionInput, &QLineEdit::textChanged, this, &MainWindow::onVersionTextChanged);
}

void MainWindow::updateCurrentVersionLabel() const {
    if (!_updaterData.isInstalled() || _installedVersion.empty()) {
        _currentVersionLabel->setText(tr("kDrive is not installed or the version could not be detected."));
        return;
    }

    LOGW_INFO(KDC::Log::instance()->getLogger(),
              L"Current kDrive version: " << QString::fromStdString(_installedVersion).toStdWString());

    _currentVersionLabel->setText(tr("Installed version: <b>%1</b>").arg(QString::fromStdString(_installedVersion)));
}

void MainWindow::fetchAndSetDefaultVersion() {
    if (_updaterData.appId().empty()) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"App ID is empty. Cannot fetch default version.");
        return;
    }

    const auto job = std::make_shared<KDC::GetAppVersionJob>(_updaterData.distributionChannel(), _updaterData.appId());
    if (const auto exitInfo = job->runSynchronously(); !exitInfo) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"Failed to fetch default version: " << exitInfo);
        return;
    }

    _fetchedVersionInfo = job->versionInfo();
    if (_fetchedVersionInfo.tag.empty() || _fetchedVersionInfo.buildVersion == 0) {
        return;
    }

    _versionInput->setText(QString::fromStdString(_fetchedVersionInfo.fullVersion()));
    LOG_INFO(KDC::Log::instance()->getLogger(), "Default version set to: " << _fetchedVersionInfo.fullVersion());
}
void MainWindow::onVersionTextChanged(const QString &text) const {
    std::string errorMsg;
    const bool valid = validateInputVersion(text.toStdString(), errorMsg);
    _installButton->setEnabled(valid);

    if (!valid && !text.isEmpty()) {
        _validationHint->setText(QString::fromStdString(errorMsg));
    } else {
        _validationHint->clear();
    }
}

bool MainWindow::validateInputVersion(const std::string &inputVersion, std::string &errorMsg) const {
    if (inputVersion.empty()) {
        errorMsg = tr("Please enter a version number.").toStdString();
        return false;
    }

    if (!isValidVersion(inputVersion)) {
        errorMsg = tr("Invalid version format. Use numbers separated by dots (e.g., 3.6.10.1).").toStdString();
        return false;
    }

    if (!_installedVersion.empty() && KDC::CommonUtility::isVersionLower(inputVersion, _installedVersion)) {
        errorMsg = tr("You cannot install a version older than the one currently installed (%1).")
                           .arg(QString::fromStdString(_installedVersion))
                           .toStdString();
        return false;
    }

    return true;
}

void MainWindow::onInstallClicked() {
    const std::string desiredVersion = _versionInput->text().toStdString();
    if (std::string errorMsg; !validateInputVersion(desiredVersion, errorMsg)) {
        QMessageBox::warning(this, tr("Validation Error"), QString::fromStdString(errorMsg));
        return;
    }

    if (!_fetchedVersionInfo.isValid()) {
        QMessageBox::critical(this, tr("No version info"),
                              tr("Cannot install: no version information was fetched. Please restart the updater."));
        return;
    }

    _installButton->setEnabled(false);
    _statusLog->append(tr("Starting installation of kDrive %1...").arg(QString::fromStdString(desiredVersion)));
    _progressBar->setValue(10);

    const KDC::VersionInfo fetchedInfo = _fetchedVersionInfo; // copy for thread safety
    QPointer<MainWindow> weakThis = this;
    std::thread([weakThis, desiredVersion, fetchedInfo]() {
        LOGW_INFO(KDC::Log::instance()->getLogger(),
                  L"Starting installation thread for version " << QString::fromStdString(desiredVersion).toStdWString());

        bool success = false;
        QString message;

        // Build specific version info with user-specified version
        KDC::VersionInfo specificVersion = fetchedInfo;
        specificVersion.checksum.clear(); // we don't know the specific version's checksum

        const std::string originalUrl = specificVersion.downloadUrl;
        const std::string oldVersion = specificVersion.fullVersion();
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"OLD version: " << QString::fromStdString(oldVersion).toStdWString());
        if (auto pos = specificVersion.downloadUrl.find(oldVersion); pos != std::string::npos) {
            specificVersion.downloadUrl.replace(pos, oldVersion.length(), desiredVersion);
        } else if (pos = specificVersion.downloadUrl.find(specificVersion.tag); pos != std::string::npos) {
            // Fallback: try to replace tag only
            specificVersion.downloadUrl.replace(pos, specificVersion.tag.length(), desiredVersion);
        } else {
            message = tr("Failed to construct download URL for version %1.").arg(QString::fromStdString(desiredVersion));
            LOGW_INFO(KDC::Log::instance()->getLogger(), message.toStdWString());
            if (weakThis) {
                QMetaObject::invokeMethod(
                        weakThis, [weakThis, message]() { weakThis->onInstallFinished(false, message); }, Qt::QueuedConnection);
                return message;
            }
        }


        LOGW_INFO(KDC::Log::instance()->getLogger(),
                  L"Original download URL: " << QString::fromStdString(originalUrl).toStdWString());
        LOGW_INFO(KDC::Log::instance()->getLogger(),
                  L"Target download URL for version " << QString::fromStdString(desiredVersion).toStdWString() << L": "
                                                      << QString::fromStdString(specificVersion.downloadUrl).toStdWString());

        if (weakThis) {
            QMetaObject::invokeMethod(
                    weakThis,
                    [weakThis]() {
                        weakThis->_statusLog->append(tr("Downloading installer..."));
                        weakThis->_progressBar->setValue(30);
                    },
                    Qt::QueuedConnection);
        }

#if defined(KD_WINDOWS)
        auto updater = KDC::createUpdater();
        updater->setVersionInfo(specificVersion);
        auto exitCode = updater->installVersion();
        success = (exitCode == KDC::ExitCode::Ok);
        if (!success) {
            if (exitCode == KDC::ExitCode::NetworkError) {
                message = tr("The specified version does not exist or the download failed.");
            } else if (exitCode == KDC::ExitCode::UpdateError) {
                message = tr("File verification failed. The downloaded file may be corrupted.");
            } else {
                message = tr("Installation failed (error code %1).").arg(static_cast<int>(exitCode));
            }
        } else {
            message = tr("Installer launched. The update will install silently.");
        }
#elif defined(KD_LINUX)
        auto updater = KDC::createUpdater();
        updater->setVersionInfo(specificVersion);
        auto exitCode = updater->installVersion();
        success = (exitCode == KDC::ExitCode::Ok);
        if (!success) {
            if (exitCode == KDC::ExitCode::NetworkError) {
                message = tr("The specified version does not exist or the download failed.");
            } else {
                message = tr("Installation failed (error code %1).").arg(static_cast<int>(exitCode));
            }
        } else {
            message = tr("AppImage downloaded to ~/Applications/. If needed, move it to replace your current kDrive AppImage.");
        }
#else // macOS
        if (weakThis) {
            success = weakThis->installMacOS(specificVersion, message);
        } else {
            message = tr("Window was closed during operation.");
        }
#endif

        if (weakThis) {
            QMetaObject::invokeMethod(
                    weakThis, [weakThis, success, message]() { weakThis->onInstallFinished(success, message); },
                    Qt::QueuedConnection);
        }
    }).detach();
}

void MainWindow::onInstallProgress(int percent, const QString &message) {
    _progressBar->setValue(percent);
    if (!message.isEmpty()) {
        _statusLog->append(message);
    }
}

void MainWindow::onInstallFinished(bool success, const QString &message) {
    _progressBar->setValue(success ? 100 : 0);
    _installButton->setEnabled(true);
    if (success) {
        _statusLog->append(tr("Success: %1").arg(message));
        QMessageBox::information(this, tr("Installation Complete"), message);
    } else {
        _statusLog->append(tr("Failed: %1").arg(message));
        QMessageBox::critical(this, tr("Installation Failed"), message);
    }
}

#if defined(KD_MACOS)
bool MainWindow::installMacOS(const KDC::VersionInfo &versionInfo, QString &outMessage) {
    KDC::SyncPath tmpDir;
    if (!KDC::CommonUtility::deviceTempDirectoryPath(tmpDir)) {
        outMessage = tr("Failed to get temp directory.");
        return false;
    }

    const KDC::SyncPath appcastXmlPath = tmpDir / "appcast.xml";
    std::filesystem::remove(appcastXmlPath);

    const auto appcastJob = std::make_shared<KDC::DirectDownloadJob>(appcastXmlPath, versionInfo.downloadUrl);
    const auto exitInfo = appcastJob->runSynchronously();
    if (!exitInfo) {
        if (appcastJob->httpResponse().getStatus() == 404) {
            outMessage = tr("The specified version does not exist or the download failed.");
        } else {
            outMessage = tr("Failed to download appcast: %1").arg(QString::fromStdString(KDC::toString(exitInfo)));
        }
        return false;
    }

    QFile file(QString::fromStdString(appcastXmlPath.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        outMessage = tr("Failed to read appcast.");
        return false;
    }

    QXmlStreamReader reader(&file);
    QString pkgUrl;
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("enclosure")) {
            pkgUrl = QString(reader.attributes().value(QStringLiteral("url")));
            if (!pkgUrl.isEmpty()) {
                break;
            }
        }
    }
    file.close();

    if (pkgUrl.isEmpty()) {
        outMessage = tr("Could not find download link in appcast.");
        return false;
    }

    // Download .pkg
    const auto pkgFilename = pkgUrl.mid(pkgUrl.lastIndexOf('/') + 1);
    const auto pkgPath = QString::fromStdString(tmpDir.string()) + QStringLiteral("/") + pkgFilename;
    std::filesystem::remove(KDC::SyncPath(pkgPath.toStdString()));

    const auto pkgJob = std::make_shared<KDC::DirectDownloadJob>(KDC::SyncPath(pkgPath.toStdString()), pkgUrl.toStdString());
    const auto pkgExitInfo = pkgJob->runSynchronously();
    if (!pkgExitInfo) {
        if (pkgJob->httpResponse().getStatus() == 404) {
            outMessage = tr("The specified version does not exist or the download failed.");
        } else {
            outMessage = tr("Failed to download package.");
        }
        return false;
    }

    if (!QFile::exists(pkgPath)) {
        outMessage = tr("Package file not found after download.");
        return false;
    }

    if (!QProcess::startDetached(QStringLiteral("open"), QStringList{pkgPath})) {
        outMessage = tr("Failed to open installer. Please install manually: %1").arg(pkgPath);
        return false;
    }

    outMessage = tr("Installer opened: %1").arg(pkgPath);
    return true;
}
#endif

} // namespace KDUpdater
