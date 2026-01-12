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
#include "synctriggerprogressupdatejob.h"
#include "syncsetsupportsvirtualfilesjob.h"
#include "syncsetrootpinstatejob.h"
#include "blacklistednodelistjob.h"
#include "blacklistednodesetlistjob.h"
#include "nodepathjob.h"
#include "nodeinfojob.h"
#include "nodesubfoldersjob.h"
#include "nodesubfolders2job.h"
#include "nodefoldersizejob.h"
#include "nodecreatemissingfoldersjob.h"
#include "errorinfolistjob.h"
#if defined(KD_MACOS)
#include "exclappgetlistjob.h"
#include "exclappsetlistjob.h"
#include "exclappgetfetchingapplistjob.h"
#endif
#include "excltemplgetexcludedjob.h"
#include "excltemplgetlistjob.h"
#include "excltemplsetlistjob.h"
#include "excltemplpropagatechangejob.h"
#include "parametersinfojob.h"
#include "parametersupdatejob.h"

#include "utilityfindgoodpathfornewsyncjob.h"
#include "utilitybestvfsavailablemodejob.h"
#include "utilityispathvalidfornewsyncjob.h"
#include "utilityactivateloadinfojob.h"
#include "utilitycheckcommstatusjob.h"
#include "utilityhassystemlaunchonstartupjob.h"
#include "utilitysetappstatejob.h"
#include "utilitygetappstatejob.h"
#include "utilitysendlogtosupportjob.h"
#include "utilitycancellogtosupportjob.h"
#include "utilitygetlogestimatedsizejob.h"
#include "utilityquitjob.h"
#include "utilitysendappstarttracejob.h"

#include "updaterversioninfojob.h"
#include "updaterstatejob.h"
#include "updaterstartinstallerjob.h"
#include "updaterskipversionjob.h"

namespace KDC {

GuiJobFactory::GuiJobFactory() {
    _makeMap = {
        {RequestNum::LOGIN_REQUESTTOKEN, makeShared<LoginRequestTokenJob>},
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
        {RequestNum::SYNC_ASKFORSTATUS, makeShared<SyncTriggerProgressUpdateJob>},
        {RequestNum::SYNC_SETSUPPORTSVIRTUALFILES, makeShared<SyncSetSupportsVirtualFilesJob>},
        {RequestNum::SYNC_SETROOTPINSTATE, makeShared<SyncSetRootPinStateJob>},
        {RequestNum::BLACKLISTED_NODE_LIST, makeShared<BlacklistedNodeListJob>},
        {RequestNum::BLACKLISTED_NODE_SETLIST, makeShared<BlacklistedNodeSetListJob>},
        {RequestNum::NODE_PATH, makeShared<NodePathJob>},
        {RequestNum::NODE_INFO, makeShared<NodeInfoJob>},
        {RequestNum::SYNC_GETPRIVATELINKURL, makeShared<SyncGetPrivateLinkUrlJob>},
        {RequestNum::NODE_SUBFOLDERS, makeShared<NodeSubFoldersJob>},
        {RequestNum::NODE_SUBFOLDERS2, makeShared<NodeSubFolders2Job>},
        {RequestNum::NODE_FOLDER_SIZE, makeShared<NodeFolderSizeJob>},
        {RequestNum::NODE_CREATEMISSINGFOLDERS, makeShared<NodeCreateMissingFoldersJob>},
        {RequestNum::ERROR_INFOLIST, makeShared<ErrorInfolistJob>},
#if defined(KD_MACOS)
        {RequestNum::EXCLAPP_GETLIST, makeShared<ExclAppGetListJob>},
        {RequestNum::EXCLAPP_SETLIST, makeShared<ExclAppSetListJob>},
        {RequestNum::EXCLAPP_GET_FETCHING_APP_LIST, makeShared<ExclAppGetFetchingAppListJob>},
#endif
        {RequestNum::EXCLTEMPL_GETEXCLUDED, makeShared<ExclTemplGetExcludedJob>},
        {RequestNum::EXCLTEMPL_GETLIST, makeShared<ExclTemplGetListJob>},
        {RequestNum::EXCLTEMPL_SETLIST, makeShared<ExclTemplSetListJob>},
        {RequestNum::EXCLTEMPL_PROPAGATE_CHANGE, makeShared<ExclTemplPropagateChangeJob>},
        {RequestNum::PARAMETERS_INFO, makeShared<ParametersInfoJob>},
        {RequestNum::PARAMETERS_UPDATE, makeShared<ParametersUpdateJob>},
        {RequestNum::EXCLTEMPL_GETEXCLUDED, makeShared<ExclTemplGetExcludedJob>},
        {RequestNum::EXCLTEMPL_GETLIST, makeShared<ExclTemplGetListJob>},
        {RequestNum::EXCLTEMPL_SETLIST, makeShared<ExclTemplSetListJob>},
        {RequestNum::EXCLTEMPL_PROPAGATE_CHANGE, makeShared<ExclTemplPropagateChangeJob>},
        {RequestNum::PARAMETERS_INFO, makeShared<ParametersInfoJob>},
        {RequestNum::PARAMETERS_UPDATE, makeShared<ParametersUpdateJob>},
        {RequestNum::UTILITY_BESTVFSAVAILABLEMODE, makeShared<UtilityBestVfsAvailableModeJob>},
        {RequestNum::UTILITY_FINDGOODPATHFORNEWSYNC, makeShared<UtilityFindGoodPathForNewSyncJob>},
        {RequestNum::UTILITY_ISPATHVALIDFORNEWSYNC, makeShared<UtilityIsPathValidForNewSyncJob>},
        {RequestNum::UTILITY_ACTIVATELOADINFO, makeShared<UtilityActivateLoadInfoJob>},
        {RequestNum::UTILITY_CHECKCOMMSTATUS, makeShared<UtilityCheckCommStatusJob>},
        {RequestNum::UTILITY_HASSYSTEMLAUNCHONSTARTUP, makeShared<UtilityHasSystemLaunchOnStartupJob>},
        {RequestNum::UTILITY_SET_APPSTATE, makeShared<UtilitySetAppStateJob>},
        {RequestNum::UTILITY_GET_APPSTATE, makeShared<UtilityGetAppStateJob>},
        {RequestNum::UTILITY_SEND_LOG_TO_SUPPORT, makeShared<UtilitySendLogToSupportJob>},
        {RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT, makeShared<UtilityCancelLogToSupportJob>},
        {RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE, makeShared<UtilityGetLogEstimatedSizeJob>},
        {RequestNum::UTILITY_SEND_LOG_TO_SUPPORT, makeShared<UtilitySendLogToSupportJob>},
        {RequestNum::UTILITY_CANCEL_LOG_TO_SUPPORT, makeShared<UtilityCancelLogToSupportJob>},
        {RequestNum::UTILITY_GET_LOG_ESTIMATED_SIZE, makeShared<UtilityGetLogEstimatedSizeJob>},
        {RequestNum::UTILITY_QUIT, makeShared<UtilityQuitJob>},
        {RequestNum::UTILITY_SEND_APP_START_TRACE, makeShared<UtilitySendAppStartTraceJob>},
        {RequestNum::UPDATER_VERSION_INFO, makeShared<UpdaterVersionInfoJob>},
        {RequestNum::UPDATER_STATE, makeShared<UpdaterStateJob>},
        {RequestNum::UPDATER_START_INSTALLER, makeShared<UpdaterStartInstallerJob>},
        {RequestNum::UPDATER_SKIP_VERSION, makeShared<UpdaterSkipVersionJob>}
    };
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
