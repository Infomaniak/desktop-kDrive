/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

import Foundation

extension MsgType: Codable {}
extension KDC.ExitCause: Codable {}
extension KDC.ExitCode: Codable {}
extension KDC.NodeType: Codable {}
extension KDC.VirtualFileMode: Codable {}
extension KDC.SyncFileStatus: Codable {}
extension KDC.SyncDirection: Codable {}
extension KDC.SyncFileInstruction: Codable {}
extension KDC.ConflictType: Codable {}
extension KDC.InconsistencyType: Codable {}
extension KDC.CancelType: Codable {}
extension KDC.SyncStatus: Codable {}
extension KDC.SyncStep: Codable {}
extension KDC.ErrorLevel: Codable {}
extension KDC.Language: Codable {}
extension KDC.LogLevel: Codable {}
extension KDC.NotificationsDisabled: Codable {}
extension KDC.ProxyType: Codable {}
extension KDC.VersionChannel: Codable {}
extension KDC.UpdateState: Codable {}
extension KDC.ReplicaSide: Codable {}
extension KDC.SyncConfiguration: Codable {}
extension SignalNum: Codable {}
extension RequestNum: Codable {}

extension MsgType: Hashable {}
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
extension KDC.VersionChannel: Hashable {}
extension KDC.UpdateState: Hashable {}
extension KDC.SyncConfiguration: Hashable {}
extension SignalNum: Hashable {}
extension RequestNum: Hashable {}
