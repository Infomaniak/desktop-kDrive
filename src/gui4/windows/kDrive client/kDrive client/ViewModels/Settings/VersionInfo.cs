using Infomaniak.kDrive.Types;
using System;

namespace Infomaniak.kDrive.ViewModels
{
    public class VersionInfo : UISafeObservableObject
    {
        public VersionChannel Channel { get; set; } = VersionChannel.Unknown;
        public string Tag { get; set; } = string.Empty; // Version number. Example: 3.6.4
        public Int64 buildVersion = 0; // Example: 20240816
    }
}
