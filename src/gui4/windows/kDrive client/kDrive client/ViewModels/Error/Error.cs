/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

using CommunityToolkit.Mvvm.Collections;
using CommunityToolkit.Mvvm.ComponentModel;
using KDrive.ServerCommunication;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace KDrive.ViewModels.Error
{
    internal abstract class ErrorBase : ObservableObject
    {
        private int _dbId = -1;
        private DateTime _time;
        private int _exitCode = -1;
        private int _exitCause = -1;

        public ErrorBase(int dbId)
        {
            DbId = dbId;
        }

        public int DbId
        {
            get => _dbId;
            set => SetProperty(ref _dbId, value);
        }

        public DateTime Time
        {
            get => _time;
            set => SetProperty(ref _time, value);
        }

        public int ExitCode
        {
            get => _exitCode;
            set => SetProperty(ref _exitCode, value);
        }

        public int ExitCause
        {
            get => _exitCause;
            set => SetProperty(ref _exitCause, value);
        }

        public string DescriptionShort
        {
            get
            {
                string desc = Description();
                if (desc.Length > 100)
                {
                    return desc.Substring(0, 100) + "...";
                }
                return desc;
            }
        }

        abstract public string Description();

    }
}