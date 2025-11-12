using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.TemplateSelectors
{
    public class SyncStatusTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? SyncUpToDateTemplate { get; set; }
        public DataTemplate? SyncInProgressTemplate { get; set; }
        public DataTemplate? SyncInPauseTemplate { get; set; }
        public DataTemplate? SyncOfflineTemplate { get; set; }
        public DataTemplate? SyncLoadingTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            // Null value can be passed by IDE designer
            if (item == null)
                return null;

            SyncStatus syncStatus;
            if (item is SyncStatus status)
            {
                syncStatus = status;
            }
            else if (item is ViewModels.Sync sync)
            {
                syncStatus = sync.SyncStatus;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unexpected type in SelectTemplateCore");
                return SyncLoadingTemplate;
            }

            switch (syncStatus)
            {
                case SyncStatus.Starting:
                case SyncStatus.PauseAsked:
                case SyncStatus.Undefined:
                    return SyncLoadingTemplate;
                case SyncStatus.Running:
                    return SyncInProgressTemplate;
                case SyncStatus.Paused:
                case SyncStatus.Stopped:
                    return SyncInPauseTemplate;
                case SyncStatus.Offline:
                    return SyncOfflineTemplate;
                case SyncStatus.Idle:
                    return SyncUpToDateTemplate;
                default:
                    return SyncLoadingTemplate;
            }
        }
    }
}
