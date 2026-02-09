using System;
using Windows.ApplicationModel.Resources;

namespace Infomaniak.kDrive.Localizer
{
    internal static class Localizer
    {
        private static readonly ResourceLoader resourceLoader = ResourceLoader.GetForViewIndependentUse();

        public static string GetString(string key)
        {
            return GetString(key, null);
        }

        public static string GetString(string key, params object?[]? args)
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

            // Format the string if arguments are provided
            if (args != null && args.Length > 0)
            {
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
    }
}
