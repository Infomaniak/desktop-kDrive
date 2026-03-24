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

#include <QDebug>
#include <QGuiApplication>

#include <Poco/Dynamic/Struct.h>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

#ifdef QT_DEBUG
    KDC::IpcClient client;

    QObject::connect(&client, &KDC::IpcClient::connected, [&client]() {
        qDebug() << "[IpcClient] Connected to server";
        const int id = client.sendRequest(RequestNum::USER_INFOLIST);
        qDebug() << "[IpcClient] Sent USER_INFOLIST request with id:" << id;
    });

    QObject::connect(&client, &KDC::IpcClient::disconnected, []() {
        qDebug() << "[IpcClient] Disconnected from server";
    });

    QObject::connect(&client, &KDC::IpcClient::messageReceived,
                     [](uint8_t type, int32_t id, uint8_t num, const Poco::DynamicStruct params) {
                         std::string numstr = type == 1 ? toString(static_cast<RequestNum>(num)):
                                                        toString(static_cast<SignalNum>(num));

                         qDebug() << "[IpcClient] Message received — type:" << type << "id:" << id << "num:" << numstr
                                  << "params:" << QString::fromStdString(Poco::Dynamic::structToString(params));
                     });

    qDebug() << "[IpcClient] Connecting to server...";
    client.connectToServer();
#endif
    return QGuiApplication::exec();
}