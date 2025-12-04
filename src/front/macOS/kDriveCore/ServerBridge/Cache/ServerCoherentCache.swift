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
}

/// This cache must track 1:1 the server, can only be purged on server restart
public actor ServerCoherentCache: CoherentCache, CoherentCacheObservable {
    private var users: IndexedUsers = [:]

    private nonisolated let usersSubject = PassthroughSubject<IndexedUsers, Never>()

    public nonisolated var usersPublisher: AnyPublisher<IndexedUsers, Never> {
        usersSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    enum CacheError: Error {
        case userNotFound(_ dbId: Int32)
        case accountNotFound(_ dbId: Int32)
        case driveNotFound(_ dbId: Int32)
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
        notifyUpdate()
    }

    public func removeUser(dbId: Int32) {
        users.removeValue(forKey: dbId)
        notifyUpdate()
    }

    public func updateUser(_ user: User) {
        if let existingUser = users[user.dbId],
           let updatedUser = existingUser.updated(with: user) {
            users[user.dbId] = updatedUser
        } else {
            users[user.dbId] = user
        }

        notifyUpdate()
    }

    public func updateAvailableDrives(_ drives: [AvailableDrive], forUserDbId userDbId: Int32) throws {
        guard var user = getUser(dbId: userDbId) else {
            throw CacheError.userNotFound(userDbId)
        }

        let indexedDrives: IndexedAvailableDrives = Dictionary(uniqueKeysWithValues:
            drives.map { drive in (drive.driveId, drive) }
        )

        user.availableDrives = indexedDrives

        updateUser(user)
    }

    // MARK: - ACCOUNT

    public func getAccount(accountDbId: Int32, userDbId: Int32) -> Account? {
        users[userDbId]?.accounts[accountDbId]
    }

    public func getAccount(accountDbId: Int32) -> Account? {
        for user in users.values {
            if let account = user.accounts[accountDbId] {
                return account
            }
        }
        return nil
    }

    public func addAccount(_ account: Account, userDbId: Int32) {
        guard var user = users[userDbId] else { return }
        user.accounts[account.id] = account
        users[userDbId] = user

        notifyUpdate()
    }

    public func removeAccount(accountDbId: Int32) {
        for var user in users.values {
            guard user.accounts[accountDbId] != nil else {
                continue
            }

            user.accounts.removeValue(forKey: accountDbId)
            users[user.dbId] = user
        }

        notifyUpdate()
    }

    public func updateAccount(_ account: Account) throws {
        guard var user = users.values.first(where: { $0.accounts.keys.contains(account.dbId) }) else {
            throw CacheError.accountNotFound(account.dbId)
        }

        user.accounts[account.dbId] = account
        users[user.dbId] = user

        notifyUpdate()
    }

    // MARK: - DRIVE

    public func getDrive(driveDbId: Int32, accountDbId: Int32, userDbId: Int32) -> Drive? {
        users[userDbId]?.accounts[accountDbId]?.drives[driveDbId]
    }

    public func getDrive(driveDbId: Int32) -> Drive? {
        for user in users.values {
            for account in user.accounts.values {
                if let drive = account.drives[driveDbId] {
                    return drive
                }
            }
        }
        return nil
    }

    public func addDrive(_ drive: Drive, accountDbId: Int32) throws {
        let userDbId = drive.userDbId
        guard var user = users[userDbId] else {
            throw CacheError.userNotFound(userDbId)
        }
        guard var account = user.accounts[accountDbId] else {
            throw CacheError.accountNotFound(accountDbId)
        }

        account.drives[drive.id] = drive
        user.accounts[accountDbId] = account
        users[userDbId] = user

        notifyUpdate()
    }

    public func removeDrive(driveDbId: Int32, accountDbId: Int32, userDbId: Int32) {
        guard var user = users[userDbId],
              var account = user.accounts[accountDbId]
        else { return }

        account.drives.removeValue(forKey: driveDbId)
        user.accounts[accountDbId] = account
        users[userDbId] = user

        notifyUpdate()
    }

    public func updateDrive(drive: Drive) throws {
        let userDbId = drive.userDbId
        let accountId = drive.accountId

        guard var user = users[userDbId] else {
            throw CacheError.userNotFound(userDbId)
        }
        guard var account = user.accounts[accountId] else {
            throw CacheError.accountNotFound(accountId)
        }

        var indexedDrives = account.drives
        indexedDrives[drive.id] = drive

        account.drives = indexedDrives
        user.accounts[accountId] = account
        users[userDbId] = user

        notifyUpdate()
    }

    // MARK: - SYNCHRO

    public func getSynchro(synchroDbId: Int32, driveDbId: Int32, accountDbId: Int32, userDbId: Int32) -> Synchro? {
        users[userDbId]?
            .accounts[accountDbId]?
            .drives[driveDbId]?
            .synchros[synchroDbId]
    }

    // TODO: If synchro has driveDbId, reuse getDrive
    public func getSynchro(synchroDbId: Int32) -> Synchro? {
        for user in users.values {
            for account in user.accounts.values {
                for drive in account.drives.values {
                    if let synchro = drive.synchros[synchroDbId] {
                        return synchro
                    }
                }
            }
        }
        return nil
    }

    public func addSynchro(_ synchro: Synchro, toDrive driveDbId: Int32, accountDbId: Int32, userDbId: Int32) {
        guard var user = users[userDbId],
              var account = user.accounts[accountDbId],
              var drive = account.drives[driveDbId]
        else { return }

        drive.synchros[synchro.id] = synchro
        account.drives[driveDbId] = drive
        user.accounts[accountDbId] = account
        users[userDbId] = user

        notifyUpdate()
    }

    public func removeSynchro(synchroDbId: Int32, driveDbId: Int32, accountDbId: Int32, userDbId: Int32) {
        guard var user = users[userDbId],
              var account = user.accounts[accountDbId],
              var drive = account.drives[driveDbId]
        else { return }

        drive.synchros.removeValue(forKey: synchroDbId)
        account.drives[driveDbId] = drive
        user.accounts[accountDbId] = account
        users[userDbId] = user

        notifyUpdate()
    }

    public func updateSynchro(_ synchro: Synchro) throws {
        guard var drive = getDrive(driveDbId: synchro.driveDbId) else {
            throw CacheError.driveNotFound(synchro.driveDbId)
        }

        drive.synchros[synchro.dbId] = synchro
        try updateDrive(drive: drive)

        notifyUpdate()
    }

    // MARK: - Observation

    private func notifyUpdate() {
        usersSubject.send(users)
    }

    // MARK: - Cleanup

    public func clearOnServerRestart() {
        users = [:]
    }
}
