/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#pragma once

#include <QAbstractSocket>
#include <QIODevice>

class CommServerPrivate;
class CommChannelPrivate;

class CommChannel : public QIODevice {
        Q_OBJECT
    public:
        CommChannel(QObject *parent, CommChannelPrivate *p);
        ~CommChannel();

        qint64 readData(char *data, qint64 maxlen) override;
        qint64 writeData(const char *data, qint64 len) override;

        bool isSequential() const override { return true; }
        qint64 bytesAvailable() const override;
        bool canReadLine() const override;

    signals:
        void disconnected();

    private:
        // Use Qt's p-impl system to hide objective-c types from C++ code including this file
        Q_DECLARE_PRIVATE(CommChannel)
        QScopedPointer<CommChannelPrivate> d_ptr;
        friend class CommServerPrivate;
};

class CommServer : public QObject {
        Q_OBJECT
    public:
        CommServer();
        ~CommServer();

        void close();
        bool listen(const QString &name);
        CommChannel *nextPendingConnection();
        CommChannel *guiConnection();

        static bool removeServer(const QString &) { return false; }

    signals:
        void newExtConnection();
        void newGuiConnection();

    private:
        Q_DECLARE_PRIVATE(CommServer)
        QScopedPointer<CommServerPrivate> d_ptr;
};
