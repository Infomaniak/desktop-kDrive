/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "updateerrordialog.h"
#include "updaterclient.h"
#include "libcommongui/commclient.h"
#include "libcommon/asserts.h"
#include "libcommon/comm.h"

#include <QDataStream>

namespace KDC {

UpdaterClient *UpdaterClient::_instance = nullptr;

UpdaterClient::~UpdaterClient() {
    _instance = nullptr;
}

UpdaterClient *UpdaterClient::instance() {
    return _instance;
}

QString UpdaterClient::version() const {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UPDATER_VERSION, QByteArray(), results)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    QString version;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> version;

    return version;
}

bool UpdaterClient::isKDCUpdater() {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UPDATER_ISKDCUPDATER, QByteArray(), results)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    bool ret;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> ret;

    return ret;
}

bool UpdaterClient::isSparkleUpdater() {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UPDATER_ISSPARKLEUPDATER, QByteArray(), results)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    bool ret;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> ret;

    return ret;
}

QString UpdaterClient::statusString() const {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UPDATER_STATUSSTRING, QByteArray(), results)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    QString status;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> status;

    return status;
}

bool UpdaterClient::downloadCompleted() const {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UPDATER_DOWNLOADCOMPLETED, QByteArray(), results)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    bool ret;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> ret;

    return ret;
}

bool UpdaterClient::updateFound() const {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UPDATER_UPDATEFOUND, QByteArray(), results)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }

    bool ret;
    QDataStream resultStream(&results, QIODevice::ReadOnly);
    resultStream >> ret;

    return ret;
}

void UpdaterClient::startInstaller() const {
    QByteArray results;
    if (!CommClient::instance()->execute(REQUEST_NUM_UPDATER_STARTINSTALLER, QByteArray(), results)) {
        throw std::runtime_error(EXECUTE_ERROR_MSG);
    }
}

void UpdaterClient::showWindowsUpdaterDialog(const QString &targetVersion, const QString &targetVersionString,
                                             const QString &clientVersion) {
    KDC::UpdateErrorDialog *dialog = new KDC::UpdateErrorDialog(targetVersion, targetVersionString, clientVersion);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // skip
    bool skip = false;
    connect(dialog, &KDC::UpdateErrorDialog::skip, this, [&skip]() { skip = true; });
    // askagain: do nothing
    // retry
    bool retry = false;
    connect(dialog, &KDC::UpdateErrorDialog::retry, this, [&retry]() { retry = true; });

    dialog->exec();

    if (skip || retry) {
        // Send result to server
        QByteArray params;
        QDataStream paramsStream(&params, QIODevice::WriteOnly);
        paramsStream << skip;

        QByteArray result;
        if (!KDC::CommClient::instance()->execute(REQUEST_NUM_UPDATER_UPDATE_DIALOG_RESULT, params, result)) {
            // Nothing to do
        }
    }
}

UpdaterClient::UpdaterClient(QObject *) {
    ASSERT(!_instance);
    _instance = this;
}

}  // namespace KDC
