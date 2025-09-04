using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ViewModels.Errors
{
    internal class AppError : BaseError
    {
        public AppError(int dbId) : base(dbId)
        {
        }

        public override sealed string TitleStr()
        {
            return "Assistance technique";
        }

        public override sealed  string HowToSolveStr()
        {
            return "Veuillez contacter notre support client.";
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
