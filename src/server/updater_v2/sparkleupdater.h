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

#include "../updater/updaterserver.h"
#include "../../libcommon/utility/utility.h"
#include "abstractupdater.h"

namespace KDC {

enum DownloadState { Unknown = 0, FindValidUpdate, DidNotFindUpdate, AbortWithError };

class SparkleUpdater final : public AbstractUpdater {
    public:
        explicit SparkleUpdater();
        ~SparkleUpdater();

        void onUpdateFound(const std::string &downloadUrl) override;

        void setUpdateUrl(const std::string &url);
        void setQuitCallback(const QuitCallback &quitCallback) final;
        bool startUpdater();

        // unused in this updater
        void checkForUpdate();
        void backgroundCheckForUpdate();
        // UpdateState updateState() const override {
        //     switch (state()) {
        //         using enum KDC::UpdateState;
        //         case FindValidUpdate:
        //             return Ready;
        //         case DidNotFindUpdate:
        //             return None;
        //         case AbortWithError:
        //         case Unknown:
        //         default:
        //             return Error;
        //     }
        // }
        int state() const;
        // QString statusString()
        //         const // defined in header file because "mm" file not read by "lupdate" command for translation file generation
        // {
        //     switch (state()) {
        //         case Unknown:
        //             return tr("Update status is unknown: Did not check for new updates.");
        //         case FindValidUpdate:
        //             return tr("An update is available: %1").arg(CommonUtility::currentVersion().c_str());
        //         case DidNotFindUpdate:
        //             return tr("%1 is up to date!").arg(APPLICATION_NAME);
        //         case AbortWithError:
        //             return tr("Check for update aborted.");
        //         default:
        //             return QString();
        //     }
        // }
        bool updateFound() const;

        // public slots:
        //     void slotStartInstaller();

    private:
        class Private;
        Private *d;

        std::string _feedUrl;
};

} // namespace KDC
