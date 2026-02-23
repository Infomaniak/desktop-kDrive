using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public sealed partial class DefaultError : UserControl
    {
        private readonly Error? _error;

        public bool HasError => _error is not null;
        public bool ShowDetails => _error is not null;

        public DefaultError(Error error)
        {
            this.InitializeComponent();
            _error = error;
            UpdateCard();
            Logger.Log(Logger.Level.Error, $"DefaultError displayed: {_error?.ToString() ?? "null"}");
        }

        private void UpdateCard()
        {
            if (_error is not null)
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

                AddIfNotEmpty("ID:", _error.DbId);
                AddIfNotEmpty("Timestamp:", _error.Timestamp.ToString("O"));
                AddIfNotEmpty("Level:", _error.ErrorLevel);
                AddIfNotEmpty("Exit Code:", _error.ExitCode);
                AddIfNotEmpty("Exit Cause:", _error.ExitCause);
                AddIfNotEmpty("Node Type:", _error.NodeType);
                AddIfNotEmpty("Path:", _error.Path);
                AddIfNotEmpty("Destination:", _error.DestinationPath);
                AddIfNotEmpty("Conflict:", _error.ConflictType);
                AddIfNotEmpty("Inconsistency:", _error.InconsistencyType);
                AddIfNotEmpty("Cancel Type:", _error.CancelType);
                AddIfNotEmpty("Auto-resolved:", _error.AutoResolved);

                DetailsTextBlock.Text = lines.Count > 0 
                    ? string.Join(Environment.NewLine, lines)
                    : "No additional error details available.";
            }
            else
            {
                DetailsTextBlock.Text = "An unexpected error has occurred.";
            }
        }
    }
}
