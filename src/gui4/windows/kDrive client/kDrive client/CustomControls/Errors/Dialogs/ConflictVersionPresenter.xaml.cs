using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using System;
using System.Threading.Tasks;

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
            if (NodeAbsolutePath is null)
            {
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }
            var control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            await Utility.OpenFileAsync(NodeAbsolutePath);
            await Task.Delay(5000); // Avoid multiple click by the time the fie open
            if (control is not null)
                control.IsEnabled = true;
        }
    }
}
