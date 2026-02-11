using System;
using Windows.ApplicationModel.Resources;

namespace Infomaniak.kDrive.Localizer
{
    internal class Localizer : UISafeObservableObject
    {
        private static readonly ResourceLoader resourceLoader = ResourceLoader.GetForViewIndependentUse();
        public static Localizer Instance { get; } = new Localizer();

        public void TriggerRefresh()
        {
            OnPropertyChanged(nameof(GetString1s));
            OnPropertyChanged(nameof(GetStringWithPlural1i));
            OnPropertyChanged(nameof(GetStringWithPlural));
            OnPropertyChanged(nameof(IsValidKey));
        }

        public string GetString1s(string key, string arg1)
        {
            return GetString(key, new object?[] { arg1 });
        }

        public string GetStringWithPlural1i(string key, int count, int arg1)
        {
            return GetStringWithPlural(key, count, new object?[] { arg1 });
        }

        // This method is used to get a localized string that has a singular and plural form based on the count.
        // The singularKey is the key for the singular form of the string, and the plural form is expected to be defined in the resource file with the key singularKey + "-plural".
        public string GetStringWithPlural(string singularKey, int count, params object?[]? args)
        {
            // Compute wich form to use based on the count and the pluralization rules of the current culture
            bool usePluralForm = System.Globalization.CultureInfo.CurrentUICulture.Name switch
            {
                "en-EN" => count != 1,
                _ => count > 1
            };

            string keyToUse = usePluralForm ? singularKey + "-plural" : singularKey;
            return GetString(keyToUse, args);
        }

        public string GetString(string key)
        {
            return GetString(key, null);
        }

        public string GetString(string key, params object?[]? args)
        {
            key = key.Replace(".", "/");

            string? localizedString = resourceLoader.GetString(key);

            if (localizedString is null || localizedString.Length == 0)
            {
                Logger.Log(Logger.Level.Error, $"Missing localization for key: {key} in current culture {System.Globalization.CultureInfo.CurrentUICulture.Name}");
                return $"!{key}!"; // Return the key wrapped in exclamation marks to indicate a missing localization
            }

            // Replace literal \r\n with real newlines
            localizedString = localizedString.Replace("\\r\\n", Environment.NewLine);

            // Replace each %@ with {0}, {1}, etc. for string formatting
            if (args is not null && args.Length > 0)
            {
                const string macOSPlaceholder = "%@";
                int argIndex = 0;
                while (localizedString.Contains(macOSPlaceholder))
                {
                    int pos = localizedString.IndexOf(macOSPlaceholder);
                    if (pos == -1)
                        break;

                    localizedString = localizedString.Substring(0, pos) + "{" + argIndex + "}" + localizedString.Substring(pos + macOSPlaceholder.Length);
                    ++argIndex;
                }

                try
                {
                    localizedString = string.Format(localizedString, args);
                }
                catch (Exception e)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to format localized string: {localizedString} with args: {string.Join(", ", args)}. Error: {e.Message}");
                }
            }

            return localizedString;
        }

        public bool IsValidKey(string key)
        {
            string localizedString = GetString(key);
            return !localizedString.StartsWith("!") && !localizedString.EndsWith("!");
        }
    }
}
