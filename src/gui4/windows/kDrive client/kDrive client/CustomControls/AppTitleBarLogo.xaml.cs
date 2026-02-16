using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using System;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class AppTitleBarLogo : UserControl
    {
        public AppTitleBarLogo()
        {
            InitializeComponent();
        }

        private void AppTitleBarLogo_PointerPressed(object sender, PointerRoutedEventArgs e)
        {
            int legacyCommPort = ((App)Application.Current).LegacyCommPort;
            try
            {
                if (legacyCommPort <= 0)
                {
                    Logger.Log(Logger.Level.Warning, "Legacy communication port not set, starting legacy kDrive client without port argument.");
                    System.Diagnostics.Process.Start("..\\kDrive_client.exe");
                }
                else
                {
                    Logger.Log(Logger.Level.Info, $"Starting legacy kDrive client with communication port: {legacyCommPort}");
                    System.Diagnostics.Process.Start("..\\kDrive_client.exe", legacyCommPort.ToString());
                }

                App.ExitApplication(); // If the legacy client starts successfully, exit the current application
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to start legacy kDrive client: {ex.Message}");
            }
        }
    }
}
