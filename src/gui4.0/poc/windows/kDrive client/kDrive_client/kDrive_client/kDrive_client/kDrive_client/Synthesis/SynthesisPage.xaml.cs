using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace kDrive_client
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class SynthesisPage : Page
    {
        public SynthesisPage()
        {
            this.InitializeComponent();
        }

        private void MenuFlyoutItem_Click(object sender, RoutedEventArgs e)
        {
            s_window = new SettingsWindow();
            s_window.Activate();
            //s_window.AppWindow.MoveAndResize(new Windows.Graphics.RectInt32(600, 500, 500, 600));
            var mainWindow = (Application.Current as App)?.Window;
            if (mainWindow != null && mainWindow.AppWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.Minimize();
                s_window.Closed += (s, args) =>
                {
                    presenter.Restore();
                };
            }
        }
        private Window? s_window;

    }
}
