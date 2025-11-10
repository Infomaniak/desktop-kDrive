using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reactive.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class NetworkSettings : ObservableObject
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
            set => SetProperty(ref _proxyType, value);
        }

        public string ProxyAddress
        {
            get => _proxyAddress;
            set => SetProperty(ref _proxyAddress, value);
        }

        public int ProxyPort
        {
            get => _proxyPort;
            set => SetProperty(ref _proxyPort, value);
        }

        public ProxyProtocol CurrentProxyProtocol
        {
            get => _proxyProtocol;
            set => SetProperty(ref _proxyProtocol, value);
        }

        public bool ProxyRequiresAuth
        {
            get => _proxyRequiresAuth;
            set => SetProperty(ref _proxyRequiresAuth, value);
        }

        public string ProxyUsername
        {
            get => _proxyUsername;
            set => SetProperty(ref _proxyUsername, value);
        }

        public string ProxyPassword
        {
            get => _proxyPassword;
        }

    }
}
