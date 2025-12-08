using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public sealed partial class DefaultError : UserControl
    {
        private Error _error;
        public DefaultError(Error error)
        {
            this.InitializeComponent();
            _error = error;
            UpdateCard();
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            await Windows.System.Launcher.LaunchUriAsync(App.Constants.GitHubRepoUrl);
        }

        private void UpdateCard()
        {
            if (_error != null)
            {
                ErrorCard.Title = $"{_error.ExitCode.ToString()} - {_error.ExitCause.ToString()}";
                var lines = new List<string>
                {
                    "An unexpected error has occurred.",
                    ""
                };

                void AddIfNotEmpty(string label, object value)
                {
                    if (value == null)
                        return;

                    string text = value.ToString();
                    if (string.IsNullOrWhiteSpace(text))
                        return;

                    lines.Add($"{label,-20} {text}");
                }
                AddIfNotEmpty("DbId:", _error.DbId);
                AddIfNotEmpty("Timestamp:", _error.Timestamp);
                AddIfNotEmpty("Error level:", _error.ErrorLevel);
                AddIfNotEmpty("Exit code:", _error.ExitCode);
                AddIfNotEmpty("Exit cause:", _error.ExitCause);
                AddIfNotEmpty("Node type:", _error.NodeType);
                AddIfNotEmpty("Path:", _error.Path);
                AddIfNotEmpty("Destination path:", _error.DestinationPath);
                AddIfNotEmpty("Conflict type:", _error.ConflictType);
                AddIfNotEmpty("Inconsistency type:", _error.InconsistencyType);
                AddIfNotEmpty("Cancel Type:", _error.CancelType);
                AddIfNotEmpty("Auto-resolved:", _error.AutoResolved);

                string description = string.Join(Environment.NewLine, lines);
                ErrorCard.Description = description;
            }
            else
            {
                ErrorCard.Description = "An unexpected error has occurred.";
            }
        }
    }
}
