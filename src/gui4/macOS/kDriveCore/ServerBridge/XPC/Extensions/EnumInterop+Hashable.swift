/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import CppInterop

extension KDC.ExitCause: Hashable {}
extension KDC.ExitCode: Hashable {}
extension KDC.NodeType: Hashable {}
extension KDC.VirtualFileMode: Hashable {}
extension KDC.SyncFileStatus: Hashable {}
extension KDC.SyncDirection: Hashable {}
extension KDC.SyncFileInstruction: Hashable {}
extension KDC.ConflictType: Hashable {}
extension KDC.InconsistencyType: Hashable {}
extension KDC.CancelType: Hashable {}
extension KDC.SyncStatus: Hashable {}
extension KDC.SyncStep: Hashable {}
extension KDC.ErrorLevel: Hashable {}
extension KDC.Language: Hashable {}
extension KDC.LogLevel: Hashable {}
extension KDC.NotificationsDisabled: Hashable {}
extension KDC.ProxyType: Hashable {}
extension KDC.DistributionChannel: Hashable {}
extension KDC.UpdateState: Hashable {}
extension KDC.SyncConfiguration: Hashable {}
extension KDC.ConflictResolutionStrategy: Hashable {}
extension MsgType: @retroactive Hashable {}
extension SignalNum: @retroactive Hashable {}
extension RequestNum: @retroactive Hashable {}
