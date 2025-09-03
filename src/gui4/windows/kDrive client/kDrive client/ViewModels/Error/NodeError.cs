using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ViewModels.Error
{
    internal class NodeError : ErrorBase
    {
        public NodeError(int dbId) : base(dbId)
        {
        }

        public override string Description()
        {
            // TODO: Implement a more detailed description based on ExitCode and ExitCause
            return $"NodeError: DbId={DbId}, Time={Time}, ExitCode={ExitCode}, ExitCause={ExitCause}";
        }
    }
}
