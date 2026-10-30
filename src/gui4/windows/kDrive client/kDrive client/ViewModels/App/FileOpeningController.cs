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
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    /// View model backing the pop-up window of a single file opening request.
    public class FileOpeningRequest : UISafeObservableObject
    {
        private FileOpeningPhase _phase;
        private int _percent;
        private string _itemName;

        public FileOpeningRequest(ulong jobId, FileOpeningPhase phase, int percent, string itemName)
        {
            JobId = jobId;
            _phase = phase;
            _percent = percent;
            _itemName = itemName;
        }

        public ulong JobId { get; }

        public FileOpeningPhase Phase
        {
            get => _phase;
            private set
            {
                if (SetPropertyInUIThread(ref _phase, value))
                {
                    OnPropertyChangedInUIThread(nameof(IsDownloading));
                    OnPropertyChangedInUIThread(nameof(Message));
                }
            }
        }

        /// Download progress in percent. Only meaningful while <see cref="IsDownloading"/> is true.
        public int Percent
        {
            get => _percent;
            private set => SetPropertyInUIThread(ref _percent, value);
        }

        /// Name of the item being opened. Empty while the item has not been resolved by the engine (WaitingSync phase).
        public string ItemName
        {
            get => _itemName;
            private set
            {
                if (SetPropertyInUIThread(ref _itemName, value))
                {
                    OnPropertyChangedInUIThread(nameof(HasItemName));
                }
            }
        }

        public bool IsDownloading => Phase == FileOpeningPhase.Hydrating;

        public bool HasItemName => !string.IsNullOrEmpty(ItemName);

        public string Message => Localizer.Instance.GetString(IsDownloading ? "fileOpeningHydratingMessage" : "fileOpeningWaitingSyncMessage");

        public void Update(FileOpeningPhase phase, int percent, string itemName)
        {
            Phase = phase;
            Percent = percent;
            ItemName = itemName;
        }
    }

    public class FileOpeningStatusEventArgs : EventArgs
    {
        public FileOpeningRequest Request { get; }

        public FileOpeningStatusEventArgs(FileOpeningRequest request)
        {
            Request = request;
        }
    }

    /// Tracks the file opening requests reported by the server through FILE_OPENING_STATUS signals and drives the
    /// pop-up window of the view: the pop-up of a request is opened or refreshed on every status update, and closed
    /// when the request finishes (including when the user canceled it).
    public class FileOpeningController
    {
        private readonly Dictionary<ulong, FileOpeningRequest> _requests = new();

        /// Raised when a request was created or updated: the view must open its pop-up window for it, or refresh the
        /// bindings of the already open one.
        public event EventHandler<FileOpeningStatusEventArgs>? ShowRequested;

        /// Raised when a request reached the Finished phase: the view must close its pop-up window, if it is open.
        public event EventHandler<FileOpeningStatusEventArgs>? DismissRequested;

        /// Called on the UI thread for every FILE_OPENING_STATUS signal received from the server.
        public void OnFileOpeningStatus(ulong jobId, FileOpeningPhase phase, int percent, string itemName)
        {
            if (phase == FileOpeningPhase.Finished)
            {
                // The request has been processed (successfully or not, or canceled): the pop-up window must be closed.
                if (_requests.Remove(jobId, out var dismissedRequest))
                {
                    DismissRequested?.Invoke(this, new FileOpeningStatusEventArgs(dismissedRequest));
                }

                return;
            }

            if (_requests.TryGetValue(jobId, out var request))
            {
                request.Update(phase, percent, itemName);
            }
            else
            {
                request = new FileOpeningRequest(jobId, phase, percent, itemName);
                _requests[jobId] = request;
            }

            ShowRequested?.Invoke(this, new FileOpeningStatusEventArgs(request));
        }

        /// Asks the server to cancel the file opening request. The server confirms the cancellation by reporting the
        /// Finished phase of the request, which closes the pop-up window.
        public async Task<bool> CancelRequestAsync(ulong jobId)
        {
            var serverCommService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            if (!await serverCommService.CancelFileOpening(jobId, CancellationToken.None))
            {
                Logger.LogWarning($"Failed to cancel the file opening request. JobId: {jobId}", "Failed to cancel the file opening request");
                return false;
            }

            Logger.LogInfo($"The server acknowledged the cancellation of the file opening request. JobId: {jobId}");
            return true;
        }
    }
}
