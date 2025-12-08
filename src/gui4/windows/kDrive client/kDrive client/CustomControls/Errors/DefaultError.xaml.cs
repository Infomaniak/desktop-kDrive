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
        public DefaultError()
        {
            this.InitializeComponent();
        }

        public Error Error
        {
            get
            {
                return (Error)GetValue(ErrorProperty);
            }
            set
            {
                SetValue(ErrorProperty, value);
                UpdateCard();
            }
        }

        public static readonly DependencyProperty ErrorProperty =
         DependencyProperty.Register(nameof(Error), typeof(Error), typeof(ErrorCard), new PropertyMetadata(null));


        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            await Windows.System.Launcher.LaunchUriAsync(App.Constants.GitHubRepoUrl);
        }

        private void UpdateCard()
        {
            if (Error != null)
            {
                ErrorCard.Title = $"{Error.ExitCode.ToString()} - {Error.ExitCause.ToString()}";
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
                AddIfNotEmpty("DbId:", Error.DbId);
                AddIfNotEmpty("Timestamp:", Error.Timestamp);
                AddIfNotEmpty("Error level:", Error.ErrorLevel);
                AddIfNotEmpty("Exit code:", Error.ExitCode);
                AddIfNotEmpty("Exit cause:", Error.ExitCause);
                AddIfNotEmpty("Node type:", Error.NodeType);
                AddIfNotEmpty("Path:", Error.Path);
                AddIfNotEmpty("Destination path:", Error.DestinationPath);
                AddIfNotEmpty("Conflict type:", Error.ConflictType);
                AddIfNotEmpty("Inconsistency type:", Error.InconsistencyType);
                AddIfNotEmpty("Auto-resolved:", Error.AutoResolved);

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
