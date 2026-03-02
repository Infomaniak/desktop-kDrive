using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class ConflictVersionPresenter : UserControl
    {
        private int _currentPlayCount = 0;

        public ConflictVersionPresenter()
        {
            this.InitializeComponent();
        }

        // DependencyProperty
        public ReplicaSide FileSide
        {
            get => (ReplicaSide)GetValue(FileSideProperty);
            set => SetValue(FileSideProperty, value);
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

        public static readonly DependencyProperty FileSideProperty =
            DependencyProperty.Register(
            nameof(FileSide),
            typeof(ReplicaSide),
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


        // Events
        public event EventHandler Selected;
        public event EventHandler Unselected;

        private string GetToggleButtonText(bool? isToggled)
        {
            return isToggled ?? false ? "Selectionné" : "Selectionner";
        }

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
    }
}
