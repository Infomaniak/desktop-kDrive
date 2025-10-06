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

#include "commclient.h"
#include "libcommon/utility/utility.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

namespace KDC {

std::shared_ptr<CommClient> CommClient::_instance = 0;

Q_LOGGING_CATEGORY(lcCommClient, "gui.commclient", QtInfoMsg)

std::shared_ptr<CommClient> CommClient::instance(QObject *parent) {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<CommClient>(new CommClient(parent));
        } catch (...) {
            return nullptr;
        }
    }

    return _instance;
}

CommClient::CommClient(QObject *parent) :
    QObject(parent),
    _requestWorkerThread(new QtLoggingThread()),
    _requestWorker(new Worker()),
    _tcpConnection(new QTcpSocket()),
    _buffer(QByteArray()) {
    // Start worker thread
    _requestWorker->moveToThread(_requestWorkerThread);
    connect(_requestWorkerThread, &QThread::started, _requestWorker, &Worker::onStart);
    connect(_requestWorker, &Worker::finished, _requestWorkerThread, &QThread::quit);
    connect(_requestWorker, &Worker::finished, _requestWorker, &Worker::deleteLater);
    connect(_requestWorkerThread, &QThread::finished, _requestWorkerThread, &QObject::deleteLater);
    connect(_requestWorker, &Worker::sendRequest, this, &CommClient::onSendRequest);
    connect(_requestWorker, &Worker::signalReceived, this, &CommClient::onSignalReceived);

    _requestWorkerThread->start();
    _requestWorkerThread->setPriority(QThread::Priority::HighestPriority);
}

bool CommClient::connectToServer(quint16 commPort) {
    if (commPort == 0) {
        qCDebug(lcCommClient()) << "Server is not running!";
        return false;
    }

    // Connection to server
    connect(_tcpConnection, &QTcpSocket::disconnected, this, &CommClient::onDisconnected);
    connect(_tcpConnection, &QTcpSocket::errorOccurred, this, &CommClient::onErrorOccurred);
    connect(_tcpConnection, &QTcpSocket::bytesWritten, this, &CommClient::onBytesWritten);
    connect(_tcpConnection, &QTcpSocket::readyRead, this, &CommClient::onReadyRead);

    QEventLoop waitLoop(this);
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &waitLoop, [&]() {
        qCDebug(lcCommClient()) << "Connection timeout";
        waitLoop.exit(false);
    });

    connect(_tcpConnection, &QTcpSocket::connected, &waitLoop, [&]() {
        qCDebug(lcCommClient()) << "Connection done";
        timer.stop();
        waitLoop.exit(true);
    });

    qCDebug(lcCommClient()) << "Connection to server port" << commPort;
    _tcpConnection->connectToHost(QHostAddress::LocalHost, commPort);

    timer.start(COMM_AVERAGE_TIMEOUT);
    if (!waitLoop.exec()) {
        return false;
    }

    return true;
}

bool CommClient::sendRequest(int id, RequestNum num, const QByteArray &params) {
    if (!_tcpConnection) {
        qCWarning(lcCommClient()) << "Not connected!";
        return false;
    }

    if (!_tcpConnection->isOpen()) {
        qCWarning(lcCommClient()) << "Socket closed!";
        return false;
    }

    QJsonObject requestObj;
    requestObj[MSG_TYPE] = toInt(MsgType::REQUEST);
    requestObj[MSG_REQUEST_ID] = id;
    requestObj[MSG_REQUEST_NUM] = toInt(num);
    requestObj[MSG_REQUEST_PARAMS] = QString(params.toBase64());

    QJsonDocument requestDoc(requestObj);
    QByteArray request(requestDoc.toJson(QJsonDocument::Compact));

    try {
        qCDebug(lcCommClient()) << "Snd rqst" << id << num;

        _tcpConnection->write(KDC::CommonUtility::toQByteArray(static_cast<int>(request.size())));
        _tcpConnection->write(request);
#ifdef Q_OS_WIN
        _tcpConnection->flush();
#endif
    } catch (...) {
        qCWarning(lcCommClient()) << "Socket write error!";
        return false;
    }

    return true;
}

void CommClient::onBytesWritten(qint64 numBytes) {
    Q_UNUSED(numBytes)

    // qCDebug(lcCommClient()) << "Bytes written" << numBytes;
}

void CommClient::onReadyRead() {
    while (_tcpConnection && _tcpConnection->bytesAvailable() > 0) {
        // Read from socket
        _buffer.append(_tcpConnection->readAll());

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

            // Parse reply
            QJsonParseError parseError;
            QJsonDocument msgDoc = QJsonDocument::fromJson(data, &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                qCWarning(lcCommClient()) << "Bad message received!";
                return;
            }

            if (!msgDoc.isObject()) {
                qCWarning(lcCommClient()) << "Bad message received!";
                return;
            }

            QJsonObject msgObj = msgDoc.object();
            if (msgObj[MSG_TYPE].toInt() == toInt(MsgType::REPLY)) {
                const int id(msgObj[MSG_REPLY_ID].toInt());
                const QByteArray result(QByteArray::fromBase64(msgObj[MSG_REPLY_RESULT].toString().toUtf8()));

                // Add reply to worker queue
                _requestWorker->addReply(id, result);
            } else if (msgObj[MSG_TYPE].toInt() == toInt(MsgType::SIGNAL)) {
                const int id(msgObj[MSG_SIGNAL_ID].toInt());
                const SignalNum num(static_cast<SignalNum>(msgObj[MSG_SIGNAL_NUM].toInt()));
                const QByteArray params(QByteArray::fromBase64(msgObj[MSG_SIGNAL_PARAMS].toString().toUtf8()));

                // Add signal to worker queue
                _requestWorker->addSignal(id, num, params);
            } else {
                qCWarning(lcCommClient()) << "Bad message received!";
            }
        }
    }
}

void CommClient::onDisconnected() {
    qCDebug(lcCommClient()) << "Client disconnected";

    QTcpSocket *socket = reinterpret_cast<QTcpSocket *>(sender());
    socket->deleteLater();
    _tcpConnection = nullptr;

    emit disconnected();
}

void CommClient::onErrorOccurred(QAbstractSocket::SocketError socketError) {
    QTcpSocket *socket = reinterpret_cast<QTcpSocket *>(sender());

    qCWarning(lcCommClient()) << "Error occurred!" << socket->errorString();

    if (socketError == QTcpSocket::RemoteHostClosedError) {
        socket->close();
    }
}

void CommClient::onSendRequest(int id, RequestNum num, const QByteArray &params) {
    QTimer::singleShot(0, this, [this, id, num, params]() {
        if (!sendRequest(id, num, params)) {
            emit sendError(id);
        }
    });
}

void CommClient::onSignalReceived(int id, SignalNum num, const QByteArray &params) {
    QTimer::singleShot(0, this, [this, id, num, params]() { emit signalReceived(id, num, params); });
}

CommClient::~CommClient() {
    if (isConnected()) {
        stop();
    }
}

bool CommClient::execute(const RequestNum num, const QByteArray &params, QByteArray &results,
                         const int timeout /*= COMM_SHORT_TIMEOUT*/) {
    if (!_tcpConnection) {
        return false;
    }

    int id = 0;
    QEventLoop waitLoop(this);

    // Set timer
    QTimer timer;
    timer.setSingleShot(true);
    timer.setTimerType(Qt::PreciseTimer);
    connect(&timer, &QTimer::timeout, &waitLoop, [&]() {
        qCDebug(lcCommClient()) << "Snd timeout" << id;
        waitLoop.exit(false);
    });

    connect(_requestWorker, &Worker::replyReceived, &waitLoop, [&](int idTmp, const QByteArray &resultsTmp) {
        if (id == idTmp) {
            qCDebug(lcCommClient()) << "Rpl rcvd" << id;
            results = resultsTmp;
            timer.stop();
            waitLoop.exit(true);
        }
    });

    connect(this, &CommClient::sendError, &waitLoop, [&](int idTmp) {
        if (id == idTmp) {
            qCDebug(lcCommClient()) << "Snd error" << id;
            timer.stop();
            waitLoop.exit(false);
        }
    });

    // Add request to worker queue
    _requestWorker->addRequest(num, params, id);

    // Wait for reply
    // qCDebug(lcCommClient()) << "Wait for reply" << id;
    timer.start(timeout);
    bool ret = waitLoop.exec(QEventLoop::ExcludeUserInputEvents);

    return ret;
}

bool CommClient::execute(const RequestNum num, const QByteArray &params, const int timeout) {
    QByteArray result;
    return execute(num, params, result, timeout);
}

bool CommClient::execute(const RequestNum num, const int timeout) {
    QByteArray results;
    return execute(num, {}, results, timeout);
}

void CommClient::stop() {
    _requestWorker->stop();

    _requestWorkerThread->quit();
    if (!_requestWorkerThread->wait(1000)) {
        _requestWorkerThread->terminate();
        _requestWorkerThread->wait();
    }

    delete _requestWorkerThread;
    delete _requestWorker;

    if (_tcpConnection) {
        if (_tcpConnection->isOpen()) {
            _tcpConnection->close();
        }
        // Test again the pointer value since it might has been set to nullptr in CommClient::onDisconnected()
        if (_tcpConnection) _tcpConnection->deleteLater();
    }

    _instance = nullptr;
}

Worker::Worker(QObject *parent) :
    QObject(parent),
    _requestId(0),
    _stop(false) {}

Worker::~Worker() {}

void Worker::addRequest(RequestNum num, const QByteArray &params, int &id) {
    _mutex.lock();
    id = _requestId++;
    _requestQueue.push_front(id);
    _requestMap[id] = qMakePair(num, params);
    _mutex.unlock();
    _queueWC.wakeAll();

    // qCDebug(lcCommClient()) << "Request added" << id;
}

void Worker::addReply(int id, const QByteArray &result) {
    _mutex.lock();
    _replyQueue.push_front(id);
    _replyMap[id] = result;
    _mutex.unlock();
    _queueWC.wakeAll();

    // qCDebug(lcCommClient()) << "Reply added" << id;
}

void Worker::addSignal(int id, SignalNum num, const QByteArray &params) {
    _mutex.lock();
    _signalQueue.push_front(id);
    _signalMap[id] = qMakePair(num, params);
    _mutex.unlock();
    _queueWC.wakeAll();

    // qCDebug(lcCommClient()) << "Signal added" << id;
}

void Worker::stop() {
    _mutex.lock();
    _stop = true;
    _mutex.unlock();
    _queueWC.wakeAll();
}

void Worker::onStart() {
    qCDebug(lcCommClient) << "Worker started";

    forever {
        _mutex.lock();
        if (_requestQueue.empty() && _replyQueue.empty() && _signalQueue.empty() && !_stop) {
            // qCDebug(lcCommClient) << "Worker wait";
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

            // qCDebug(lcCommClient) << "Worker process reply" << id;
            emit replyReceived(id, reply);
        } else if (!_signalQueue.empty()) {
            // 2- process signals
            int id = _signalQueue.back();
            SignalNum num = _signalMap[id].first;
            QByteArray params = _signalMap[id].second;
            _signalQueue.pop_back();
            _signalMap.remove(id);
            _mutex.unlock();

            // qCDebug(lcCommClient) << "Worker process signal" << id << num;
            emit signalReceived(id, num, params);
        } else if (!_requestQueue.empty()) {
            // 3- process new requests
            int id = _requestQueue.back();
            RequestNum num = _requestMap[id].first;
            QByteArray params = _requestMap[id].second;
            _requestQueue.pop_back();
            _requestMap.remove(id);
            _mutex.unlock();

            // qCDebug(lcCommClient) << "Worker process request" << id << num;
            emit sendRequest(id, num, params);
        } else {
            _mutex.unlock();
        }
    }

    qCDebug(lcCommClient) << "Worker ended";
}

} // namespace KDC
