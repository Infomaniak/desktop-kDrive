using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.Model
{
    internal class App : ObservableObject
    {
        /*
        * App Data Structure:
        *
        * App Data
        * │
        * └── Users
        *     └── Accounts / Organizations
        *         └── Drives
        *             └── Syncs
        *
        * Example expanded view:
        *
        * App Data
        * ├─ User 1
        * │  ├─ Account 1
        * │  │  ├─ Drive 1
        * │  │  │  └─ Sync 1
        * │  │  └─ Drive 2
        * │  │     └─ Sync 1
        * │  └─ Account 2
        * │     └─ ...
        * ├─ User 2
        * │  └─ ...
        * └─ User n
        */


        private List<User?> _users = new List<User?>();

        public List<User?> Users
        {
            get => _users;
            set => SetProperty(ref _users, value);
        }
    }
}
