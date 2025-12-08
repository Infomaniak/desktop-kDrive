using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Media.Animation;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommProtocol;

namespace Infomaniak.kDrive.ServerCommunication.Interfaces
{
    internal interface IServerCommService
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
         * Returns a Task that completes when the operation is done.
         */
        Task RefreshUsers(CancellationToken cancellationToken);

        /* Removes the user with the specified DbId from the server and local storage.
         * The view model's Users collection is updated accordingly.
         * Returns true on success, false on failure.
         */
        Task RemoveUser(DbId userDbId, CancellationToken cancellationToken);

        // Account-related requests
        /* Refreshes the list of accounts for all users from the server.
         * Accounts are added/removed as necessary to match the server state.
         * Returns a Task that completes when the operation is done.
         * Note: This does not refresh drives within accounts, only the accounts themselves.
         */
        Task RefreshAccounts(CancellationToken cancellationToken);


        // Drive-related requests
        Task RefreshDrives(CancellationToken cancellationToken);
        Task RefreshUserDrivesAvailable(DbId userDbId, CancellationToken cancellationToken);

        // Sync-related requests
        Task RefreshSyncs(CancellationToken cancellationToken);
        Task StartSync(DbId syncDbId, CancellationToken cancellationToken);
        Task PauseSync(DbId syncDbId, CancellationToken cancellationToken);
        Task RemoveSync(DbId syncDbId, CancellationToken cancellationToken);
        Task<bool> AddSync(NewSync newSync, CancellationToken cancellationToken);

        // Node-related requests
        Task<List<Node>?> GetSubFolders(DbId userDbId, DriveId driveId, NodeId parentNodeId /*Leave empty for root node*/, CancellationToken cancellationToken);
        Task<Node?> GetNodeInfo(DbId userDbId, DriveId driveId, NodeId nodeId, CancellationToken cancellationToken);
        Task<Int64?> GetFolderSize(DbId userDbId, DriveId driveId, NodeId nodeId, CancellationToken cancellationToken);
        Task<List<NodeId>?> GetBlacklistedNodeIdList(DbId syncDbId, CancellationToken cancellationToken);
        Task SetBlacklistedNodeIdList(DbId syncDbId, List<NodeId> idList, CancellationToken cancellationToken);

        // Setting-related requests
        Task RefreshSettings(CancellationToken cancellationToken);

        // Saves the settings provided in the Settings view model to the server.
        Task SaveSettings(CancellationToken cancellationToken);

        // Update-related requests
        Task StartUpdate(CancellationToken cancellationToken);
        Task RefreshUpdaterVersionInfo(CancellationToken cancellationToken);
        Task ChangeUpdaterChannel(VersionChannel newChannel, CancellationToken cancellationToken);

        // Error-related requests
        Task RefreshErrors(CancellationToken cancellationToken);

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

        // Event handlers for error-related signals
        Task HandleErrorAddedAsync(object? sender, SignalEventArgs args);
        Task HandleErrorRemovedAsync(object? sender, SignalEventArgs args);
    }
}