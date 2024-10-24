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

#pragma once

#include "libcommon/comm.h"

#include <deque>

#include <QByteArray>
#include <QDataStream>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QWaitCondition>

namespace KDC {

class Worker;

class CommClient : public QObject {
        Q_OBJECT

    public:
        static std::shared_ptr<CommClient> instance(QObject *parent = nullptr);
        ~CommClient();

        CommClient(CommClient const &) = delete;
        void operator=(CommClient const &) = delete;

        bool connectToServer(quint16 commPort);
        bool execute(RequestNum num, const QByteArray &params, QByteArray &results, int timeout = COMM_SHORT_TIMEOUT);
        bool execute(RequestNum num, const QByteArray &params, int timeout = COMM_SHORT_TIMEOUT);
        bool execute(RequestNum num, int timeout = COMM_SHORT_TIMEOUT);

        void stop();
        static bool isConnected() { return _instance != nullptr; }

    signals:
        void sendError(int id);
        void signalReceived(int id, SignalNum num, const QByteArray &params);
        void disconnected();

    private:
        static std::shared_ptr<CommClient> _instance;
        QThread *_requestWorkerThread;
        Worker *_requestWorker;
        QTcpSocket *_tcpConnection;
        QByteArray _buffer;

        explicit CommClient(QObject *parent = nullptr);

        bool sendRequest(int id, RequestNum num, const QByteArray &params);

    private slots:
        void onDisconnected();
        void onErrorOccurred(QAbstractSocket::SocketError socketError);
        void onBytesWritten(qint64 numBytes);
        void onReadyRead();
        void onSendRequest(int id, RequestNum num, const QByteArray &params);
        void onSignalReceived(int id, SignalNum num, const QByteArray &params);

        friend class AppClient;
};

class Worker : public QObject {
        Q_OBJECT

    public:
        Worker(QObject *parent = nullptr);
        ~Worker();

        void addRequest(RequestNum num, const QByteArray &params, int &id);
        void addReply(int id, const QByteArray &result);
        void addSignal(int id, SignalNum num, const QByteArray &params);
        void stop();

    public slots:
        void onStart();

    signals:
        void finished();
        void sendRequest(int id, RequestNum num, const QByteArray &params);
        void replyReceived(int id, const QByteArray &result);
        void signalReceived(int id, SignalNum num, const QByteArray &params);

    private:
        QMutex _mutex;
        int _requestId;
        std::deque<int> _requestQueue;
        QWaitCondition _queueWC;
        std::deque<int> _replyQueue;
        std::deque<int> _signalQueue;
        bool _stop;
        QHash<int, QPair<RequestNum, QByteArray>> _requestMap;
        QHash<int, QByteArray> _replyMap;
        QHash<int, QPair<SignalNum, QByteArray>> _signalMap;
};

} // namespace KDC
