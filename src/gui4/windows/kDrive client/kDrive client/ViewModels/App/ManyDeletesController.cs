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
using System.Collections.Generic;
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
            SyncDbId = manyDeletesInfo.SyncDbId;
            NotificationType = manyDeletesInfo.NotificationType;
            _filesPaths = Cap(manyDeletesInfo.FilesPaths.Distinct());
            _totalFilesCount = manyDeletesInfo.FilesPaths.Count;
        }

        public DbId SyncDbId { get; }

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

    public class ManyDeletesController : UISafeObservableObject
    {
        private readonly List<ManyDeletesNotification> _queue = new();
        private ManyDeletesNotification? _current;
        private bool _isAcknowledging;

        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public ManyDeletesNotification? CurrentManyDeleteNotification
        {
            get => _current;
            private set => SetPropertyInUIThread(ref _current, value);
        }

        public bool IsAcknowledging
        {
            get => _isAcknowledging;
            private set => SetPropertyInUIThread(ref _isAcknowledging, value);
        }

        public void AddOrMergeManyDeletes(ManyDeletesInfo manyDeletesInfo)
        {
            var existing = _queue.FirstOrDefault(notification => notification.SyncDbId == manyDeletesInfo.SyncDbId);
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

        public async Task<bool> AcknowledgeManyDeletes(ManyDeletesNotification acknowledgedNotification, TooManyDeletesUserChoice userChoice)
        {
            var notification = _queue.FirstOrDefault();
            if (IsAcknowledging || notification is null || notification.SyncDbId != acknowledgedNotification.SyncDbId)
            {
                Logger.Log(Logger.Level.Debug, $"Cannot acknowledge this many delete notification now. SyncDbId: {acknowledgedNotification.SyncDbId}");
                return false;
            }

            IsAcknowledging = true;
            try
            {
                if (notification.IsHardLimit)
                {
                    if (userChoice is not (TooManyDeletesUserChoice.Continue or TooManyDeletesUserChoice.Revert))
                    {
                        Logger.Log(Logger.Level.Warning, $"Invalid user choice for a hard limit notification. SyncDbId: {notification.SyncDbId}, UserChoice: {userChoice}");
                        return false;
                    }

                    var serverCommService = App.ServiceProvider.GetRequiredService<IServerCommService>();
                    if (!await serverCommService.AcknowledgeManyDeletes(notification.SyncDbId, userChoice, CancellationToken.None))
                    {
                        Logger.Log(Logger.Level.Warning, $"Failed to acknowledge many deletes on the server. SyncDbId: {notification.SyncDbId}");
                        return false;
                    }

                    _queue.Remove(notification);
                }
                else if (userChoice == TooManyDeletesUserChoice.IgnoreNext)
                {
                    if (!await ViewModel.Settings.ChangeNotifyBeforeDelete(false))
                    {
                        Logger.Log(Logger.Level.Warning, "Failed to apply NotifyBeforeDelete preferences");
                    }

                    _queue.RemoveAll(queued => !queued.IsHardLimit);
                }
                else
                {
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
    }
}