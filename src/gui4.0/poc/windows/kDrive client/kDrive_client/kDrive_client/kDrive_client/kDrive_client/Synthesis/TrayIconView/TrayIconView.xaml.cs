using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using H.NotifyIcon.Core;
using H.NotifyIcon;
using System.Windows.Input;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace kDrive_client.Synthesis.TrayIconView
{
    public sealed partial class TrayIconView : UserControl
    {
        private Window m_window;
        public ICommand ShowHideWindowCommand { get; }
        public ICommand ExitApplicationCommand { get; }

        public TrayIconView()
        {
            this.InitializeComponent();
            ShowHideWindowCommand = new RelayCommand(ShowHideWindow);
            ExitApplicationCommand = new RelayCommand(ExitApplication);
            m_window = App.MainWindow;
        }

        public void ShowHideWindow()
        {
            if (m_window == null)
            {
                m_window = App.MainWindow;
            }

            if (m_window.Visible)
            {
                m_window.Hide();
            }
            else
            {
                m_window.Show();
                m_window.Activate();
            }
        }
        public void ExitApplication()
        {
            if (m_window == null)
            {
                m_window = App.MainWindow;
            }
            m_window?.Close();
        }

        private sealed class RelayCommand : ICommand
        {
            private Action _action;
            public RelayCommand(Action action)
            {
                this._action = action;
            }

            public event EventHandler? CanExecuteChanged;

            public bool CanExecute(object? parameter)
            {
                return true;
            }

            public void Execute(object? parameter)
            {
                this._action.Invoke();
            }
        }
    }
}
