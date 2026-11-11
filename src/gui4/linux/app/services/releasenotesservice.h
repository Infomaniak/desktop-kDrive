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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QObject>
#include <QPointer>
#include <QVariantList>

#include <optional>

class QNetworkReply;

namespace KDC {

class ParametersStore;

/**
 * Downloads the published release notes of a version and exposes them as sections of items. They are shown only when
 * the configured proxy can be applied by the client: an authenticated proxy keeps its password on the server side.
 */
class ReleaseNotesService final : public QObject {
        Q_OBJECT

    public:
        explicit ReleaseNotesService(const ParametersStore &parametersStore, QObject *parent = nullptr);

        void load(const QString &tag, const QString &languageCode);

        /** Sections as `{ title: string, items: string[] }`, empty when the notes are unavailable. */
        [[nodiscard]] const QVariantList &sections() const { return _sections; }

        [[nodiscard]] static QVariantList parse(const QString &html);

    signals:
        void changed();

    private:
        void request(const QString &languageCode);
        [[nodiscard]] std::optional<QNetworkProxy> proxy(const QUrl &url) const;

        const ParametersStore &_parametersStore;
        QNetworkAccessManager _network;
        QPointer<QNetworkReply> _reply;
        QString _tag;
        QString _languageCode;
        QVariantList _sections;
};

} // namespace KDC
