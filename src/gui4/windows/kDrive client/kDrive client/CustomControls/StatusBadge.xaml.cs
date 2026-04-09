using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class StatusBadge : UserControl
    {
        public StatusBadge()
        {
            this.InitializeComponent();
        }

        private void UserControl_Unloaded(object sender, RoutedEventArgs e)
        {
            Bindings.StopTracking();
        }

        // DependencyProperty
        public string IconUri
        {
            get => (string)GetValue(IconUriProperty);
            set => SetValue(IconUriProperty, value);
        }

        public static readonly DependencyProperty IconUriProperty = DependencyProperty.Register(nameof(IconUri), typeof(string), typeof(StatusBadge), new PropertyMetadata(null));

        private double GetIconSize(double size) => size * 0.55;
    }
}
