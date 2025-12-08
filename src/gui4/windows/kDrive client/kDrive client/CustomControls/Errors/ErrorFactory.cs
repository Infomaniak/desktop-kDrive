using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors
{
    internal class ErrorFactory
    {
        public ErrorFactory() { }
        public static UserControl CreateErrorControl(Error error)
        {
            if(error.InconsistencyType == Types.InconsistencyType.ForbiddenChar)
            {
                return new ForbiddenCharError(error);
            }
            return new DefaultError(error);
        }
    }
}
