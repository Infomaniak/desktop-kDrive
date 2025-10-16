using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reactive.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class NetworkSettings : UISafeObservableObject
    {
        public enum ProxyType
        {
            System,
            Manual
        }

        public enum ProxyProtocol
        {
            HTTP,
            HTTPS
        }

        private ProxyType _proxyType = ProxyType.System;

        // Manual proxy settings
        private string _proxyAddress = "";
        private int _proxyPort = 8080;
        private ProxyProtocol _proxyProtocol = ProxyProtocol.HTTP;
        private bool _proxyRequiresAuth = false;

        // Proxy authentication
        private string _proxyUsername = "";
        private string _proxyPassword = "";

        public ProxyType CurrentProxyType
        {
            get => _proxyType;
            set => SetPropertyInUIThread(ref _proxyType, value);
        }

        public string ProxyAddress
        {
            get => _proxyAddress;
            set => SetPropertyInUIThread(ref _proxyAddress, value);
        }

        public int ProxyPort
        {
            get => _proxyPort;
            set => SetPropertyInUIThread(ref _proxyPort, value);
        }

        public ProxyProtocol CurrentProxyProtocol
        {
            get => _proxyProtocol;
            set => SetPropertyInUIThread(ref _proxyProtocol, value);
        }

        public bool ProxyRequiresAuth
        {
            get => _proxyRequiresAuth;
            set => SetPropertyInUIThread(ref _proxyRequiresAuth, value);
        }

        public string ProxyUsername
        {
            get => _proxyUsername;
            set => SetPropertyInUIThread(ref _proxyUsername, value);
        }

        public string ProxyPassword
        {
            get => _proxyPassword;
        }

    }
}
