using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class ItemNamePresenter : UserControl
    {
        public ItemNamePresenter()
        {
            this.InitializeComponent();
        }

        // DependencyProperty

        // The path or name of the item to display. The control will handle truncation and tooltip if the name is too long.
        public string ItemPath
        {
            get => (string)GetValue(ItemPathProperty);
            set => SetValue(ItemPathProperty, value);
        }

        public NodeType? NodeType
        {
            get => (NodeType)GetValue(NodeTypeProperty);
            set => SetValue(NodeTypeProperty, value);
        }

        public Visibility IsDirectoryIconVisible
        {
            get => NodeType == Types.NodeType.Directory ? Visibility.Visible : Visibility.Collapsed;
        }

        public Visibility IsFileIconVisible
        {
            get => NodeType == Types.NodeType.File ? Visibility.Visible : Visibility.Collapsed;
        }

        public static readonly DependencyProperty ItemPathProperty = DependencyProperty.Register(nameof(ItemPath), typeof(string), typeof(ItemNamePresenter), new PropertyMetadata(null));
        public static readonly DependencyProperty NodeTypeProperty = DependencyProperty.Register(nameof(NodeType), typeof(NodeType), typeof(ItemNamePresenter), new PropertyMetadata(Types.NodeType.File));
    }
}
