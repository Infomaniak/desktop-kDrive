using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System.ComponentModel;
using System.Drawing;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class DriveIcon : PathIcon, INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        public DriveIcon()
        {
            IconColor = Color.AliceBlue;
            this.InitializeComponent();
        }

        public Color IconColor
        {
            get => (Color)GetValue(IconColorProperty);
            set
            {
                SetValue(IconColorProperty, value);
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IconColor)));
            }
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
