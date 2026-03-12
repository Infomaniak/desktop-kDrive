using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public sealed partial class DefaultError : UserControl
    {
        private Error Error { get; init; }

        public bool ShowDetails => Error is not null;

        public DefaultError(Error error)
        {
            this.InitializeComponent();
            Error = error;
            UpdateCard();
            Logger.Log(Logger.Level.Error, $"DefaultError displayed: {Error?.ToString() ?? "null"}");
        }

        private void UpdateCard()
        {
            if (Error is not null)
            {
                var lines = new List<string>();

                void AddIfNotEmpty(string label, object? value)
                {
                    if (value is null)
                        return;

                    string text = value.ToString() ?? "";
                    if (string.IsNullOrWhiteSpace(text) || text == "None" || text == "Unknown")
                        return;

                    lines.Add($"{label,-25} {text}");
                }

                AddIfNotEmpty("ID:", Error.DbId);
                AddIfNotEmpty("Timestamp:", Error.Timestamp.ToString("O"));
                AddIfNotEmpty("Level:", Error.ErrorLevel);
                AddIfNotEmpty("Exit Code:", Error.ExitCode);
                AddIfNotEmpty("Exit Cause:", Error.ExitCause);
                AddIfNotEmpty("Node Type:", Error.NodeType);
                AddIfNotEmpty("Path:", Error.Path);
                AddIfNotEmpty("Destination:", Error.DestinationPath);
                AddIfNotEmpty("Conflict:", Error.ConflictType);
                AddIfNotEmpty("Inconsistency:", Error.InconsistencyType);
                AddIfNotEmpty("Cancel Type:", Error.CancelType);
                AddIfNotEmpty("Auto-resolved:", Error.AutoResolved);

                DetailsTextBlock.Text = lines.Count > 0
                    ? string.Join(Environment.NewLine, lines)
                    : "No additional error details available.";
            }
            else
            {
                DetailsTextBlock.Text = "An unexpected error has occurred.";
            }
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            if(!await kDrive.Localizer.Instance.TryLaunchUriAsync("helpURL"))
            {
                Logger.Log(Logger.Level.Error, "Failed to launch HelpDesk URI.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            await Task.Delay(1000);
            if (control is not null)
                control.IsEnabled = true;
        }
    }
}
