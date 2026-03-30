using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public struct DefaultErrorProperty
    {
        public string Name { get; set; }
        public string Value { get; set; }
    }

    public sealed partial class DefaultError : UserControl
    {
        private Error Error { get; set; }

        public ObservableCollection<DefaultErrorProperty> Properties = new ObservableCollection<DefaultErrorProperty>();

        public DefaultError(Error error)
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
            if (!await kDrive.Localizer.Instance.TryLaunchUriAsync("helpURL"))
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
