using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Drawing;
using System.Linq;
using Windows.Storage;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class LottiePlayer : UserControl
    {
        public LottiePlayer()
        {
            this.InitializeComponent();
        }

        // DependencyProperty to control the icon color
        public Uri UriSource
        {
            get => (Uri)GetValue(UriSourceProperty);
            set => SetValue(UriSourceProperty, value);
        }

        public static readonly DependencyProperty UriSourceProperty =
            DependencyProperty.Register(
                nameof(UriSource),
                typeof(Uri),
                typeof(LottiePlayer),
                new PropertyMetadata(null)
            );

        private void AnimatedVisualPlayer_Loaded(object sender, RoutedEventArgs e)
        {
            ((FrameworkElement)((AnimatedVisualPlayer)sender).Parent).Opacity = 1;
        }




    }
}
