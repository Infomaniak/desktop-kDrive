using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client.Model
{
    internal class User : ObservableObject
    {
        private long _id = -1;
        private string _name = "";
        private string _email = "";
        private string _avatarUrl = "";
        private List<Account?> _accounts = new List<Account?>();

        public long Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }

        public string Name
        {
            get => _name;
            set => SetProperty(ref _name, value);
        }

        public string Email
        {
            get => _email;
            set => SetProperty(ref _email, value);
        }

        public string AvatarUrl
        {
            get => _avatarUrl;
            set => SetProperty(ref _avatarUrl, value);
        }

        public List<Account?> Accounts
        {
            get { _accounts = _accounts.Where(a => a != null).ToList(); return _accounts; }
            set => SetProperty(ref _accounts, value);
        }
    }
}
