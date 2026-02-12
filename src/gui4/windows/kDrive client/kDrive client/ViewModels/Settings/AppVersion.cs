using Infomaniak.kDrive.Types;
using System;
using System.Linq;
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

        // Compare versions based on BuildVersion, then Tag, then Channel
        // isHigherThan returns true if this version is higher than the other version
        public bool IsHigherThan(AppVersion other)
        {
            if (other == null)
                return true;

            var thisTagParts = Tag.Split('.').Select(int.Parse).ToArray();
            var otherTagParts = other.Tag.Split('.').Select(int.Parse).ToArray();

            for (int i = 0; i < Math.Min(thisTagParts.Length, otherTagParts.Length); i++)
            {
                if (thisTagParts[i] > otherTagParts[i])
                    return true;
                else if (thisTagParts[i] < otherTagParts[i])
                    return false;
            }

            if (thisTagParts.Length != otherTagParts.Length)
            {
                Logger.Log(Logger.Level.Error, $"Tag format mismatch: '{Tag}' vs '{other.Tag}' with same prefix. Considering the longer tag as higher.");
                return thisTagParts.Length > otherTagParts.Length;
            }

            return this.BuildVersion > other.BuildVersion;
        }

        public bool IsSameVersion(AppVersion other)
        {
            if (other == null)
                return false;
            return this.BuildVersion == other.BuildVersion && this.Tag == other.Tag && this.Channel == other.Channel;
        }

        public bool IsLowerThan(AppVersion other)
        {
            return !IsHigherThan(other) && !IsSameVersion(other);
        }
    }
}
