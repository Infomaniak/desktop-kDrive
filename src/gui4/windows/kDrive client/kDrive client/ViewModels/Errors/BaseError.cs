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

namespace KDrive.ViewModels.Errors
{
    public abstract class BaseError : ObservableObject
    {
        public class ButtonData
        {
            public string Text { get; set; }
            public Func<object /*sender*/, Task> Action { get; set; }
            public ButtonData(string text, Func<object /*sender*/, Task> action)
            {
                Text = text;
                Action = action;
            }
        }


        private DbId _dbId = -1;
        private DateTime _time;
        private ExitCode _exitCode = -1;
        private ExitCause _exitCause = -1;
        private ButtonData? _solveButton = null;
        private ButtonData? _infoHyperLink = null;

        protected BaseError(DbId dbId)
        {
            DbId = dbId;
        }

        public DbId DbId
        {
            get => _dbId;
            set => SetProperty(ref _dbId, value);
        }

        public DateTime Time
        {
            get => _time;
            set => SetProperty(ref _time, value);
        }

        public ExitCode ExitCode
        {
            get => _exitCode;
            set => SetProperty(ref _exitCode, value);
        }

        public ExitCause ExitCause
        {
            get => _exitCause;
            set => SetProperty(ref _exitCause, value);
        }

        public string Title
        {
            get => TitleStr();
        }

        public string HowToSolve
        {
            get => HowToSolveStr();
        }

        public string Cause
        {
            get => CauseStr();
        }

        public Uri Icon
        {
            get => IconUri();
        }

        public ButtonData? SolveButton
        {
            get => _solveButton;
            set => SetProperty(ref _solveButton, value);
        }

        public bool HasSolveButton
        {
            get => _solveButton != null;
        }

        public ButtonData? InfoHyperLink
        {
            get => _infoHyperLink;
            set => SetProperty(ref _infoHyperLink, value);
        }

        public bool HasInfoHyperLink
        {
            get => _infoHyperLink != null;
        }

        abstract public string TitleStr();
        abstract public string HowToSolveStr();
        abstract public string CauseStr();
        abstract public Uri IconUri();

        protected static string GetLocalizedString(string key)
        {
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse("SyncErrors");
            return resourceLoader.GetString(key);
        }
    }
}