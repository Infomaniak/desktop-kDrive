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


#pragma once

#include "3rdparty/qt-piwik-tracker/piwiktracker.h"

namespace KDC {
    using matomo_enum_t = uint8_t;

    enum class MatomoNameField : matomo_enum_t {
        /* WebView names*/
        WV_LoginPage, // Login

        /* Pages */
        PG_SynthesisPopover,
        PG_SynthesisPopover_KebabMenu,

        PG_Preferences,
        PG_Preferences_Debugging,
        PG_Preferences_FilesToExclude,
        PG_Preferences_Proxy,
#ifdef Q_OS_MAC
        PG_Preferences_LiteSync,
#endif
        PG_Preferences_About,
        PG_Preferences_Beta,
#ifdef Q_OS_WIN
        PG_Preferences_UpdateDialog,
#endif
        PG_Parameters,
        PG_Parameters_NewSync_LocalFolder,
        PG_Parameters_NewSync_RemoteFolder,
        PG_Parameters_NewSync_Summary,

        PG_AddNewDrive_SelectDrive,
        PG_AddNewDrive_ActivateLiteSync,
        PG_AddNewDrive_SelectRemoteFolder,
        PG_AddNewDrive_SelectLocalFolder,
        PG_AddNewDrive_ExtensionSetup,
        PG_AddNewDrive_Confirmation,

        Unknown // Default Case
    };

    /**
     * Enum representing different actions that can be tracked in Matomo.
     */
    enum class MatomoEventAction : matomo_enum_t {
        Click,
        Input,
        Unknown // for now, only CLICK is implemented
    };

    class MatomoClient final : public PiwikTracker {
        public:
            /**
             * Meyer singleton pattern
             *
             * Delete copy constructor and assignment operator.
             */
            static MatomoClient &instance(const QString &clientId = QString());
            MatomoClient(const MatomoClient &) = delete;
            MatomoClient &operator=(const MatomoClient &) = delete;

            static void sendVisit(MatomoNameField page);
            static void sendEvent(const QString &category, // the category of the event (or equals to the path)
                                  MatomoEventAction action = MatomoEventAction::Click, // the action of the event, CLICK or
                                                                                       // UNKNOWN @see Matomo_Event_Action
                                  const QString &name = QString(), // the name of the event (here, the name of the button)
                                  int value = 0);

        private:
            MatomoClient(QCoreApplication *app, const QString &clientId);

            std::unordered_map<MatomoNameField, std::pair<QString, QString>> _nameFieldMap;
            void initNameFieldMap();
            void getPathAndAction(MatomoNameField name, QString &path, QString &action) const;
    };

} // namespace KDC
