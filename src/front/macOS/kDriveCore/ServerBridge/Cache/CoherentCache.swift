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

/// Structure always follow this nested model: User → Account → Drive → Synchro
public protocol CoherentCacheProtocol: Sendable {
    // MARK: - User

    func getUser(_ id: Int32) async -> User?
    func getFirstAvailableUser() async -> User?
    func addUser(_ user: User) async
    func removeUser(_ id: Int32) async
    func updateUser(_ user: User) async

    // MARK: - Account

    func getAccount(_ accountId: Int32, forUser userId: Int32) async -> Account?
    func addAccount(_ account: Account, toUser userId: Int32) async
    func removeAccount(_ accountId: Int32, fromUser userId: Int32) async

    // MARK: - Drive

    func getDrive(_ driveId: Int32, accountId: Int32, userId: Int32) async -> Drive?
    func addDrive(_ drive: Drive, toAccount accountId: Int32, userId: Int32) async
    func removeDrive(_ driveId: Int32, fromAccount accountId: Int32, userId: Int32) async

    // MARK: - Synchro

    func getSynchro(_ syncroId: Int32, driveId: Int32, accountId: Int32, userId: Int32) async -> Synchro?
    func addSynchro(_ syncro: Synchro, toDrive driveId: Int32, accountId: Int32, userId: Int32) async
    func removeSynchro(_ syncroId: Int32, fromDrive driveId: Int32, accountId: Int32, userId: Int32) async

    // MARK: - Cleanup

    func clearOnServerRestart() async
}

public typealias IndexedUsers = [Int32: User]

/// This cache must track 1:1 the server, can only be purged on server restart
public actor CoherentCache: CoherentCacheProtocol {
    private var users: IndexedUsers = [:]

    private nonisolated let usersSubject = PassthroughSubject<IndexedUsers, Never>()

    public nonisolated var usersPublisher: AnyPublisher<IndexedUsers, Never> {
        usersSubject
            .subscribe(on: DispatchQueue.global(qos: .background))
            .eraseToAnyPublisher()
    }

    public init() {}

    // MARK: - USER

    public func getUser(_ id: Int32) -> User? {
        guard let user = users[id] else {
            IKLogger.cache.info("user miss")
            return nil
        }
        IKLogger.cache.info("user hit")
        return user
    }

    public func getFirstAvailableUser() -> User? {
        users.first?.value
    }

    public func addUser(_ user: User) {
        users[user.id] = user
        usersSubject.send(users)
    }

    public func removeUser(_ id: Int32) {
        users.removeValue(forKey: id)
        usersSubject.send(users)
    }

    public func updateUser(_ user: User) {
        IKLogger.cache.info("updateUser \(user)")
        // TODO: Diff merge, not swap
        users[user.dbId] = user
        usersSubject.send(users)
    }

    // MARK: - ACCOUNT

    public func getAccount(_ accountId: Int32, forUser userId: Int32) -> Account? {
        users[userId]?.accounts[accountId]
    }

    public func addAccount(_ account: Account, toUser userId: Int32) {
        guard var user = users[userId] else { return }
        user.accounts[account.id] = account
        users[userId] = user
    }

    public func removeAccount(_ accountId: Int32, fromUser userId: Int32) {
        guard var user = users[userId] else { return }
        user.accounts.removeValue(forKey: accountId)
        users[userId] = user
    }

    // MARK: - DRIVE

    public func getDrive(_ driveId: Int32, accountId: Int32, userId: Int32) -> Drive? {
        users[userId]?.accounts[accountId]?.drives[driveId]
    }

    public func addDrive(_ drive: Drive, toAccount accountId: Int32, userId: Int32) {
        guard var user = users[userId],
              var account = user.accounts[accountId]
        else { return }

        account.drives[drive.id] = drive
        user.accounts[accountId] = account
        users[userId] = user
    }

    public func removeDrive(_ driveId: Int32, fromAccount accountId: Int32, userId: Int32) {
        guard var user = users[userId],
              var account = user.accounts[accountId]
        else { return }

        account.drives.removeValue(forKey: driveId)
        user.accounts[accountId] = account
        users[userId] = user
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

    // MARK: - Clenup

    public func clearOnServerRestart() {
        users = [:]
    }
}
