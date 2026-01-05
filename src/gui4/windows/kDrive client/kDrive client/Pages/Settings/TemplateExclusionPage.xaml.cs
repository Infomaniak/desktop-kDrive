using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;
using System.Threading;
using Windows.ApplicationModel.Contacts;

namespace Infomaniak.kDrive.Pages.Settings
{
    public sealed partial class TemplateExclusionPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get => _viewModel; }
        private ExclusionTemplateListModel? _templateList;
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
            _templateList = new ExclusionTemplateListModel();
            await _templateList.LoadTemplates();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "Navigating away from TemplateExclusionPage");
            _templateList?.Dispose();
            _templateList = null;
        }

        private void SetupNavBar()
        {
            NavBar.ItemsSource = new string[] { Utility.GetLocalizedString("Page_SettingsPage_Title/Text"), Utility.GetLocalizedString("Page_TemplateExclusionPage_Title/Text") };
        }
        private void NavBar_ItemClicked(BreadcrumbBar sender, BreadcrumbBarItemClickedEventArgs args)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to SettingsPage");
            Frame.Navigate(typeof(SettingsPage));
        }
        private async void AddRuleButton_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            ContentDialog dialog = new ContentDialog();
            var result = await AddTemplateContentDialog.ShowAsync();
            if (result == ContentDialogResult.Primary)
            {
                var template = NewTemplateTextBox.Text;
                if (string.IsNullOrEmpty(template))
                {
                    Logger.Log(Logger.Level.Warning, "Template name is empty. Aborting addition of new exclusion template.");
                    return;
                }
                if (_templateList is not null)
                {
                    if (_templateList.Templates.Any(t => t.Template.Equals(template, StringComparison.Ordinal)))
                    {
                        Logger.Log(Logger.Level.Warning, $"An exclusion template with the template '{template}' already exists. Aborting addition.");
                        return;
                    }
                    await _templateList.AddTemplate(new ExclusionTemplate(template));
                    Logger.Log(Logger.Level.Info, $"Added new exclusion template: {template}");
                }
                else
                {
                    Logger.Log(Logger.Level.Error, "Failed to add new exclusion template: Template list is null");
                }
            }

        }

        private async void TemplateDeleteMenuItem_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            var item = (sender as FrameworkElement)?.DataContext;
            var exclusionTemplate = item as ExclusionTemplate;
            if (_templateList is not null && exclusionTemplate is not null)
                await _templateList.RemoveTemplate(exclusionTemplate);
            else
                Logger.Log(Logger.Level.Error, "Failed to delete exclusion template: DataContext is not an ExclusionTemplate");
        }
    }
}
