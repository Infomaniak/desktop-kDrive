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

#include "MatomoClient.h"
#include <QCoreApplication>
#include "config.h"

namespace KDC {

/**
 * Singleton instance of MatomoClient.
 *
 * @param clientId clientId (field _id & cid in the url), if empty, a new one is generated and stored in QSettings.
 * @return the MatomoClient instance
 */
MatomoClient& MatomoClient::instance(const QString& clientId)
{
    static MatomoClient _inst(
        qobject_cast<QCoreApplication*>(QCoreApplication::instance()),
        clientId
    );
    // We needed to set the parent to the QCoreApplication instance to retrieve screen resolutions (and other details) in PiwikTracker,
    // but we then set it to nullptr to prevent Qt from freeing this object when QCoreApplication is destroyed.
    _inst.setParent(nullptr);
    return _inst;
}

/**
 * Private constructor, called by the singleton instance() method.<br>
 * `MATOMO_URL` / `MATOMO_SITE_ID` are hard-coded and obtained from the config.h file.
 *
 * @param app the QCoreApplication instance
 * @param clientId the clientId (field _id & cid in the URL), if empty, a new one is generated and stored in QSettings.
 */
MatomoClient::MatomoClient(QCoreApplication* app, const QString& clientId)
    : PiwikTracker(app,
                   QUrl(MATOMO_URL),
                   MATOMO_SITE_ID,
                   clientId) {}

/**
 * Sends a visitPage request to Matomo.
 *
 * @param page the page to track
 */
void MatomoClient::sendVisit(const MatomoNameField page)
{
    QString path;
    QString action;
    switch (page) {
#ifdef Q_OS_WIN
        case Matomo_NameField::WV_ReleaseNotes :                    path = "webview";                       action = "release-notes";         break;
#endif
        case MatomoNameField::VW_LoginPage:                         path = "webview";                       action = "login";                 break;

        case MatomoNameField::PG_SynthesisPopover:                  path = "popover";                       action = "popover";               break;
        case MatomoNameField::PG_SynthesisPopover_KebabMenu:        path = "popover";                       action = "kebab_menu";            break;

        case MatomoNameField::PG_Preferences:                       path = "preferences";                   action = "preferences";           break;
        case MatomoNameField::PG_Preferences_Debugging:             path = "preferences/debugging";         action = "debugging";             break;
        case MatomoNameField::PG_Preferences_FilesToExclude:        path = "preferences/files_to_exclude";  action = "files_to_exclude";      break;
        case MatomoNameField::PG_Preferences_Proxy:                 path = "preferences/proxy";             action = "proxy";                 break;
#ifdef Q_OS_MAC
        case MatomoNameField::PG_Preferences_LiteSync:              path = "preferences/litesync";          action = "litesync";              break;
#endif
        case MatomoNameField::PG_Preferences_About:                 path = "preferences/about";             action = "about";                 break;
        case MatomoNameField::PG_Preferences_Beta:                  path = "preferences/beta";              action = "beta";                  break;

        case MatomoNameField::PG_Parameters:                        path = "parameters";                    action = "parameters";            break;
        case MatomoNameField::PG_Parameters_NewSync_LocalFolder:    path = "parameters/newsync";            action = "local_folder";          break;
        case MatomoNameField::PG_Parameters_NewSync_RemoteFolder:   path = "parameters/newsync";            action = "remote_folder";         break;
        case MatomoNameField::PG_Parameters_NewSync_Summary:        path = "parameters/newsync";            action = "summary";               break;

        case MatomoNameField::PG_AddNewDrive_SelectDrive:           path = "add_new_drive";                 action = "select_drive";          break;
        case MatomoNameField::PG_AddNewDrive_ActivateLiteSync:      path = "add_new_drive";                 action = "activate_litesync";     break;
        case MatomoNameField::PG_AddNewDrive_SelectRemoteFolder:    path = "add_new_drive";                 action = "select_remote_folder";  break;
        case MatomoNameField::PG_AddNewDrive_SelectLocalFolder:     path = "add_new_drive";                 action = "select_local_folder";   break;
        case MatomoNameField::PG_AddNewDrive_ExtensionSetup:        path = "add_new_drive";                 action = "extension_setup";       break;
        case MatomoNameField::PG_AddNewDrive_Confirmation:          path = "add_new_drive";                 action = "confirmation";          break;


        default: // MatomoNameField::Unknown
            action = path = "unknown";
            qCWarning(lcMatomoClient) << "MatomoClient::sendVisit triggered with page value unknown (" << static_cast<uint8_t>(page) << ").";
            break;
    }

    qCDebug(lcMatomoClient()) << "MatomoClient::sendVisit(page=" << static_cast<int>(page) << ")";
    instance().PiwikTracker::sendVisit(path, action);
}

/**
 * Sends an Event request to Matomo.
 *
 * @param category the category of the event (or the path)
 * @param action the action of the event, CLICK, INPUT or UNKNOWN @see Matomo_Event_Action
 * @param name the name of the event
 * @param value the value of the event
 */
void MatomoClient::sendEvent(const QString& category,
                             const MatomoEventAction action,
                             const QString& name,
                             const int value)
{

    QString actionStr;
    switch (action) {
        case MatomoEventAction::Click:  actionStr = "click"; break;
        case MatomoEventAction::Input:  actionStr = "input"; break;
        default:                        actionStr = "unknown"; break;
    }

    qCDebug(lcMatomoClient()) << "MatomoClient::sendEvent(category=" << category << ", action=" << actionStr << ", name=" << name << ", value=" << value << ")";
    instance().PiwikTracker::sendEvent(
        category,
        category,
        actionStr,
        name,
        value
    );
}

} // namespace KDC