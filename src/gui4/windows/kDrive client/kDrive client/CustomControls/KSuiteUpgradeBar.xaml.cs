using Microsoft.UI.Xaml.Controls;
using System;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class KSuiteUpgradeBar : UserControl
    {
        public KSuiteUpgradeBar()
        {
            InitializeComponent();

            Uri? upgradekSuiteUri;
            // TODO: Fetch the type of the kSuite the user is using (my or pro)
            Uri.TryCreate(Utility.GetLocalizedString("Global_UpgradeOfferMy_Url"), UriKind.Absolute, out upgradekSuiteUri);
            //Uri.TryCreate(Utility.GetLocalizedString("Global_UpgradeOfferProUrl"), UriKind.Absolute, out upgradekSuiteUri);
            UpgradeHyperLinkButton.NavigateUri = upgradekSuiteUri;
        }
    }
}
