using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;

namespace Infomaniak.kDrive.Pages.Settings
{
    public sealed partial class TemplateExclusionPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get => _viewModel; }
        private ExclusionTemplateListModel? _templateListModel;
        public Drive? ManagedDrive { get; set; }


        public TemplateExclusionPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to TemplateExclusionPage - Initializing TemplateExclusionPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "TemplateExclusionPage components initialized");
        }
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            SetupNavBar();
            _templateListModel = new ExclusionTemplateListModel();
            if (!await _templateListModel.LoadTemplates())
            {
                Logger.Log(Logger.Level.Error, "Failed to load exclusion templates. The template list will be empty.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Navigating away from TemplateExclusionPage");
            _templateListModel?.Dispose();
            _templateListModel = null;
        }

        private void SetupNavBar()
        {
            NavBar.ItemsSource = new string[] { Localizer.Instance.GetString("settingsTitle"), Localizer.Instance.GetString("filesToExclude") };
        }
        private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to SettingsPage");
            Frame.Navigate(typeof(SettingsPage));
        }
        private async void AddRuleButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            var popupPage = new Pages.Popup.AddExclusionRulePopup();

            ContentDialog dialog = new ContentDialog()
            {
                XamlRoot = this.XamlRoot,
                Content = popupPage,
                PrimaryButtonText = Localizer.Instance.GetString("dialogNewExclusionRulePrimaryButton"),
                CloseButtonText = Localizer.Instance.GetString("buttonCancel"),
                DefaultButton = ContentDialogButton.Primary
            };

            var result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary)
            {
                var template = popupPage.ExclusionRuleTextBox.Text;
                if (string.IsNullOrEmpty(template))
                {
                    Logger.Log(Logger.Level.Info, "Template name is empty. Aborting addition of new exclusion template.");
                    return;
                }
                if (_templateListModel is not null)
                {
                    if (_templateListModel.Templates.Any(t => t.Template.Equals(template, StringComparison.Ordinal)))
                    {
                        Logger.Log(Logger.Level.Warning, $"An exclusion template with the template '{template}' already exists. Aborting addition.");
                        return;
                    }
                    await _templateListModel.AddTemplate(new ExclusionTemplate(template, popupPage.WarningCheckBox.IsChecked ?? false));
                    Logger.Log(Logger.Level.Info, $"Added new exclusion template: {template}");
                }
                else
                {
                    Logger.Log(Logger.Level.Error, "Failed to add new exclusion template: Template list is null");
                }
            }
        }

        private void TemplateCheckBox_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            UpdateSelectedCount();
        }

        private void SelectAllCheckBox_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (_templateListModel is null)
                return;

            bool isChecked = (sender as CheckBox)?.IsChecked ?? false;
            foreach (var template in _templateListModel.UserDefinedTemplates)
            {
                template.IsSelected = isChecked;
            }
            UpdateSelectedCount();
        }

        private async void DeleteButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (_templateListModel is null)
                return;

            var selectedTemplates = _templateListModel.UserDefinedTemplates.Where(t => t.IsSelected).ToList();
            if (selectedTemplates.Count == 0)
                return;

            // Deselect all items
            foreach (var template in selectedTemplates)
            {
                template.IsSelected = false;
            }

            // Uncheck the select all checkbox
            SelectAllCheckBox.IsChecked = false;

            // Delete the selected templates
            await _templateListModel.RemoveTemplates(selectedTemplates);
            UpdateSelectedCount();
        }

        private void UpdateSelectedCount()
        {
            if (_templateListModel is null)
                return;

            _templateListModel.UpdateSelectedCount();
            int selectedCount = _templateListModel.SelectedCount;

            // Update the select all checkbox state
            int totalCount = _templateListModel.UserDefinedTemplates.Count;
            if (selectedCount == 0)
            {
                SelectAllCheckBox.IsChecked = false;
            }
            else if (selectedCount == totalCount)
            {
                SelectAllCheckBox.IsChecked = true;
            }
            else
            {
                SelectAllCheckBox.IsChecked = null; // Indeterminate state
            }
        }

        private async void WarningToggleSwitch_Toggled(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (_templateListModel is null)
            {
                Logger.Log(Logger.Level.Error, "Template list model is null. Cannot save template changes.");
                return;
            }

            ExclusionTemplate? exclusionTemplate = (sender as FrameworkElement)?.DataContext as ExclusionTemplate;
            if (exclusionTemplate is null)
            {
                Logger.Log(Logger.Level.Error, "DataContext is not an ExclusionTemplate. Cannot save template changes.");
                return;
            }

            ToggleSwitch? toggleSwitch = sender as ToggleSwitch;
            if (toggleSwitch is null)
            {
                Logger.Log(Logger.Level.Error, "Sender is not a ToggleSwitch. Cannot save template changes.");
                return;
            }

            exclusionTemplate.Warning = toggleSwitch.IsOn;
            await _templateListModel.SaveUserTemplates();
        }
    }
}
