using Microsoft.UI.Xaml;

namespace Infomaniak.kDrive.TrayIcon
{
    public sealed partial class TrayIconResources : ResourceDictionary
    {
        public string ButtonOpenKDrive =>
         Localizer.Localizer.GetString("buttonOpenKDrive");

        public string ButtonCloseKDrive =>
           Localizer.Localizer.GetString("buttonCloseKDrive");

        public TrayIconResources()
        {
            this.InitializeComponent();
        }
    }
}