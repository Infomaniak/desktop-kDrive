using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Windows.Automation.Peers;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace KDriveClient.CustomControls
{
    public sealed partial class KSuiteUpgradeBar : UserControl
    {
        public Uri KSuiteUpgradeLink
        {
            get
            {
                var resourceLoader = new Microsoft.Windows.ApplicationModel.Resources.ResourceLoader();
                string lang = resourceLoader.GetString("InfomabiakWebSiteLanguageCode");
                string kSuiteTypeName = "myksuite"; // TODO: Fetch the type of the kSuite the user is using (my or pro)
                string result = "https://www.infomaniak.com/" + lang + "/ksuite/" + kSuiteTypeName;
                return new Uri(result);
                
            }
        }
        public KSuiteUpgradeBar()
        {
            InitializeComponent();
        }

    }
}
