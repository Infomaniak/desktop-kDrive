using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class DriveStatusIndicator : UserControl
    {
        public DriveStatusIndicator()
        {
            this.InitializeComponent();
        }

        // DependencyProperty
        public string IconUri
        {
            get => (string)GetValue(IconUriProperty);
            set => SetValue(IconUriProperty, value);
        }

        public Brush IconForegroundBrush
        {
            get => (Brush)GetValue(IconForegroundBrushProperty);
            set => SetValue(IconForegroundBrushProperty, value);
        }

        public Brush IconBackgroundBrush
        {
            get => (Brush)GetValue(IconBackgroundBrushProperty);
            set => SetValue(IconBackgroundBrushProperty, value);
        }

        public Drive Drive
        {
            get => (Drive)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }

        public static readonly DependencyProperty IconUriProperty = DependencyProperty.Register(nameof(IconUri), typeof(string), typeof(DriveStatusIndicator), new PropertyMetadata(null));
        public static readonly DependencyProperty DriveProperty = DependencyProperty.Register(nameof(DriveProperty), typeof(Drive), typeof(DriveStatusIndicator), new PropertyMetadata(null));
        public static readonly DependencyProperty IconForegroundBrushProperty = DependencyProperty.Register(nameof(IconForegroundBrush), typeof(Brush), typeof(DriveStatusIndicator), new PropertyMetadata(null));
        public static readonly DependencyProperty IconBackgroundBrushProperty = DependencyProperty.Register(nameof(IconBackgroundBrush), typeof(Brush), typeof(DriveStatusIndicator), new PropertyMetadata(null));
    }
}
