/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;

namespace Infomaniak.kDrive
{
    internal class LocalizerXamlHelper
    {
        public static string PrettySyncRunningStatus(Localizer localizer, Sync sync, SyncStep syncStep, Int64 totalFiles, Int64 currentFile)
        {
            if (sync is null)
                return Localizer.Instance.GetString("syncStatusAnalysing");

            switch (syncStep)
            {
                case SyncStep.Idle:
                case SyncStep.UpdateDetection1:
                    return Localizer.Instance.GetString("syncStatusExploring");
                case SyncStep.UpdateDetection2:
                case SyncStep.Reconciliation1:
                case SyncStep.Reconciliation2:
                case SyncStep.Reconciliation3:
                case SyncStep.Reconciliation4:
                case SyncStep.Propagation1:
                    return Localizer.Instance.GetString("syncStatusAnalysing");
                case SyncStep.Propagation2:
                    return Localizer.Instance.GetStringWithPlural("syncStatusPropagating", currentFile, currentFile, totalFiles);
                default:
                    return "...";

            }
        }

    }
}
