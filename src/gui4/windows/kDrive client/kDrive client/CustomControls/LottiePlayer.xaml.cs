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
        private int _currentPlayCount = 0;

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

        public int PlayCount
        {
            get => (int)GetValue(PlayCountProperty);
            set => SetValue(PlayCountProperty, value);
        }

        public static readonly DependencyProperty PlayCountProperty =
            DependencyProperty.Register(
            nameof(PlayCount),
            typeof(int),
            typeof(LottiePlayer),
            new PropertyMetadata(0));

        private static async void OnUriSourceChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is LottiePlayer lottiePlayer && e.NewValue is Uri)
            {
                try
                {
                    await lottiePlayer.UpdateLottieAsync();
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to load Lottie animation: {ex.Message}");
                }
            }
        }

        private async Task UpdateLottieAsync()
        {
            string fullPath = Path.Combine(AppContext.BaseDirectory, UriSource.LocalPath.TrimStart('\\', '/')).Replace("/", "\\");
            StorageFile file = await StorageFile.GetFileFromPathAsync(fullPath);
            await VisualSource.SetSourceAsync(file);
            await PlayAnimationAsync();
        }

        private async Task PlayAnimationAsync()
        {
            if (PlayCount > 0)
            {
                _currentPlayCount = 0;
                while (_currentPlayCount < PlayCount)
                {
                    await AnimatedVisualPlayer.PlayAsync(0, 1, false);
                    _currentPlayCount++;
                }
            }
            else
            {
                await AnimatedVisualPlayer.PlayAsync(0, 1, true);
            }
        }

        private void AnimatedVisualPlayer_Loaded(object sender, RoutedEventArgs e)
        {
            ((FrameworkElement)((AnimatedVisualPlayer)sender).Parent).Opacity = 1;
        }
    }
}
