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
import InfomaniakDI
import kDriveCore
import kDriveCoreUI

public protocol SettingsObserving: Sendable {
    var settings: UIParametersInfo { get }
    var settingsPublisher: AnyPublisher<UIParametersInfo, Never> { get }
}

/// Observes the `SettingsCache` and republishes its content as a UI model (`UIParametersInfo`),
/// mirroring the observation pattern used for the coherent server cache (e.g. `UISynchroStateObserver`).
public final class SettingsObserver: SettingsObserving {
    @MainActor public private(set) var settings = UIParametersInfo() {
        didSet {
            settingsSubject.send(settings)
        }
    }

    @MainActor private let settingsSubject = PassthroughSubject<UIParametersInfo, Never>()
    @MainActor public var settingsPublisher: AnyPublisher<UIParametersInfo, Never> {
        settingsSubject.eraseToAnyPublisher()
    }

    @MainActor private var cancellable: AnyCancellable?

    public init() {
        Task { @MainActor [weak self] in
            self?.observeSettings()
        }
    }

    deinit {
        Task { @MainActor [weak self] in
            self?.cancellable?.cancel()
        }
    }

    @MainActor
    private func observeSettings() {
        Task {
            @InjectService var cache: SettingsCaching
            if let currentSettings = await cache.getSettings() {
                settings = UIParametersInfo(parametersInfo: currentSettings)
            }

            @InjectService var cacheObservable: SettingsCacheObservable
            cancellable = cacheObservable.settingsPublisher
                .map { UIParametersInfo(parametersInfo: $0) }
                .removeDuplicates()
                .receive(on: RunLoop.main)
                .sink { [weak self] output in
                    self?.settings = output
                }
        }
    }
}
