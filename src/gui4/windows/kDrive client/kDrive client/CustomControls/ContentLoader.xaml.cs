using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class ContentLoader : UserControl
    {
        public ContentLoader()
        {
            InitializeComponent();
        }

        public bool IsLoading
        {
            get { return (bool)GetValue(IsLoadingProperty); }
            set { SetValue(IsLoadingProperty, value); }
        }
        public object LoaderContent
        {
            get { return (object)GetValue(LoaderContentProperty); }
            set { SetValue(LoaderContentProperty, value); }
        }
        public double MinLoaderHeight
        {
            get { return (double)GetValue(MinLoaderHeightProperty); }
            set { SetValue(MinLoaderHeightProperty, value); }
        }

        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register("IsLoading", typeof(bool), typeof(ContentLoader), new PropertyMetadata(false));

        public static readonly DependencyProperty LoaderContentProperty =
            DependencyProperty.Register("LoaderContent", typeof(object), typeof(ContentLoader), new PropertyMetadata(null));

        public static readonly DependencyProperty MinLoaderHeightProperty =
            DependencyProperty.Register("MinLoaderHeight", typeof(double), typeof(ContentLoader), new PropertyMetadata(0.0));
    }
}
