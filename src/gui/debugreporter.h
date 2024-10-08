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

#include <QProgressDialog>
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace KDC {

class DebugReporter : public QProgressDialog {
        Q_OBJECT
    public:
        enum MapKeyType { DriveId = 0, DriveName, UserId, UserName, LogName };

        DebugReporter(const QUrl &url, QWidget *parent = nullptr);
        ~DebugReporter();

        void setReportData(MapKeyType keyType, int keyIndex, const QByteArray &content);
        void setReportData(MapKeyType keyType, int keyIndex, const QByteArray &content, const QByteArray &contentType,
                           const QByteArray &fileName);

    private:
        class MapKey {
            public:
                inline explicit MapKey(MapKeyType keyType, int keyIndex) : _keyType(keyType), _keyIndex(keyIndex) {}
                inline bool operator<(const MapKey &mapKey) const {
                    return (_keyType == mapKey._keyType ? _keyIndex < mapKey._keyIndex : _keyType < mapKey._keyType);
                }
                inline MapKeyType keyType() const { return _keyType; }
                inline int keyIndex() const { return _keyIndex; }

            private:
                MapKeyType _keyType;
                int _keyIndex;
        };

        QNetworkRequest *m_request;
        QNetworkReply *m_reply;
        QUrl m_url;

        QMap<MapKey, QByteArray> m_formContents;
        QMap<MapKey, QByteArray> m_formContentTypes;
        QMap<MapKey, QByteArray> m_formFileNames;

    signals:
        void sent(bool retCode, const QString &debugId = QString());

    public slots:
        void send();

    private slots:
        void onDone();
        void onProgress(qint64 sent, qint64 total);
        void onFail(int error, const QString &errorString);
};

} // namespace KDC
