using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Specialized;
using System.ComponentModel;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public partial class ErrorBar : UserControl
    {
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public ErrorBar()
        {
            InitializeComponent();
        }

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            ViewModel.SelectedSyncChanged += ViewModel_SelectedSyncChanged;
            if (ViewModel.SelectedSync?.SyncErrors is not null)
                ViewModel.SelectedSync.SyncErrors.CollectionChanged += SyncErrors_CollectionChanged;
            UpdateInfoBar();
        }

        private void UserControl_Unloaded(object sender, RoutedEventArgs e)
        {
            DetachEventHandlers();
        }

        private void DetachEventHandlers()
        {
            ViewModel.SelectedSyncChanged -= ViewModel_SelectedSyncChanged;
            if (ViewModel.SelectedSync?.SyncErrors is not null)
                ViewModel.SelectedSync.SyncErrors.CollectionChanged -= SyncErrors_CollectionChanged;
        }

        private void ViewModel_SelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            if (e.OldValue is not null)
                e.OldValue.SyncErrors.CollectionChanged -= SyncErrors_CollectionChanged;
            if (e.NewValue is not null)
                e.NewValue.SyncErrors.CollectionChanged += SyncErrors_CollectionChanged;
            UpdateInfoBar();
        }

        private void SyncErrors_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
        {
            UpdateInfoBar();
        }

        private void UpdateInfoBar()
        {
            if (ViewModel.SelectedSync is null)
                return;
            if (ViewModel.SelectedSync.SyncErrors.Count == 0)
            {
                InfoBar_.IsOpen = false;
            }
            else
            {
                InfoBar_.IsOpen = true;
                InfoBar_.Title = Utility.GetLocalizedString("CC_ErrorBar/Title", ViewModel.SelectedSync.SyncErrors.Count);
            }
        }

        public static readonly DependencyProperty FrameProperty =
            DependencyProperty.Register(nameof(Frame), typeof(Frame), typeof(ErrorBar), new PropertyMetadata(null));

        private void InfoBarButton_Click(object sender, RoutedEventArgs e)
        {
            Frame? frame = Utility.GetFrame(this);
            if (frame is not null)
            {
                Logger.Log(Logger.Level.Info, "Navigating to ErrorPage.");
                frame.Navigate(typeof(Pages.ErrorPage));
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Could not find Frame in visual tree to navigate to error page");
            }
        }
    }
}
