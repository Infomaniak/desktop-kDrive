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

#include "matomoclient.h"
#include "config.h"
#include "gui/parameterscache.h"

#include <QCoreApplication>

inline Q_LOGGING_CATEGORY(lcMatomoClient, "gui.matomo", QtInfoMsg)

        namespace KDC {
    constexpr int matomoTimeout = 10000; // Timeout for Matomo requests in milliseconds
    constexpr int matomoTimeoutMax = 2; // Max number of Matomo timeout request before disabling Matomo tracking
    int matomoTimeoutCounter = 0; // Number of Matomo timeout requests
    bool matomoDisabled = false; // if true, Matomo is disabled and no events / page visit will be sent.

    /**
     * Singleton instance of MatomoClient.
     *
     * @param clientId clientId (field _id & cid in the url), if empty, a new one is generated and stored in QSettings.
     * @return the MatomoClient instance
     */
    MatomoClient &MatomoClient::instance(const QString &clientId) {
        static MatomoClient _inst(qobject_cast<QCoreApplication *>(QCoreApplication::instance()), clientId);
        // We needed to set the parent to the QCoreApplication instance to retrieve screen resolutions (and other details) in
        // PiwikTracker, but we then set it to nullptr to prevent Qt from freeing this object when QCoreApplication is destroyed.
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
    MatomoClient::MatomoClient(QCoreApplication * app, const QString &clientId) :
        PiwikTracker(app, QUrl(MATOMO_URL), MATOMO_SITE_ID, clientId) {
        initNameFieldMap();
        qDebug(lcMatomoClient) << "MatomoClient initialized with URL:" << MATOMO_URL << "and site ID:" << MATOMO_SITE_ID;

        const auto children = this->findChildren<QNetworkAccessManager *>();
        if (!children.isEmpty()) {
            QNetworkAccessManager *piwikNAM = children.first();
            /**
             *
             * A transfer timeout means that if no data is transferred between the client and the server
             * for a certain amount of time, the request will be aborted.
             * (This is not a global timeout: it resets every time data is sent or received.)
             *
             * Since Matomo requests are lightweight, this transfer timeout behaves approximately like a global timeout.
             *
             */
            piwikNAM->setTransferTimeout(matomoTimeout);
            (void) connect(piwikNAM, &QNetworkAccessManager::finished, this, [](const QNetworkReply *reply) {
                if (reply->error() == QNetworkReply::TimeoutError // Error given by the TransferTimeout
                    || reply->error() == QNetworkReply::ConnectionRefusedError // Error given when MATOMO_URL is blocked and
                                                                               // return :: in ipv6 or 0.0.0.0 in ipv4
                ) {
                    matomoTimeoutCounter++;
                    qWarning(lcMatomoClient) << "Timeout #" << matomoTimeoutCounter;
                    if (matomoTimeoutCounter >= matomoTimeoutMax && !matomoDisabled) {
                        matomoDisabled = true;
                        qWarning(lcMatomoClient) << "Disabled after" << matomoTimeoutMax << "timeouts.";
                    }
                }
            });
            if (ParametersCache::instance()->parametersInfo().extendedLog()) {
                qDebug(lcMatomoClient) << "Transfer timeout set to" << matomoTimeout << "ms.";
            }
        }
    }

    /**
     * Sends a visitPage request to Matomo.
     *
     * @param page the page to track
     */
    void MatomoClient::sendVisit(const MatomoNameField page) {
        QString path;
        QString action;
        instance().getPathAndAction(page, path, action);

        if (ParametersCache::instance()->parametersInfo().extendedLog()) {
            qCDebug(lcMatomoClient()) << "MatomoClient::sendVisit(path=" << path << ", action=" << action << ")"
                                      << (matomoDisabled ? " => Trigger but not sent, tracking disabled" : "");
        }
        if (matomoDisabled) return; // If Matomo is disabled, do not send the visit.

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
    void MatomoClient::sendEvent(const QString &category, const MatomoEventAction action, const QString &name, const int value) {
        QString actionStr;
        switch (action) {
            case MatomoEventAction::Click:
                actionStr = "click";
                break;
            case MatomoEventAction::Input:
                actionStr = "input";
                break;
            default:
                actionStr = "unknown";
                break;
        }

        if (ParametersCache::instance()->parametersInfo().extendedLog()) {
            qCDebug(lcMatomoClient()) << "MatomoClient::sendEvent(category=" << category << ", action=" << actionStr
                                      << ", name=" << name << ", value=" << value << ")"
                                      << (matomoDisabled ? " => Trigger but not sent, tracking disabled" : "");
        }
        if (matomoDisabled) return; // If Matomo is disabled, do not send the event.

        instance().PiwikTracker::sendEvent(category, category, actionStr, name, value);
    }

    /**
     * Initializes the name field map for Matomo tracking.
     *
     * Populates the _nameFieldMap with the appropriate mappings between MatomoNameField enums and their corresponding
     * string representations. Conditional compilation is used to include platform-specific entries.
     */
    void MatomoClient::initNameFieldMap() {
        _nameFieldMap = {
#ifdef Q_OS_MAC
                {MatomoNameField::PG_Preferences_LiteSync, {"preferences/litesync", "litesync"}},
#endif
                {MatomoNameField::WV_LoginPage, {"webview", "login"}},
                {MatomoNameField::PG_SynthesisPopover, {"popover", "popover"}},
                {MatomoNameField::PG_SynthesisPopover_KebabMenu, {"popover", "kebab_menu"}},
                {MatomoNameField::PG_Preferences, {"preferences", "preferences"}},
                {MatomoNameField::PG_Preferences_Debugging, {"preferences/debugging", "debugging"}},
                {MatomoNameField::PG_Preferences_FilesToExclude, {"preferences/files_to_exclude", "files_to_exclude"}},
                {MatomoNameField::PG_Preferences_Proxy, {"preferences/proxy", "proxy"}},
                {MatomoNameField::PG_Preferences_About, {"preferences/about", "about"}},
                {MatomoNameField::PG_Preferences_Beta, {"preferences/beta", "beta"}},
#ifdef Q_OS_WIN
                {MatomoNameField::PG_Preferences_UpdateDialog, {"preferences/update_dialog", "update_dialog"}},
#endif
                {MatomoNameField::PG_Parameters, {"parameters", "parameters"}},
                {MatomoNameField::PG_Parameters_NewSync_LocalFolder, {"parameters/newsync", "local_folder"}},
                {MatomoNameField::PG_Parameters_NewSync_RemoteFolder, {"parameters/newsync", "remote_folder"}},
                {MatomoNameField::PG_Parameters_NewSync_Summary, {"parameters/newsync", "summary"}},
                {MatomoNameField::PG_AddNewDrive_SelectDrive, {"add_new_drive", "select_drive"}},
                {MatomoNameField::PG_AddNewDrive_ActivateLiteSync, {"add_new_drive", "activate_litesync"}},
                {MatomoNameField::PG_AddNewDrive_SelectRemoteFolder, {"add_new_drive", "select_remote_folder"}},
                {MatomoNameField::PG_AddNewDrive_SelectLocalFolder, {"add_new_drive", "select_local_folder"}},
                {MatomoNameField::PG_AddNewDrive_ExtensionSetup, {"add_new_drive", "extension_setup"}},
                {MatomoNameField::PG_AddNewDrive_Confirmation, {"add_new_drive", "confirmation"}},
        };
    }

    /**
     * Retrieves the path and action strings for a given MatomoNameField.
     *
     * @param name the MatomoNameField for which to retrieve the path and action
     * @param path[out] the path associated with the MatomoNameField
     * @param action[out] the action associated with the MatomoNameField
     */
    void MatomoClient::getPathAndAction(const MatomoNameField name, QString &path, QString &action) const {
        const auto it = _nameFieldMap.find(name);
        if (it != _nameFieldMap.end()) {
            path = it->second.first;
            action = it->second.second;
        } else {
            qCWarning(lcMatomoClient) << "MatomoClient::getPathAndAction triggered with page value unknown ("
                                      << static_cast<matomo_enum_t>(name) << ").";
            path = action = "unknown";
        }
    }

} // namespace KDC
