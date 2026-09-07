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
using System.ComponentModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class ManyDeletesNotification : UISafeObservableObject
    {
        private const int SoftLimitMaxFilesPaths = 200;

        private List<string> _filesPaths;
        private int _totalFilesCount;

        public ManyDeletesNotification(ManyDeletesInfo manyDeletesInfo)
        {
            AssociatedSync = App.ServiceProvider.GetRequiredService<AppModel>().AllSyncs.FirstOrDefault(sync => sync.DbId == manyDeletesInfo.SyncDbId);
            if (AssociatedSync is null)
                Logger.Log(Logger.Level.Warning, $"ManyDeletesNotification created for a non-existing sync. SyncDbId: {manyDeletesInfo.SyncDbId}");

            NotificationType = manyDeletesInfo.NotificationType;
            _filesPaths = Cap(manyDeletesInfo.FilesPaths.Distinct());
            _totalFilesCount = manyDeletesInfo.FilesPaths.Count;
        }
        public Sync? AssociatedSync { get; }

        public TooManyDeletesNotificationType NotificationType { get; }

        public bool IsHardLimit => NotificationType == TooManyDeletesNotificationType.HardLimit;

        public IReadOnlyList<string> FilesPaths => _filesPaths;

        public int TotalFilesCount
        {
            get => _totalFilesCount;
            private set => SetPropertyInUIThread(ref _totalFilesCount, value);
        }

        public bool IsTruncated => _filesPaths.Count < TotalFilesCount;

        public void Merge(ManyDeletesInfo manyDeletesInfo)
        {
            var addedPaths = manyDeletesInfo.FilesPaths.Except(_filesPaths).ToList();

            SetPropertyInUIThread(ref _filesPaths, Cap(_filesPaths.Concat(addedPaths)), nameof(FilesPaths));
            TotalFilesCount += addedPaths.Count;
            OnPropertyChangedInUIThread(nameof(IsTruncated));
        }

        private List<string> Cap(IEnumerable<string> filesPaths)
        {
            return IsHardLimit ? filesPaths.ToList() : filesPaths.Take(SoftLimitMaxFilesPaths).ToList();
        }
    }

    public enum ManyDeletesUserAction
    {
        /// The dialog was closed programmatically, no choice was made.
        Dismissed,
        /// Hard limit only: keep the deletions and let the sync continue.
        Continue,
        /// Hard limit only: restore the deleted files.
        Revert,
        /// Soft limit only: acknowledge the deletions.
        Close,
        /// Soft limit only: acknowledge the deletions and open the drive's online trash.
        OpenTrash,
        /// Soft limit only: acknowledge the deletions and stop notifying before deletions.
        IgnoreNext
    }

    public class ManyDeletesController : UISafeObservableObject
    {
        private readonly List<ManyDeletesNotification> _queue = new();
        private readonly SemaphoreSlim _displaySemaphore = new(1, 1);
        private ManyDeletesNotification? _current;
        private ManyDeletesNotification? _displayed;
        private bool _isAcknowledging;

        private AppModel ViewModel => App.ServiceProvider.GetRequiredService<AppModel>();

        /// Raised when a notification is ready to be displayed by the view.
        public event EventHandler? ShowRequested;

        /// Raised when the currently displayed dialog must be closed programmatically, because a
        /// higher priority notification superseded it.
        public event EventHandler? DismissRequested;

        /// Raised when the content of the notification currently displayed changed, so that the
        /// view can refresh its bindings.
        public event EventHandler? NotificationChanged;

        public ManyDeletesNotification? CurrentManyDeleteNotification
        {
            get => _current;
            private set
            {
                if (ReferenceEquals(_current, value))
                    return;

                if (_current is not null)
                    _current.PropertyChanged -= OnCurrentNotificationPropertyChanged;

                SetPropertyInUIThread(ref _current, value);

                if (_current is not null)
                    _current.PropertyChanged += OnCurrentNotificationPropertyChanged;

                RaiseInUIThread(NotificationChanged);
                RequestDisplay();
            }
        }

        public bool IsAcknowledging
        {
            get => _isAcknowledging;
            private set => SetPropertyInUIThread(ref _isAcknowledging, value);
        }

        /// True while the view is asked to close the open dialog programmatically, so it can tell
        /// this apart from a light dismiss (which must be prevented).
        public bool IsDismissing { get; private set; }

        public bool AskBeforeDelete => ViewModel.Settings.AskBeforeDelete;

        private void OnCurrentNotificationPropertyChanged(object? sender, PropertyChangedEventArgs e) => RaiseInUIThread(NotificationChanged);

        private void RaiseInUIThread(EventHandler? handler)
        {
            if (handler is null)
                return;

            _ = Utility.RunOnUIThread(() => handler.Invoke(this, EventArgs.Empty));
        }

        /// Asks the view to display the notification currently exposed, if any. Also used by the
        /// view once it is loaded, to display a notification that arrived before it was ready.
        public void RequestDisplay()
        {
            if (CurrentManyDeleteNotification is null)
                return;

            // A different notification took priority over the one being displayed: close its dialog
            // first, the display loop will then pick the new one up.
            if (_displayed is not null && !ReferenceEquals(_displayed, CurrentManyDeleteNotification))
            {
                IsDismissing = true;
                RaiseInUIThread(DismissRequested);
                return;
            }

            RaiseInUIThread(ShowRequested);
        }

        /// Displays notifications one after the other using the given view callback, until none is
        /// left or the view could not show its dialog.
        public async Task RunDisplayLoop(Func<ManyDeletesNotification, Task<ManyDeletesUserAction?>> showDialogAsync)
        {
            if (!await _displaySemaphore.WaitAsync(0))
                return; // Another loop is already running

            try
            {
                while (CurrentManyDeleteNotification is ManyDeletesNotification notification)
                {
                    _displayed = notification;
                    ManyDeletesUserAction? action;
                    try
                    {
                        action = await showDialogAsync(notification);
                    }
                    finally
                    {
                        _displayed = null;
                        IsDismissing = false;
                    }

                    if (action is null)
                        return; // The dialog could not be shown; it will be retried later.

                    if (action == ManyDeletesUserAction.Dismissed)
                        continue; // Superseded by another notification, nothing to acknowledge.

                    if (!await AcknowledgeManyDeletes(notification, action.Value))
                        return;
                }
            }
            finally
            {
                _displaySemaphore.Release();
            }
        }

        public void AddOrMergeManyDeletes(ManyDeletesInfo manyDeletesInfo)
        {
            var existing = _queue.FirstOrDefault(notification => notification.AssociatedSync?.DbId == manyDeletesInfo.SyncDbId);
            var isHardLimit = manyDeletesInfo.NotificationType == TooManyDeletesNotificationType.HardLimit;

            if (existing is not null && !isHardLimit)
            {
                if (!existing.IsHardLimit)
                {
                    existing.Merge(manyDeletesInfo);
                }
                return;
            }

            if (existing is not null)
            {
                _queue.Remove(existing);
            }

            var notification = new ManyDeletesNotification(manyDeletesInfo);
            _queue.Insert(isHardLimit ? _queue.FindLastIndex(queued => queued.IsHardLimit) + 1 : _queue.Count, notification);
            CurrentManyDeleteNotification = _queue[0];
        }

        public async Task<bool> AcknowledgeManyDeletes(ManyDeletesNotification acknowledgedNotification, ManyDeletesUserAction userAction)
        {
            var notification = _queue.FirstOrDefault();
            if (IsAcknowledging || notification is null || notification.AssociatedSync is null || notification.AssociatedSync.DbId != acknowledgedNotification.AssociatedSync?.DbId)
            {
                Logger.Log(Logger.Level.Debug, $"Cannot acknowledge this many delete notification now. SyncDbId: {acknowledgedNotification.AssociatedSync?.DbId}");
                return false;
            }

            IsAcknowledging = true;
            try
            {
                if (notification.IsHardLimit)
                {
                    if (userAction is not (ManyDeletesUserAction.Continue or ManyDeletesUserAction.Revert))
                    {
                        Logger.Log(Logger.Level.Warning, $"Invalid user action for a hard limit notification. SyncDbId: {notification.AssociatedSync?.DbId}, UserAction: {userAction}");
                        return false;
                    }

                    var userChoice = userAction == ManyDeletesUserAction.Continue ? TooManyDeletesUserChoice.Continue : TooManyDeletesUserChoice.Revert;
                    var serverCommService = App.ServiceProvider.GetRequiredService<IServerCommService>();
                    if (!await serverCommService.AcknowledgeManyDeletes(notification.AssociatedSync.DbId, userChoice, CancellationToken.None))
                    {
                        Logger.Log(Logger.Level.Warning, $"Failed to acknowledge many deletes on the server. SyncDbId: {notification.AssociatedSync?.DbId}");
                        return false;
                    }

                    _queue.Remove(notification);
                }
                else if (userAction == ManyDeletesUserAction.IgnoreNext)
                {
                    if (!await ViewModel.Settings.ChangeNotifyBeforeDelete(false))
                    {
                        Logger.Log(Logger.Level.Warning, "Failed to apply NotifyBeforeDelete preferences");
                    }

                    _queue.RemoveAll(queued => !queued.IsHardLimit);
                }
                else
                {
                    if (userAction == ManyDeletesUserAction.OpenTrash)
                        await OpenTrash(notification.AssociatedSync.DbId);

                    _queue.Remove(notification);
                }

                CurrentManyDeleteNotification = _queue.FirstOrDefault();
                return true;
            }
            finally
            {
                IsAcknowledging = false;
            }
        }

        private async Task OpenTrash(DbId syncDbId)
        {
            Uri? trashUrl = ViewModel.AllSyncs.FirstOrDefault(sync => sync.DbId == syncDbId)?.Drive.GetWebTrashUri();
            if (trashUrl is null)
            {
                Logger.Log(Logger.Level.Error, $"Unable to get the trash URL for the sync with DbId {syncDbId}.");
                return;
            }

            Logger.Log(Logger.Level.Debug, $"Launching the trash URL: {trashUrl}");
            await Windows.System.Launcher.LaunchUriAsync(trashUrl);
        }
    }
}
