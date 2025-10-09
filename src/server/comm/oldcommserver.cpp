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

#include "oldcommserver.h"
#include "common/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"

#include <QDataStream>
#include <QDir>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QTimer>

#include <fstream>

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::shared_ptr<OldCommServer> OldCommServer::_instance = nullptr;

std::shared_ptr<OldCommServer> OldCommServer::instance(QObject *parent) {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<OldCommServer>(new OldCommServer(parent));
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

OldCommServer::OldCommServer(QObject *parent) :
    QObject(parent),
    _requestWorkerThread(new QtLoggingThread()),
    _requestWorker(new Worker()),
    _tcpSocket(nullptr),
    _buffer(QByteArray()),
    _hasQuittedProperly(false) {
    // Start worker thread
    _requestWorker->moveToThread(_requestWorkerThread);
    connect(_requestWorkerThread, &QThread::started, _requestWorker, &Worker::onStart);
    connect(_requestWorker, &Worker::finished, _requestWorkerThread, &QThread::quit);
    connect(_requestWorker, &Worker::finished, _requestWorker, &Worker::deleteLater);
    connect(_requestWorkerThread, &QThread::finished, _requestWorkerThread, &QObject::deleteLater);
    connect(_requestWorker, &Worker::requestReceived, this, &OldCommServer::onRequestReceived);
    connect(_requestWorker, &Worker::sendReply, this, &OldCommServer::onSendReply);
    connect(_requestWorker, &Worker::sendSignal, this, &OldCommServer::onSendSignal);

    _requestWorkerThread->start();
    _requestWorkerThread->setPriority(QThread::Priority::HighestPriority);

    // Start tcp server
    start();
}

OldCommServer::~OldCommServer() {
    if (_tcpSocket) {
        if (_tcpSocket->isOpen()) {
            _tcpSocket->close();
        }
        _tcpSocket->deleteLater();
    }

    _tcpServer.close();
    _tcpServer.deleteLater();

    _instance = nullptr;
}

void OldCommServer::sendReply(int id, const QByteArray &result) {
    if (_tcpSocket && _tcpSocket->isOpen()) {
        _requestWorker->addReply(id, result);
    }
}

bool OldCommServer::sendSignal(const SignalNum num, const QByteArray &params) {
    int id = 0;
    return sendSignal(num, params, id);
}

bool OldCommServer::sendSignal(SignalNum num, const QByteArray &params, int &id) {
    if (_tcpSocket && _tcpSocket->isOpen()) {
        _requestWorker->addSignal(num, params, id);
        return true;
    }
    return false;
}

void OldCommServer::start() {
    // (Re)start tcp server
    if (_tcpServer.isListening()) {
        _tcpServer.close();
    } else {
       // connect(&_tcpServer, &QTcpServer::newConnection, this, &OldCommServer::onNewConnection);
    }

    _tcpServer.listen(QHostAddress::LocalHost);
    LOG_DEBUG(Log::instance()->getLogger(), "Comm server started - port=" << _tcpServer.serverPort());

#ifdef QT_DEBUG
    // Save comm port value into .comm file
    std::filesystem::path commPath(QStr2Path(QDir::homePath()));
    commPath.append(".comm");

    std::ofstream commFile(commPath.string(), std::ios_base::trunc | std::ios_base::out);
    commFile << _tcpServer.serverPort();
    commFile.close();
#endif
}

void OldCommServer::onNewConnection() {
    LOG_DEBUG(Log::instance()->getLogger(), "New connection");

    if (_tcpSocket) {
        disconnect(_tcpSocket);
        if (_tcpSocket->isOpen()) {
            _tcpSocket->close();
        }
        _tcpSocket->deleteLater();
    }

    _tcpSocket = _tcpServer.nextPendingConnection();
    if (!_tcpSocket || !_tcpSocket->isValid()) {
        LOG_WARN(Log::instance()->getLogger(), "Error: got invalid pending connection!");
        return;
    }

    connect(_tcpSocket, &QTcpSocket::errorOccurred, this, &OldCommServer::onErrorOccurred);
    connect(_tcpSocket, &QTcpSocket::readyRead, this, &OldCommServer::onReadyRead);
    connect(_tcpSocket, &QTcpSocket::bytesWritten, this, &OldCommServer::onBytesWritten);
}

void OldCommServer::onBytesWritten(qint64 numBytes) {
    Q_UNUSED(numBytes)

    // LOG_DEBUG(Log::instance()->getLogger(), "Bytes written" << numBytes);
}

void OldCommServer::onReadyRead() {
    while (_tcpSocket && _tcpSocket->bytesAvailable() > 0) {
        // Read from socket
        _buffer.append(_tcpSocket->readAll());

        while (_buffer.size()) {
            // Read size
            if (_buffer.size() < (int) sizeof(qint32)) {
                break;
            }

            int size = CommonUtility::toInt(_buffer.mid(0, (qint32) sizeof(qint32)));

            // Read data
            if (_buffer.size() < (int) sizeof(qint32) + size) {
                break;
            }

            _buffer.remove(0, (int) sizeof(qint32));
            QByteArray data = _buffer.mid(0, size);
            _buffer.remove(0, size);

            // Parse request
            QJsonParseError parseError;
            QJsonDocument msgDoc = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                LOG_WARN(Log::instance()->getLogger(), "Bad message received!");
                return;
            }

            if (!msgDoc.isObject()) {
                LOG_WARN(Log::instance()->getLogger(), "Bad message received!");
                return;
            }

            QJsonObject msgObj = msgDoc.object();
            if (msgObj[MSG_TYPE].toInt() == toInt(MsgType::REQUEST)) {
                const int id(msgObj[MSG_REQUEST_ID].toInt());
                const RequestNum num(static_cast<RequestNum>(msgObj[MSG_REQUEST_NUM].toInt()));
                const QByteArray params(QByteArray::fromBase64(msgObj[MSG_REQUEST_PARAMS].toString().toUtf8()));

                // Request received
                LOG_DEBUG(Log::instance()->getLogger(), "Rqst rcvd " << id << " " << num);
                _requestWorker->addRequest(id, num, params);
            } else {
                LOG_WARN(Log::instance()->getLogger(), "Bad message received!");
            }
        }
    }
}

void OldCommServer::onErrorOccurred(QAbstractSocket::SocketError socketError) {
    QTcpSocket *socket = reinterpret_cast<QTcpSocket *>(sender());

    if (!_hasQuittedProperly) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Client connection was interrupted - err=" << (socket ? socket->errorString().toStdString() : "") << " ("
                                                            << socketError << ")");
        // Restart comm server
        start();
        // Restart client
        emit restartClient();
        return;
    }

    if (socketError == QTcpSocket::RemoteHostClosedError) {
        LOG_DEBUG(Log::instance()->getLogger(), "Client has closed connection");
        _requestWorker->clear();
    }
}

void OldCommServer::onRequestReceived(int id, RequestNum num, const QByteArray &params) {
    QTimer::singleShot(0, this, [=, this]() { emit requestReceived(id, (RequestNum) num, params); });
}

void OldCommServer::onSendReply(int id, const QByteArray &result) {
    if (!_tcpSocket) {
        LOG_WARN(Log::instance()->getLogger(), "Not connected!");
        return;
    }

    if (!_tcpSocket->isOpen()) {
        LOG_WARN(Log::instance()->getLogger(), "Socket closed!");
        return;
    }

    QJsonObject replyObj;
    replyObj[MSG_TYPE] = toInt(MsgType::REPLY);
    replyObj[MSG_REPLY_ID] = id;
    replyObj[MSG_REPLY_RESULT] = QString(result.toBase64());

    QJsonDocument replyDoc(replyObj);
    QByteArray reply(replyDoc.toJson(QJsonDocument::Compact));

    try {
        LOG_DEBUG(Log::instance()->getLogger(), "Snd rpl " << id);

        _tcpSocket->write(KDC::CommonUtility::toQByteArray(static_cast<int>(reply.size())));
        _tcpSocket->write(reply);
#ifdef Q_OS_WIN
        _tcpSocket->flush();
#endif
    } catch (...) {
        LOG_WARN(Log::instance()->getLogger(), "Socket write error!");
        return;
    }
}

void OldCommServer::onSendSignal(int id, SignalNum num, const QByteArray &params) {
    if (!_tcpSocket) {
        // LOG_WARN(Log::instance()->getLogger(), "Not connected!");
        return;
    }

    if (!_tcpSocket->isOpen()) {
        // LOG_WARN(Log::instance()->getLogger(), "Socket closed!");
        return;
    }

    QJsonObject signalObj;
    signalObj[MSG_TYPE] = toInt(MsgType::SIGNAL);
    signalObj[MSG_SIGNAL_ID] = id;
    signalObj[MSG_SIGNAL_NUM] = toInt(num);
    signalObj[MSG_SIGNAL_PARAMS] = QString(params.toBase64());

    QJsonDocument signalDoc(signalObj);
    QByteArray signal(signalDoc.toJson(QJsonDocument::Compact));

    try {
        LOG_DEBUG(Log::instance()->getLogger(), "Snd sgnl " << id << " " << num);

        _tcpSocket->write(KDC::CommonUtility::toQByteArray(static_cast<int>(signal.size())));
        _tcpSocket->write(signal);
#ifdef Q_OS_WIN
        _tcpSocket->flush();
#endif
    } catch (...) {
        LOG_WARN(Log::instance()->getLogger(), "Socket write error!");
        return;
    }
}

Worker::Worker(QObject *parent) :
    QObject(parent),
    _signalId(0),
    _stop(false) {}

Worker::~Worker() {}

void Worker::addRequest(int id, RequestNum num, const QByteArray &params) {
    _mutex.lock();
    _requestQueue.push_front(id);
    _requestMap[id] = qMakePair(num, params);
    _mutex.unlock();
    _queueWC.wakeAll();

    // LOG_DEBUG(Log::instance()->getLogger(), "Request added " << id);
}

void Worker::addReply(int id, const QByteArray &result) {
    _mutex.lock();
    _replyQueue.push_front(id);
    _replyMap[id] = result;
    _mutex.unlock();
    _queueWC.wakeAll();

    // LOG_DEBUG(Log::instance()->getLogger(), "Reply added " << id);
}

void Worker::addSignal(SignalNum num, const QByteArray &params, int &id) {
    _mutex.lock();
    id = _signalId++;
    _signalQueue.push_front(id);
    _signalMap[id] = qMakePair(num, params);
    _mutex.unlock();
    _queueWC.wakeAll();

    // LOG_DEBUG(Log::instance()->getLogger(), "Signal added " << id);
}

void Worker::clear() {
    _mutex.lock();
    _requestQueue.clear();
    _replyQueue.clear();
    _signalQueue.clear();
    _requestMap.clear();
    _replyMap.clear();
    _signalMap.clear();
    _mutex.unlock();
}

void Worker::stop() {
    _mutex.lock();
    _stop = true;
    _mutex.unlock();
    _queueWC.wakeAll();
}

void Worker::onStart() {
    LOG_DEBUG(Log::instance()->getLogger(), "Worker started");

    forever {
        _mutex.lock();
        if (_requestQueue.empty() && _replyQueue.empty() && _signalQueue.empty() && !_stop) {
            // LOG_DEBUG(Log::instance()->getLogger(), "Worker wait");
            _queueWC.wait(&_mutex);
        }

        if (_stop) {
            _mutex.unlock();
            break;
        }

        if (!_replyQueue.empty()) {
            // 1- process replies
            int id = _replyQueue.back();
            QByteArray reply = _replyMap[id];
            _replyQueue.pop_back();
            _replyMap.remove(id);
            _mutex.unlock();

            // LOG_DEBUG(Log::instance()->getLogger(), "Worker process reply " << id);
            emit sendReply(id, reply);
        } else if (!_requestQueue.empty()) {
            // 2- process new requests
            int id = _requestQueue.back();
            RequestNum num = _requestMap[id].first;
            QByteArray params = _requestMap[id].second;
            _requestQueue.pop_back();
            _requestMap.remove(id);
            _mutex.unlock();

            // LOG_DEBUG(Log::instance()->getLogger(), "Worker process request " << id << " " << num);
            emit requestReceived(id, num, params);
        } else if (!_signalQueue.empty()) {
            // 3- process signals
            int id = _signalQueue.back();
            SignalNum num = _signalMap[id].first;
            QByteArray params = _signalMap[id].second;
            _signalQueue.pop_back();
            _signalMap.remove(id);
            _mutex.unlock();

            // LOG_DEBUG(Log::instance()->getLogger(), "Worker process signal " << id << " " << num);
            emit sendSignal(id, num, params);
        } else {
            _mutex.unlock();
        }
    }

    LOG_DEBUG(Log::instance()->getLogger(), "Worker ended");
}

} // namespace KDC
