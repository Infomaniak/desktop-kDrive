using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Windows.ApplicationModel.DataTransfer;

namespace Infomaniak.kDrive.CustomControls.Errors;

/// <summary>
/// Dialog displaying details about a local file access error and offering to open the parent folder.
/// </summary>
public partial class LocalAccessErrorDialog : Page
{
    private readonly Error _error;
    public string FilePath => _error.Path ?? string.Empty;
    public Sync? Sync => _error.Sync;

    public LocalAccessErrorDialog(Error error)
    {
        _error = error;
        InitializeComponent();
    }

    private void CopyPathButton_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrEmpty(_error.Path))
            return;

        var dataPackage = new DataPackage();
        dataPackage.SetText(_error.Path);
        Clipboard.SetContent(dataPackage);
    }
}
