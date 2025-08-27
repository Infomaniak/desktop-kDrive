using CommunityToolkit.Mvvm.ComponentModel;
using kDrive_client.ServerCommunication;
using Microsoft.UI.Dispatching;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.DataModel
{
    internal class AppModel : ObservableObject
    {
        /*
        * App Data Structure:
        *
        * App Data
        * │
        * └── Users
        *     └── Drives
        *         └── Syncs
        *
        * Example expanded view:
        *
        * App Data
        * ├─ User 1
        * │  ├─ Drive 1
        * │  │  └─ Sync 1
        * │  ├─ Drive 2
        * │  │  └─ Sync 1
        * │  └─ Drive 3
        * │     └─ ...
        * ├─ User 2
        * │  └─ ...
        * └─ User n
        */

        private bool _isInitialized = false;

        private List<User?> _users = new List<User?>();
        private ObservableCollection<Drive> _drives = new ObservableCollection<Drive>() { new Drive(0){Name = "testc"}, new Drive(0) { Name = "test2" } };

        public ObservableCollection<Drive> Drives
        {
            get => _drives;
        }

        public List<User?> Users
        {
            get => _users;
            set => SetProperty(ref _users, value);
        }
        public bool IsInitialized
        {
            get => _isInitialized;
        }
        public async Task InitializeAsync()
        {
            var userDbIds = await ServerCommunication.CommRequests.GetUserDbIds().ConfigureAwait(false);
            if (userDbIds != null)
            {
                List<Task> reloadTasks = new List<Task>();
                List<User> users = new List<User>();
                for (int i = 0; i < userDbIds.Count; i++)
                {
                    User user = new User(userDbIds[i]);
                    reloadTasks.Add(user.Reload());
                    users.Add(user);
                }
                await Task.WhenAll(reloadTasks).ConfigureAwait(false);
                Users.AddRange(users);
            }         

            _isInitialized = true;
        }
    }
}
