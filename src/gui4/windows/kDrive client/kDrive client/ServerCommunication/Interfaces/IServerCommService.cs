using Infomaniak.kDrive.ViewModels;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
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


        // Account-related requests
        /* Refreshes the list of accounts for all users from the server.
         * Accounts are added/removed as necessary to match the server state.
         * Returns a Task that completes when the operation is done.
         * Note: This does not refresh drives within accounts, only the accounts themselves.
         */
        Task RefreshAccounts(CancellationToken cancellationToken);


        // Drive-related requests
        Task RefreshDrives(CancellationToken cancellationToken);


        // Sync-related requests
        Task RefreshSyncs(CancellationToken cancellationToken);



        // Event handlers for user-related signals
        void HandleUserUpdatedOrAddedAsync(object? sender, SignalEventArgs args);

    }
}