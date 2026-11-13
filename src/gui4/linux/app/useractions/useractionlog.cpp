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

#include "useractionlog.h"

#include "app/services/matomoservice.h"
#include "app/useractions/analyticscatalog.h"

#include <QJSValueIterator>
#include <QLoggingCategory>

#include <algorithm>
#include <optional>

using namespace Qt::StringLiterals;

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcUserAction, "gui.v4.useraction", QtInfoMsg)

/**
 * Reads the own properties of a JavaScript object, sorted by name: compiled QML can hand the object over as a map, which
 * loses the declaration order. Anything else (undefined, a primitive) yields no detail.
 */
UserActionLog::Details detailsFromJs(const QJSValue &value) {
    UserActionLog::Details details;
    if (!value.isObject()) {
        return details;
    }

    QJSValueIterator iterator(value);
    while (iterator.hasNext()) {
        (void) iterator.next();
        details.append({iterator.name(), iterator.value().toVariant()});
    }

    (void) std::ranges::sort(details, {}, &std::pair<QString, QVariant>::first);
    return details;
}

/**
 * Event value read from the detail named by the catalog: a boolean becomes 1 or 0, a number is kept as is.
 */
std::optional<double> eventValue(const AnalyticsCatalog::Event &event, const UserActionLog::Details &details) {
    if (event.valueDetail.isEmpty()) {
        return std::nullopt;
    }

    for (const auto &[key, value]: details) {
        if (key != event.valueDetail) {
            continue;
        }

        if (value.typeId() == QMetaType::Bool) {
            return value.toBool() ? 1.0 : 0.0;
        }

        bool isNumber = false;
        if (const double number = value.toDouble(&isNumber); isNumber) {
            return number;
        }

        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace

UserActionLog::UserActionLog(MatomoService &matomoService, QObject *const parent) :
    QObject(parent),
    _matomoService(matomoService) {}

void UserActionLog::action(const QString &actionId, const QJSValue &details) const {
    recordAction(actionId, detailsFromJs(details));
}

void UserActionLog::screen(const QString &screenId, const QJSValue &details) const {
    recordScreen(screenId, detailsFromJs(details));
}

void UserActionLog::dialog(const QString &dialogId, const bool opened, const QJSValue &details) const {
    write(opened ? u"Dialog opened"_s : u"Dialog closed"_s, dialogId, detailsFromJs(details));

    if (!opened) {
        return;
    }

    if (const auto category = AnalyticsCatalog::pageViewForScreen(dialogId); category.has_value()) {
        _matomoService.trackPageView(*category);
    }
}

void UserActionLog::window(const QString &windowId, const bool shown, const QJSValue &details) const {
    write(shown ? u"Window shown"_s : u"Window hidden"_s, windowId, detailsFromJs(details));
}

void UserActionLog::recordAction(const QString &actionId, const Details &details) const {
    write(u"User action"_s, actionId, details);

    if (const auto event = AnalyticsCatalog::eventForAction(actionId); event.has_value()) {
        _matomoService.trackEvent(event->category, event->name, eventValue(*event, details));
    }
}

void UserActionLog::recordScreen(const QString &screenId, const Details &details) const {
    write(u"Screen shown"_s, screenId, details);

    if (const auto category = AnalyticsCatalog::pageViewForScreen(screenId); category.has_value()) {
        _matomoService.trackPageView(*category);
    }
}

void UserActionLog::write(const QString &event, const QString &id, const Details &details) const {
    QString message = event + u" | "_s + id;
    for (const auto &[key, value]: details) {
        message += u" | "_s + key + u": "_s + value.toString();
    }

    qCInfo(lcUserAction).noquote() << message;
}

} // namespace KDC
