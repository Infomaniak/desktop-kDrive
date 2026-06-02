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

using Infomaniak.kDrive.ServerCommunication.CommStruct;
using Infomaniak.kDrive.ServerCommunication.JsonConverters;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ServerCommunication.Services
{
    /// <summary>
    /// Mock server protocol for testing error displays.
    /// Extends MockServerCommProtocol with automatic generation of test errors
    /// for all error templates. Errors are created with DbIds starting at 200.
    /// </summary>
    public class MockServerCommProtocolTestError : SocketServerCommProtocol
    {
        private DbId _nextErrorDbId = 200;

        public MockServerCommProtocolTestError() : base()
        {
            Task.Run(async () =>
            {
                while (!App.ServiceProvider.GetRequiredService<AppModel>().IsInitialized)
                {
                    await Task.Delay(500);
                }
                await SimulateSignals();
            });
        }

        public async Task SimulateSignals()
        {
            // Wait a bit for the UI to be ready
            await Task.Delay(2000);

            var appModel = App.ServiceProvider.GetRequiredService<AppModel>();
            // Get the first sync
            var syncs = appModel.Users[0].Accounts[0].Drives[0].Syncs;
            if (syncs.Count == 0)
            {
                Logger.Log(Logger.Level.Warning, "No syncs available for test errors");
                return;
            }

            var syncDbId = syncs[0].DbId;

            // Inconsistency errors
            await CreateAndEnqueueError(syncDbId, InconsistencyType.ForbiddenChar, "file_with_forbidden_char<>.txt");
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, InconsistencyType.ForbiddenCharEndWithSpace, "filename with space.txt ");
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, InconsistencyType.ReservedName, "CON");
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, InconsistencyType.NameLength, "very_long_filename_" + new string('x', 250) + ".txt");
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, InconsistencyType.PathLength, "folder/" + new string('f', 300) + "/file.txt");
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, InconsistencyType.NotYetSupportedChar, "file_with_emoji_😀.txt");
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, InconsistencyType.ForbiddenCharOnlySpaces, "     ");
            await Task.Delay(50);

            // Cancel/Operation errors
            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.Create);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.Edit);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.Move, path: "original_location/file.txt", destinationPath: "new_location/file.txt");
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.Delete);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.TmpBlacklisted);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.ExcludedByTemplate);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.Hardlink);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, cancelType: CancelType.FileRescued);
            await Task.Delay(50);

            // BackError errors
            await CreateAndEnqueueError(syncDbId, exitCode: ExitCode.BackError, exitCause: ExitCause.HttpErrForbidden);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, exitCode: ExitCode.BackError, exitCause: ExitCause.FileTooBig);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, exitCode: ExitCode.BackError, exitCause: ExitCause.QuotaExceeded);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, exitCode: ExitCode.BackError, exitCause: ExitCause.FileLocked);
            await Task.Delay(50);

            // SystemError errors
            await CreateAndEnqueueError(syncDbId, exitCode: ExitCode.SystemError, exitCause: ExitCause.FileAccessError);
            await Task.Delay(50);

            await CreateAndEnqueueError(syncDbId, exitCode: ExitCode.SystemError, exitCause: ExitCause.NotEnoughDiskSpace);
            await Task.Delay(50);

            // SyncPal errors
            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.NetworkError, ExitCause.SocketsDefuncted);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.NetworkError, ExitCause.NetworkTimeout);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.DataError, ExitCause.SyncDirChanged);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.DataError, ExitCause.InvalidSnapshot);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.BackError, ExitCause.DriveMaintenance);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.BackError, ExitCause.DriveNotRenew);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.BackError, ExitCause.DriveWakingUp);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.BackError, ExitCause.DriveAsleep);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.BackError, ExitCause.DriveAccessError);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.SystemError, ExitCause.SyncDirAccessError);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.SystemError, ExitCause.SyncDirDiskMissing);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.SystemError, ExitCause.NotEnoughDiskSpace);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.SystemError, ExitCause.UnableToStartVfs);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.SystemError, ExitCause.FileOrDirectoryCorrupted);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.InvalidSync, ExitCause.SyncDirNestingError);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.InvalidSync, ExitCause.SyncDirAccessError);
            await Task.Delay(50);

            await CreateAndEnqueueSyncPalError(syncDbId, ExitCode.LogicError, ExitCause.FullListParsingError);
            await Task.Delay(50);
        }

        private Task CreateAndEnqueueError(
            DbId syncDbId,
            InconsistencyType? inconsistencyType = null,
            string? path = null,
            CancelType? cancelType = null,
            ExitCode? exitCode = null,
            ExitCause? exitCause = null,
            string? destinationPath = null)
        {
            var errorInfo = new ErrorInfo
            {
                DbId = _nextErrorDbId++,
                Time = DateTime.Now,
                Level = ErrorLevel.Node,
                SyncDbId = syncDbId,
                NodeType = NodeType.File,
                Path = path ?? $"test_file_{_nextErrorDbId}.txt",
                DestinationPath = destinationPath ?? "",
                InconsistencyType = inconsistencyType ?? InconsistencyType.None,
                CancelType = cancelType ?? CancelType.None,
                ExitCode = exitCode ?? ExitCode.Unknown,
                ExitCause = exitCause ?? ExitCause.Unknown,
                AutoResolved = false
            };

            var errorJson = SerializeErrorInfo(errorInfo);
            RaiseSignal(SignalNum.UTILITY_ERROR_ADDED, errorJson);

            return Task.CompletedTask;
        }

        private Task CreateAndEnqueueSyncPalError(
            DbId syncDbId,
            ExitCode exitCode,
            ExitCause exitCause = ExitCause.Unknown)
        {
            var errorInfo = new ErrorInfo
            {
                DbId = _nextErrorDbId++,
                Time = DateTime.Now,
                Level = ErrorLevel.SyncPal,
                SyncDbId = syncDbId,
                NodeType = NodeType.Unknown,
                Path = "",
                DestinationPath = "",
                InconsistencyType = InconsistencyType.None,
                CancelType = CancelType.None,
                ExitCode = exitCode,
                ExitCause = exitCause,
                AutoResolved = false
            };

            var errorJson = SerializeErrorInfo(errorInfo);
            RaiseSignal(SignalNum.UTILITY_ERROR_ADDED, errorJson);

            return Task.CompletedTask;
        }

        private JsonObject SerializeErrorInfo(ErrorInfo errorInfo)
        {
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = false,
                DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
            };
            options.Converters.Add(new Base64StringJsonConverter());
            options.Converters.Add(new IntToDateTimeConverter());

            var json = JsonSerializer.SerializeToNode(errorInfo, options);
            var jsonObject = json as JsonObject ?? [];

            return new JsonObject
            {
                { JsonKeys.ErrorInfo, jsonObject }
            };
        }
    }
}
