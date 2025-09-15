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

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class KSuiteUpgradeBar : UserControl
    {
        public KSuiteUpgradeBar()
        {
            InitializeComponent();
            var resourceLoader = new Microsoft.Windows.ApplicationModel.Resources.ResourceLoader();

            // TODO: Fetch the type of the kSuite the user is using (my or pro)
            if (true /* Selected kSuite is "my"*/)
            {
                UpgradeHyperLinkButton.NavigateUri = new Uri(resourceLoader.GetString("Global_UpgradeOfferMy_Url"));
            }
            else
            {
               UpgradeHyperLinkButton.NavigateUri = new Uri(resourceLoader.GetString("Global_UpgradeOfferProUrl"));
            }
        }
    }
}
