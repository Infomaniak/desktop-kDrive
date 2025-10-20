namespace Infomaniak.kDrive.ViewModels
{
    public class UpdateData
    {
        public string Tag { get; set; } = string.Empty; // e.g., "1.2.3"
        public string BuildVersion { get; set; } = string.Empty; // e.g., "20250401"
        public string Url { get; set; } = string.Empty;
        public string Changelog { get; set; } = string.Empty;
    }
}
