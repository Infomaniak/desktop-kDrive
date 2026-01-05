using DynamicData;
using Infomaniak.kDrive.Types;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class ProxyConfig : UISafeObservableObject
    {
        private ProxyType _type = ProxyType.None;
        private string _hostName = "";
        private int _port = 0;
        private bool _needsAuth = false;
        private string _user = "";
        private string _pwd = "";
        public bool _manualConfigurationRequired = false;
        public bool ManualConfigrationRequired
        {
            get => _manualConfigurationRequired;
            set => SetPropertyInUIThread(ref _manualConfigurationRequired, value);
        }

        public ProxyType Type
        {
            get => _type;
            set
            {
                SetPropertyInUIThread(ref _type, value);
                UpdateManualConfigurationRequired();
            }
        }
        public string HostName
        {
            get => _hostName;
            set => SetPropertyInUIThread(ref _hostName, value);
        }
        public int Port
        {
            get => _port;
            set => SetPropertyInUIThread(ref _port, value);
        }
        public bool NeedsAuth
        {
            get => _needsAuth;
            set => SetPropertyInUIThread(ref _needsAuth, value);
        }
        public string User
        {
            get => _user;
            set => SetPropertyInUIThread(ref _user, value);
        }
        public string Pwd
        {
            get => _pwd;
            set => SetPropertyInUIThread(ref _pwd, value);
        }

        private void UpdateManualConfigurationRequired()
        {
            ManualConfigrationRequired = Type == ProxyType.HTTP;
        }
    }
}
