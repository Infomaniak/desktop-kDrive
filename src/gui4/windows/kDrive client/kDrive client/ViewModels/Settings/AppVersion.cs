using Infomaniak.kDrive.Types;
using System;
using System.Globalization;

namespace Infomaniak.kDrive.ViewModels
{
    public class AppVersion
    {
        public VersionChannel Channel { get; set; } = VersionChannel.Prod;
        public string Tag { get; set; } = string.Empty; // e.g., "1.2.3"
        public int BuildVersion { get; set; } = 0; // e.g., "20250401"
        public Uri ChangeLogUrl
        {
            get
            {
                string languageCode = System.Globalization.CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;
                return App.Constants.Storage.ReleaseNoteUrl(Tag, languageCode);
            }
        }

        public DateTime BuildDate
        {
            get
            {
                /*if (DateTime.TryParseExact(BuildVersion, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None, out DateTime parsedDate))
                {
                    return parsedDate;
                }
                else
                {*/
                Logger.Log(Logger.Level.Warning, "BuildVersion string is not in the expected format 'yyyyMMdd'.");
                return DateTime.MinValue;
                //}
            }
        }

        public string PrettyBuildDate
        {
            get
            {
                return BuildDate.ToString("D", CultureInfo.CurrentCulture);
            }
        }
    }
}
