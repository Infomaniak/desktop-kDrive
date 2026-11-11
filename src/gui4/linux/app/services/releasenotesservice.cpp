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

#include "releasenotesservice.h"

#include "app/cache/parametersstore.h"
#include "config.h"

#include <QLoggingCategory>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTextDocumentFragment>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcReleaseNotesService, "gui.v4.releasenotes", QtInfoMsg)

constexpr auto englishCode = "en";

QString plainText(const QString &html) {
    return QTextDocumentFragment::fromHtml(html).toPlainText().simplified();
}
} // namespace

ReleaseNotesService::ReleaseNotesService(const ParametersStore &parametersStore, QObject *const parent) :
    QObject(parent),
    _parametersStore(parametersStore) {}

void ReleaseNotesService::load(const QString &tag, const QString &languageCode) {
    if (tag == _tag && languageCode == _languageCode) {
        return;
    }

    _tag = tag;
    _languageCode = languageCode;
    if (_reply) {
        _reply->abort();
    }
    if (!_sections.isEmpty()) {
        _sections.clear();
        emit changed();
    }

    if (!_tag.isEmpty()) {
        request(_languageCode);
    }
}

// Published as kDrive-<tag>-linux-<language>.html; a language without notes falls back to English.
void ReleaseNotesService::request(const QString &languageCode) {
    const QUrl url(u"%1-%2-linux-%3.html"_s.arg(QStringLiteral(APPLICATION_STORAGE_URL), _tag, languageCode));
    const auto networkProxy = proxy(url);
    if (!networkProxy) {
        qCInfo(lcReleaseNotesService) << "Release notes not loaded: the configured proxy cannot be used by the client";
        return;
    }

    _network.setProxy(*networkProxy);
    auto *const reply = _network.get(QNetworkRequest(url));
    _reply = reply;
    (void) connect(reply, &QNetworkReply::finished, this, [this, reply, languageCode, tag = _tag] {
        reply->deleteLater();
        if (tag != _tag || reply->error() == QNetworkReply::OperationCanceledError) {
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            qCInfo(lcReleaseNotesService) << "Release notes unavailable | url:" << reply->url() << "/ error:" << reply->error();
            if (languageCode != QLatin1String(englishCode)) {
                request(QString::fromLatin1(englishCode));
            }
            return;
        }

        _sections = parse(QString::fromUtf8(reply->readAll()));
        qCInfo(lcReleaseNotesService) << "Release notes loaded | url:" << reply->url() << "/ sections:" << _sections.size();
        emit changed();
    });
}

std::optional<QNetworkProxy> ReleaseNotesService::proxy(const QUrl &url) const {
    const auto parameters = _parametersStore.parameters();
    if (!parameters) {
        return std::nullopt;
    }

    const auto &proxyConfig = parameters->proxyConfig();
    switch (proxyConfig.type()) {
        case ProxyType::None:
            return QNetworkProxy(QNetworkProxy::NoProxy);
        case ProxyType::System: {
            const auto proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));
            return proxies.isEmpty() ? QNetworkProxy(QNetworkProxy::NoProxy) : proxies.first();
        }
        case ProxyType::HTTP:
            if (proxyConfig.needsAuth()) {
                return std::nullopt;
            }
            return QNetworkProxy(QNetworkProxy::HttpProxy, QString::fromStdString(proxyConfig.hostName()),
                                 static_cast<quint16>(proxyConfig.port()));
        default:
            return std::nullopt;
    }
}

// Keeps the section titles (h2) and their items (li), in document order.
QVariantList ReleaseNotesService::parse(const QString &html) {
    static const QRegularExpression element(
            u"<(h2|li)[^>]*>(.*?)</\\1>"_s,
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    QVariantList sections;
    QString title;
    QStringList items;
    const auto flush = [&] {
        if (!items.isEmpty()) {
            sections.append(QVariantMap{{u"title"_s, title}, {u"items"_s, items}});
        }
        items.clear();
    };

    for (auto it = element.globalMatch(html); it.hasNext();) {
        const auto match = it.next();
        const auto text = plainText(match.captured(2));
        if (match.captured(1).compare(u"h2"_s, Qt::CaseInsensitive) == 0) {
            flush();
            title = text;
        } else if (!text.isEmpty()) {
            items.append(text);
        }
    }
    flush();

    return sections;
}

} // namespace KDC
