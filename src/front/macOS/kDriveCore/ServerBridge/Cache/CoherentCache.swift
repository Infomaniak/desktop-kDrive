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

import Combine
import Foundation

public protocol CoherentCacheObservable: Sendable {
    var usersPublisher: AnyPublisher<IndexedUsers, Never> { get }
    var accountsPublisher: AnyPublisher<UserAccounts, Never> { get }
}

/// Structure always follow this nested model: User → Account → Drive → Synchro
public protocol CoherentCache: Sendable {
    // MARK: - User

    func getUser(apiId: Int32) async -> User?
    func getUser(dbId: Int32) async -> User?
    func getFirstAvailableUser() async -> User?
    func addUser(_ user: User) async
    func removeUser(dbId: Int32) async
    func updateUser(_ user: User) async
    func updateAvailableDrives(_ drives: [AvailableDrive], forUserDbId accountId: Int32) async

    // MARK: - Account

    func getAccount(accountDbId: Int32, userDbId: Int32) async -> Account?
    func addAccount(_ account: Account, userDbId: Int32) async
    func removeAccount(accountDbId: Int32, userDbId: Int32) async
    func updateAccount(_ account: Account) async

    // MARK: - Drive

    func getDrive(_ driveDbId: Int32, accountDbId: Int32, userDbId: Int32) async -> Drive?
    func getDrive(_ driveDbId: Int32) async -> Drive?
    func addDrive(_ drive: Drive, toAccount accountDbId: Int32, userDbId: Int32) async
    func removeDrive(_ driveDbId: Int32, fromAccount accountDbId: Int32, userDbId: Int32) async
    func updateDrive(drive: Drive) async

    // MARK: - Synchro

    func getSynchro(_ synchroId: Int32, driveDbId: Int32, accountDbId: Int32, userId: Int32) async -> Synchro?
    func addSynchro(_ synchro: Synchro, toDrive driveDbId: Int32, accountDbId: Int32, userId: Int32) async
    func removeSynchro(_ synchroId: Int32, fromDrive driveDbId: Int32, accountDbId: Int32, userId: Int32) async
    func updateSynchro(_ synchro: Synchro) async

    // MARK: - Cleanup

    func clearOnServerRestart() async
}

/// This cache must track 1:1 the server, can only be purged on server restart
public actor ServerCoherentCache: CoherentCache, CoherentCacheObservable {
    private var users: IndexedUsers = [:]

    private nonisolated let usersSubject = PassthroughSubject<IndexedUsers, Never>()
    private nonisolated let accountsSubject = PassthroughSubject<UserAccounts, Never>()

    public nonisolated var usersPublisher: AnyPublisher<IndexedUsers, Never> {
        usersSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public nonisolated var accountsPublisher: AnyPublisher<UserAccounts, Never> {
        accountsSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public init() {}

    // MARK: - USER

    public func getUser(dbId: Int32) -> User? {
        users[dbId]
    }

    public func getUser(apiId: Int32) -> User? {
        users.values.first { $0.userId == apiId }
    }

    public func getFirstAvailableUser() -> User? {
        users.first?.value
    }

    public func addUser(_ user: User) {
        users[user.dbId] = user
        notifyUserUpdate(dbId: user.dbId, indexedAccounts: user.accounts)
    }

    public func removeUser(dbId: Int32) {
        users.removeValue(forKey: dbId)
        notifyUserUpdate(dbId: dbId, indexedAccounts: [:])
        // TODO: Make sure to cascade User → Account → Drive → Synchro notifications on deletion
    }

    public func updateUser(_ user: User) {
        if let existingUser = users[user.dbId],
           let updatedUser = existingUser.updated(with: user) {
            // TODO: fix merge `accounts` and `availableDrives`
            users[user.dbId] = updatedUser
        } else {
            users[user.dbId] = user
        }

        notifyUserUpdate(dbId: user.dbId, indexedAccounts: user.accounts)
    }

    public func updateAvailableDrives(_ drives: [AvailableDrive], forUserDbId userDbId: Int32) {
        // TODO: Implement
    }

    // MARK: - ACCOUNT

    public func getAccount(accountDbId: Int32, userDbId: Int32) -> Account? {
        users[userDbId]?.accounts[accountDbId]
    }

    public func addAccount(_ account: Account, userDbId: Int32) {
        guard var user = users[userDbId] else { return }
        user.accounts[account.id] = account
        users[userDbId] = user

        notifyAccountUpdate(userDbId: userDbId, indexedAccounts: user.accounts)
    }

    public func removeAccount(accountDbId: Int32, userDbId: Int32) {
        guard var user = users[userDbId] else { return }
        user.accounts.removeValue(forKey: accountDbId)
        users[userDbId] = user

        notifyAccountUpdate(userDbId: userDbId, indexedAccounts: user.accounts)
    }

    public func updateAccount(_ account: Account) {
        // TODO: Implement
    }

    // MARK: - DRIVE

    public func getDrive(_ driveDbId: Int32, accountDbId: Int32, userDbId: Int32) -> Drive? {
        users[userDbId]?.accounts[accountDbId]?.drives[driveDbId]
    }

    public func getDrive(_ driveDbId: Int32) -> Drive? {
        for user in users.values {
            for account in user.accounts.values {
                if let drive = account.drives[driveDbId] {
                    return drive
                }
            }
        }
        return nil
    }

    public func addDrive(_ drive: Drive, toAccount accountDbId: Int32, userDbId: Int32) {
        guard var user = users[userDbId],
              var account = user.accounts[accountDbId]
        else { return }

        account.drives[drive.id] = drive
        user.accounts[accountDbId] = account
        users[userDbId] = user
    }

    public func removeDrive(_ driveDbId: Int32, fromAccount accountDbId: Int32, userDbId: Int32) {
        guard var user = users[userDbId],
              var account = user.accounts[accountDbId]
        else { return }

        account.drives.removeValue(forKey: driveDbId)
        user.accounts[accountDbId] = account
        users[userDbId] = user
    }

    public func updateDrive(drive: Drive) {
        let userDbId = drive.userDbId
        let accountId = drive.accountId

        guard var user = users[userDbId],
              var account = user.accounts[accountId]
        else { return }

        var indexedDrives = account.drives
        indexedDrives[drive.id] = drive

        account.drives = indexedDrives
        user.accounts[accountId] = account
        users[userDbId] = user

        // TODO: Observation
    }

    // MARK: - SYNCHRO

    public func getSynchro(_ synchroId: Int32, driveDbId: Int32, accountDbId: Int32, userId: Int32) -> Synchro? {
        users[userId]?
            .accounts[accountDbId]?
            .drives[driveDbId]?
            .synchros[synchroId]
    }

    public func addSynchro(_ synchro: Synchro, toDrive driveDbId: Int32, accountDbId: Int32, userId: Int32) {
        guard var user = users[userId],
              var account = user.accounts[accountDbId],
              var drive = account.drives[driveDbId]
        else { return }

        drive.synchros[synchro.id] = synchro
        account.drives[driveDbId] = drive
        user.accounts[accountDbId] = account
        users[userId] = user
    }

    public func removeSynchro(_ synchroId: Int32, fromDrive driveDbId: Int32, accountDbId: Int32, userId: Int32) {
        guard var user = users[userId],
              var account = user.accounts[accountDbId],
              var drive = account.drives[driveDbId]
        else { return }

        drive.synchros.removeValue(forKey: synchroId)
        account.drives[driveDbId] = drive
        user.accounts[accountDbId] = account
        users[userId] = user
    }

    public func updateSynchro(_ synchro: Synchro) {
        // TODO: Implement update, lookup on DriveDbId
    }

    // MARK: - Observation

    private func notifyUserUpdate(dbId: Int32, indexedAccounts: IndexedAccounts) {
        usersSubject.send(users)
        notifyAccountUpdate(userDbId: dbId, indexedAccounts: indexedAccounts)
    }

    private func notifyAccountUpdate(userDbId: Int32, indexedAccounts: IndexedAccounts) {
        let userAccounts = UserAccounts(userDbId: userDbId, indexedAccounts: indexedAccounts)
        accountsSubject.send(userAccounts)
    }

    // MARK: - Cleanup

    public func clearOnServerRestart() {
        users = [:]
    }
}
