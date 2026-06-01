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

import Combine
import CppInterop
import Foundation
import OrderedCollections

public protocol CoherentCacheObservable: Sendable {
    var usersPublisher: AnyPublisher<IndexedUsers, Never> { get }
}

/// This cache must track 1:1 the server, can only be purged on server restart
public actor ServerCoherentCache: CoherentCache, CoherentCacheObservable {
    var users: IndexedUsers = [:]
    var serverErrors: IndexedErrors = [:]

    private nonisolated let usersSubject = PassthroughSubject<IndexedUsers, Never>()
    private nonisolated let serverErrorsSubject = PassthroughSubject<IndexedErrors, Never>()

    public nonisolated var usersPublisher: AnyPublisher<IndexedUsers, Never> {
        usersSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public nonisolated var serverErrorsPublisher: AnyPublisher<IndexedErrors, Never> {
        serverErrorsSubject
            .subscribe(on: DispatchQueue.global(qos: .userInitiated))
            .eraseToAnyPublisher()
    }

    public enum CacheError: Error {
        case userNotFound(_ dbId: Int32)
        case accountNotFound(_ dbId: Int32)
        case driveNotFound(_ dbId: Int32)
        case synchroNotFound(_ dbId: Int32)
        case errorNotFound(_ dbId: Int32)
        case notAServerError
    }

    public init() {}

    // MARK: - USER

    public func getUsers() async -> OrderedDictionary<Int32, User> {
        return users
    }

    public func getUser(dbId: Int32) -> User? {
        users[dbId]
    }

    public func getUser(apiId: Int32) -> User? {
        users.values.first { $0.userId == apiId }
    }

    public func getFirstAvailableUser() -> User? {
        users.values.first
    }

    public func addUser(_ user: User) {
        users[user.dbId] = user
        notifyUpdate()
    }

    public func removeUser(dbId: Int32) {
        users.removeValue(forKey: dbId)
        notifyUpdate()
    }

    public func updateUser(_ user: User, updateOptions: User.UpdateOptions) {
        if let existingUser = users[user.dbId],
           let updatedUser = existingUser.updated(with: user, updateOptions: updateOptions) {
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

        let indexedDrives = IndexedAvailableDrives(
            uniqueKeysWithValues: drives.map { ($0.driveId, $0) }
        )

        user.availableDrives = indexedDrives

        updateUser(user, updateOptions: .availableDrives)
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

    public func addOrUpdateAccount(_ account: Account) throws {
        guard var user = users[account.userDbId] else {
            throw CacheError.accountNotFound(account.dbId)
        }
        user.accounts[account.dbId] = account
        users[user.dbId] = user

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

        account.drives[drive.driveDbId] = drive
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

    public func removeDrive(driveDbId: Int32) throws {
        for user in users.values {
            for var account in user.accounts.values {
                guard account.drives.removeValue(forKey: driveDbId) != nil else {
                    continue
                }

                try addOrUpdateAccount(account)
                return
            }
        }

        throw CacheError.driveNotFound(driveDbId)
    }

    public func updateDrive(drive: Drive) throws {
        let accountDbId = drive.accountDbId
        guard var account = getAccount(accountDbId: accountDbId) else {
            throw CacheError.accountNotFound(accountDbId)
        }

        var indexedDrives = account.drives
        indexedDrives[drive.driveDbId] = drive
        account.drives = indexedDrives

        try addOrUpdateAccount(account)
    }

    // MARK: - AVAILABLE DRIVE

    public func getAvailableDrive(driveDb: Int32, userDbId: Int32) async -> AvailableDrive? {
        users[userDbId]?.availableDrives[driveDb]
    }

    public func getAvailableDrive(driveDb: Int32) async -> AvailableDrive? {
        for user in users.values {
            if let availableDrive = user.availableDrives[driveDb] {
                return availableDrive
            }
        }
        return nil
    }

    // MARK: - SYNCHRO

    public func getSynchro(synchroDbId: Int32, driveDbId: Int32, accountDbId: Int32, userDbId: Int32) -> Synchro? {
        users[userDbId]?
            .accounts[accountDbId]?
            .drives[driveDbId]?
            .synchros[synchroDbId]
    }

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

    public func addSynchro(_ synchro: Synchro) throws {
        let driveDbId = synchro.driveDbId
        guard var drive = getDrive(driveDbId: driveDbId) else {
            throw CacheError.driveNotFound(driveDbId)
        }

        drive.synchros[synchro.dbId] = synchro
        try updateDrive(drive: drive)
    }

    public func removeSynchro(synchroDbId: Int32, driveDbId: Int32) throws {
        guard var drive = getDrive(driveDbId: driveDbId) else {
            throw CacheError.driveNotFound(driveDbId)
        }

        guard drive.synchros.removeValue(forKey: synchroDbId) != nil else {
            throw CacheError.synchroNotFound(synchroDbId)
        }

        try updateDrive(drive: drive)
    }

    public func removeSynchro(synchroDbId: Int32) throws {
        for user in users.values {
            for account in user.accounts.values {
                for var drive in account.drives.values {
                    guard drive.synchros.removeValue(forKey: synchroDbId) != nil else {
                        continue
                    }

                    try updateDrive(drive: drive)
                    return
                }
            }
        }

        throw CacheError.synchroNotFound(synchroDbId)
    }

    public func updateSynchro(_ synchro: Synchro) throws {
        guard var drive = getDrive(driveDbId: synchro.driveDbId) else {
            throw CacheError.driveNotFound(synchro.driveDbId)
        }

        drive.synchros[synchro.dbId] = synchro
        try updateDrive(drive: drive)
    }

    // MARK: - SynchroContexts

    public func getSynchroContexts() -> [SynchroContext] {
        var synchroContexts = [SynchroContext]()
        for user in users.values {
            for account in user.accounts.values {
                for drive in account.drives.values {
                    for synchro in drive.synchros.values {
                        synchroContexts.append(SynchroContext(synchro: synchro, drive: drive, account: account, user: user))
                    }
                }
            }
        }

        return synchroContexts
    }

    public func getSynchroContext(_ synchroDbId: Int32) -> SynchroContext? {
        for user in users.values {
            for account in user.accounts.values {
                for drive in account.drives.values {
                    if let synchro = drive.synchros[synchroDbId] {
                        return SynchroContext(synchro: synchro, drive: drive, account: account, user: user)
                    }
                }
            }
        }

        return nil
    }

    // MARK: - SynchroNodeContexts

    public func getSynchroNodeContexts(_ synchroDbId: Int32) -> [SynchroNodeContext] {
        guard let synchroContext = getSynchroContext(synchroDbId) else {
            return []
        }

        var synchroNodeContexts = [SynchroNodeContext]()
        for node in synchroContext.synchro.synchNodes.values {
            synchroNodeContexts.append(SynchroNodeContext(
                node: node,
                synchro: synchroContext.synchro,
                drive: synchroContext.drive,
                account: synchroContext.account,
                user: synchroContext.user
            ))
        }

        return synchroNodeContexts
    }

    // MARK: - Errors

    public func addOrUpdateError(_ error: ErrorInfo) async throws {
        if error.level == .Server {
            try await addOrUpdateServerError(error)
            return
        }

        guard var synchro = getSynchro(synchroDbId: error.synchroDbId) else {
            throw ServerCoherentCache.CacheError.synchroNotFound(error.synchroDbId)
        }

        synchro.errors[error.dbId] = error
        if synchro.latestError == nil {
            synchro.latestError = SynchroError(errorInfo: error)
        }

        try updateSynchro(synchro)
    }

    private func addOrUpdateServerError(_ error: ErrorInfo) async throws {
        guard error.level == .Server else {
            throw CacheError.notAServerError
        }

        serverErrors[error.dbId] = error
        notifyServerErrorsUpdate()
    }

    public func updateErrors(_ errors: [ErrorInfo]) async throws {
        serverErrors.removeAll()

        for error in errors {
            if error.level == .Server {
                try await addOrUpdateServerError(error)
            } else {
                guard var synchro = getSynchro(synchroDbId: error.synchroDbId) else {
                    continue
                }
                synchro.errors[error.dbId] = error
                if synchro.latestError == nil {
                    synchro.latestError = SynchroError(errorInfo: error)
                }
                try? updateSynchro(synchro)
            }
        }
    }

    public func removeError(_ errorDbId: Int32) async throws {
        guard try await removeServerError(errorDbId) == nil else {
            return
        }

        for user in users.values {
            for account in user.accounts.values {
                for drive in account.drives.values {
                    for var synchro in drive.synchros.values {
                        guard synchro.errors.removeValue(forKey: errorDbId) != nil else {
                            continue
                        }

                        if let remainingError = synchro.errors.values.first {
                            synchro.latestError = SynchroError(errorInfo: remainingError)
                        } else {
                            synchro.latestError = nil
                        }

                        try updateSynchro(synchro)
                        return
                    }
                }
            }
        }

        throw CacheError.errorNotFound(errorDbId)
    }

    @discardableResult
    private func removeServerError(_ errorDbId: Int32) async throws -> ErrorInfo? {
        guard let errorInfo = serverErrors.removeValue(forKey: errorDbId) else {
            return nil
        }

        notifyServerErrorsUpdate()
        return errorInfo
    }

    public func clearErrors() async {
        if !serverErrors.isEmpty {
            serverErrors.removeAll()
            notifyServerErrorsUpdate()
        }

        for user in users.values {
            for account in user.accounts.values {
                for drive in account.drives.values {
                    for var synchro in drive.synchros.values {
                        synchro.errors.removeAll()
                        synchro.latestError = nil
                        try? updateSynchro(synchro)
                    }
                }
            }
        }
    }

    // MARK: - Observation

    private func notifyUpdate() {
        usersSubject.send(users)
    }

    private func notifyServerErrorsUpdate() {
        serverErrorsSubject.send(serverErrors)
    }

    // MARK: - Management

    public func refresh() async throws {
        try await UserJobs().userInfoList()
        try await AccountJobs().accountInfoList()
        try await DriveJobs().driveInfoList()
        try await SyncJobs().availableSync()
        try await ErrorJobs().errorInfoList()
    }

    public func clearAndRefresh() async throws {
        users = [:]
        await clearErrors()
        try await refresh()
    }
}
