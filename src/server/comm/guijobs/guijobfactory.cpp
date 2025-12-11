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
#include "syncgetprivatelinkurljob.h"
#include "syncsetsupportsvirtualfilesjob.h"
#include "syncsetrootpinstatejob.h"
#include "blacklistednodelistjob.h"
#include "blacklistednodesetlistjob.h"
#include "nodeinfojob.h"
#include "nodesubfoldersjob.h"
#include "nodesubfolders2job.h"
#include "nodefoldersizejob.h"
#include "blacklistednodelistjob.h"
#include "blacklistednodesetlistjob.h"
#include "errorinfolistjob.h"
#include "nodecreatemissingfoldersjob.h"
#include "parametersinfojob.h"
#include "parametersupdatejob.h"
#include "utilitysendlogtosupportjob.h"
#include "utilitycancellogtosupportjob.h"
#include "utilitygetlogestimatedsizejob.h"

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
                {RequestNum::SYNC_GETPRIVATELINKURL, makeShared<SyncGetPrivateLinkUrlJob>},
                {RequestNum::SYNC_SETSUPPORTSVIRTUALFILES, makeShared<SyncSetSupportsVirtualFilesJob>},
                {RequestNum::SYNC_SETROOTPINSTATE, makeShared<SyncSetRootPinStateJob>},
                {RequestNum::BLACKLISTED_NODE_LIST, makeShared<BlacklistedNodeListJob>},
                {RequestNum::BLACKLISTED_NODE_SETLIST, makeShared<BlacklistedNodeSetListJob>},
                {RequestNum::NODE_INFO, makeShared<NodeInfoJob>},
                {RequestNum::NODE_SUBFOLDERS, makeShared<NodeSubFoldersJob>},
                {RequestNum::NODE_SUBFOLDERS2, makeShared<NodeSubFolders2Job>},
                {RequestNum::NODE_FOLDER_SIZE, makeShared<NodeFolderSizeJob>},
                {RequestNum::PARAMETERS_INFO, makeShared<ParametersInfoJob>},
                {RequestNum::PARAMETERS_UPDATE, makeShared<ParametersUpdateJob>},
                {RequestNum::BLACKLISTED_NODE_LIST, makeShared<BlacklistedNodeListJob>},
                {RequestNum::BLACKLISTED_NODE_SETLIST, makeShared<BlacklistedNodeSetListJob>},
                {RequestNum::ERROR_INFOLIST, makeShared<ErrorInfolistJob>},
                {RequestNum::NODE_INFO, makeShared<NodeInfoJob>},
                {RequestNum::NODE_CREATEMISSINGFOLDERS, makeShared<NodeCreateMissingFoldersJob>},
                {RequestNum::PARAMETERS_INFO, makeShared<ParametersInfoJob>},
                {RequestNum::PARAMETERS_UPDATE, makeShared<ParametersUpdateJob>},
                {RequestNum::UTILITY_SEND_LOG_TO_SUPPORT, makeShared<UtilitySendLogToSupportJob>},
                {RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT, makeShared<UtilityCancelLogToSupportJob>},
                {RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE, makeShared<UtilityGetLogEstimatedSizeJob>}};
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
