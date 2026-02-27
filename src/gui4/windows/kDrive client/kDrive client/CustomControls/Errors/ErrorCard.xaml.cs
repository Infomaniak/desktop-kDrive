using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Linq;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public partial class ErrorCard : UserControl
    {
        public ErrorCard()
        {
            InitializeComponent();
        }

        public string Title
        {
            get { return (string)GetValue(TitleProperty); }
            set { SetValue(TitleProperty, value); }
        }

        public Error? Error
        {
            get { return (Error?)GetValue(ErrorProperty); }
            set
            {
                SetValue(ErrorProperty, value);
            }
        }

        public string Description
        {
            get { return (string)GetValue(DescriptionProperty); }
            set { SetValue(DescriptionProperty, value); }
        }

        public bool HasDescription => Description.Count() > 1;

        public string? ActionText
        {
            get { return (string?)GetValue(ActionTextProperty); }
            set { SetValue(ActionTextProperty, value); }
        }

        public FrameworkElement? CustomContent
        {
            get { return (FrameworkElement?)GetValue(CustomContentProperty); }
            set { SetValue(CustomContentProperty, value); }
        }

        private bool HasItemPath => Error?.Path.Length > 0;

        public delegate void ActionClickEventHandler(object sender, RoutedEventArgs e);
        public event ActionClickEventHandler? ActionClick;

        public static readonly DependencyProperty TitleProperty =
            DependencyProperty.Register(nameof(Title), typeof(string), typeof(ErrorCard), new PropertyMetadata(false));

        public static readonly DependencyProperty ErrorProperty =
            DependencyProperty.Register(nameof(Error), typeof(Error), typeof(ErrorCard), new PropertyMetadata(null));

        public static readonly DependencyProperty DescriptionProperty =
            DependencyProperty.Register(nameof(Description), typeof(string), typeof(ErrorCard), new PropertyMetadata(string.Empty));

        public static readonly DependencyProperty ActionTextProperty =
            DependencyProperty.Register(nameof(ActionText), typeof(string), typeof(ErrorCard), new PropertyMetadata(null));

        public static readonly DependencyProperty CustomContentProperty =
            DependencyProperty.Register(nameof(CustomContent), typeof(FrameworkElement), typeof(ErrorCard), new PropertyMetadata(null));

        private void ActionButton_Click(object sender, RoutedEventArgs e)
        {
            ActionClick?.Invoke(this, e);
        }
    }
}
