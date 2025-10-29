using DynamicData.Binding;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class ActivityPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public ActivityPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ActivityPage - Initializing ActivityPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ActivityPage components initialized");
            ViewModel.PropertyChanging += ViewModel_PropertyChanging;
            ViewModel.PropertyChanged += ViewModel_PropertyChanged;
            if (ViewModel.SelectedSync is not null)
                ViewModel.SelectedSync.PropertyChanged += ViewModel_SelectedSync_PropertyChanged;


            UpdateTitleTemplate();
        }

        private void ViewModel_PropertyChanging(object? sender, System.ComponentModel.PropertyChangingEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModel.SelectedSync) && ViewModel.SelectedSync != null)
            {
                ViewModel.SelectedSync.PropertyChanged -= ViewModel_SelectedSync_PropertyChanged;
            }
        }

        private void ViewModel_PropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModel.SelectedSync) && ViewModel.SelectedSync != null)
            {
                ViewModel.SelectedSync.PropertyChanged += ViewModel_SelectedSync_PropertyChanged;
                UpdateTitleTemplate();
            }
        }

        private void ViewModel_SelectedSync_PropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Sync.SyncStatus))
                UpdateTitleTemplate();
        }

        private void UpdateTitleTemplate()
        {
            switch (ViewModel.SelectedSync?.SyncStatus)
            {
                case SyncStatus.Running:
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["InProgressTitleTemplate"];
                    break;
                case SyncStatus.Idle:
                    if (ViewModel.SelectedSync.SyncErrors.Any())
                        TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["ErrorTitleTemplate"];
                    else if (!ViewModel.SelectedSync.SyncActivities.Any())
                        TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["NoActivityTitleTemplate"];
                    else
                        TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["UpToDateTitleTemplate"];
                    break;
                case SyncStatus.Offline:
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["OfflineTitleTemplate"];
                    break;
                case SyncStatus.Pause:
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["InPauseTitleTemplate"];
                    break;
                default:
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["LoadingTitleTemplate"];
                    break;
            }
        }
    }
}
