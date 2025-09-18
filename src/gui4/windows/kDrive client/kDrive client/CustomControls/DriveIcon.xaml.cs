using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System.Drawing;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class DriveIcon : UserControl
    {
        public DriveIcon()
        {
            this.InitializeComponent();
        }

        // DependencyProperty to control the icon color
        public Color IconColor
        {
            get => (Color)GetValue(IconColorProperty);
            set => SetValue(IconColorProperty, value);
        }

        public static readonly DependencyProperty IconColorProperty =
            DependencyProperty.Register(
                nameof(IconColor),
                typeof(Color),
                typeof(DriveIcon),
                new PropertyMetadata(Color.Blue)
            );

    }
}
