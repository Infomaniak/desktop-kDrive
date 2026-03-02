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
        var frame = ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView.Frame;
        if (frame is null)
        {
            Logger.Log(Logger.Level.Error, "Failed to navigate to the sync setup page after a sync directory change error because the main frame could not be found.");
            return;
        }

        var destPage = (Error.Sync?.IsAdvanced ?? false) ? typeof(Pages.Settings.DriveAdvancedSyncsPage) : typeof(Pages.Settings.DriveManagementPage);
        frame.Navigate(destPage, Error.Sync?.Drive);
    }

    private async void FaqHyperlink_Click(Hyperlink sender, HyperlinkClickEventArgs args)
    {
        await Windows.System.Launcher.LaunchUriAsync(App.Constants.Drive.FAQUri);
    }
}
