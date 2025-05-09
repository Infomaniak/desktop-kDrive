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

#ifndef MATOMOCLIENT_H
#define MATOMOCLIENT_H

#pragma once

#include "../3rdparty/qt-piwik-tracker/piwiktracker.h"

inline Q_LOGGING_CATEGORY(lcMatomoClient, "gui.matomo", QtInfoMsg)

namespace KDC {

    enum class MatomoNameField : uint8_t {
        /* WebView names*/
        VW_LoginPage,             // Login
#ifdef Q_OS_WIN
        WV_ReleaseNotes, // Release Notes Webview (only rendered on windows)
#endif
        /* Pages */
        PG_SynthesisPopover,
        PG_SynthesisPopover_KebabMenu,

        PG_Preferences,
        PG_Preferences_Debugging,
        PG_Preferences_FileToExclude,
        PG_Preferences_Proxy,
#ifdef Q_OS_MAC
        PG_Preferences_LiteSync,
#endif
        PG_Preferences_About,
        PG_Preferences_Beta,

        PG_Parameters,

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
enum class MatomoEventAction : uint8_t {
    Click,
    Input,
    Unknown // for now, only CLICK is implemented
};

class MatomoClient final : public PiwikTracker
{
    public:
        static MatomoClient& instance(const QString& clientId = QString());

        static void sendVisit(MatomoNameField page);
        static void sendEvent(const QString& category,                  // the category of the event (or equals to the path)
                              MatomoEventAction action = MatomoEventAction::Click,   // the action of the event, CLICK or UNKNOWN @see Matomo_Event_Action
                              const QString& name = QString(),      // the name of the event (here, the name of the button)
                              int value = 0);

        /**
         * Disable copy constructor and assignment operator, because this class is a singleton
         */
        MatomoClient(const MatomoClient&) = delete;
        MatomoClient& operator=(const MatomoClient&) = delete;

    private:
        MatomoClient(QCoreApplication* app, const QString& clientId);
};

} // namespace KDC


#endif //MATOMOCLIENT_H
