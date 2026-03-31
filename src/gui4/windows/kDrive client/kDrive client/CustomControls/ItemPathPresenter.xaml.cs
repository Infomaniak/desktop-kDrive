using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.IO;
using Windows.ApplicationModel.DataTransfer;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class ItemPathPresenter : UserControl
    {
        public ItemPathPresenter()
        {
            this.InitializeComponent();
        }

        // DependencyProperty

        // The relative path of the item to display. The control shows the path with a copy-to-clipboard button.
        public string RelativeItemPath
        {
            get => (string)GetValue(ItemRelativePathProperty);
            set
            {
                SetValue(ItemRelativePathProperty, value);
                UpdateDisplayedPath();
            }
        }

        public Sync? Sync
        {
            get => (Sync?)GetValue(SyncProperty);
            set
            {
                SetValue(SyncProperty, value);
                UpdateDisplayedPath();
            }
        }

        public string AbsoluteItemPath
        {
            get => (string)GetValue(ItemAbsolutePathProperty);
            set
            {
                SetValue(ItemAbsolutePathProperty, value);
                UpdateDisplayedPath();
            }
        }

        private string DisplayedPath
        {
            get => (string)GetValue(DisplayedPathProperty);
            set => SetValue(DisplayedPathProperty, value);
        }

        private string ToolTipPath { 
            get => (string)GetValue(ToolTipPathProperty);
            set => SetValue(ToolTipPathProperty, value);
        }

        public static readonly DependencyProperty ItemRelativePathProperty = DependencyProperty.Register(nameof(RelativeItemPath), typeof(string), typeof(ItemPathPresenter), new PropertyMetadata(string.Empty));
        public static readonly DependencyProperty SyncProperty = DependencyProperty.Register(nameof(Sync), typeof(Sync), typeof(ItemPathPresenter), new PropertyMetadata(null));
        public static readonly DependencyProperty ItemAbsolutePathProperty = DependencyProperty.Register(nameof(AbsoluteItemPath), typeof(string), typeof(ItemPathPresenter), new PropertyMetadata(string.Empty));
        public static readonly DependencyProperty DisplayedPathProperty = DependencyProperty.Register(nameof(DisplayedPath), typeof(string), typeof(ItemPathPresenter), new PropertyMetadata(string.Empty));
        public static readonly DependencyProperty ToolTipPathProperty = DependencyProperty.Register(nameof(ToolTipPath), typeof(string), typeof(ItemPathPresenter), new PropertyMetadata(string.Empty));

        public void UpdateDisplayedPath()
        {
            if (!string.IsNullOrWhiteSpace(AbsoluteItemPath))
            {
                ToolTipPath = AbsoluteItemPath.Replace('\\', '/');
                DisplayedPath = AbsoluteItemPath.Replace('\\', '/');
                return;
            }

            if (string.IsNullOrWhiteSpace(RelativeItemPath))
            {
                ToolTipPath = string.Empty;
                DisplayedPath = string.Empty;
                return;
            }

            if (Sync is not null && !string.IsNullOrWhiteSpace(Sync.LocalPath))
            {
                ToolTipPath = System.IO.Path.Combine(Sync.LocalPath ?? "", RelativeItemPath).Replace('\\', '/');
                DisplayedPath = System.IO.Path.Combine(Path.GetFileName(Sync.LocalPath) ?? "", RelativeItemPath).Replace('\\', '/');
            }
            else
            {
                ToolTipPath = RelativeItemPath.Replace('\\', '/');
                DisplayedPath = RelativeItemPath.Replace('\\', '/');
            }
        }

        private void CopyPathButton_Click(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrWhiteSpace(ToolTipPath))
                return;

            DataPackage dataPackage = new()
            {
                RequestedOperation = DataPackageOperation.Copy
            };
            dataPackage.SetText(ToolTipPath);
            Clipboard.SetContent(dataPackage);

            Utility.ShowTeachingTip(Localizer.Instance.GetString("pathCopiedToClipboard"));
        }
    }
}
