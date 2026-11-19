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
using System.Globalization;

namespace Infomaniak.kDrive
{
    internal class LocalizerXamlHelper
    {
        public static string PrettySyncRunningStatus(Localizer localizer, bool showEllipsis, Sync sync, SyncStep syncStep, Int64 totalFiles, Int64 currentFile, ulong nbItems)
        {
            string suffix = showEllipsis ? localizer.Ellipsis : string.Empty;
            if (sync is null)
                return Localizer.Instance.GetString("syncStatusAnalysing") + suffix;

            switch (syncStep)
            {
                case SyncStep.Idle:
                case SyncStep.UpdateDetection1:
                    return Localizer.Instance.GetString("syncStatusExploring", nbItems.ToString("N0", CultureInfo.CurrentCulture)) + suffix;
                case SyncStep.UpdateDetection2:
                case SyncStep.Reconciliation1:
                case SyncStep.Reconciliation2:
                case SyncStep.Reconciliation3:
                case SyncStep.Reconciliation4:
                case SyncStep.Propagation1:
                    return Localizer.Instance.GetString("syncStatusAnalysing") + suffix;
                case SyncStep.Propagation2:
                    return Localizer.Instance.GetStringWithPlural("syncStatusPropagating", currentFile, currentFile.ToString("N0", CultureInfo.CurrentCulture), totalFiles.ToString("N0", CultureInfo.CurrentCulture)) + suffix;
                default:
                    return "…";

            }
        }

    }
}
