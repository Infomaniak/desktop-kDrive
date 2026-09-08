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
using System;
using System.Linq;
using System.Reflection;

namespace Infomaniak.kDrive.ViewModels
{
    public class AppVersion
    {
        public DistributionChannel Channel { get; set; } = DistributionChannel.Prod;
        public string Tag { get; set; } = string.Empty; // e.g., "1.2.3"
        public int BuildVersion { get; set; } = 0;

        public string FullVersion
        {
            get => $"{Tag}.{BuildVersion}";
        }

        public Uri ChangeLogUrlLocalized
        {
            get
            {
                return App.Constants.Storage.ReleaseNoteUrl(Tag, Localizer.Instance.CurrentLanguage);
            }
        }
        public Uri ChangeLogUrlDefaultLanguage
        {
            get
            {
                return App.Constants.Storage.ReleaseNoteUrl(Tag, "en");
            }
        }

        public static AppVersion Current()
        {
            var assembly = Assembly.GetExecutingAssembly();
            var version = assembly.GetName().Version;

            if (version != null)
            {
                return new AppVersion
                {
                    Tag = $"{version.Major}.{version.Minor}.{version.Build}",
                    BuildVersion = version.Revision,
                    Channel = DistributionChannel.Prod
                };
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unable to retrieve assembly version.");
                return new AppVersion
                {
                    Tag = "0.0.0",
                    BuildVersion = 0,
                    Channel = DistributionChannel.Prod
                };
            }
        }

        // Compare versions based on BuildVersion, then Tag, then Channel
        // isHigherThan returns true if this version is higher than the other version
        public bool IsHigherThan(AppVersion other)
        {
            if (other == null)
                return true;

            try
            {
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
            }
            catch (System.FormatException)
            {
                Logger.Log(Logger.Level.Error, "Unable to parse the version tag");
                return false;
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
