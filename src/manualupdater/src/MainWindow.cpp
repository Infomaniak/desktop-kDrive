#include "MainWindow.h"

#include "libcommon/utility/utility.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <memory>
#include <regex>
#include <string>

namespace KDUpdater {

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent) {
    setupUi();
    updateCurrentVersionLabel();
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

void MainWindow::updateCurrentVersionLabel() {
    _installedVersion = {};
    if (_installedVersion.empty()) {
        _currentVersionLabel->setText(tr("kDrive is not installed or the version could not be detected."));
    } else {
        _currentVersionLabel->setText(tr("Installed version: <b>%1</b>").arg(QString::fromStdString(_installedVersion)));
    }
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

bool isValidVersion(const std::string &version) {
    static const std::regex re(R"(^\d+\.\d+\.\d+(\.\d+)?$)");
    return std::regex_match(version, re);
}

bool MainWindow::validateInputVersion(const std::string &inputVersion, std::string &errorMsg) const {
    if (inputVersion.empty()) {
        errorMsg = tr("Please enter a version number.").toStdString();
        return false;
    }

    // use regex to make sure the version is either num.num.num or num.num.num.num
    if (!isValidVersion(inputVersion)) {
        errorMsg = tr("Invalid version format. Use numbers separated by dots (e.g., 3.6.10).").toStdString();
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

    _statusLog->append(tr("Starting download of kDrive %1...").arg(QString::fromStdString(desiredVersion)));
    _progressBar->setValue(10);
    _installButton->setEnabled(false);

    _statusLog->append(tr("(Download logic is not yet implemented.)"));
    _progressBar->setValue(0);
    _installButton->setEnabled(true);
}

} // namespace KDUpdater
