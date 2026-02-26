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

public partial class SystemErrorSyncDirAccessErrorDialog : Page
{
    private Error Error { get; init; }
    public string FilePath => Error.Path ?? string.Empty;
    public Sync? Sync => Error.Sync;

    public SystemErrorSyncDirAccessErrorDialog(Error error)
    {
        Error = error;
        InitializeComponent();
        InitTextBlocks();
    }

    public void InitTextBlocks()
    {
        // Access Modified
        string sentence = Localizer.Instance.GetString("localFileAccessErrorDialogFaqLink");
        string faqText = Localizer.Instance.GetString("labelFAQ");

        // Split the sentence around the placeholder
        var parts = sentence.Split(new[] { "{0}" }, StringSplitOptions.None);

        // Add first part
        AccessRightsModifiedTextBlock.Inlines.Add(new Run { Text = parts[0] });

        // Add hyperlink
        var hyperlink = new Hyperlink();
        hyperlink.UnderlineStyle = UnderlineStyle.None;
        hyperlink.Inlines.Add(new Run { Text = faqText });
        hyperlink.Click += FaqHyperlink_Click;
        AccessRightsModifiedTextBlock.Inlines.Add(hyperlink);

        // Add remaining part
        if (parts.Length > 1)
            AccessRightsModifiedTextBlock.Inlines.Add(new Run { Text = parts[1] });

        // Folder Deleted
        sentence = Localizer.Instance.GetString("labelCreateNewSync");
        string newSyncText = Localizer.Instance.GetString("labelNewSync");

        // Split the sentence around the placeholder
        parts = sentence.Split(new[] { "{0}" }, StringSplitOptions.None);

        // Add first part
        FolderDeletedTextBlock.Inlines.Add(new Run { Text = parts[0] });

        // Add hyperlink
        var hyperlink2 = new Hyperlink();
        hyperlink2.UnderlineStyle = UnderlineStyle.None;
        hyperlink2.Inlines.Add(new Run { Text = newSyncText });
        hyperlink2.Click += RecreateSync_Click;
        FolderDeletedTextBlock.Inlines.Add(hyperlink2);

        // Add remaining part
        if (parts.Length > 1)
            FolderDeletedTextBlock.Inlines.Add(new Run { Text = parts[1] });
    }

    private async void RecreateSync_Click(Hyperlink sender, HyperlinkClickEventArgs args)
    {
        Sync? sync = Error.Sync;
        if (sync is null)
        {
            Logger.Log(Logger.Level.Error, "Sync information is missing for the sync error.");
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }

        var commServices = App.ServiceProvider.GetRequiredService<IServerCommService>();
        string userProfile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        string desiredFolderName = sync.Drive.Name.StartsWith("kDrive") ? sync.Drive.Name : $"kDrive {sync.Drive.Name}";
        string desiredPath = Path.Combine(userProfile, desiredFolderName);
        string? result = await commServices.GetGoodPathForNewSync(sync.Drive, desiredPath, CancellationToken.None);
        if (result is null)
        {
            Logger.Log(Logger.Level.Error, $"Failed to get a valid sync path for drive '{sync.Drive.Name}'");
            Utility.ShowTeachingTipFromxUid("InvalidDefaultSyncLocationTeachingTip");
            return;
        }

        NewSync newSync = new() { Drive = sync.Drive, DefaultPath = result, LocalPath = result, RemoteNodeId = sync.RemoteNodeId, RemotePath = sync.RemotePath };
        await newSync.SelectBestVfsMode();
        var excludedNodeIds = await sync.GetExcludedNodeIds();
        if (excludedNodeIds is not null)
        {
            newSync.ExcludedNodeIds.AddRange(excludedNodeIds);
        }
        else
        {
            Logger.Log(Logger.Level.Error, $"Failed to get excluded node IDs for drive '{sync.Drive.Name}', the sync setup dialog will proceed without excluding any nodes.");
        }

        List<NewSync> newSyncs = new() { newSync };

        var currentDialog = this.Parent as ContentDialog;
        if (currentDialog is not null)
        {
            currentDialog.Hide();
        }
        else
        {
            Logger.Log(Logger.Level.Warning, "The sync setup dialog could not be closed before opening the new sync setup dialog.");
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }

        CustomControls.DriveSetupContentDialog dialog = new(this.XamlRoot, newSyncs);
        await dialog.ShowAsync();

        if (dialog.Result == CustomControls.DriveSetupContentDialog.DriveSetupResult.Cancelled)
        {
            Logger.Log(Logger.Level.Info, $"User canceled main sync setup for drive '{sync.Drive.Name}'");
            return;
        }

        var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();

        Logger.Log(Logger.Level.Debug, $"Setting up new sync: LocalPath={newSync.LocalPath}, RemotePath={newSync.RemotePath}, Drive={newSync.Drive.Name}");
        if (!await commService.AddSync(newSync, CancellationToken.None))
        {
            Logger.Log(Logger.Level.Error, $"Failed to add new sync for drive '{sync.Drive.Name}'");
            Utility.ShowUnexpectedErrorTeachingTip();
            return;
        }


    }

    private async void FaqHyperlink_Click(Hyperlink sender, HyperlinkClickEventArgs args)
    {
        await Windows.System.Launcher.LaunchUriAsync(App.Constants.Drive.FAQUri);
    }
}
