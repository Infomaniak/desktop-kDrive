using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Linq;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SyncIcon : UserControl
    {
        private const string ClassicSyncIconResourceKey = "Infomaniak.DS.Icons.Products.kDrive";
        private const string AdvancedSyncIconUriResourceKey = "Infomaniak.DS.Icons.Documents.folder";

        // DependencyProperty
        public ISync Sync
        {
            get => (ISync)GetValue(SyncProperty);
            set => SetValue(SyncProperty, value);
        }

        public string IconUri
        {
            get => (string)GetValue(IconUriProperty);
            set => SetValue(IconUriProperty, value);
        }

        public static readonly DependencyProperty SyncProperty = DependencyProperty.Register(nameof(Sync), typeof(ISync), typeof(SyncIcon), new PropertyMetadata("", OnSyncChanged));

        private static readonly DependencyProperty IconUriProperty = DependencyProperty.Register(nameof(IconUri), typeof(string), typeof(SyncIcon), new PropertyMetadata(null));

        public SyncIcon()
        {
            this.InitializeComponent();
        }

        private static void OnSyncChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var control = d as SyncIcon;
            if (control is null)
                return;

            var sync = e.NewValue as ISync;
            string iconKey;
            if (sync is not null && sync.RemoteNodeId is not null && sync.RemoteNodeId.Any())
                iconKey = AdvancedSyncIconUriResourceKey;
            else
                iconKey = ClassicSyncIconResourceKey;

            var application = Application.Current;
            if (application is not null && application.Resources.ContainsKey(iconKey))
                control.IconUri = (string)application.Resources[iconKey];
            else
                control.IconUri = "";
        }
    }
}
