using Microsoft.UI.Xaml;
using System;

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
            const string resourceKey = "Infomaniak.DS.Icons.Devices.headphones";
            if (Application.Current.Resources[resourceKey] is string iconUriStr)
            {
                return new Uri(iconUriStr);
            }
            Logger.Log(Logger.Level.Error, $"Resource for AppError icon should be {resourceKey} but was not found or is not a string.");
            return new Uri("");
        }
    }
}
