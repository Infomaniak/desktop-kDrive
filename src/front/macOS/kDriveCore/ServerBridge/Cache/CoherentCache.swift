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

/// Structure always follow this nested model: User → Account → Drive → Synchro
public protocol CoherentCache: Sendable {
    // MARK: - User

    func getUser(apiId: Int32) async -> User?
    func getUser(dbId: Int32) async -> User?
    func getFirstAvailableUser() async -> User?
    func addUser(_ user: User) async
    func removeUser(dbId: Int32) async
    func updateUser(_ user: User) async
    func updateAvailableDrives(_ drives: [AvailableDrive], forUserDbId accountId: Int32) async throws

    // MARK: - Account

    func getAccount(accountDbId: Int32, userDbId: Int32) async -> Account?
    func getAccount(accountDbId: Int32) async -> Account?
    func addAccount(_ account: Account, userDbId: Int32) async
    func removeAccount(accountDbId: Int32) async
    func updateAccount(_ account: Account) async throws

    // MARK: - Drive

    func getDrive(driveDbId: Int32, accountDbId: Int32, userDbId: Int32) async -> Drive?
    func getDrive(driveDbId: Int32) async -> Drive?
    func addDrive(_ drive: Drive, accountDbId: Int32) async throws
    func removeDrive(driveDbId: Int32, accountDbId: Int32, userDbId: Int32) async
    func removeDrive(driveDbId: Int32) async throws
    func updateDrive(drive: Drive) async throws

    // MARK: - Synchro

    func getSynchro(synchroDbId: Int32, driveDbId: Int32, accountDbId: Int32, userDbId: Int32) async -> Synchro?
    func getSynchro(synchroDbId: Int32) async -> Synchro?
    func addSynchro(_ synchro: Synchro) async throws
    func removeSynchro(synchroDbId: Int32, driveDbId: Int32) async throws
    func updateSynchro(_ synchro: Synchro) async throws

    // MARK: - Cleanup

    func clearOnServerRestart() async
}
