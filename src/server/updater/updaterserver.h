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

#include "libcommon/updater.h"

#include <QLoggingCategory>
#include <QObject>

class QUrl;
class QUrlQuery;

namespace KDC {

Q_DECLARE_LOGGING_CATEGORY(lcUpdater)

using QuitCallback = std::function<void()>;

class UpdaterServer : public Updater {
        Q_OBJECT
    public:
        struct Helper {
                static qint64 stringVersionToInt(const QString &version);
                static qint64 currentVersionToInt();
                static qint64 versionToInt(qint64 major, qint64 minor, qint64 patch, qint64 build);
        };

        static UpdaterServer *instance();
        static QUrl updateUrl();

        QString version() const override;
        bool isKDCUpdater() override;
        bool isSparkleUpdater() override;
        QString statusString() const override;
        bool downloadCompleted() const override;
        bool updateFound() const override;
        void startInstaller() const override;

        virtual void checkForUpdate() = 0;
        virtual void backgroundCheckForUpdate() = 0;
        virtual bool handleStartup() = 0;

#ifdef Q_OS_MACOS
        virtual void setQuitCallback(const QuitCallback &quitCallback) = 0;
#endif

    protected:
        static QString clientVersion();
        UpdaterServer() : Updater(nullptr) {}

    private:
        static QString getSystemInfo();
        static QUrlQuery getQueryParams();
        static UpdaterServer *create();
        static UpdaterServer *_instance;
};

} // namespace KDC
