using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;

namespace Infomaniak.kDrive.ServerCommunication.Interfaces
{
    public interface IServerCommService
    {

        // User-related requests

        /* Returns the list of user DbIds known to the server.
         * Returns null on error.
         */
        Task<List<DbId>?> GetUserDbIds(CancellationToken cancellationToken);

        /* Adds a new user or re-authenticates an existing user using the provided OAuth2 code and code verifier.
         * The view model's Users collection is updated accordingly.
         * In the case of a new user, only DbId is set initially, other properties will be updated later by a userAdded signal.
         * Returns the User object on success, or null on failure.
         */
        Task<User?> AddOrRelogUser(string oAuthCode, string oAuthCodeVerifier, CancellationToken cancellationToken);

        /* Refreshes the information of all users from the server.
         * This includes their properties (name, email, avatar, etc.), accounts are not refreshed here, call RefreshAccounts for that.
         * User are added/removed as necessary to match the server state.
         * Returns true on success, false on failure.
         * Note: This does not refresh accounts within users, only the users themselves.
         */
        Task<bool> RefreshUsers(CancellationToken cancellationToken);

        /* Removes the user with the specified DbId from the server and local storage.
         * The view model's Users collection is updated accordingly.
         * Returns true on success, false on failure.
         */
        Task<bool> RemoveUser(DbId userDbId, CancellationToken cancellationToken);

        // Account-related requests
        /* Refreshes the list of accounts for all users from the server.
         * Accounts are added/removed as necessary to match the server state.
         * Returns true on success, false on failure.
         * Note: This does not refresh drives within accounts, only the accounts themselves.
         */
        Task<bool> RefreshAccounts(CancellationToken cancellationToken);


        // Drive-related requests
        Task<bool> RefreshDrives(CancellationToken cancellationToken);
        Task<bool> RefreshUserDrivesAvailable(DbId userDbId, CancellationToken cancellationToken);

        // Sync-related requests
        Task<bool> RefreshSyncs(CancellationToken cancellationToken);
        Task<bool> StartSync(DbId syncDbId, CancellationToken cancellationToken);
        Task<bool> PauseSync(DbId syncDbId, CancellationToken cancellationToken);
        Task<bool> RemoveSync(DbId syncDbId, CancellationToken cancellationToken);
        Task<bool> AddSync(NewSync newSync, CancellationToken cancellationToken);
        Task<bool> SetSyncType(DbId syncDbId, SyncType type, CancellationToken cancellationToken);

        Task<bool?> CanPathSupportLiteSync(string absoluteLocalPath, CancellationToken cancellationToken);

        // Returns a valid path for a new sync as close as possible to the desiredPath, if not known, the driveDbId can be set to -1
        Task<string?> GetGoodPathForNewSync(IDrive? drive, string desiredPath, CancellationToken cancellationToken);
        Task<bool?> IsPathValidForNewSync(string path, CancellationToken cancellationToken);
        Task<List<SearchItem>?> SearchItem(DbId syncDbId, string searchString, CancellationToken cancellationToken);
        Task<UInt64?> GetSyncOfflineFilesSize(DbId syncDbId, CancellationToken cancellationToken);

        // Node-related requests
        Task<List<Node>?> GetSubFolders(DbId userDbId, DriveId driveId, NodeId parentNodeId /*Leave empty for root node*/, CancellationToken cancellationToken);
        Task<Node?> GetNodeInfo(DbId userDbId, DriveId driveId, NodeId nodeId, CancellationToken cancellationToken);
        Task<Int64?> GetFolderSize(DbId userDbId, DriveId driveId, NodeId nodeId, CancellationToken cancellationToken);
        Task<List<NodeId>?> GetBlacklistedNodeIdList(DbId syncDbId, CancellationToken cancellationToken);
        Task<bool> SetBlacklistedNodeIdList(DbId syncDbId, List<NodeId> idList, CancellationToken cancellationToken);
        Task<Uri?> GetPublicLink(DbId driveDbId, NodeId nodeId, CancellationToken cancellationToken);

        // Setting-related requests
        Task<bool> RefreshSettings(CancellationToken cancellationToken);

        // Saves the settings provided in the Settings view model to the server.
        Task<bool> SaveSettings(CancellationToken cancellationToken);

        // Exclusion template-related requests
        Task<List<ExclusionTemplate>?> GetExclusionTemplates(CancellationToken cancellationToken);
        Task SetUserExclusionTemplates(List<ExclusionTemplate> templates, CancellationToken cancellationToken);

        // Update-related requests
        Task<bool> StartUpdate(CancellationToken cancellationToken);
        Task<bool> RefreshUpdaterVersionInfo(CancellationToken cancellationToken);

        // Log-related requests
        Task<bool> StartLogUpload(bool includeArchivedLogs, CancellationToken cancellationToken);
        Task<bool> CancelLogUpload(CancellationToken cancellationToken);


        // App-related requests
        Task<bool> ActivateLoadInfo(CancellationToken cancellationToken);
        Task Exit(); // Notify the server that the application is exiting. No cancellation token is required as the app is closing.

        // Error-related requests
        Task<bool> RefreshErrors(CancellationToken cancellationToken);

        // Event handlers for user-related signals
        Task HandleUserUpdatedOrAddedAsync(object? sender, SignalEventArgs args);
        Task HandleUserRemovedAsync(object? sender, SignalEventArgs args);

        // Event handlers for account-related signals
        Task HandleAccountUpdatedOrAddedAsync(object? sender, SignalEventArgs args);
        Task HandleAccountRemovedAsync(object? sender, SignalEventArgs args);


        // Event handlers for drive-related signals
        Task HandleDriveUpdatedOrAddedAsync(object? sender, SignalEventArgs args);
        Task HandleDriveRemovedAsync(object? sender, SignalEventArgs args);


        // Event handlers for sync-related signals
        Task HandleSyncUpdatedOrAddedAsync(object? sender, SignalEventArgs args);

        Task HandleSyncRemovedAsync(object? sender, SignalEventArgs args);
        Task HandleSyncProgressInfo(object? sender, SignalEventArgs args);
        Task HandleSyncCompletedItem(object? sender, SignalEventArgs args);

        // Event handlers for Update-related signals
        Task HandleUpdaterStateChangedAsync(object? sender, SignalEventArgs args);

        // Event handlers for log-related signals
        Task HandleLogUploadProgressAsync(object? sender, SignalEventArgs args);

        // Event handlers for error-related signals
        Task HandleErrorAddedAsync(object? sender, SignalEventArgs args);
        Task HandleErrorRemovedAsync(object? sender, SignalEventArgs args);
    }
}