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

using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.ServerCommunication;
using Infomaniak.kDrive.ViewModels.Errors;
using Microsoft.UI.Xaml;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation;
using Infomaniak.kDrive.Types;

namespace Infomaniak.kDrive.ViewModels
{
    public class Sync : ObservableObject
    {
        // Sync properties
        private DbId _dbId;
        private readonly Drive _drive;
        private SyncId _id = -1;
        private SyncPath _localPath = "";
        private SyncPath _remotePath = "";
        private bool _supportVfs = false;
        private readonly ObservableCollection<SyncActivity> _syncActivities = new();
        private SyncStatus _syncStatus = SyncStatus.Pause;

        private ObservableCollection<Errors.BaseError> _syncErrors = new();

        // Sync UI properties
        private bool _showIncomingActivity = true;


        public SyncStatus SyncStatus
        {
            get => _syncStatus;
            set => SetProperty(ref _syncStatus, value);
        }

        //TODO: Remove this test function

        private SyncActivity GenerateTestActivity()
        {
            Random rand = new Random();
            string[] sampleFileExtension = new string[]
            {
                "docx",
                "doc",
                "png",
                "jpg",
                "xls",
                "xlsx",
                "txt",
            };

            string[] sampleDirectories = new string[]
            {
                "Documents",
                "Photos",
                "Music",
                "Videos",
                "Work",
                "Personal",
                "Projects",
                "Backups"
            };

            string[] sampleFileNames = new string[]
            {
                "Report",
                "Presentation",
                "Notes",
                "Meeting",
                "Holiday",
                "Family",
                "ProjectPlan",
                "Budget"
            };

            int randomPathDepth = rand.Next(0, 2); // Random depth between 0 and 1

            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < randomPathDepth; i++)
            {
                sb.Append(sampleDirectories[rand.Next(sampleDirectories.Length)] + "/");
            }

            bool isFile = rand.Next(2) == 0; // Randomly decide if it's a file or directory
            if (isFile)
            {
                sb.Append(sampleFileNames[rand.Next(sampleFileNames.Length)] + "." + sampleFileExtension[rand.Next(sampleFileExtension.Length)]);
            }
            else
            {
                sb.Append(sampleFileNames[rand.Next(sampleFileNames.Length)]);
            }
            SyncDirection direction = (SyncDirection)rand.Next(2); // Randomly choose direction
            NodeType nodeType = isFile ? NodeType.File : NodeType.Directory;
            long size = isFile ? rand.Next(0, 5000000) : 0; // Random size for files, 0 for directories
            DateTime activityTime = DateTime.Now;
            return new SyncActivity()
            {
                LocalPath = "C:/Users/Herve/kDrive/" + sb.ToString(),
                Direction = direction,
                NodeType = nodeType,
                Size = size,
                ActivityTime = activityTime,
                RemoteId = rand.Next(1, 1000)
            };
        }

        public Sync(DbId dbId, Drive drive)
        {
            _dbId = dbId;
            _drive = drive;

            Task.Run(async () =>
            {
                Random random = new Random();

                while (true)
                {
                    await Task.Delay(random.Next(1, 5000)); // Wait between 0 to 5 seconds
                    if (SyncStatus != SyncStatus.Running)
                    {
                        continue;
                    }
                    var newActivity = GenerateTestActivity();

                    AppModel.UIThreadDispatcher.TryEnqueue(() =>
                    {
                        _syncActivities.Insert(0, newActivity);
                        if (_syncActivities.Count > 100)
                        {
                            _syncActivities.RemoveAt(_syncActivities.Count - 1);
                        }
                    });
                }
            });
        }

        public DbId DbId
        {
            get => _dbId;
            set => SetProperty(ref _dbId, value);
        }

        public SyncId Id
        {
            get => _id;
            set => SetProperty(ref _id, value);
        }

        public SyncPath LocalPath
        {
            get => _localPath;
            set => SetProperty(ref _localPath, value);

        }

        public SyncPath RemotePath
        {
            get => _remotePath;
            set
            {
                // Ensure the path ends with a directory separator
                if (!string.IsNullOrEmpty(_remotePath) && !_remotePath.EndsWith(Path.DirectorySeparatorChar.ToString()))
                {
                    _remotePath += Path.DirectorySeparatorChar;
                }
                SetProperty(ref _remotePath, value);
            }
        }

        public bool SupportVfs
        {
            get => _supportVfs;
            set => SetProperty(ref _supportVfs, value);
        }

        public ObservableCollection<SyncActivity> SyncActivities
        {
            get => _syncActivities;
        }

        public Drive Drive
        {
            get => _drive;
        }

        public ObservableCollection<Errors.BaseError> SyncErrors
        {
            get => _syncErrors;
            set => SetProperty(ref _syncErrors, value);
        }

        public async Task Reload()
        {
            Logger.Log(Logger.Level.Info, $"Reloading sync properties for DbId {DbId}...");
            Task[] tasks = new Task[]
            {
               CommRequests.GetSyncId(DbId).ContinueWith(t => { if (t.Result != null) Id = t.Result.Value; }),
               CommRequests.GetSyncLocalPath(DbId).ContinueWith(t => { if (t.Result != null) LocalPath = t.Result; }),
               CommRequests.GetSyncRemotePath(DbId).ContinueWith(t => { if (t.Result != null) RemotePath = t.Result; }),
               CommRequests.GetSyncSupportVfs(DbId).ContinueWith(t => { if (t.Result != null) SupportVfs = t.Result.Value; }),
            };
            await Task.WhenAll(tasks).ConfigureAwait(false);
            Logger.Log(Logger.Level.Info, $"Finished reloading sync properties for DbId {DbId}.");
        }

        public bool ShowIncomingActivity
        {
            get => _showIncomingActivity;
            set => SetProperty(ref _showIncomingActivity, value);
        }
    }
}
