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

import Cocoa
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources

final class AppReadyViewController: OnboardingStepViewController {
    @LazyInjectService private var windowRouter: WindowRouter

    override init(nibName nibNameOrNil: NSNib.Name?, bundle nibBundleOrNil: Bundle?) {
        super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
        setupUI()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupUI()
    }

    private func setupUI() {
        titleLabel.stringValue = KDriveLocalizable.onboardingSynchronizationAppReadyTitle
        descriptionLabel.stringValue = KDriveLocalizable.onboardingSynchronizationAppReadyDescription

        primaryButton.isHidden = false
        primaryButton.title = KDriveLocalizable.buttonOpenKDrive
        primaryButton.target = self
        primaryButton.action = #selector(didTapOpenApp)
        secondaryButton.isHidden = true
    }

    @objc private func didTapOpenApp() {
        windowRouter.navigate(to: .splitView)
        UserDefaults.standard.shouldPresentOnboarding = false
    }
}
