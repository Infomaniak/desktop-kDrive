using CommunityToolkit.Mvvm.ComponentModel;
using KDriveClient.ViewModels;
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
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace KDriveClient.CustomControls;

public sealed partial class SyncStatusBar : UserControl
{
    internal AppModel _viewModel = ((App)Application.Current).Data;
    internal AppModel ViewModel
    {
        get { return _viewModel; }
    }

    public SyncStatusBar()
    {
        InitializeComponent();
    }

    private async void StartPauseButton_Click(object sender, RoutedEventArgs e)
    {
        StartPauseButton.IsEnabled = false;
        if (ViewModel.SelectedDrive?.SyncStatus == SyncStatus.Pause)
        {
            ViewModel.SelectedDrive.SyncStatus = SyncStatus.Starting;
            await Task.Delay(5000); // Simulate some delay for pausing
            ViewModel.SelectedDrive.SyncStatus = SyncStatus.Running;
        }
        else if (ViewModel.SelectedDrive?.SyncStatus == SyncStatus.Running)
        {
            ViewModel.SelectedDrive.SyncStatus = SyncStatus.Pausing;
            await Task.Delay(5000); // Simulate some delay for pausing
            ViewModel.SelectedDrive.SyncStatus = SyncStatus.Pause;
        }
        StartPauseButton.IsEnabled = true;
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
        if (item == null) return null;
        if (item is SyncStatus syncStatus)
        {
            switch (syncStatus)
            {
                case SyncStatus.Running:
                    return PauseButtonTemplate;
                case SyncStatus.Pause:
                    return StartButtonTemplate;
                default:
                    return LoadingButtonTemplate;
            }
        }
        return LoadingButtonTemplate;
    }
}

public class TextStatusTemplateSelector : DataTemplateSelector
{
    public DataTemplate? PausedStatusTemplate { get; set; }
    public DataTemplate? PausingStatusTemplate { get; set; }
    public DataTemplate? StartingStatusTemplate { get; set; }
    public DataTemplate? SyncingStatusTemplate { get; set; }
    public DataTemplate? SyncedStatusTemplate { get; set; }
    public DataTemplate? UnknownStatusTemplate { get; set; }

    protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
    {
        // Null value can be passed by IDE designer
        if (item == null) return null;
        if (item is SyncStatus syncStatus)
        {
            switch (syncStatus)
            {
                case SyncStatus.Running:
                    return SyncingStatusTemplate;
                case SyncStatus.Starting:
                    return StartingStatusTemplate;
                case SyncStatus.Pausing:
                    return PausingStatusTemplate;
                case SyncStatus.Pause:
                    return PausedStatusTemplate;
                default:
                    return UnknownStatusTemplate;
            }
        }
        return UnknownStatusTemplate;
    }

}

public class SyncStatusToButtonEnabledConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is SyncStatus syncStatus)
        {
            return (syncStatus == SyncStatus.Running || syncStatus == SyncStatus.Pause);
        }
        return false;
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        throw new NotImplementedException();
    }
}
