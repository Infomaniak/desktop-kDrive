using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels.Errors
{
    public class AppError : BaseError
    {
        public AppError(DbId dbId) : base(dbId)
        {
        }

        public override sealed string TitleStr()
        {
            return GetLocalizedString("AppError_Title");
        }

        public override sealed string HowToSolveStr()
        {
            return GetLocalizedString("AppError_HowToSolve");
        }

        public override string CauseStr()
        {
            return string.Format(GetLocalizedString("AppError_Cause"), ExitCode, ExitCause);
        }

        public override Uri IconUri()
        {
            return AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "headphones");
        }
    }
}
