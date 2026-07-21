#include "processchecker.h"

#include "libcommonserver/log/log.h"

#include <QCoreApplication>
#include <QProcess>
#include <QThread>

namespace KDUpdater {

namespace {
constexpr int kGracefulWaitMs = 2000;

// Process names differ by platform.
#if defined(KD_WINDOWS)
const QStringList kDriveProcessNames = {QStringLiteral("kDrive.exe"), QStringLiteral("kDrive_client.exe")};
#else
const QStringList kDriveProcessNames = {QStringLiteral("kDrive"), QStringLiteral("kDrive_client")};
#endif

// Forward declarations within anonymous namespace.
QStringList pidsForProcess(const QString &name);
bool gracefulQuit(const QStringList &names);
bool forceQuit(const QStringList &names);

/**
 * @brief Check if any of the named processes appears in the system process list.
 */
bool anyProcessRunning(const QStringList &names) {
    for (const QString &name: names) {
        if (pidsForProcess(name).isEmpty()) continue;
        return true; // At least one instance found.
    }
    return false;
}

/**
 * @brief Retrieve PIDs for a given process name.
 */
QStringList pidsForProcess(const QString &name) {
    QProcess proc;
#if defined(KD_WINDOWS)
    proc.start(QStringLiteral("tasklist"),
               QStringList{QStringLiteral("/FI"), QStringLiteral("IMAGENAME eq %1").arg(name), QStringLiteral("/NH")});
#else
    proc.start(QStringLiteral("pgrep"), QStringList{QStringLiteral("-x"), name});
#endif
    if (!proc.waitForStarted(5000)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"ProcessChecker: failed to start pgrep/tasklist for " << name.toStdWString());
        return {};
    }
    if (!proc.waitForFinished(5000)) {
        LOGW_WARN(KDC::Log::instance()->getLogger(), L"ProcessChecker: pgrep/tasklist timed out for " << name.toStdWString());
        return {};
    }

    const QByteArray output = proc.readAllStandardOutput();

#if defined(KD_WINDOWS)
    // tasklist output: "kDrive.exe                    1234 Console ..."
    QStringList pids;
    const QStringList lines = QString::fromLocal8Bit(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line: lines) {
        QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        // First token is image name. If it contains our process name, second token is PID.
        if (parts[0].compare(name, Qt::CaseInsensitive) == 0 && parts.size() > 1) {
            pids.append(parts[1]);
        }
    }
    return pids;
#else
    // pgrep returns one PID per line
    QStringList pids;
    const QStringList lines = QString::fromLocal8Bit(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line: lines) {
        if (!line.trimmed().isEmpty()) {
            pids.append(line.trimmed());
        }
    }
    return pids;
#endif
}

/**
 * @brief Gracefully terminate the named processes.
 */
bool gracefulQuit(const QStringList &names) {
    bool anySuccess = false;

#if defined(KD_WINDOWS)
    // Windows: taskkill without /F sends WM_CLOSE
    for (const QString &name: names) {
        QProcess proc;
        proc.start(QStringLiteral("taskkill"), QStringList{QStringLiteral("/IM"), name});
        if (proc.waitForStarted(5000) && proc.waitForFinished(5000)) {
            anySuccess = true; // At least one command ran
        }
    }
#else
    // macOS / Linux: SIGTERM (or killall / osascript for macOS app bundle)
    for (const QString &name: names) {
        const QStringList pids = pidsForProcess(name);
        if (pids.isEmpty()) continue;

#if defined(KD_MACOS)
        // Try osascript "tell application ... to quit" first for the GUI
        if (name == QLatin1String("kDrive_client")) {
            QProcess appleScript;
            appleScript.start(QStringLiteral("osascript"),
                              QStringList{QStringLiteral("-e"),
                                          QStringLiteral("tell application \"%1\" to quit").arg(name)});
            if (appleScript.waitForStarted(3000) && appleScript.waitForFinished(3000)) {
                anySuccess = true;
                continue;
            }
            // Fall through to kill -TERM
        }
#endif
        // kill -TERM <pid> ...
        QProcess proc;
        QStringList args;
        args << QStringLiteral("-TERM");
        for (const QString &pid: pids) args << pid;
        proc.start(QStringLiteral("kill"), args);
        if (proc.waitForStarted(3000) && proc.waitForFinished(3000)) {
            anySuccess = true;
        }
    }
#endif

    return anySuccess;
}

/**
 * @brief Forcefully terminate the named processes.
 */
bool forceQuit(const QStringList &names) {
    bool anySuccess = false;

#if defined(KD_WINDOWS)
    // Windows: taskkill /F
    for (const QString &name: names) {
        QProcess proc;
        proc.start(QStringLiteral("taskkill"), QStringList{QStringLiteral("/F"), QStringLiteral("/IM"), name});
        if (proc.waitForStarted(5000) && proc.waitForFinished(5000)) {
            if (proc.exitCode() == 0) anySuccess = true;
        }
    }
    return anySuccess;
#else
    // macOS / Linux: SIGKILL
    for (const QString &name: names) {
        const QStringList pids = pidsForProcess(name);
        if (pids.isEmpty()) continue;

        QProcess proc;
        QStringList args;
        args << QStringLiteral("-9");
        for (const QString &pid: pids) args << pid;
        proc.start(QStringLiteral("kill"), args);
        if (proc.waitForStarted(3000) && proc.waitForFinished(3000) && proc.exitCode() == 0) {
            anySuccess = true;
        }
    }
    return anySuccess;
#endif
}
} // anonymous namespace

bool ProcessChecker::isKDriveRunning() {
    return anyProcessRunning(kDriveProcessNames);
}

bool ProcessChecker::terminateKDrive(QString &outMessage) {
    if (!isKDriveRunning()) {
        return true; // Nothing to do.
    }

    // 1. Graceful quit attempt
    LOGW_INFO(KDC::Log::instance()->getLogger(), L"ProcessChecker: attempting graceful quit of kDrive processes.");
    (void) gracefulQuit(kDriveProcessNames);
    QThread::msleep(kGracefulWaitMs);

    // Re-check
    if (!isKDriveRunning()) {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"ProcessChecker: graceful quit succeeded.");
        return true;
    }

    // 2. Force quit attempt
    LOGW_WARN(KDC::Log::instance()->getLogger(), L"ProcessChecker: graceful quit failed, attempting force quit.");
    if (!forceQuit(kDriveProcessNames)) {
        LOGW_ERROR(KDC::Log::instance()->getLogger(), L"ProcessChecker: forceQuit helper reported failure.");
    }
    QThread::msleep(kGracefulWaitMs);

    // 3. Final verification
    if (!isKDriveRunning()) {
        LOGW_INFO(KDC::Log::instance()->getLogger(), L"ProcessChecker: force quit succeeded.");
        return true;
    }

    LOGW_ERROR(KDC::Log::instance()->getLogger(), L"ProcessChecker: could not terminate kDrive processes.");
    outMessage = QObject::tr("Could not close the running kDrive application. "
                             "Please close it manually and try again.");
    return false;
}

} // namespace KDUpdater
