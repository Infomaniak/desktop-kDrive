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

#include "libcommon/comm.h"
#include "libcommon/utility/utility.h"

#include <deque>

#include <QDataStream>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>
#include <QObject>
#include <QWaitCondition>

namespace KDC {

class Worker;

class OldCommServer : public QObject {
        Q_OBJECT

    public:
        static std::shared_ptr<OldCommServer> instance(QObject *parent = nullptr);
        ~OldCommServer();

        OldCommServer(OldCommServer const &) = delete;
        void operator=(OldCommServer const &) = delete;

        void sendReply(int id, const QByteArray &result);
        bool sendSignal(SignalNum num, const QByteArray &params);
        bool sendSignal(SignalNum num, const QByteArray &params, int &id);
        inline quint16 commPort() const { return _tcpServer.serverPort(); }

        void setHasQuittedProperly(bool hasQuittedProperly) { _hasQuittedProperly = hasQuittedProperly; }
        bool hasQuittedProperly() const { return _hasQuittedProperly; }

        void start();
        inline bool isListening() { return _tcpServer.isListening(); }

    signals:
        void requestReceived(int id, RequestNum num, const QByteArray &params);
        void restartClient();

    private:
        static std::shared_ptr<OldCommServer> _instance;
        QtLoggingThread *_requestWorkerThread;
        Worker *_requestWorker;
        QTcpServer _tcpServer;
        QTcpSocket *_tcpSocket;
        QByteArray _buffer;

        bool _hasQuittedProperly;

        explicit OldCommServer(QObject *parent = nullptr);

    private slots:
        void onNewConnection();
        void onBytesWritten(qint64 numBytes);
        void onReadyRead();
        void onErrorOccurred(QAbstractSocket::SocketError socketError);
        void onRequestReceived(int id, RequestNum num, const QByteArray &params);
        void onSendReply(int id, const QByteArray &result);
        void onSendSignal(int id, SignalNum num, const QByteArray &params);

        friend class AppServer;
};

class Worker : public QObject {
        Q_OBJECT

    public:
        Worker(QObject *parent = nullptr);
        ~Worker();

        void addRequest(int id, RequestNum num, const QByteArray &params);
        void addReply(int id, const QByteArray &result);
        void addSignal(SignalNum num, const QByteArray &params, int &id);
        void clear();
        void stop();

    public slots:
        void onStart();

    signals:
        void finished();
        void requestReceived(int id, RequestNum num, const QByteArray &params);
        void sendReply(int id, const QByteArray &result);
        void sendSignal(int id, SignalNum num, const QByteArray &params);

    private:
        QMutex _mutex;
        int _signalId;
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
