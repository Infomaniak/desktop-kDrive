using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class KSuiteUpgradeBar : UserControl
    {
        public KSuiteUpgradeBar()
        {
            InitializeComponent();
        }
        private void UserControl_Unloaded(object sender, RoutedEventArgs e)
        {
            Bindings.StopTracking();
        }
    }
}
