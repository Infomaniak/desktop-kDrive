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
import OrderedCollections

// trigger mac test

/// Structure always follow this nested model: User → Account → Drive → Synchro
public protocol CoherentCache: Sendable {
    // MARK: - User

    func getUsers() async -> OrderedDictionary<Int32, User>
    func getUser(apiId: Int32) async -> User?
    func getUser(dbId: Int32) async -> User?
    func getFirstAvailableUser() async -> User?
    func addUser(_ user: User) async
    func removeUser(dbId: Int32) async
    func updateUser(_ user: User, updateOptions: User.UpdateOptions) async
    func updateAvailableDrives(_ drives: [AvailableDrive], forUserDbId accountId: Int32) async throws

    // MARK: - Account

    func getAccount(accountDbId: Int32, userDbId: Int32) async -> Account?
    func getAccount(accountDbId: Int32) async -> Account?
    func addOrUpdateAccount(_ account: Account) async throws
    func removeAccount(accountDbId: Int32) async

    // MARK: - Drive

    func getDrive(driveDbId: Int32, accountDbId: Int32, userDbId: Int32) async -> Drive?
    func getDrive(driveDbId: Int32) async -> Drive?
    func addDrive(_ drive: Drive, accountDbId: Int32) async throws
    func removeDrive(driveDbId: Int32, accountDbId: Int32, userDbId: Int32) async
    func removeDrive(driveDbId: Int32) async throws
    func updateDrive(drive: Drive) async throws

    // MARK: - Available Drive

    func getAvailableDrive(driveDb: Int32, userDbId: Int32) async -> AvailableDrive?
    func getAvailableDrive(driveDb: Int32) async -> AvailableDrive?

    // MARK: - Synchro

    func getSynchro(synchroDbId: Int32, driveDbId: Int32, accountDbId: Int32, userDbId: Int32) async -> Synchro?
    func getSynchro(synchroDbId: Int32) async -> Synchro?
    func addSynchro(_ synchro: Synchro) async throws
    func removeSynchro(synchroDbId: Int32, driveDbId: Int32) async throws
    func removeSynchro(synchroDbId: Int32) async throws
    func updateSynchro(_ synchro: Synchro) async throws

    // MARK: - SynchroContexts

    func getSynchroContexts() async -> [SynchroContext]
    func getSynchroContext(_ synchroDbId: Int32) async -> SynchroContext?

    // MARK: - SynchroNodeContexts

    func getSynchroNodeContexts(_ synchroDbId: Int32) async -> [SynchroNodeContext]

    // MARK: - Errors

    func addOrUpdateError(_ error: ErrorInfo) async throws
    func removeError(_ errorDbId: Int32) async throws
    func clearErrors() async

    // MARK: - Management

    func refresh() async throws
    func clearAndRefresh() async throws
}
