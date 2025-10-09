using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
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
using System.Threading.Tasks;
using Windows.Data.Xml.Dom;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls;

public sealed partial class SyncStartPauseButton : UserControl
{
    private AppModel _viewModel = (App.Current as App).Data;
    public AppModel ViewModel
    {
        get { return _viewModel; }
    }

    public SyncStartPauseButton()
    {
        InitializeComponent();
    }

    private async void StartPauseButton_Click(object sender, RoutedEventArgs e)
    {
        // TODO: Replace with actual start/pause logic
        if (ViewModel.SelectedSync?.SyncStatus == SyncStatus.Pause)
        {
            Logger.Log(Logger.Level.Info, "Starting sync...");
            ViewModel.SelectedSync.SyncStatus = SyncStatus.Starting;
            await Task.Delay(1000); // Simulate some delay for pausing
            ViewModel.SelectedSync.SyncStatus = SyncStatus.Running;
        }
        else if (ViewModel.SelectedSync?.SyncStatus == SyncStatus.Running || ViewModel.SelectedSync?.SyncStatus == SyncStatus.Idle)
        {
            Logger.Log(Logger.Level.Info, "Pausing sync...");
            ViewModel.SelectedSync.SyncStatus = SyncStatus.Pausing;
            await Task.Delay(1000); // Simulate some delay for pausing
            ViewModel.SelectedSync.SyncStatus = SyncStatus.Pause;
        }
    }
}

public class StartPauseButtonTemplateSelector : DataTemplateSelector
{
    public DataTemplate? PauseButtonTemplate { get; set; }
    public DataTemplate? StartButtonTemplate { get; set; }
    public DataTemplate? LoadingButtonTemplate { get; set; }

    protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
    {
        // Null value can be passed by IDE designer
        if (item == null)
            return null;
        if (item is SyncStatus syncStatus)
        {
            switch (syncStatus)
            {
                case SyncStatus.Idle:
                case SyncStatus.Running:
                    return PauseButtonTemplate;
                case SyncStatus.Pause:
                case SyncStatus.Offline:
                    return StartButtonTemplate;
                default:
                    return LoadingButtonTemplate;
            }
        }
        return LoadingButtonTemplate;
    }
}

public class SyncStatusToButtonEnabledConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is SyncStatus syncStatus)
        {
            return (syncStatus == SyncStatus.Running || syncStatus == SyncStatus.Pause || syncStatus == SyncStatus.Idle);
        }
        return false;
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        throw new NotImplementedException();
    }
}
