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
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.System;
using WinRT;
using static kDrive_client.ExplorerItem;
using static System.Runtime.InteropServices.JavaScript.JSType;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace kDrive_client
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class DrivePage : Page
    {
        private ObservableCollection<ExplorerItem> DataSource;

        public DrivePage()
        {
            this.InitializeComponent();
            this.DataContext = this;
            DataSource = GetData();
        }

        private ObservableCollection<ExplorerItem> GetData()
        {
            var list = new ObservableCollection<ExplorerItem>();
            ExplorerItem folder1 = new ExplorerItem()
            {
                Name = "C:/Users",
                Type = ExplorerItem.ExplorerItemType.Folder,
                IsLoading = false,
            };


            list.Add(folder1);
            return list;
        }

        private async void FolderTree_Expanding(TreeView sender, TreeViewExpandingEventArgs args)
        {
            var item = args.Item.As<ExplorerItem>();
            var dispacher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
            await Task.Run(async () =>
            {
                updateExplorerItem(item, dispacher);
            });
        }

        private void updateExplorerItem(ExplorerItem item, Microsoft.UI.Dispatching.DispatcherQueue dispatcherQueue)
        {
            if (item.Children.Count != 1 || !item.Children[0].IsLoading) return;
            string[] files;
            string[] directories;
            try
            {
                //files = Directory.GetFiles(item.path);
                directories = Directory.GetDirectories(item.path);
            }
            catch (Exception ex)
            {
                dispatcherQueue.TryEnqueue(() =>
                {
                    item.Children.Add(new ExplorerItem { Name = ex.Message, IsLoading = false, Parent = item });
                    item.Children.RemoveAt(item.Children.Count - 2);
                });

                return;
            }
            foreach (string child in directories)
            {
                ExplorerItem childItem = new ExplorerItem()
                {
                    Name = child.Replace("\\", "/").Split("/").Last(),
                    Parent = item,
                    Type = ExplorerItem.ExplorerItemType.Folder,
                    IsLoading = false,
                };
                dispatcherQueue.TryEnqueue(async () =>
                {
                    item.Children.Insert(item.Children.Count() - 1, childItem);
                    await Task.Run(() =>
                    {
                        loadSize(item.Children.ElementAt(item.Children.Count() - 2), dispatcherQueue);
                    });
                });
                Thread.Sleep(200);
            }

           /* foreach (string child in files)
            {
                ExplorerItem childItem = new ExplorerItem()
                {
                    Name = child.Replace("\\", "/").Split("/").Last(),
                    Parent = item,
                    Type = ExplorerItem.ExplorerItemType.File,
                    IsLoading = false,
                };
                dispatcherQueue.TryEnqueue(() =>
                {
                    item.Children.Insert(item.Children.Count()-1, childItem);
                });
                Thread.Sleep(200);
            }*/
            dispatcherQueue.TryEnqueue(() =>
            {
                item.Children.RemoveAt(item.Children.Count - 1);
            });
        }

        private void loadSize(ExplorerItem item, Microsoft.UI.Dispatching.DispatcherQueue dispatcherQueue)
        {
            Thread.Sleep(500);
            long size = DirSize(new DirectoryInfo(item.path));
            dispatcherQueue.TryEnqueue(() =>
            {
                item.Size = size;
            });
        }
        public static long DirSize(DirectoryInfo d)
        {
            long size = 1;
            // Add file sizes.
            try
            {
                FileInfo[] fis = d.GetFiles();
                foreach (FileInfo fi in fis)
                {
                    size += fi.Length;
                }
                // Add subdirectory sizes.
                DirectoryInfo[] dis = d.GetDirectories();
                foreach (DirectoryInfo di in dis)
                {
                    size += DirSize(di);
                }
            }
            catch (Exception ex)
            {
                // Do nothing
            }
            return size;
        }

    }

    public class ExplorerItem : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        public enum ExplorerItemType { Folder, File };
        public string Name { get; set; }
        public ExplorerItem Parent { get; set; }

        public ExplorerItemType Type { get; set; }
        private bool m_loading;
        public bool IsLoading
        {
            get
            {
                return m_loading;
            }
            set
            {
                if (m_loading != value)
                {
                    m_loading = value;
                    NotifyPropertyChanged("IsLoading");
                }

            }
        }
        private long m_size = 0;
        public long Size
        {
            get
            {
                return m_size;
            }
            set
            {
                if (m_size != value)
                {
                    m_size = value;
                    NotifyPropertyChanged("Size");
                    NotifyPropertyChanged("SizeVisibility");
                    NotifyPropertyChanged("SizeLoadingVisibility");

                }

            }
        }

        public Visibility SizeVisibility
        {
            get
            {
                return Size == 0 ? Visibility.Collapsed : Visibility.Visible;
            }
         
        }
        public Visibility SizeLoadingVisibility
        {
            get
            {
                return Size != 0 ? Visibility.Collapsed : Visibility.Visible;
            }

        }
        public string path
        {
            get
            {
                if (Parent == null)
                {
                    return Name;
                }
                else
                {
                    return Parent.path + "/" + Name;
                }
            }
        }


        private ObservableCollection<ExplorerItem> m_children;
        public ObservableCollection<ExplorerItem> Children
        {
            get
            {
                if (m_children == null)
                {
                    m_children = new ObservableCollection<ExplorerItem>();
                    m_children.Add(new ExplorerItem { IsLoading = true, Parent = this });
                }
                return m_children;
            }
            set
            {
                m_children = value;
            }
        }

        private bool m_isExpanded;
        public bool IsExpanded
        {
            get { return m_isExpanded; }
            set
            {
                if (m_isExpanded != value)
                {
                    m_isExpanded = value;
                    NotifyPropertyChanged("IsExpanded");
                }
            }
        }

        private void NotifyPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    class ExplorerItemTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? FolderTemplate { get; set; }
        public DataTemplate? FileTemplate { get; set; }
        public DataTemplate? LoadingPlaceHolder { get; set; }

        protected override DataTemplate SelectTemplateCore(object item)
        {
            var explorerItem = (ExplorerItem)item;
            if (explorerItem.IsLoading)
            {
                return LoadingPlaceHolder;
            }
            return explorerItem.Type == ExplorerItem.ExplorerItemType.Folder ? FolderTemplate : FileTemplate;
        }
    }
}
