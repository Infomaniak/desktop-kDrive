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
using System.Linq;
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
        private string _webUrl;
        private long _driveDbId;
        private string _remoteNodeId = "";
        private ItemDownloadPhase _itemDownloadPhase = ItemDownloadPhase.Idle;
        private ulong? _itemDownloadId;
        private int _itemDownloadPercent;
        private long _itemDownloadReceivedBytes = -1;
        private long _itemDownloadTotalBytes = -1;

        /// Phase of the download of the item into the download folder of the user, triggered from the failure window.
        public enum ItemDownloadPhase
        {
            /// The item can be downloaded into the download folder of the user.
            Idle,
            /// The item is being downloaded: the failure window shows the progress of the download.
            Downloading,
            /// The download failed: the failure window shows an error and falls back to the kDrive web application.
            Failed
        }

        public FileOpeningRequest(FileOpeningStatusUpdate update)
        {
            JobId = update.JobId;
            _phase = update.Phase;
            _percent = update.Percent;
            _itemName = update.ItemName;
            _itemPath = update.ItemPath;
            _receivedBytes = update.ReceivedBytes;
            _totalBytes = update.TotalBytes;
            _webUrl = update.WebUrl;
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
                    OnPropertyChangedInUIThread(nameof(IsFailed));
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

        /// kDrive web application address of the item, from which it can be downloaded manually when the opening
        /// failed. Empty while the request has not failed.
        public string WebUrl
        {
            get => _webUrl;
            private set
            {
                if (SetPropertyInUIThread(ref _webUrl, value))
                {
                    OnPropertyChangedInUIThread(nameof(IsFailed));
                }
            }
        }

        /// True once the request failed (generic failure, or item not found on the remote side): the user interface
        /// must present the failure window instead of the progress one.
        public bool IsFailed => Phase is FileOpeningPhase.Failed or FileOpeningPhase.NotFound;

        /// True when the request failed because the item does not exist on the remote side: the failure window shows
        /// a dedicated message and offers no download, since there is nothing to download.
        public bool IsNotFound => Phase == FileOpeningPhase.NotFound;

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

        /// Updates the state of the request from the delivered signal. The identity of the item is kept when the
        /// update does not carry it: the Failed phase is reported with an empty identity, and the failure window
        /// must still show the card of the item.
        public void Update(FileOpeningStatusUpdate update)
        {
            Phase = update.Phase;
            Percent = update.Percent;
            if (!string.IsNullOrEmpty(update.ItemName))
            {
                ItemName = update.ItemName;
            }

            if (!string.IsNullOrEmpty(update.ItemPath))
            {
                ItemPath = update.ItemPath;
            }

            ReceivedBytes = update.ReceivedBytes;
            TotalBytes = update.TotalBytes;
            WebUrl = update.WebUrl;
            if (update.DriveDbId != 0)
            {
                DriveDbId = update.DriveDbId;
            }

            if (!string.IsNullOrEmpty(update.RemoteNodeId))
            {
                RemoteNodeId = update.RemoteNodeId;
            }
        }

        /// Local database id of the drive of the item, reported by the failure signals: together with the remote node
        /// id, it identifies the item to download into the download folder of the user.
        public long DriveDbId
        {
            get => _driveDbId;
            private set
            {
                if (SetPropertyInUIThread(ref _driveDbId, value))
                {
                    OnPropertyChangedInUIThread(nameof(HasItemIdentity));
                }
            }
        }

        /// Remote node id of the item, reported by the failure signals for the same purpose.
        public string RemoteNodeId
        {
            get => _remoteNodeId;
            private set
            {
                if (SetPropertyInUIThread(ref _remoteNodeId, value))
                {
                    OnPropertyChangedInUIThread(nameof(HasItemIdentity));
                }
            }
        }

        /// True when the identity of the item is known and the item exists on the remote side: the failure window
        /// can then trigger its download into the download folder of the user.
        public bool HasItemIdentity => !IsNotFound && DriveDbId != 0 && !string.IsNullOrEmpty(RemoteNodeId);

        /// Phase of the download of the item into the download folder of the user, triggered from the failure window.
        public ItemDownloadPhase DownloadPhase
        {
            get => _itemDownloadPhase;
            private set
            {
                if (SetPropertyInUIThread(ref _itemDownloadPhase, value))
                {
                    OnPropertyChangedInUIThread(nameof(IsItemDownloading));
                    OnPropertyChangedInUIThread(nameof(IsItemDownloadFailed));
                }
            }
        }

        public bool IsItemDownloading => DownloadPhase == ItemDownloadPhase.Downloading;

        public bool IsItemDownloadFailed => DownloadPhase == ItemDownloadPhase.Failed;

        /// Id of the download of the item into the download folder of the user, to be matched with the
        /// FILE_OPENING_DOWNLOAD_STATE signals. Null while no download is in progress.
        public ulong? ItemDownloadId
        {
            get => _itemDownloadId;
            private set => SetPropertyInUIThread(ref _itemDownloadId, value);
        }

        /// Progress of the download of the item into the download folder of the user, in percent. Only meaningful
        /// while <see cref="IsItemDownloading"/> is true and the total size of the item is unknown.
        public int ItemDownloadPercent
        {
            get => _itemDownloadPercent;
            private set
            {
                if (SetPropertyInUIThread(ref _itemDownloadPercent, value))
                {
                    OnPropertyChangedInUIThread(nameof(ItemDownloadGaugeValue));
                }
            }
        }

        /// Number of bytes downloaded so far into the download folder of the user, -1 while unknown.
        public long ItemDownloadReceivedBytes
        {
            get => _itemDownloadReceivedBytes;
            private set
            {
                if (SetPropertyInUIThread(ref _itemDownloadReceivedBytes, value))
                {
                    OnPropertyChangedInUIThread(nameof(ItemDownloadGaugeValue));
                    OnPropertyChangedInUIThread(nameof(HasItemDownloadProgress));
                    OnPropertyChangedInUIThread(nameof(ItemDownloadProgressText));
                }
            }
        }

        /// Total size of the item in bytes, -1 while unknown.
        public long ItemDownloadTotalBytes
        {
            get => _itemDownloadTotalBytes;
            private set
            {
                if (SetPropertyInUIThread(ref _itemDownloadTotalBytes, value))
                {
                    OnPropertyChangedInUIThread(nameof(ItemDownloadGaugeMaximum));
                    OnPropertyChangedInUIThread(nameof(ItemDownloadGaugeValue));
                    OnPropertyChangedInUIThread(nameof(HasItemDownloadProgress));
                    OnPropertyChangedInUIThread(nameof(ItemDownloadProgressText));
                }
            }
        }

        /// True once both the downloaded amount and the total size of the item are known: the gauge tracks the bytes
        /// and the downloaded amount is displayed under it.
        public bool HasItemDownloadProgress => ItemDownloadTotalBytes > 0 && ItemDownloadReceivedBytes >= 0;

        /// Upper bound of the gauge of the download into the download folder of the user: the total size of the item
        /// when known, 100 with the percent otherwise.
        public double ItemDownloadGaugeMaximum => ItemDownloadTotalBytes > 0 ? ItemDownloadTotalBytes : 100;

        /// Current value of the gauge of the download into the download folder of the user: the downloaded amount
        /// when the total size is known, the percent otherwise.
        public double ItemDownloadGaugeValue => ItemDownloadTotalBytes > 0
            ? Math.Max(ItemDownloadReceivedBytes, 0)
            : Math.Max(ItemDownloadPercent, 0);

        /// Downloaded amount as a localized "12.5 MB over 48 MB" string, only meaningful when
        /// <see cref="HasItemDownloadProgress"/> is true.
        public string ItemDownloadProgressText => Localizer.Instance.GetString1s2s("fileOpeningDownloadProgress",
            (string)BytesConverter.Convert(ItemDownloadReceivedBytes, typeof(string), string.Empty, ""),
            (string)BytesConverter.Convert(ItemDownloadTotalBytes, typeof(string), string.Empty, ""));

        /// The server accepted the download of the item into the download folder of the user.
        public void StartItemDownload(ulong downloadId)
        {
            ItemDownloadId = downloadId;
            ItemDownloadPercent = 0;
            ItemDownloadReceivedBytes = 0;
            ItemDownloadTotalBytes = -1;
            DownloadPhase = ItemDownloadPhase.Downloading;
        }

        /// The download of the item into the download folder of the user progressed.
        public void UpdateItemDownloadProgress(FileOpeningDownloadStateUpdate update)
        {
            if (DownloadPhase != ItemDownloadPhase.Downloading || ItemDownloadId != update.DownloadId)
            {
                // A state of a download that is not the current one, or reordered after its terminal phase: ignore.
                return;
            }

            ItemDownloadPercent = update.Percent;
            ItemDownloadReceivedBytes = update.ReceivedBytes;
            ItemDownloadTotalBytes = update.TotalBytes;
        }

        /// The download of the item into the download folder of the user succeeded: the failure window closes.
        public void CompleteItemDownload()
        {
            ItemDownloadId = null;
            DownloadPhase = ItemDownloadPhase.Idle;
        }

        /// The download of the item into the download folder of the user failed: the failure window shows an error
        /// and falls back to the kDrive web application page of the item.
        public void FailItemDownload()
        {
            ItemDownloadId = null;
            DownloadPhase = ItemDownloadPhase.Failed;
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

    /// The state of a file opening request as reported by a FILE_OPENING_STATUS signal of the server.
    public sealed class FileOpeningStatusUpdate
    {
        public ulong JobId { get; init; }
        public FileOpeningPhase Phase { get; init; }
        public int Percent { get; init; }
        public string ItemName { get; init; } = "";
        public string ItemPath { get; init; } = "";
        public long ReceivedBytes { get; init; } = -1;
        public long TotalBytes { get; init; } = -1;

        /// kDrive web application address of the item, from which it can be downloaded manually when the opening
        /// failed. Empty otherwise.
        public string WebUrl { get; init; } = "";

        /// Local database id of the drive of the item, reported by the failure signals so that the failure window
        /// can trigger the download of the item into the download folder of the user. Zero otherwise.
        public long DriveDbId { get; init; }

        /// Remote node id of the item, reported by the failure signals for the same purpose. Empty otherwise.
        public string RemoteNodeId { get; init; } = "";
    }

    /// The state of a download of the item of a failed file opening request into the download folder of the user, as
    /// reported by a FILE_OPENING_DOWNLOAD_STATE signal of the server.
    public sealed class FileOpeningDownloadStateUpdate
    {
        public ulong DownloadId { get; init; }
        public FileOpeningDownloadState State { get; init; }
        public int Percent { get; init; }
        public long ReceivedBytes { get; init; }
        public long TotalBytes { get; init; }

        /// Path into which the item is downloaded.
        public string FinalPath { get; init; } = "";
    }

    /// The reply of a FILE_OPENING_DOWNLOAD request: the id of the started download, to be matched with the
    /// FILE_OPENING_DOWNLOAD_STATE signals, and the path into which the item is downloaded.
    public sealed class FileOpeningDownloadResponse
    {
        public ulong DownloadId { get; init; }
        public string FinalPath { get; init; } = "";
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

    /// Download identifiers of the item downloads triggered from the failure windows, mapped to the job identifier of
    /// their request, so that the FILE_OPENING_DOWNLOAD_STATE signals find their request.
    private readonly Dictionary<ulong, ulong> _jobIdsByDownloadId = new();

        /// Requests currently tracked, i.e. reported by the server and not finished yet. Used to replay the
        /// pending requests to a presentation surface subscribing after the requests have started.
        public IEnumerable<FileOpeningRequest> ActiveRequests =>
            _requests.Values.Where(request => !_finishedJobIds.Contains(request.JobId));

        /// Raised when a request was created or updated: the view must open its pop-up window for it, or refresh the
        /// bindings of the already open one. The request phase tells a progress window from a failure window.
        public event EventHandler<FileOpeningStatusEventArgs>? ShowRequested;

        /// Raised when a request reached the Finished phase: the view must close its pop-up window, if it is open.
        public event EventHandler<FileOpeningStatusEventArgs>? DismissRequested;

        /// Called on the UI thread for every FILE_OPENING_STATUS signal received from the server.
        public void OnFileOpeningStatus(FileOpeningStatusUpdate update)
        {
            if (update.Phase == FileOpeningPhase.Finished)
            {
                // The request has been processed (successfully or not, or canceled): the pop-up window must be
                // closed. Remember the terminal phase, so that a progress update reordered after it is ignored.
                // The request is kept: the failure phases follow the Finished one and feed the failure window with
                // the identity of the item.
                _finishedJobIds.Add(update.JobId);
                if (_requests.TryGetValue(update.JobId, out var dismissedRequest))
                {
                    DismissRequested?.Invoke(this, new FileOpeningStatusEventArgs(dismissedRequest));
                }

                return;
            }

            // The failure phases are reported after the Finished one and carry the outcome of the request: process
            // them even though the request has already been dismissed. Any other update for a dismissed request is
            // ignored: recreating it would leave a pop-up window open forever.
            if (update.Phase is not (FileOpeningPhase.Failed or FileOpeningPhase.NotFound) &&
                _finishedJobIds.Contains(update.JobId))
            {
                return;
            }

            if (!_requests.TryGetValue(update.JobId, out var request))
            {
                request = new FileOpeningRequest(update);
                _requests[update.JobId] = request;
            }
            else
            {
                request.Update(update);
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

        /// Asks the server to download the item of a failed file opening request into the download folder of the
        /// user. The server resolves the identity of the item from its API, shifts the name of the downloaded file
        /// until it does not collide with an existing item of the folder, and reports the progress and the completion
        /// of the download through the FILE_OPENING_DOWNLOAD_STATE signals.
        public async Task<bool> DownloadRequestItemAsync(ulong jobId)
        {
            if (!_requests.TryGetValue(jobId, out var request) || !request.HasItemIdentity)
            {
                Logger.LogWarning($"Cannot download the item of the file opening request: unknown request or unknown item identity. JobId: {jobId}",
                    "Cannot download the item of the file opening request");
                return false;
            }

            var serverCommService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            var response = await serverCommService.DownloadFileOpeningItem(request.DriveDbId, request.RemoteNodeId, "", CancellationToken.None);
            if (response is null)
            {
                Logger.LogWarning($"Failed to start the download of the item of the file opening request. JobId: {jobId}",
                    "Failed to start the download of the item of the file opening request");
                request.FailItemDownload();
                return false;
            }

            Logger.LogInfo($"Downloading the item of the file opening request into {response.FinalPath}. JobId: {jobId}, DownloadId: {response.DownloadId}");
            _jobIdsByDownloadId[response.DownloadId] = jobId;
            request.StartItemDownload(response.DownloadId);
            return true;
        }

        /// Called on the UI thread for every FILE_OPENING_DOWNLOAD_STATE signal received from the server: forwards
        /// the progress to the failure window of the request, closes the window once the item has been downloaded, or
        /// makes it fall back to the kDrive web application when the download failed.
        public void OnFileOpeningDownloadState(FileOpeningDownloadStateUpdate update)
        {
            if (!_jobIdsByDownloadId.TryGetValue(update.DownloadId, out var jobId) || !_requests.TryGetValue(jobId, out var request))
            {
                Logger.LogWarning($"Received a download state for an unknown download. DownloadId: {update.DownloadId}",
                    "Unknown download for FILE_OPENING_DOWNLOAD_STATE signal");
                return;
            }

            switch (update.State)
            {
                case FileOpeningDownloadState.Downloading:
                    request.UpdateItemDownloadProgress(update);
                    break;
                case FileOpeningDownloadState.Done:
                    Logger.LogInfo($"The item of the file opening request has been downloaded into {update.FinalPath}. JobId: {jobId}");
                    _jobIdsByDownloadId.Remove(update.DownloadId);
                    request.CompleteItemDownload();
                    DismissRequested?.Invoke(this, new FileOpeningStatusEventArgs(request));
                    break;
                case FileOpeningDownloadState.Failed:
                    Logger.LogError($"The download of the item of the file opening request failed. JobId: {jobId}",
                        "The download of the item of the file opening request failed");
                    _jobIdsByDownloadId.Remove(update.DownloadId);
                    request.FailItemDownload();
                    break;
            }
        }
    }
}
