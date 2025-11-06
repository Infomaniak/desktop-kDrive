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

public struct User: Identifiable, Hashable, Sendable {
    /// usedDbId
    public let id: Int
    public var name: String
    public var accounts: [Int: Account]
}

public struct Account: Identifiable, Hashable, Sendable {
    public let id: Int
    public var name: String
    public var drives: [Int: Drive]
}

public struct Drive: Identifiable, Hashable, Sendable {
    public let id: Int
    public var name: String
    public var syncros: [Int: Syncro]
}

public struct Syncro: Identifiable, Hashable, Sendable {
    public let id: Int
    public var name: String
}

/// Structure always follow this nested model: User → Account → Drive → Syncro
public protocol CoherentCacheProtocol: Sendable {
    // MARK: - User

    func getUser(_ id: Int) async -> User?
    func getFirstAvailableUser() async -> User?
    func addUser(_ user: User) async
    func removeUser(_ id: Int) async

    // MARK: - Account

    func getAccount(_ accountId: Int, forUser userId: Int) async -> Account?
    func addAccount(_ account: Account, toUser userId: Int) async
    func removeAccount(_ accountId: Int, fromUser userId: Int) async

    // MARK: - Drive

    func getDrive(_ driveId: Int, accountId: Int, userId: Int) async -> Drive?
    func addDrive(_ drive: Drive, toAccount accountId: Int, userId: Int) async
    func removeDrive(_ driveId: Int, fromAccount accountId: Int, userId: Int) async

    // MARK: - Syncro

    func getSyncro(_ syncroId: Int, driveId: Int, accountId: Int, userId: Int) async -> Syncro?
    func addSyncro(_ syncro: Syncro, toDrive driveId: Int, accountId: Int, userId: Int) async
    func removeSyncro(_ syncroId: Int, fromDrive driveId: Int, accountId: Int, userId: Int) async

    // MARK: - Cleanup

    func clearOnServerRestart() async
}

/// This cache must track 1:1 the server, can only be purged on server restart
actor CoherentCache: CoherentCacheProtocol {
    private var users: [Int: User] = [:]

    // MARK: - USER

    func getUser(_ id: Int) -> User? {
        guard let user = users[id] else {
            IKLogger.cache.info("user miss")
            return nil
        }
        IKLogger.cache.info("user hit")
        return user
    }

    func getFirstAvailableUser() -> User? {
        users.first?.value
    }

    func addUser(_ user: User) {
        users[user.id] = user
    }

    func removeUser(_ id: Int) {
        users.removeValue(forKey: id)
    }

    // MARK: - ACCOUNT

    func getAccount(_ accountId: Int, forUser userId: Int) -> Account? {
        users[userId]?.accounts[accountId]
    }

    func addAccount(_ account: Account, toUser userId: Int) {
        guard var user = users[userId] else { return }
        user.accounts[account.id] = account
        users[userId] = user
    }

    func removeAccount(_ accountId: Int, fromUser userId: Int) {
        guard var user = users[userId] else { return }
        user.accounts.removeValue(forKey: accountId)
        users[userId] = user
    }

    // MARK: - DRIVE

    func getDrive(_ driveId: Int, accountId: Int, userId: Int) -> Drive? {
        users[userId]?.accounts[accountId]?.drives[driveId]
    }

    func addDrive(_ drive: Drive, toAccount accountId: Int, userId: Int) {
        guard var user = users[userId],
              var account = user.accounts[accountId]
        else { return }

        account.drives[drive.id] = drive
        user.accounts[accountId] = account
        users[userId] = user
    }

    func removeDrive(_ driveId: Int, fromAccount accountId: Int, userId: Int) {
        guard var user = users[userId],
              var account = user.accounts[accountId]
        else { return }

        account.drives.removeValue(forKey: driveId)
        user.accounts[accountId] = account
        users[userId] = user
    }

    // MARK: - SYNCRO

    func getSyncro(_ syncroId: Int, driveId: Int, accountId: Int, userId: Int) -> Syncro? {
        users[userId]?
            .accounts[accountId]?
            .drives[driveId]?
            .syncros[syncroId]
    }

    func addSyncro(_ syncro: Syncro, toDrive driveId: Int, accountId: Int, userId: Int) {
        guard var user = users[userId],
              var account = user.accounts[accountId],
              var drive = account.drives[driveId]
        else { return }

        drive.syncros[syncro.id] = syncro
        account.drives[driveId] = drive
        user.accounts[accountId] = account
        users[userId] = user
    }

    func removeSyncro(_ syncroId: Int, fromDrive driveId: Int, accountId: Int, userId: Int) {
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

    func clearOnServerRestart() {
        users = [:]
    }
}
