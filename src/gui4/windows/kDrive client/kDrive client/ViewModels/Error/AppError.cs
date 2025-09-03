using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ViewModels.Error
{
    internal class AppError : ErrorBase
    {
        public AppError(int dbId) : base(dbId)
        {
        }

        public override string Description()
        {
            // TODO: Implement a more detailed description based on ExitCode and ExitCause
            return $"AppError: DbId={DbId}, Time={Time}, ExitCode={ExitCode}, ExitCause={ExitCause}";
        }
    }
}
