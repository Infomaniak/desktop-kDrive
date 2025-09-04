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
            SolveButton = new ButtonData("Choisir la version à conserver", async (object d) =>
            {
                //TODO: Implement conflict resolution dialog
                await Task.Delay(5000);
            });
        }
        public override sealed string HowToSolveStr()
        {
            return "Veuillez renommer ou supprimer le fichier en conflit.";
        }
        public override string CauseStr()
        {
            return "Le fichier a été modifié en même temps par un autre utilisateur. Vos modifications ont été enregistrées dans une copie";
        }
        
    }
}
