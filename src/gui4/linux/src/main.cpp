/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "communicationlayer/IpcClient.h"
#include "libcommongui/logger.h"

#include <QGuiApplication>
#include <QLoggingCategory>

#include <Poco/Dynamic/Struct.h>

Q_LOGGING_CATEGORY(lcMain, "gui.v4.main", QtInfoMsg)

static void setupLogging() {
    auto *logger = KDC::Logger::instance(); // installs Qt message handler
    logger->setIsCLientLog(true);
    logger->setLogDebug(true);
    logger->setupTemporaryFolderLogDir();
    logger->enterNextLogFile();
}

std::string toString(const KDC::GuiJobType type) {
    switch (type) {
        case KDC::GuiJobType::Query:
            return "Query";
        case KDC::GuiJobType::Signal:
            return "Signal";
        default:
            return "Unknown";
    }
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    setupLogging();

#ifdef QT_DEBUG
    KDC::IpcClient client;

    QObject::connect(&client, &KDC::IpcClient::connected, [&client]() {
        qCDebug(lcMain) << "Connected to server";
        (void) client.sendRequest(RequestNum::USER_INFOLIST);
    });

    QObject::connect(&client, &KDC::IpcClient::disconnected, []() {
        qCDebug(lcMain) << "Disconnected from server";
    });

    QObject::connect(&client, &KDC::IpcClient::messageReceived,
                     [](KDC::GuiJobType type, int32_t id, uint8_t num, const Poco::DynamicStruct &params) {
                         const std::string numstr = type == KDC::GuiJobType::Query ? toString(static_cast<RequestNum>(num)):
                                                        toString(static_cast<SignalNum>(num));

                         qCDebug(lcMain) << "Message received — type:" << toString(type) << "id:" << id << "num:" << numstr
                                         << "params:" << QString::fromStdString(Poco::Dynamic::structToString(params));
                     });

    client.connectToServer();
#endif
    return QGuiApplication::exec();
}
