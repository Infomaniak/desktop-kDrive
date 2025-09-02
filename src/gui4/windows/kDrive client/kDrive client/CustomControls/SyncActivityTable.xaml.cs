using KDrive.ViewModels;
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
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace KDrive.CustomControls
{
    public sealed partial class SyncActivityTable : UserControl
    {
        internal AppModel _viewModel = ((App)Application.Current).Data;
        internal AppModel ViewModel
        {
            get { return _viewModel; }
        }
        public SyncActivityTable()
        {
            InitializeComponent();
        }

        private void ListView_ContainerContentChanging(ListViewBase sender, ContainerContentChangingEventArgs args)
        {
            if (args.ItemContainer is ListViewItem item)
            {
                int index = sender.IndexFromContainer(item);

                if (item.ContentTemplateRoot is Grid itemGrid)
                {
                    itemGrid.Background = (index % 2 == 0)
                    ? new SolidColorBrush(Colors.White)
                    : new SolidColorBrush(Colors.WhiteSmoke);
                }
            }
        }
    }
}
