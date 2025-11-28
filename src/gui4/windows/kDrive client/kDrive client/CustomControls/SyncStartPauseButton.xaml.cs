using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Data;
using System;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls;

public sealed partial class SyncStartPauseButton : UserControl
{
    private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
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
        if (ViewModel?.SelectedSync?.SyncStatus == SyncStatus.Running || ViewModel.SelectedSync?.SyncStatus == SyncStatus.Idle)
        {
            Logger.Log(Logger.Level.Info, "Pausing sync...");
            await ViewModel.SelectedSync?.Pause();
        }
        else if (ViewModel is not null)
        {
            Logger.Log(Logger.Level.Info, "Starting sync...");
            await ViewModel.SelectedSync?.Start();
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
                case SyncStatus.Paused:
                case SyncStatus.Offline:
                case SyncStatus.Error:
                case SyncStatus.Stopped:
                    return StartButtonTemplate;
                default:
                    return LoadingButtonTemplate;
            }
        }
        return LoadingButtonTemplate;
    }
}

public class SyncToButtonEnabledConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is Sync sync)
        {
            SyncStatus syncStatus = sync.SyncStatus; ;
            return (syncStatus == SyncStatus.Running || syncStatus == SyncStatus.Paused || syncStatus == SyncStatus.Idle || syncStatus == SyncStatus.Stopped || syncStatus == SyncStatus.Error);
        }
        return false;
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        throw new NotImplementedException();
    }
}
