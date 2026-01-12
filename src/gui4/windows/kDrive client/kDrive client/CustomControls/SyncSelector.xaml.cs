using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
 
    public sealed partial class SyncSelector : UserControl
    {
        public AppModel ViewModel { get; } =
            App.ServiceProvider.GetRequiredService<AppModel>();

        public SyncSelector()
        {
            InitializeComponent();
        }
    }

    public partial class SyncTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? MainSyncTemplate { get; set; }
        public DataTemplate? AdvancedSyncTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item == null)
                return null;
            if (item is Sync sync)
            {
                if(sync.RemotePath == "")
                {
                    return MainSyncTemplate;
                }
                else
                {
                    return AdvancedSyncTemplate;
                }
            }
            Logger.Log(Logger.Level.Warning, "SyncTemplateSelector: item is not of type Sync");
            return MainSyncTemplate;
        }
    }
}

