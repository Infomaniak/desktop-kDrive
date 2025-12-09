using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public sealed partial class DefaultError : UserControl
    {
        private Error _error;

        // Texte affiché dans l'Expander (dropdown)
        public string DetailsText { get; private set; } = string.Empty;

        public DefaultError(Error error)
        {
            this.InitializeComponent();
            _error = error;
            UpdateCard();
            Logger.Log(Logger.Level.Error, $"DefaultError displayed: {_error?.ToString() ?? "null"}");
        }

        private void UpdateCard()
        {
            if (_error != null)
            {
                ErrorCard.Title = $"{_error.ExitCode} - {_error.ExitCause}";

                // Lignes détaillées affichées dans le dropdown
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
                    if (string.IsNullOrWhiteSpace(text) || text == "None" || text == "Unknown")
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

                // Description courte sur la carte, détails complets dans le dropdown
                ErrorCard.Description = "An unexpected error has occurred.";
                DetailsText = string.Join(Environment.NewLine, lines);
            }
            else
            {
                ErrorCard.Description = "An unexpected error has occurred.";
                DetailsText = "An unexpected error has occurred.";
                this.Bindings?.Update();
            }
        }
    }
}
