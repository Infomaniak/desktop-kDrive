using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.Pages.Popup
{
    public sealed partial class AddExclusionRulePopup : Page
    {
        public CheckBox WarningCheckBox => CheckBox;
        public TextBox ExclusionRuleTextBox => TextBox;
        public AddExclusionRulePopup()
        {
            Logger.Log(Logger.Level.Info, "Navigated to AddExclusionRulePopup - Initializing AddExclusionRulePopup components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "AddExclusionRulePopup components initialized");
        }
    }
}
