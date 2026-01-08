using Infomaniak.kDrive.ViewModels;
using Infomaniak.kDrive.ViewModels.Errors;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class ErrorItem : UserControl
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;

        public static readonly DependencyProperty ErrorProperty =
            DependencyProperty.Register(
                nameof(Error),
                typeof(BaseError),
                typeof(ErrorItem),
                new PropertyMetadata(null));

        public ErrorItem()
        {
            InitializeComponent();
        }

        public BaseError? Error
        {
            get => (BaseError?)GetValue(ErrorProperty);
            set => SetValue(ErrorProperty, value);
        }

        private async void HyperlinkButton_Click(object sender, RoutedEventArgs e)
        {
            if (sender is HyperlinkButton button)
            {
                button.IsEnabled = false;
                var task = Error?.InfoHyperLink?.Action(sender);
                if (task != null)
                {
                    Logger.Log(Logger.Level.Info, "Executing InfoHyperLink action.");
                    await task;
                }
                else
                {
                    Logger.Log(Logger.Level.Error, "No action defined for InfoHyperLink.");
                }
                button.IsEnabled = true;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Sender is not a Button");
            }
        }

        private async void SolveButton_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button button)
            {
                button.IsEnabled = false;
                var task = Error?.SolveButton?.Action(sender);
                if (task != null)
                {
                    Logger.Log(Logger.Level.Info, "Executing SolveButton action.");
                    await task;
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "No action defined for SolveButton.");
                }
                button.IsEnabled = true;
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "Sender is not a Button");
            }
        }
    }
}

