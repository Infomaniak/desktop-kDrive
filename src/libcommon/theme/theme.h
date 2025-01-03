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

#include "../utility/types.h"

#include <QObject>
#include <QHash>

namespace KDC {

class Theme : public QObject {
        Q_OBJECT
    public:
        /* returns a singleton instance. */
        static Theme *instance();

        ~Theme();

        virtual QString appNameGUI() const;
        virtual QString appName() const;
        virtual QString appClientName() const;

        virtual QIcon syncStateIcon(KDC::SyncStatus status, bool sysTray = false, bool sysTrayMenuVisible = false,
                                    bool alert = false) const;

        virtual QIcon folderOfflineIcon(bool sysTray = false, bool sysTrayMenuVisible = false) const;
        virtual QIcon applicationIcon() const;

        virtual QString version() const;

        virtual QString helpUrl() const;
        QString feedbackUrl(Language language) const;

        virtual QString conflictHelpUrl() const;

        virtual QString defaultClientFolder() const;

        void setSystrayUseMonoIcons(bool mono);
        bool systrayUseMonoIcons() const;
        virtual QString debugReporterUrl() const;

        virtual bool linkSharing() const;
        virtual bool userGroupSharing() const;

        virtual QString versionSwitchOutput() const;

    protected:
        QIcon themeIcon(const QString &name, bool sysTray = false, bool sysTrayMenuVisible = false) const;
        void updateIconWithText(QIcon &icon, QString text) const;
        Theme();

    signals:
        void systrayUseMonoIconsChanged(bool);

    private:
        Theme(Theme const &);

        static Theme *_instance;
        bool _mono;
        mutable QHash<QString, QIcon> _iconCache;
};
} // namespace KDC
