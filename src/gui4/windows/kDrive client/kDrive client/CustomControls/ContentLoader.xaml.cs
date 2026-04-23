using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

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
            get { return GetValue(LoaderContentProperty); }
            set { SetValue(LoaderContentProperty, value); }
        }
        public double LoaderMinHeight
        {
            get { return (double)GetValue(MinLoaderHeightProperty); }
            set { SetValue(MinLoaderHeightProperty, value); }
        }

        public double LoaderMinWidth
        {
            get { return (double)GetValue(MinLoaderWidthProperty); }
            set { SetValue(MinLoaderWidthProperty, value); }
        }

        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(nameof(IsLoading), typeof(bool), typeof(ContentLoader), new PropertyMetadata(false));

        public static readonly DependencyProperty LoaderContentProperty =
            DependencyProperty.Register(nameof(LoaderContent), typeof(object), typeof(ContentLoader), new PropertyMetadata(null));

        public static readonly DependencyProperty MinLoaderHeightProperty =
            DependencyProperty.Register(nameof(LoaderMinHeight), typeof(double), typeof(ContentLoader), new PropertyMetadata(0.0));

        public static readonly DependencyProperty MinLoaderWidthProperty =
            DependencyProperty.Register(nameof(LoaderMinWidth), typeof(double), typeof(ContentLoader), new PropertyMetadata(0.0));


    }
}
