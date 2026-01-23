using Infomaniak.kDrive.Types;
using System;
using System.Reflection;

namespace Infomaniak.kDrive.ViewModels
{
    public class AppVersion
    {
        public VersionChannel Channel { get; set; } = VersionChannel.Prod;
        public string Tag { get; set; } = string.Empty; // e.g., "1.2.3"
        public int BuildVersion { get; set; } = 0;
        public Uri ChangeLogUrl
        {
            get
            {
                string languageCode = System.Globalization.CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;
                return App.Constants.Storage.ReleaseNoteUrl(Tag, languageCode);
            }
        }
    
        static public AppVersion Current()
        {
            var assembly = Assembly.GetExecutingAssembly();
            var version = assembly.GetName().Version;

            if (version != null)
            {
                return new AppVersion
                {
                    Tag = $"{version.Major}.{version.Minor}.{version.Build}",
                    BuildVersion = version.Revision,
                    Channel = VersionChannel.Prod
                };
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unable to retrieve assembly version.");
                return new AppVersion
                {
                    Tag = "0.0.0",
                    BuildVersion = 0,
                    Channel = VersionChannel.Prod
                };
            }
        }
    }
}
