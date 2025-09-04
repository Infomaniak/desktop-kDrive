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
            SolveButton = new ButtonData("Contacter le support", async (sender) =>
            {
                await Windows.System.Launcher.LaunchUriAsync(new Uri("https://www.infomaniak.com/fr/support"));
            });

            InfoHyperLink = new ButtonData("En savoir plus", async (sender) =>
            {
                await Windows.System.Launcher.LaunchUriAsync(new Uri("https://www.infomaniak.com/fr/support/faq/admin2/kdrive"));
            });
        }
        public override sealed string TitleStr()
        {
            return "Syncpal Error";
        }

        public override string HowToSolveStr()
        {
            // TODO: Implement a more detailed description based on ExitCode and ExitCause
            return $"Essayer de redemarrer l'application, si l'erreur persiste, veuillez contacter le support";
        }

        public override string CauseStr()
        {
            return $"Une erreur technique s’est produite (ERR:{ExitCode}:{ExitCause})";
        }

        public override Uri IconUri()
        {
            return new Uri("ms-appx:///Assets/Icons/headphones.svg");
        }
    }
}
