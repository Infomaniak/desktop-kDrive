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

using namespace KDC;

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
    QString action;
    QString path;
    switch (page) {
#ifdef Q_OS_WIN
        case Matomo_NameField::WV_ReleaseNotes :    action = "release-notes"; path = "WebView"; break;
#endif
        case MatomoNameField::VW_LoginPage:        action = "login";      path = "WebView";    break;
        case MatomoNameField::PG_SynthesisPopover: action = "popover";    path = "popover";       break;

        default: // Matomo_NameField::Unknown
            action = path = "unknown";
            qCWarning(lcMatomoClient) << "MatomoClient::sendVisit triggered with page value unknown (" << static_cast<uint8_t>(page) << ").";
            break;
    }

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
        default:                          actionStr = "unknown"; break;
    }

    instance().PiwikTracker::sendEvent(
        category,
        category,
        actionStr,
        name,
        value
    );
}
