using DynamicData;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Documents;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;

namespace Infomaniak.kDrive.CustomControls.Errors;

public partial class SystemErrorSyncDirDiskMissingErrorDialog : Page
{
    private Error Error { get; init; }
    public string FilePath => Error.Path ?? string.Empty;
    public Sync? Sync => Error.Sync;

    public SystemErrorSyncDirDiskMissingErrorDialog(Error error)
    {
        Error = error;
        InitializeComponent();
    }
}
