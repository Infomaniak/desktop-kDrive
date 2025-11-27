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

#include "guijobfactory.h"
#include "loginrequesttokenjob.h"
#include "unknownrequestjob.h"
#include "userdbidlistjob.h"
#include "userinfolistjob.h"
#include "userdeletejob.h"
#include "useravailabledrivesjob.h"
#include "accountinfolistjob.h"
#include "driveinfolistjob.h"
#include "driveupdatejob.h"
#include "drivedeletejob.h"
#include "drivesearchjob.h"
#include "syncinfolistjob.h"
#include "syncstartjob.h"
#include "syncstopjob.h"
#include "syncstatusjob.h"
#include "syncaddjob.h"
#include "syncadd2job.h"
#include "syncstartafterloginjob.h"
#include "syncdeletejob.h"
#include "syncgetpubliclinkurljob.h"
#include "nodesubfoldersjob.h"
#include "nodefoldersizejob.h"
#include "syncnodelistjob.h"
#include "syncnodesetlistjob.h"
#include "nodeinfojob.h"
#include "syncgetprivatelinkurljob.h"
#include "excltemplgetexcludedjob.h"

namespace KDC {

GuiJobFactory::GuiJobFactory() {
    _makeMap = {{RequestNum::LOGIN_REQUESTTOKEN, makeShared<LoginRequestTokenJob>},
                {RequestNum::USER_DBIDLIST, makeShared<UserDbIdListJob>},
                {RequestNum::USER_INFOLIST, makeShared<UserInfoListJob>},
                {RequestNum::USER_DELETE, makeShared<UserDeleteJob>},
                {RequestNum::USER_AVAILABLEDRIVES, makeShared<UserAvailableDrivesJob>},
                {RequestNum::ACCOUNT_INFOLIST, makeShared<AccountInfoListJob>},
                {RequestNum::DRIVE_INFOLIST, makeShared<DriveInfoListJob>},
                {RequestNum::DRIVE_UPDATE, makeShared<DriveUpdateJob>},
                {RequestNum::DRIVE_DELETE, makeShared<DriveDeleteJob>},
                {RequestNum::DRIVE_SEARCH, makeShared<DriveSearchJob>},
                {RequestNum::SYNC_INFOLIST, makeShared<SyncInfoListJob>},
                {RequestNum::SYNC_START, makeShared<SyncStartJob>},
                {RequestNum::SYNC_STOP, makeShared<SyncStopJob>},
                {RequestNum::SYNC_STATUS, makeShared<SyncStatusJob>},
                {RequestNum::SYNC_ADD, makeShared<SyncAddJob>},
                {RequestNum::SYNC_ADD2, makeShared<SyncAdd2Job>},
                {RequestNum::SYNC_START_AFTER_LOGIN, makeShared<SyncStartAfterLoginJob>},
                {RequestNum::SYNC_DELETE, makeShared<SyncDeleteJob>},
                {RequestNum::SYNC_GETPUBLICLINKURL, makeShared<SyncGetPublicLinkUrlJob>},
                {RequestNum::NODE_SUBFOLDERS, makeShared<NodeSubFoldersJob>},
                {RequestNum::NODE_FOLDER_SIZE, makeShared<NodeFolderSizeJob>},
                {RequestNum::SYNCNODE_LIST, makeShared<SyncNodeListJob>},
                {RequestNum::SYNCNODE_SETLIST, makeShared<SyncNodeSetListJob>},
                {RequestNum::NODE_INFO, makeShared<NodeInfoJob>},
                {RequestNum::SYNC_GETPRIVATELINKURL, makeShared<SyncGetPrivateLinkUrlJob>},
                {RequestNum::EXCLTEMPL_GETEXCLUDED, makeShared<ExclTemplGetExcludedJob>}};
}

std::shared_ptr<AbstractGuiJob> GuiJobFactory::make(RequestNum requestNum, std::shared_ptr<CommManager> commManager,
                                                    int requestId, const Poco::DynamicStruct &inParams,
                                                    std::shared_ptr<AbstractCommChannel> channel) {
    if (const auto makeElt = _makeMap.find(requestNum); makeElt != _makeMap.end())
        return makeElt->second(commManager, requestId, inParams, channel);
    else
        return std::make_shared<UnknownRequestJob>(commManager, requestId, inParams, channel);
}

} // namespace KDC
