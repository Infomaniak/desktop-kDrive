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

using Infomaniak.kDrive.Converters;
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
        private static readonly BytesToHumanReadableStringConverter BytesConverter = new();

        private FileOpeningPhase _phase;
        private int _percent;
        private string _itemName;
        private string _itemPath;
        private long _receivedBytes;
        private long _totalBytes;

        public FileOpeningRequest(ulong jobId, FileOpeningPhase phase, int percent, string itemName, string itemPath,
                                  long receivedBytes, long totalBytes)
        {
            JobId = jobId;
            _phase = phase;
            _percent = percent;
            _itemName = itemName;
            _itemPath = itemPath;
            _receivedBytes = receivedBytes;
            _totalBytes = totalBytes;
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
                    OnPropertyChangedInUIThread(nameof(IsWaitingSync));
                    OnPropertyChangedInUIThread(nameof(IsIndeterminate));
                    OnPropertyChangedInUIThread(nameof(Message));
                }
            }
        }

        /// Download progress in percent. Only meaningful while <see cref="IsDownloading"/> is true, and only used
        /// while the total size of the item is unknown.
        public int Percent
        {
            get => _percent;
            private set
            {
                if (SetPropertyInUIThread(ref _percent, value))
                {
                    OnPropertyChangedInUIThread(nameof(IsIndeterminate));
                    OnPropertyChangedInUIThread(nameof(GaugeValue));
                    OnPropertyChangedInUIThread(nameof(GaugeMaximum));
                }
            }
        }

        /// True while the gauge has no measurable progress to display: no byte downloaded yet and no percent reported.
        public bool IsIndeterminate => IsDownloading && TotalBytes <= 0 && Percent <= 0;

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

        /// Path of the parent folder of the item, relative to the home directory while it is being downloaded, and
        /// relative to the drive root while it is not synchronized yet. Empty when unknown.
        public string ItemPath
        {
            get => _itemPath;
            private set
            {
                if (SetPropertyInUIThread(ref _itemPath, value))
                {
                    OnPropertyChangedInUIThread(nameof(HasItemPath));
                }
            }
        }

        /// Number of bytes downloaded so far, -1 while unknown.
        public long ReceivedBytes
        {
            get => _receivedBytes;
            private set
            {
                if (SetPropertyInUIThread(ref _receivedBytes, value))
                {
                    OnPropertyChangedInUIThread(nameof(GaugeValue));
                    OnPropertyChangedInUIThread(nameof(HasDownloadProgress));
                    OnPropertyChangedInUIThread(nameof(ProgressText));
                    OnPropertyChangedInUIThread(nameof(IsIndeterminate));
                }
            }
        }

        /// Total size of the item in bytes, -1 while unknown.
        public long TotalBytes
        {
            get => _totalBytes;
            private set
            {
                if (SetPropertyInUIThread(ref _totalBytes, value))
                {
                    OnPropertyChangedInUIThread(nameof(GaugeValue));
                    OnPropertyChangedInUIThread(nameof(GaugeMaximum));
                    OnPropertyChangedInUIThread(nameof(HasDownloadProgress));
                    OnPropertyChangedInUIThread(nameof(ProgressText));
                    OnPropertyChangedInUIThread(nameof(IsIndeterminate));
                }
            }
        }

        public bool IsDownloading => Phase == FileOpeningPhase.Hydrating;

        public bool IsWaitingSync => !IsDownloading;

        public bool HasItemName => !string.IsNullOrEmpty(ItemName);

        public bool HasItemPath => !string.IsNullOrEmpty(ItemPath);

        /// True once both the downloaded amount and the total size of the item are known: the gauge tracks the bytes
        /// and the downloaded amount is displayed under it.
        public bool HasDownloadProgress => TotalBytes > 0 && ReceivedBytes >= 0;

        /// Upper bound of the gauge: the total size of the item when known, 100 with the percent otherwise.
        public double GaugeMaximum => TotalBytes > 0 ? TotalBytes : 100;

        /// Current value of the gauge: the downloaded amount when the total size is known, the percent otherwise.
        public double GaugeValue => TotalBytes > 0 ? Math.Max(ReceivedBytes, 0) : Math.Max(Percent, 0);

        /// Downloaded amount as a localized "12.5 MB over 48 MB" string, only meaningful when
        /// <see cref="HasDownloadProgress"/> is true.
        public string ProgressText => Localizer.Instance.GetString1s2s("fileOpeningDownloadProgress",
            (string)BytesConverter.Convert(ReceivedBytes, typeof(string), string.Empty, ""),
            (string)BytesConverter.Convert(TotalBytes, typeof(string), string.Empty, ""));

        public string Message => Localizer.Instance.GetString(IsDownloading ? "fileOpeningHydratingMessage" : "fileOpeningWaitingSyncMessage");

        /// Title of the pop-up window while the item is being downloaded.
        public string TitleText => Localizer.Instance.GetString("fileOpeningDownloadTitle");

        public void Update(FileOpeningPhase phase, int percent, string itemName, string itemPath, long receivedBytes,
                           long totalBytes)
        {
            Phase = phase;
            Percent = percent;
            ItemName = itemName;
            ItemPath = itemPath;
            ReceivedBytes = receivedBytes;
            TotalBytes = totalBytes;
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

    /// Job identifiers that reached the Finished phase: the server delivers the signals through a concurrent
    /// pool, so a progress update can be reordered after the Finished phase of the same request, and it must not
    /// resurrect a dismissed request.
    private readonly HashSet<ulong> _finishedJobIds = new();

        /// Requests currently tracked, i.e. reported by the server and not finished yet. Used to replay the
        /// pending requests to a presentation surface subscribing after the requests have started.
        public IEnumerable<FileOpeningRequest> ActiveRequests => _requests.Values;

        /// Raised when a request was created or updated: the view must open its pop-up window for it, or refresh the
        /// bindings of the already open one.
        public event EventHandler<FileOpeningStatusEventArgs>? ShowRequested;

        /// Raised when a request reached the Finished phase: the view must close its pop-up window, if it is open.
        public event EventHandler<FileOpeningStatusEventArgs>? DismissRequested;

        /// Called on the UI thread for every FILE_OPENING_STATUS signal received from the server.
        public void OnFileOpeningStatus(ulong jobId, FileOpeningPhase phase, int percent, string itemName, string itemPath,
                                        long receivedBytes, long totalBytes)
        {
            if (phase == FileOpeningPhase.Finished)
            {
                // The request has been processed (successfully or not, or canceled): the pop-up window must be
                // closed. Remember the terminal phase, so that a progress update reordered after it is ignored.
                _finishedJobIds.Add(jobId);
                if (_requests.Remove(jobId, out var dismissedRequest))
                {
                    DismissRequested?.Invoke(this, new FileOpeningStatusEventArgs(dismissedRequest));
                }

                return;
            }

            // Ignore a progress update delivered after the Finished phase of the same request: the request is
            // already dismissed, and recreating it would leave a pop-up window open forever.
            if (_finishedJobIds.Contains(jobId))
            {
                return;
            }

            if (_requests.TryGetValue(jobId, out var request))
            {
                request.Update(phase, percent, itemName, itemPath, receivedBytes, totalBytes);
            }
            else
            {
                request = new FileOpeningRequest(jobId, phase, percent, itemName, itemPath, receivedBytes, totalBytes);
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
