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
import Combine
import kDriveCore
import kDriveCoreUI
import kDriveResources
import Lottie

class OnboardingAnimationsView: NSView {
    private let flowCoordinator: OnboardingFlowCoordinator

    private let animationView = ThemedAnimationView()

    private var bindStore = Set<AnyCancellable>()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        self.flowCoordinator = flowCoordinator
        super.init(frame: .zero)

        setupAnimationView()
        bindCoordinator()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func draw(_ dirtyRect: NSRect) {
        NSColor.Tokens.Surface.secondary.setFill()
        bounds.fill()
    }

    private func setupAnimationView() {
        animationView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(animationView)

        NSLayoutConstraint.activate([
            animationView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: AppPadding.padding32),
            trailingAnchor.constraint(equalTo: animationView.trailingAnchor, constant: AppPadding.padding32),
            animationView.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
    }

    private func bindCoordinator() {
        transitionAnimation(forStep: flowCoordinator.currentStep)
        flowCoordinator.$currentStep
            .receiveOnMain(store: &bindStore) { [weak self] step in
                self?.transitionAnimation(forStep: step)
            }
    }

    private func transitionAnimation(forStep step: OnboardingStep) {
        let themedAnimation = getThemedAnimation(forStep: step)
        loadAndPlayAnimation(themedAnimation)
    }

    private func loadAndPlayAnimation(_ animation: ThemedAnimation) {
        Task {
            try await animationView.loadAnimation(themedAnimation: animation)

            animationView.loopMode = .loop
            animationView.play()
        }
    }

    private func getThemedAnimation(forStep step: OnboardingStep) -> ThemedAnimation {
        switch step {
        case .login:
            return .kDriveLoader
        case .drivesSelection:
            return .kDriveSynchronizeFiles
        case .permissions:
            fatalError("Not Implemented Yet")
        case .synchronization:
            fatalError("Not Implemented Yet")
        }
    }
}
