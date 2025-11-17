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

public protocol CoherentCacheObservation: Sendable {
    var usersPublisher: AnyPublisher<IndexedUsers, Never> { get }
    var accountsPublisher: AnyPublisher<UserAccounts, Never> { get }
}

/// Structure always follow this nested model: User → Account → Drive → Synchro
public protocol CoherentCacheProtocol: Sendable {
    // MARK: - User

    func getUser(apiId: Int32) async -> User?
    func getUser(dbId: Int32) async -> User?
    func getFirstAvailableUser() async -> User?
    func addUser(_ user: User) async
    func removeUser(dbId: Int32) async
    func updateUser(_ user: User) async

    // MARK: - Account

    func getAccount(_ accountId: Int32, userDbId: Int32) async -> Account?
    func addAccount(_ account: Account, userDbId: Int32) async
    func removeAccount(_ accountId: Int32, userDbId: Int32) async
    func updateAccount(_ account: Account) async

    // MARK: - Drive

    func getDrive(_ driveId: Int32, accountId: Int32, userDbId: Int32) async -> Drive?
    func addDrive(_ drive: Drive, toAccount accountId: Int32, userDbId: Int32) async
    func removeDrive(_ driveId: Int32, fromAccount accountId: Int32, userDbId: Int32) async
    func updateDrive(drive: Drive) async

    // MARK: - Synchro

    func getSynchro(_ syncroId: Int32, driveId: Int32, accountId: Int32, userId: Int32) async -> Synchro?
    func addSynchro(_ syncro: Synchro, toDrive driveId: Int32, accountId: Int32, userId: Int32) async
    func removeSynchro(_ syncroId: Int32, fromDrive driveId: Int32, accountId: Int32, userId: Int32) async

    // MARK: - Cleanup

    func clearOnServerRestart() async
}

/// This cache must track 1:1 the server, can only be purged on server restart
public actor CoherentCache: CoherentCacheProtocol, CoherentCacheObservation {
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
    }

    public func updateUser(_ user: User) {
        if let existingUser = users[user.id],
           let updatedUser = existingUser.updated(with: user) {
            users[user.id] = updatedUser
        } else {
            users[user.id] = user
        }

        notifyUserUpdate(dbId: user.userId, indexedAccounts: user.accounts)
    }

    // MARK: - ACCOUNT

    public func getAccount(_ accountId: Int32, userDbId: Int32) -> Account? {
        users[userDbId]?.accounts[accountId]
    }

    public func addAccount(_ account: Account, userDbId: Int32) {
        guard var user = users[userDbId] else { return }
        user.accounts[account.id] = account
        users[userDbId] = user

        notifyAccountUpdate(userDbId: userDbId, indexedAccounts: user.accounts)
    }

    public func removeAccount(_ accountId: Int32, userDbId: Int32) {
        guard var user = users[userDbId] else { return }
        user.accounts.removeValue(forKey: accountId)
        users[userDbId] = user

        notifyAccountUpdate(userDbId: userDbId, indexedAccounts: user.accounts)
    }

    public func updateAccount(_ account: Account) {
        // TODO: Implement
    }

    // MARK: - DRIVE

    public func getDrive(_ driveId: Int32, accountId: Int32, userDbId: Int32) -> Drive? {
        users[userDbId]?.accounts[accountId]?.drives[driveId]
    }

    public func addDrive(_ drive: Drive, toAccount accountId: Int32, userDbId: Int32) {
        guard var user = users[userDbId],
              var account = user.accounts[accountId]
        else { return }

        account.drives[drive.id] = drive
        user.accounts[accountId] = account
        users[userDbId] = user
    }

    public func removeDrive(_ driveId: Int32, fromAccount accountId: Int32, userDbId: Int32) {
        guard var user = users[userDbId],
              var account = user.accounts[accountId]
        else { return }

        account.drives.removeValue(forKey: driveId)
        user.accounts[accountId] = account
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

    // MARK: - SYNCRO

    public func getSynchro(_ syncroId: Int32, driveId: Int32, accountId: Int32, userId: Int32) -> Synchro? {
        users[userId]?
            .accounts[accountId]?
            .drives[driveId]?
            .syncros[syncroId]
    }

    public func addSynchro(_ syncro: Synchro, toDrive driveId: Int32, accountId: Int32, userId: Int32) {
        guard var user = users[userId],
              var account = user.accounts[accountId],
              var drive = account.drives[driveId]
        else { return }

        drive.syncros[syncro.id] = syncro
        account.drives[driveId] = drive
        user.accounts[accountId] = account
        users[userId] = user
    }

    public func removeSynchro(_ syncroId: Int32, fromDrive driveId: Int32, accountId: Int32, userId: Int32) {
        guard var user = users[userId],
              var account = user.accounts[accountId],
              var drive = account.drives[driveId]
        else { return }

        drive.syncros.removeValue(forKey: syncroId)
        account.drives[driveId] = drive
        user.accounts[accountId] = account
        users[userId] = user
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
