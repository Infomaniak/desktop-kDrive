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

#include "debugreporter.h"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFileInfo>
#include <QFile>
#include <QVBoxLayout>
#include <QVector>

namespace KDC {

static const QVector<QString> mapKeyName = QVector<QString>() << QString("Drive id") << QString("Drive") << QString("User id")
                                                              << QString("User name") << QString("Log file");

Q_LOGGING_CATEGORY(lcDebugReporter, "gui.debugreporter", QtInfoMsg)

DebugReporter::DebugReporter(const QUrl &url, QWidget *parent) :
    QProgressDialog(tr("Sending of debugging information"), tr("Cancel"), 0, 100, parent), m_reply(0), m_url(url) {
    // Add a layout in order to auto-resize to the content
    QVBoxLayout *layout = new QVBoxLayout;
    foreach (QObject *obj, children()) {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (widget) layout->addWidget(widget);
    }
    setLayout(layout);

    m_request = new QNetworkRequest(m_url);
}

DebugReporter::~DebugReporter() {
    delete m_request;
    delete m_reply;
}

void DebugReporter::setReportData(MapKeyType keyType, int keyIndex, const QByteArray &content) {
    m_formContents.insert(MapKey(keyType, keyIndex), content);
}

void DebugReporter::setReportData(MapKeyType keyType, int keyIndex, const QByteArray &content, const QByteArray &contentType,
                                  const QByteArray &fileName) {
    setReportData(keyType, keyIndex, content);

    if (!contentType.isEmpty() && !fileName.isEmpty()) {
        m_formContentTypes.insert(MapKey(keyType, keyIndex), contentType);
        m_formFileNames.insert(MapKey(keyType, keyIndex), fileName);
    }
}

void DebugReporter::send() {
    QByteArray body;
    foreach (const MapKey &mapKey, m_formContents.keys()) {
        QString value = m_formContents.value(mapKey);
        if (!value.isEmpty()) {
            QString name = QString("%1 %2").arg(mapKeyName[mapKey.keyType()]).arg(mapKey.keyIndex());
            body += "--thkboundary\r\n";
            body += "Content-Disposition: form-data; name=\"" + name + "\"";

            if (!m_formFileNames.value(mapKey).isEmpty() && !m_formContentTypes.value(mapKey).isEmpty()) {
                body += "; filename=\"" + m_formFileNames.value(mapKey) + "\"\r\n";
                body += "Content-Type: " + m_formContentTypes.value(mapKey) + "\r\n";
            } else {
                body += "\r\n";
            }

            body += "\r\n";
            body += m_formContents.value(mapKey) + "\r\n";
        } else {
            qCDebug(lcDebugReporter) << "Empty value for key:" << mapKey.keyType() << mapKey.keyIndex();
        }
    }

    body += "--thkboundary\r\n";


    QNetworkAccessManager *nam = new QNetworkAccessManager();
    m_request->setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data; boundary=thkboundary");
    m_reply = nam->post(*m_request, body);

    connect(m_reply, &QNetworkReply::finished, this, &DebugReporter::onDone, Qt::QueuedConnection);
    connect(m_reply, &QNetworkReply::uploadProgress, this, &DebugReporter::onProgress);
}

void DebugReporter::onDone() {
    QByteArray data = m_reply->readAll();
    QString const response = QString::fromUtf8(data);

    if ((m_reply->error() != QNetworkReply::NoError) || !response.startsWith("DebugID=", Qt::CaseInsensitive)) {
        qCDebug(lcDebugReporter) << "Debug report error:" << m_reply->error() << " response:" << response;
        onFail(m_reply->error(), m_reply->errorString());
    } else {
        QString debugId = response.split("\n").at(0).split("=").at(1);
        qCDebug(lcDebugReporter) << "Debug report sent:" << debugId;

        emit sent(true, debugId);

        reset();
    }
}

void DebugReporter::onProgress(qint64 done, qint64 total) {
    if (wasCanceled()) {
        m_reply->abort();
    } else {
        if (total > 0) {
            setValue(static_cast<int>(100 * done / total));
        }
    }
}

void DebugReporter::onFail(int error, const QString &errorString) {
    qCDebug(lcDebugReporter) << "Debug report not sent:" << error << errorString;

    emit sent(false);

    reset();
}

} // namespace KDC
