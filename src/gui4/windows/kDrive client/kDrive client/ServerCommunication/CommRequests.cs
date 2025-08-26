using kDrive_client.DataModel;
using Microsoft.UI.Xaml;
using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.ServerCommunication
{

    internal class CommRequests
    {
        private enum RequestNum
        {
            LOGIN_REQUESTTOKEN = 1,
            USER_DBIDLIST,
            USER_INFOLIST,
            USER_DELETE,
            USER_AVAILABLEDRIVES,
            USER_ID_FROM_USERDBID,
            ACCOUNT_INFOLIST,
            DRIVE_INFOLIST,
            DRIVE_INFO,
            DRIVE_ID_FROM_DRIVEDBID,
            DRIVE_ID_FROM_SYNCDBID,
            DRIVE_DEFAULTCOLOR,
            DRIVE_UPDATE,
            DRIVE_DELETE,
            DRIVE_GET_OFFLINE_FILES_TOTAL_SIZE,
            DRIVE_SEARCH,
            SYNC_INFOLIST,
            SYNC_START,
            SYNC_STOP,
            SYNC_STATUS,
            SYNC_ISRUNNING,
            SYNC_ADD,
            SYNC_ADD2,
            SYNC_START_AFTER_LOGIN,
            SYNC_DELETE,
            SYNC_GETPUBLICLINKURL,
            SYNC_GETPRIVATELINKURL,
            SYNC_ASKFORSTATUS,
            SYNC_SETSUPPORTSVIRTUALFILES,
            SYNC_SETROOTPINSTATE,
            SYNC_PROPAGATE_SYNCLIST_CHANGE,
            SYNCNODE_LIST,
            SYNCNODE_SETLIST,
            NODE_PATH,
            NODE_INFO,
            NODE_SUBFOLDERS,
            NODE_SUBFOLDERS2,
            NODE_FOLDER_SIZE,
            NODE_CREATEMISSINGFOLDERS,
            ERROR_INFOLIST,
            ERROR_GET_CONFLICTS,
            ERROR_DELETE_SERVER,
            ERROR_DELETE_SYNC,
            ERROR_DELETE_INVALIDTOKEN,
            ERROR_RESOLVE_CONFLICTS,
            ERROR_RESOLVE_UNSUPPORTED_CHAR,
            EXCLTEMPL_GETEXCLUDED,
            EXCLTEMPL_GETLIST,
            EXCLTEMPL_SETLIST,
            EXCLTEMPL_PROPAGATE_CHANGE,
            PARAMETERS_INFO,
            PARAMETERS_UPDATE,
            UTILITY_FINDGOODPATHFORNEWSYNC,
            UTILITY_BESTVFSAVAILABLEMODE,
            UTILITY_SHOWSHORTCUT,
            UTILITY_SETSHOWSHORTCUT,
            UTILITY_ACTIVATELOADINFO,
            UTILITY_CHECKCOMMSTATUS,
            UTILITY_HASSYSTEMLAUNCHONSTARTUP,
            UTILITY_HASLAUNCHONSTARTUP,
            UTILITY_SETLAUNCHONSTARTUP,
            UTILITY_SET_APPSTATE,
            UTILITY_GET_APPSTATE,
            UTILITY_SEND_LOG_TO_SUPPORT,
            UTILITY_CANCEL_LOG_TO_SUPPORT,
            UTILITY_GET_LOG_ESTIMATED_SIZE,
            UTILITY_CRASH,
            UTILITY_QUIT,
            UTILITY_DISPLAY_CLIENT_REPORT, // Sent by the Client process as soon the UI is visible for the user.
            UPDATER_CHANGE_CHANNEL,
            UPDATER_VERSION_INFO,
            UPDATER_STATE,
            UPDATER_START_INSTALLER,
            UPDATER_SKIP_VERSION,
        }
        public static async Task<List<User>> GetUsers() // USER_INFOLIST
        {
            App app = (App)Application.Current;
            CommData data = await app.ComClient.SendRequest(((int)RequestNum.USER_INFOLIST), System.Collections.Immutable.ImmutableArray<byte>.Empty);
            ImmutableArray<byte> resultBytes = data.ResultByteArray;

            User user = new User();
            user.Email = "herve.eruam@infomanaik.com";

            User user2 = new User();
            user2.Email = "h.eruam@infomanaik.com";

            return new List<User> { user, user2 };
        }
    }
}
