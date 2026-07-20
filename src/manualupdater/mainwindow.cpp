#include "mainwindow.h"
#include "updaterdata.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "httpdownloader.h"
#include "updater/abstractosupdater.h"

#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpressionValidator>
#include <thread>
#include <regex>
#include <string>
#include <utility>

namespace KDC {

namespace {
constexpr char kVersionRegexPattern[] = R"(^[0-9]+(\.[0-9]+)*$)";

bool isValidVersion(const std::string &version) {
    static const std::regex re(kVersionRegexPattern);
    return std::regex_match(version, re);
}
} // namespace

MainWindow::MainWindow(const UpdaterData &updaterData, QWidget *const parent) :
    QMainWindow(parent),
    _updaterData(updaterData),
    _installedVersion(updaterData.installedVersion()) {
    setupUi();
    updateCurrentVersionLabel();
    fetchAndSetDefaultVersion();
}

MainWindow::~MainWindow() {
    if (_workerThread.joinable()) {
        _installInProgress = false; // allow the thread to finish naturally
        // We can't safely join here because the thread may be blocked on network I/O.
        // Detach it to finish in background.
        _workerThread.detach();
    }
}

void MainWindow::setupUi() {
    setWindowTitle(tr("kDrive Recovery Updater"));
    resize(500, 350);

    auto *const centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *const mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Info group
    auto *const infoGroup = new QGroupBox(tr("Current Installation"), centralWidget);
    auto *const infoLayout = new QVBoxLayout(infoGroup);
    _currentVersionLabel = new QLabel(tr("Detecting version..."), infoGroup);
    _currentVersionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(_currentVersionLabel);
    mainLayout->addWidget(infoGroup);

    // Input group
    auto *const inputGroup = new QGroupBox(tr("Desired kDrive Version"), centralWidget);
    auto *const inputLayout = new QVBoxLayout(inputGroup);

    auto inputRow = std::make_unique<QHBoxLayout>();
    _desiredVersionLabel = new QLabel(tr("Version:"), inputGroup);
    _versionInput = new QLineEdit(inputGroup);
    _versionInput->setPlaceholderText(tr("e.g., 3.6.10"));
    const auto *const validator =
            new QRegularExpressionValidator(QRegularExpression(QString::fromLatin1(kVersionRegexPattern)), _versionInput);
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

    (void) connect(_installButton, &QPushButton::clicked, this, &MainWindow::onInstallClicked);
    (void) connect(_versionInput, &QLineEdit::textChanged, this, &MainWindow::onVersionTextChanged);
}

void MainWindow::updateCurrentVersionLabel() const {
    if (!_updaterData.isInstalled() || _installedVersion.empty()) {
        _currentVersionLabel->setText(tr("kDrive is not installed or the version could not be detected."));
        return;
    }

    _currentVersionLabel->setText(tr("Installed version: <b>%1</b>").arg(QString::fromStdString(_installedVersion)));
}

void MainWindow::fetchAndSetDefaultVersion() {
    if (_updaterData.appId().empty()) {
        LOGW_WARN(Log::instance()->getLogger(), L"App ID is empty. Cannot fetch default version.");
        return;
    }

    if (std::string error;
        !HttpDownloader::fetchAppVersion(_updaterData.distributionChannel(), _updaterData.appId(), _fetchedVersionInfo, error)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to fetch default version: " << CommonUtility::s2ws(error));
        return;
    }
    if (_fetchedVersionInfo.tag.empty() || _fetchedVersionInfo.buildVersion == 0) {
        return;
    }

    _versionInput->setText(QString::fromStdString(_fetchedVersionInfo.fullVersion()));
    LOG_INFO(Log::instance()->getLogger(), "Default version set to: " << _fetchedVersionInfo.fullVersion());
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
        errorMsg = tr("Invalid version format. Use numbers separated by dots (e.g., 3.6.10).").toStdString();
        return false;
    }

    if (!_installedVersion.empty() && CommonUtility::isVersionLower(inputVersion, _installedVersion)) {
        errorMsg = tr("You cannot install a version older than the one currently installed (%1).")
                           .arg(QString::fromStdString(_installedVersion))
                           .toStdString();
        return false;
    }

    return true;
}

void MainWindow::onInstallClicked() {
    if (_installInProgress.load()) {
        _statusLog->append(tr("Installation already in progress."));
        return;
    }

    const std::string desiredVersion = _versionInput->text().toStdString();
    if (std::string errorMsg; !validateInputVersion(desiredVersion, errorMsg)) {
        (void) QMessageBox::warning(this, tr("Validation Error"), QString::fromStdString(errorMsg));
        return;
    }

    _statusLog->append(tr("Starting download of kDrive %1...").arg(QString::fromStdString(desiredVersion)));
    _progressBar->setValue(10);
    _installButton->setEnabled(false);
    _installInProgress = true;

    const VersionInfo fetchedInfo = _fetchedVersionInfo; // copy for thread safety

    _workerThread = std::thread([this, desiredVersion, fetchedInfo = _fetchedVersionInfo]() {
        runInstall(desiredVersion, fetchedInfo);
    });
}

void MainWindow::onInstallProgress(const int32_t percent, const QString &message) const {
    _progressBar->setValue(percent);
    if (!message.isEmpty()) {
        _statusLog->append(message);
    }
}

void MainWindow::onInstallFinished(const bool success, const QString &message) {
    _installInProgress = false;
    _progressBar->setValue(success ? 100 : 0);
    _installButton->setEnabled(true);
    if (success) {
        _statusLog->append(tr("Success: %1").arg(message));
        (void) QMessageBox::information(this, tr("Installation Complete"), message);
        (void) close();
    } else {
        _statusLog->append(tr("Failed: %1").arg(message));
        (void) QMessageBox::critical(this, tr("Installation Failed"), message);
    }
}

template<typename F>
void MainWindow::postToUi(const QPointer<MainWindow> self, F &&fn) {
    if (!self) return;
    if (const bool posted = QMetaObject::invokeMethod(self, std::forward<F>(fn), Qt::QueuedConnection); !posted) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to post callback to main thread.");
    }
}

bool MainWindow::buildDownloadUrl(VersionInfo &info, const std::string &desiredVersion, bool &versionChanged, QString &error) {
    const std::string oldVersion = info.fullVersion();
    versionChanged = (oldVersion != desiredVersion);
    if (!versionChanged) {
        return true;
    }
    if (const auto pos = info.downloadUrl.find(oldVersion); pos != std::string::npos) {
        (void) info.downloadUrl.replace(pos, oldVersion.length(), desiredVersion);
        return true;
    }
    if (const auto posTag = info.downloadUrl.find(info.tag); posTag != std::string::npos) {
        (void) info.downloadUrl.replace(posTag, info.tag.length(), desiredVersion);
        return true;
    }
    error = tr("Failed to construct download URL for version %1.").arg(QString::fromStdString(desiredVersion));
    return false;
}


void MainWindow::runInstall(const std::string &desiredVersion, VersionInfo fetchedInfo) {
    QPointer self = this;

    VersionInfo specificVersion = std::move(fetchedInfo);

    bool versionChanged = false;
    if (QString error; !buildDownloadUrl(specificVersion, desiredVersion, versionChanged, error)) {
        LOGW_INFO(Log::instance()->getLogger(), error.toStdWString());
        postToUi(self, [self, error] { self->onInstallFinished(false, error); });
        return;
    }
    if (versionChanged) {
        LOGW_INFO(Log::instance()->getLogger(), L"Version changed.");
        specificVersion.checksum.clear(); // we can't know the checksum when the version is manually enter by the user
    }

    postToUi(self, [self] {
        self->_statusLog->append(tr("Downloading installer..."));
        self->_progressBar->setValue(30);
    });

    auto updater = createOsUpdater();
    if (!updater) {
        postToUi(self, [self] { self->onInstallFinished(false, tr("Failed to create OS-specific updater.")); });
        return;
    }

    auto progressCallback = [self](const int32_t percent, const QString &msg) {
        postToUi(self, [self, percent, msg] {
            self->_progressBar->setValue(percent);
            if (!msg.isEmpty()) self->_statusLog->append(msg);
        });
    };

    QString message;
    const bool success = updater->install(specificVersion, progressCallback, message);
    postToUi(self, [self, success, message] { self->onInstallFinished(success, message); });
}

} // namespace KDC
