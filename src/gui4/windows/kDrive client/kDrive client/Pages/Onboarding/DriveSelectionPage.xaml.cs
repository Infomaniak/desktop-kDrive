using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class DriveSelectionPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onBoardingViewModel;
        private readonly Dictionary<NewSync, string> _previousSyncPaths = []; // To store previous sync paths and allow reverting if needed in advanced settings
        public AppModel ViewModel { get { return _viewModel; } }
        public ViewModels.Onboarding? ObViewModel { get => _onBoardingViewModel; }
        public DriveSelectionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to DriveSelectionPage - Initializing DriveSelectionPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "DriveSelectionPage components initialized");
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;
                if (obvm.SelectedUser is null)
                {
                    Logger.Log(Logger.Level.Error, "SelectedUser is null in OnBoardingViewModel when navigating to DriveSelectionPage");
                    obvm.Reset();
                    Frame.Navigate(typeof(Onboarding.WelcomePage), obvm);
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }
                if (!await obvm.SelectedUser.RefreshAvailableDrives(CancellationToken.None))
                {
                    Logger.Log(Logger.Level.Error, "Failed to refresh available drives for user in DriveSelectionPage");
                    obvm.Reset();
                    Frame.Navigate(typeof(Onboarding.WelcomePage), obvm);
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }


                if (!obvm.SelectedUser.AllDrives.Any())
                {
                    Logger.Log(Logger.Level.Info, "No drives found for user in DriveSelectionPage - Navigating to NoDrivePage");
                    Frame.Navigate(typeof(NoDrivesPage), ObViewModel);
                    return;
                }
                if (App.Current is App { CurrentWindow: OnBoardingWindow onBoardingWindow })
                    onBoardingWindow.UpdateLottieSource("Infomaniak.Custom.Animations.synchro-file", 219);
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "OnBoardingViewModel parameter missing when navigating to DriveSelectionPage");
                throw new Exception("OnBoardingViewModel parameter missing when navigating to DriveSelectionPage");
            }
        }

        private async void DriveListCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            if (sender is CheckBox cb && cb.DataContext is IDrive drive && _onBoardingViewModel != null)
            {
                cb.IsEnabled = false;
                var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
                string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                string desiredFolderName = drive.Name.StartsWith("kDrive") ? drive.Name : $"kDrive {drive.Name}";
                string desiredPath = Path.Combine(userProfile, desiredFolderName);
                string? result = await commServices.GetGoodPathForNewSync(drive, desiredPath, CancellationToken.None);
                if (result is null)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to get a valid sync path for drive '{drive.Name}'");
                    cb.IsChecked = false;
                    cb.IsEnabled = true;
                    Utility.ShowTeachingTipFromxUid("InvalidDefaultSyncLocationTeachingTip");
                    return;
                }

                NewSync newSync = new() { Drive = drive, DefaultPath = result, LocalPath = result };
                await newSync.SelectBestVfsMode();

                _onBoardingViewModel.NewSyncs.Add(newSync);
                cb.IsEnabled = true;
            }
        }

        private void DriveListCheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            if (sender is CheckBox cb && cb.DataContext is IDrive drive && _onBoardingViewModel != null)
            {
                cb.IsEnabled = false;
                var syncToRemove = _onBoardingViewModel.NewSyncs.FirstOrDefault(s => s.Drive == drive);
                if (syncToRemove != null)
                {
                    _onBoardingViewModel.NewSyncs.Remove(syncToRemove);
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "Drive to remove not found in NewSyncs list");
                }
                cb.IsEnabled = true;
            }
        }

        private async void AdvancedSettingsButton_Click(object sender, RoutedEventArgs e)
        {
            var driveSetupDialog = new CustomControls.DriveSetupContentDialog(this.XamlRoot, _onBoardingViewModel!.NewSyncs);
            await driveSetupDialog.ShowAsync();
        }

        private void Finish_Click(object sender, RoutedEventArgs e)
        {
            Frame.Navigate(typeof(FinishingPage), _onBoardingViewModel);
        }

        private void ChangeUserButton_Click(object sender, RoutedEventArgs e)
        {
            _onBoardingViewModel?.Reset();
            Frame.Navigate(typeof(Onboarding.WelcomePage), _onBoardingViewModel);

        }
    }

    public partial class DriveTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? SingleAccountDriveTemplate { get; set; }
        public DataTemplate? MultiAccountDriveTemplate { get; set; }
        public DataTemplate? SingleAccountDriveDisabledTemplate { get; set; }
        public DataTemplate? MultiAccountDriveDisabledTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is not IDrive drive)
                return base.SelectTemplateCore(item, container);

            User? user = App.ServiceProvider.GetRequiredService<AppModel>().Users.FirstOrDefault(u => u.DbId == drive.UserDbId);
            if (user is null)
            {
                Logger.Log(Logger.Level.Warning, "DriveTemplateSelector: User not found for drive");
                return drive.IsConfigured ? SingleAccountDriveDisabledTemplate : SingleAccountDriveTemplate; // Fallback to single account template
            }

            if (drive.IsConfigured)
                return user.AllDrives.Select(drive => drive.AccountId).Distinct().Count() > 1 ? MultiAccountDriveDisabledTemplate : SingleAccountDriveDisabledTemplate;
            else
                return user.AllDrives.Select(drive => drive.AccountId).Distinct().Count() > 1 ? MultiAccountDriveTemplate : SingleAccountDriveTemplate;
        }
    }
}
