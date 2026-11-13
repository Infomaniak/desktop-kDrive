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

#pragma once

#include <QJSValue>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <utility>

namespace KDC {

/**
 * Single entry point for user interactions, called from QML and from the system tray menu.
 *
 * Role: write one stable, greppable and language-independent line per interaction in the `gui.v4.useraction` category
 * (also a Sentry breadcrumb).
 * Ids are technical (`settings.general.autoStart`); details carry ids, booleans and enum values only, never displayed or
 * typed text, local paths, exclusion patterns or proxy settings.
 *
 * Lines: `User action | <actionId>`, `Screen shown | <screenId>`, `Dialog opened|closed | <dialogId>` and
 * `Window shown|hidden | <windowId>`, each followed by its details as `| key: value`.
 */
class UserActionLog final : public QObject {
        Q_OBJECT
        QML_ELEMENT
        QML_SINGLETON
        QML_UNCREATABLE("UserActionLog is owned by AppClientLinux and provided as an external singleton.")

    public:
        /** Ordered `key: value` context of a line. */
        using Details = QList<std::pair<QString, QVariant>>;

        explicit UserActionLog(QObject *parent = nullptr);

        /** `details` is a JavaScript object whose properties are logged sorted by name. */
        Q_INVOKABLE void action(const QString &actionId, const QJSValue &details = QJSValue()) const;
        Q_INVOKABLE void screen(const QString &screenId, const QJSValue &details = QJSValue()) const;
        Q_INVOKABLE void dialog(const QString &dialogId, bool opened, const QJSValue &details = QJSValue()) const;
        Q_INVOKABLE void window(const QString &windowId, bool shown, const QJSValue &details = QJSValue()) const;

        /** C++ counterpart of `action()`. */
        void recordAction(const QString &actionId, const Details &details = {}) const;

    private:
        void recordScreen(const QString &screenId, const Details &details) const;
        void write(const QString &event, const QString &id, const Details &details) const;
};

} // namespace KDC
