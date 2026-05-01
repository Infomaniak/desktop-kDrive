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
using Infomaniak.kDrive.Types;
using Microsoft.Windows.Globalization;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Threading.Tasks;
using Windows.ApplicationModel.Resources.Core;

namespace Infomaniak.kDrive
{
    internal class Localizer : UISafeObservableObject
    {
        public static Localizer Instance { get; } = new Localizer();
        private static readonly ResourceContext context = ResourceContext.GetForViewIndependentUse();
        private static Dictionary<Language, string> languageToCultureMap = new Dictionary<Language, string>
            {
                { Language.English, "en" },
                { Language.French, "fr" },
                { Language.Italian, "it" },
                { Language.German, "de" },
                { Language.Spanish, "es" },
                { Language.Danish, "da" },
                //{ Language.Dutch, "nl" },
                { Language.Finnish, "fi" },
                { Language.Greek, "el" },
                { Language.Norwegian, "nb" },
                { Language.Polish, "pl" },
                { Language.Portuguese, "pt" },
                { Language.Swedish, "sv" }
            };

        private string GetBestAvailableCultureName(Language language)
        {
            if(languageToCultureMap.Count != Language.GetValues(typeof(Language)).Length - 1) // -1 because of Language.Default
                Logger.Log(Logger.Level.Warning, "The language to culture map does not contain all languages defined in the Language enum. This may cause issues with localization.");

            if (language == Language.Default)
                return GetBestAvailableSystemDefaultCultureName();
            else if (!languageToCultureMap.ContainsKey(language))
            {
                Logger.Log(Logger.Level.Error, $"Unsupported Language {language}, falling back to english.");
                return "en";
            }
            else
                return languageToCultureMap[language];
        }

        private string GetBestAvailableSystemDefaultCultureName()
        {
            string systemCultureName = CultureInfo.CurrentCulture.TwoLetterISOLanguageName;
            if (languageToCultureMap.Values.ToArray().Contains(systemCultureName))
                return systemCultureName;
            else
            {
                Logger.Log(Logger.Level.Warning, $"System language {systemCultureName} is not supported, falling back to english.");
                return "en"; // Fallback to english if system language is not supported
            }
        }

        public void SetLanguage(Types.Language language)
        {
            string cultureName = GetBestAvailableCultureName(language);

            try
            {
                CultureInfo.CurrentCulture.ClearCachedData();
                ApplicationLanguages.PrimaryLanguageOverride = cultureName;
                context.QualifierValues["language"] = cultureName;
                Logger.Log(Logger.Level.Info, $"Culture set to {cultureName}");
                TriggerRefresh();
            }
            catch (Exception e)
            {
                Logger.Log(Logger.Level.Error,
                    $"Failed to set culture to {cultureName}. Error: {e.Message}");
            }
        }


        public void TriggerRefresh()
        {
            OnPropertyChanged(nameof(GetString));
            OnPropertyChanged(nameof(GetString1s));
            OnPropertyChanged(nameof(GetStringWithPlural1i));
            OnPropertyChanged(nameof(GetStringWithPlural));
            OnPropertyChanged(nameof(IsValidKey));
        }

        // The below methods are helper methods to call GetString with a specific number of arguments of specific types,
        // to make it easier to use in XAML bindings.
        // They simply forward the call to GetString with the appropriate arguments.
        public string GetString1s(string key, string arg1)
        {
            return GetString(key, new object?[] { arg1 });
        }

        // This method is similar to GetString1s but the argument is expected to be a key for another localized string, so it calls GetString on the argument before passing it to GetString.
        public string GetStringCombine1s(string key, string arg1Key)
        {
            string arg1 = GetString(arg1Key);
            return GetString(key, new object?[] { arg1 });
        }

        public string GetString1i(string key, int arg1)
        {
            return GetString(key, new object?[] { arg1 });
        }

        public string GetString1i2i(string key, int arg1, int arg2)
        {
            return GetString(key, new object?[] { arg1, arg2 });
        }

        public string GetStringWithPlural1i(string key, int count, int arg1)
        {
            return GetStringWithPlural(key, count, new object?[] { arg1 });
        }

        // This method is used to get a localized string that has a singular and plural form based on the count.
        // The singularKey is the key for the singular form of the string, and the plural form is expected to be defined in the resource file with the key singularKey + "-plural".
        public string GetStringWithPlural(string singularKey, int count, params object?[]? args)
        {
            // Compute which form to use based on the count and the pluralization rules of the current culture
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

        public async Task<bool> TryLaunchUriAsync(string uriKey)
        {
            string uriString = GetString(uriKey);

            if (!Uri.TryCreate(uriString, UriKind.Absolute, out Uri? uri))
            {
                Logger.Log(Logger.Level.Error, $"Invalid URI string for key {uriKey}: {uriString}");
                return false;
            }

            bool success = await Windows.System.Launcher.LaunchUriAsync(uri);
            if (!success)
                Logger.Log(Logger.Level.Error, $"Failed to launch URI: {uri}");

            return success;
        }

        public string GetString(string key, params object?[]? args)
        {
            key = key.Replace(".", "/");
            string localizedString = "";

            var ressources = ResourceManager.Current.MainResourceMap.GetSubtree("Resources");
            NamedResource namedResource;
            if (ressources.TryGetValue(key, out namedResource))
            {
                var resourceCandidate = namedResource.Resolve(context);
                if (resourceCandidate is not null)
                    localizedString = resourceCandidate.ValueAsString;
            }

            if (localizedString.Length == 0)
            {
                Logger.Log(Logger.Level.Error, $"Missing resource for key: {key} in current culture {System.Globalization.CultureInfo.CurrentUICulture.Name}");
                return $"!{key}!"; // Return the key wrapped in exclamation marks to indicate a missing localization
            }

            if (localizedString is null || localizedString.Length == 0)
            {
                Logger.Log(Logger.Level.Error, $"Missing localization for key: {key} in current culture {System.Globalization.CultureInfo.CurrentUICulture.Name}");
                return $"!{key}!"; // Return the key wrapped in exclamation marks to indicate a missing localization
            }

            // Replace literal \r\n with real newlines
            localizedString = localizedString.Replace("\\r\\n", Environment.NewLine);

            // Replace each %@ with {0}, {1}, etc. for string formatting
            const string macOSPlaceholder = "%@";
            int argIndex = 0;
            while (localizedString.Contains(macOSPlaceholder))
            {
                int pos = localizedString.IndexOf(macOSPlaceholder);
                if (pos == -1)
                    break;

                localizedString = localizedString.Substring(0, pos) + "{" + Math.Min(argIndex, (args?.Length - 1) ?? 0) + "}" + localizedString.Substring(pos + macOSPlaceholder.Length);
                ++argIndex;
            }

            if (args is not null && args.Length > 0)
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

        public bool IsValidKey(string? key)
        {
            if (key is null)
                return false;
            string localizedString = GetString(key);
            return !localizedString.StartsWith("!") && !localizedString.EndsWith("!");
        }
    }
}
