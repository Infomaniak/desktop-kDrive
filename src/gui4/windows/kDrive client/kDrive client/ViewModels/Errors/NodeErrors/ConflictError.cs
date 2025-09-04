using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ViewModels.Errors
{
    internal class ConflictError : NodeError
    {
        public ConflictError(int dbId) : base(dbId)
        {
            SolveButton = new ButtonData(GetLocalizedString("ConflictError_SolveButton"), async (object d) =>
            {
                //TODO: Implement conflict resolution dialog
                await Task.Delay(5000);
            });
        }
        public override sealed string HowToSolveStr()
        {
            return GetLocalizedString("ConflictError_HowToSolve");
        }
        public override string CauseStr()
        {
            return GetLocalizedString("ConflictError_Cause");
        }

    }
}
