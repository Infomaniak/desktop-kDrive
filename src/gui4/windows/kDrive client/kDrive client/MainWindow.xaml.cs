using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace kDrive_client
{
    /// <summary>
    /// An empty window that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            
        }
        private void EnglishSelected(object sender, RoutedEventArgs e)
        {
            var selectedLanguageCode = "en-US";
            showLanguageSelectionInfoBarIfNeeded(selectedLanguageCode);
            Windows.Globalization.ApplicationLanguages.PrimaryLanguageOverride = selectedLanguageCode;
        }

        private void GermanSelected(object sender, RoutedEventArgs e)
        {
            var selectedLanguageCode = "de-De";
            showLanguageSelectionInfoBarIfNeeded(selectedLanguageCode);
            Windows.Globalization.ApplicationLanguages.PrimaryLanguageOverride = selectedLanguageCode;
        }

        private void FrenchSelected(object sender, RoutedEventArgs e)
        {
            var selectedLanguageCode = "fr-FR";
            showLanguageSelectionInfoBarIfNeeded(selectedLanguageCode);
            Windows.Globalization.ApplicationLanguages.PrimaryLanguageOverride = selectedLanguageCode;
        }

        private void InfoBarRestartButton_Click(object sender, RoutedEventArgs e)
        {
            LanguageSelectionRestartNeededInfoBar.IsOpen = false;
            // Restart the application
            var currentProcess = System.Diagnostics.Process.GetCurrentProcess();
            var startInfo = new System.Diagnostics.ProcessStartInfo
            {
                FileName = currentProcess.MainModule.FileName,
                UseShellExecute = true
            };
            System.Diagnostics.Process.Start(startInfo);
            // Close the current process
            currentProcess.CloseMainWindow();
            currentProcess.Kill();
            Application.Current.Exit();
            // Note: The application will restart with the new language setting.

        }

        private void AutoSelected(object sender, RoutedEventArgs e)
        {
            Windows.Globalization.ApplicationLanguages.PrimaryLanguageOverride = string.Empty;
            LanguageSelectionRestartNeededInfoBar.IsOpen = true;
        }

        private void showLanguageSelectionInfoBarIfNeeded(string language)
        {
            if (language != Windows.Globalization.ApplicationLanguages.Languages.FirstOrDefault())
            {
                LanguageSelectionRestartNeededInfoBar.IsOpen = true;
            }
        }
    }
}
