/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading.Tasks;
using Windows.System;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public struct DefaultErrorProperty
    {
        public string Name { get; set; }
        public string Value { get; set; }
    }

    public sealed partial class DefaultError : UserControl
    {
        private Error Error { get; init; }

        public ObservableCollection<DefaultErrorProperty> Properties = new();

#pragma warning disable CS8618 // Error is initialized in the constructor, but the compiler can't statically verify that. We ensure it's always set in the constructor, so we can safely ignore this warning.
        public DefaultError(Error error)
#pragma warning restore CS8618 
        {
            this.InitializeComponent();
            Error = error;
            UpdateCard();
            Logger.Log(Logger.Level.Error, $"DefaultError displayed: {Error?.ToString() ?? "null"}");
        }
        private void UpdateCard()
        {
            Properties.Clear();
            if (Error is not null)
            {

                void AddIfNotEmpty(string label, object? value)
                {
                    if (value is null)
                        return;

                    string text;
                    if (value is DateTime dt)
                        text = dt.ToString("O");
                    else
                        text = value.ToString() ?? "";

                    if (text.EndsWith($".{label}")) // Remove Properties that doesn't provide a useful ToString (ie: "Infomaniak.kDrive.ViewModels.Sync")
                        return;

                    if (string.IsNullOrWhiteSpace(text) || text == "None" || text == "Unknown")
                        return;

                    Properties.Add(new DefaultErrorProperty { Name = label, Value = text });
                }

                foreach (var prop in Error.GetType().GetProperties())
                {
                    try
                    {
                        if (prop.Name == nameof(Error.NodeTypeTranslationKey)) continue;
                        var value = prop.GetValue(Error);
                        AddIfNotEmpty($"{prop.Name}", value);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(Logger.Level.Warning, $"Failed to get property {prop.Name} from Error: {ex.Message}");
                    }
                }
            }
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            if (!await Launcher.LaunchUriAsync(App.Constants.kSuite.HelpUri))
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
