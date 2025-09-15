using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ViewModels.Errors
{
    internal class SyncPalError : BaseError
    {
        public SyncPalError(DbId dbId) : base(dbId)
        {
            SolveButton = new ButtonData(GetLocalizedString("Global_ContactSupport")
                , async (sender) =>
            {
                await Windows.System.Launcher.LaunchUriAsync(new Uri("https://www.infomaniak.com/fr/support"));
            });

            InfoHyperLink = new ButtonData(GetLocalizedString("Global_MoreInfo"), async (sender) =>
            {
                await Windows.System.Launcher.LaunchUriAsync(new Uri("https://www.infomaniak.com/fr/support/faq/admin2/kdrive"));
            });
        }
        public override sealed string TitleStr()
        {
            return GetLocalizedString("SyncPalError_Title");
        }

        public override string HowToSolveStr()
        {
            return GetLocalizedString("SyncPalError_HowToSolve");
        }

        public override string CauseStr()
        {
            return string.Format(GetLocalizedString("SyncPalError_Cause"), ExitCode, ExitCause);
        }

        public override Uri IconUri()
        {
            return new Uri("ms-appx:///Assets/Icons/headphones.svg");
        }
    }
}
