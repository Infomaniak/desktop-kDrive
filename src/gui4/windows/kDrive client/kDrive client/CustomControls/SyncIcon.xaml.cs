using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Linq;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SyncIcon : UserControl
    {
        private const string ClassicSyncIconRessourceKey = "Infomaniak.DS.Icons.Products.kDrive";
        private const string AdvancedSyncIconUriRessourceKey = "Infomaniak.DS.Icons.Documents.folder";

        // DependencyProperty
        public ISync Sync
        {
            get => (ISync)GetValue(SyncProperty);
            set
            {
                SetValue(SyncProperty, value);
                if (value.RemoteNodeId.Count() > 0)
                    IconUri = (string)Application.Current.Resources[AdvancedSyncIconUriRessourceKey];
                else
                    IconUri = (string)Application.Current.Resources[ClassicSyncIconRessourceKey];
            }
        }

        public string IconUri
        {
            get => (string)GetValue(IconUriProperty);
            set => SetValue(IconUriProperty, value);
        }

        public static readonly DependencyProperty SyncProperty = DependencyProperty.Register(nameof(Sync), typeof(ISync), typeof(SyncIcon), new PropertyMetadata(null));

        private static readonly DependencyProperty IconUriProperty = DependencyProperty.Register(nameof(IconUri), typeof(string), typeof(SyncIcon), new PropertyMetadata(null));

        public SyncIcon()
        {
            this.InitializeComponent();
        }
    }
}
