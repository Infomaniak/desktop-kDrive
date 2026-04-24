using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Threading.Tasks;
using Windows.System;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class ConflictVersionPresenter : UserControl
    {
        public ConflictVersionPresenter()
        {
            this.InitializeComponent();
        }

        // DependencyProperty
        public NodeConflictInfo? NodeInfo
        {
            get => (NodeConflictInfo?)GetValue(NodeInfoProperty);
            set => SetValue(NodeInfoProperty, value);
        }
        public bool IsSelected
        {
            get => (bool)GetValue(IsSelectedProperty);
            set => SetValue(IsSelectedProperty, value);
        }

        public bool ShowMostRecentBadge
        {
            get => (bool)GetValue(ShowMostRecentBadgeProperty);
            set => SetValue(ShowMostRecentBadgeProperty, value);
        }

        public bool IsRemote
        {
            get => (bool)GetValue(IsRemoteProperty);
            set => SetValue(IsRemoteProperty, value);
        }

        public string? NodeAbsolutePath
        {
            get => (string?)GetValue(NodeAbsolutePathProperty);
            set => SetValue(NodeAbsolutePathProperty, value);
        }

        public Error? Error
        {
            get => (Error?)GetValue(ErrorProperty);
            set => SetValue(ErrorProperty, value);
        }

        public static readonly DependencyProperty IsRemoteProperty =
            DependencyProperty.Register(
            nameof(IsRemote),
            typeof(bool),
            typeof(ConflictVersionPresenter),
            new PropertyMetadata(null));

        public static readonly DependencyProperty IsSelectedProperty =
            DependencyProperty.Register(
            nameof(IsSelected),
            typeof(bool),
            typeof(ConflictVersionPresenter),
            new PropertyMetadata(false));

        public static readonly DependencyProperty ShowMostRecentBadgeProperty =
            DependencyProperty.Register(
            nameof(ShowMostRecentBadge),
            typeof(bool),
            typeof(ConflictVersionPresenter),
            new PropertyMetadata(false));

        public static readonly DependencyProperty NodeInfoProperty =
            DependencyProperty.Register(
            nameof(NodeInfo),
            typeof(NodeConflictInfo),
            typeof(ConflictVersionPresenter),
            new PropertyMetadata(null));

        public static readonly DependencyProperty NodeAbsolutePathProperty =
            DependencyProperty.Register(
            nameof(NodeAbsolutePath),
            typeof(string),
            typeof(ConflictVersionPresenter),
            new PropertyMetadata(null));

        public static readonly DependencyProperty ErrorProperty =
            DependencyProperty.Register(
            nameof(Error),
            typeof(Error),
            typeof(ConflictVersionPresenter),
            new PropertyMetadata(null));


        // Events
        public event EventHandler Selected = delegate { };
        public event EventHandler Unselected = delegate { };

        private void RemoteTogglebutton_Checked(object sender, RoutedEventArgs e)
        {
            if (Selected is not null)
            {
                Selected(this, EventArgs.Empty);
            }
        }

        private void RemoteTogglebutton_Unchecked(object sender, RoutedEventArgs e)
        {
            if (Unselected is not null)
            {
                Unselected(this, EventArgs.Empty);
            }
        }

        private string GetToggleButtonText(bool? isToggled)
        {
            return isToggled ?? false ? Localizer.Instance.GetString("buttonSelected") : Localizer.Instance.GetString("buttonSelect");
        }

        public SolidColorBrush GetBorderBrush(bool isSelected)
        {
            // Return AccentFillColorDefaultBrush if selected, CardStrokeColorDefaultBrush if not
            if (isSelected)
                return Application.Current.Resources["AccentFillColorDefaultBrush"] as SolidColorBrush ?? new SolidColorBrush(Microsoft.UI.Colors.Transparent);
            else
                return Application.Current.Resources["CardStrokeColorDefaultBrush"] as SolidColorBrush ?? new SolidColorBrush(Microsoft.UI.Colors.Transparent);
        }

        private async void ViewButton_Click(object sender, RoutedEventArgs e)
        {
            const int disableDurationMs = 5000; // Duration to disable the button to prevent multiple clicks

            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            try
            {
                if (IsRemote)
                {
                    if (await OpenInBrowserAsync())
                    {
                        await Task.Delay(disableDurationMs); // Avoid multiple click by the time the file open
                        return;
                    }

                    Logger.Log(Logger.Level.Info, "Failed to open remote node in browser, falling back to the local version of the remote version.");
                }


                if (NodeAbsolutePath is null)
                {
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }


                await Utility.OpenFileAsync(NodeAbsolutePath);
                await Task.Delay(disableDurationMs); // Avoid multiple click by the time the file open
                if (control is not null)
                    control.IsEnabled = true;
            }
            finally
            {
                if (control is not null)
                    control.IsEnabled = true;
            }
        }

        private async Task<bool> OpenInBrowserAsync()
        {
            if (!IsRemote || Error is null)
            {
                Logger.Log(Logger.Level.Warning, $"Attempted to open in browser but the version is not remote or error/sync info is missing: IsRemote={IsRemote}, Error is null={Error is null}, Sync is null={Error?.Sync is null}");
                return false;
            }

            if (!App.ServiceProvider.GetRequiredService<AppModel>().NetworkAvailable)
            {
                Logger.Log(Logger.Level.Info, "Network is not available, cannot open remote node in browser.");
                return false;
            }

            return await Error.OpenItemInWebViewAsync();
        }
    }
}