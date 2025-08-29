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

namespace kDrive_client
{
    public sealed partial class ActivityPage : Page
    {
        internal DataModel.AppModel _viewModel = ((App)Application.Current).Data;
        internal DataModel.AppModel ViewModel { get { return _viewModel; } }
        public ActivityPage()
        {
            InitializeComponent();
        }
    }
}
