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
import kDriveCoreUI
import Lottie

class OnboardingAnimationsView: NSView {
    private let viewModel: OnboardingViewModel

    private let animationView = ThemedAnimationView()

    private var bindStore = Set<AnyCancellable>()

    init(viewModel: OnboardingViewModel) {
        self.viewModel = viewModel
        super.init(frame: .zero)

        wantsLayer = true
        layer?.backgroundColor = NSColor.Tokens.Surface.secondary.cgColor

        setupAnimationView()
        bindViewModel()
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
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

    private func bindViewModel() {
        transitionAnimation(forStep: viewModel.currentStep)
        viewModel.$currentStep.receive(on: DispatchQueue.main)
            .sink { [weak self] step in
                self?.transitionAnimation(forStep: step)
            }
            .store(in: &bindStore)
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
        case .driveSelection:
            fatalError("Not Implemented Yet")
        case .autorisations:
            fatalError("Not Implemented Yet")
        case .synchronisation:
            fatalError("Not Implemented Yet")
        }
    }
}
