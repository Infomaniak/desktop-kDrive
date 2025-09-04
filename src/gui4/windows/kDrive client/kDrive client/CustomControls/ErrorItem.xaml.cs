using DynamicData;
using DynamicData.Binding;
using KDrive.ViewModels;
using KDrive.ViewModels.Errors;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reactive.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using WinRT;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace KDrive.CustomControls
{
    public sealed partial class ErrorItem : UserControl
    {
        internal AppModel _viewModel = ((App)Application.Current).Data;
        internal AppModel ViewModel => _viewModel;

        public static readonly DependencyProperty ErrorProperty =
            DependencyProperty.Register(
                nameof(Error),
                typeof(ViewModels.Errors.BaseError),
                typeof(ErrorItem),
                new PropertyMetadata(null));

        public ErrorItem()
        {
            InitializeComponent();
        }

        public ViewModels.Errors.BaseError? Error
        {
            get => (ViewModels.Errors.BaseError?)GetValue(ErrorProperty);
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
                    await task;
                }
                else
                {
                    Logger.Log(Logger.Level.Error, "HyperlinkButton_Click: No action defined for InfoHyperLink.");
                }
                button.IsEnabled = true;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "HyperlinkButton_Click: sender is not a Button");
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
                    await task;
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "SolveButton_Click: No action defined for SolveButton.");
                }
                button.IsEnabled = true;
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "SolveButton_Click: sender is not a Button");
            }
        }
    }
}

