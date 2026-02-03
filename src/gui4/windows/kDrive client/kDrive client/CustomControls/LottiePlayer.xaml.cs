using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.IO;
using System.Threading.Tasks;
using Windows.Storage;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class LottiePlayer : UserControl
    {
        public LottiePlayer()
        {
            this.InitializeComponent();
        }

        // DependencyProperty
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
            new PropertyMetadata(null, OnUriSourceChanged));

        private static void OnUriSourceChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is LottiePlayer lottiePlayer && e.NewValue is Uri)
            {
                // Fire-and-forget the async method
                _ = lottiePlayer.UpdateLottieAsync();
            }
        }

        private async Task UpdateLottieAsync()
        {
            string fullPath = Path.Combine(AppContext.BaseDirectory, UriSource.LocalPath.TrimStart('\\', '/')).Replace("/", "\\");
            StorageFile file = await StorageFile.GetFileFromPathAsync(fullPath);
            await VisualSource.SetSourceAsync(file);
        }

        private void AnimatedVisualPlayer_Loaded(object sender, RoutedEventArgs e)
        {
            ((FrameworkElement)((AnimatedVisualPlayer)sender).Parent).Opacity = 1;
        }
    }
}
