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
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class LogUploadManager : UISafeObservableObject
    {
        private int _percentComplete = 0;
        private LogUploadState _status = LogUploadState.None;
        private DateTime _lastSuccessfullUpload = DateTime.MinValue;

        private bool _uploadInProgress = false;

        public int PercentComplete
        {
            get => _percentComplete;
            set => SetPropertyInUIThread(ref _percentComplete, System.Math.Max(20, value));
        }

        public LogUploadState State
        {
            get => _status;
            set
            {
                SetPropertyInUIThread(ref _status, value);
                UploadInProgress = value switch
                {
                    LogUploadState.Archiving => true,
                    LogUploadState.Uploading => true,
                    LogUploadState.CancelRequested => true,
                    _ => false,
                };
                if (value == LogUploadState.Success)
                    LastSuccessfullUpload = DateTime.Now;
            }
        }

        public bool UploadInProgress
        {
            get => _uploadInProgress;
            private set => SetPropertyInUIThread(ref _uploadInProgress, value);
        }

        public DateTime LastSuccessfullUpload
        {
            get => _lastSuccessfullUpload;
            set => SetPropertyInUIThread(ref _lastSuccessfullUpload, value);
        }

        public async Task StartUpload(bool includeArchivedLogs)
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            if (!await commService.StartLogUpload(includeArchivedLogs, CancellationToken.None))
            {
                Logger.Log(Logger.Level.Error, "Log upload could not be started");
                State = LogUploadState.Failed;
            }
        }

        public static async Task CancelUpload()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            if (!await commService.CancelLogUpload(CancellationToken.None))
            {
                Logger.Log(Logger.Level.Error, "Log upload could not be cancelled");
                // We do not change the state here as it will be changed by the server signal later
            }
        }
    }
}
