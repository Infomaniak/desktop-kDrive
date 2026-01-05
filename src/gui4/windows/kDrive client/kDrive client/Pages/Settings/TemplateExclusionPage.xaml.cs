using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;
using System.Threading;

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

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            SetupNavBar();
            _templateList = new ExclusionTemplateListModel();
            _templateList.AddTemplate(new ExclusionTemplate("*.tmp", true, false));
            _templateList.AddTemplate(new ExclusionTemplate("*.test", true, false));
            _templateList.AddTemplate(new ExclusionTemplate("abc*.tmp", true, true));
            _templateList.AddTemplate(new ExclusionTemplate("efg*.hij", true, true));
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
    }


}
