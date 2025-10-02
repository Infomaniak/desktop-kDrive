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
import Lottie

final class PreloadingViewController: NSViewController {
    private var animationView = LottieAnimationView()

    override func viewDidLoad() {
        super.viewDidLoad()

        setupView()
        loadAndPlayAnimation()
    }

    override func viewDidAppear() {
        super.viewDidAppear()

        configureWindowAppearance()
        preloadApp()
    }

    private func preloadApp() {
        Task {
            @InjectService var serverBridge: ServerBridgeable
            let hasUser = await serverBridge.getConnectedUser()

            @InjectService var windowRouter: WindowRouter
            if hasUser {
                windowRouter.navigate(to: .splitView)
            } else {
                windowRouter.navigate(to: .onboarding)
            }
        }
    }
}

// MARK: - Set up UI

extension PreloadingViewController {
    private func configureWindowAppearance() {
        guard let window = view.window else { return }
        window.titlebarAppearsTransparent = true
        window.isMovableByWindowBackground = true
    }

    private func setupView() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.surfaceSecondary.cgColor

        animationView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(animationView)

        let progressIndicator = NSProgressIndicator()
        progressIndicator.translatesAutoresizingMaskIntoConstraints = false
        progressIndicator.style = .spinning
        progressIndicator.isIndeterminate = true
        progressIndicator.controlSize = .small
        progressIndicator.startAnimation(nil)
        view.addSubview(progressIndicator)

        NSLayoutConstraint.activate([
            animationView.widthAnchor.constraint(lessThanOrEqualToConstant: 150),
            animationView.heightAnchor.constraint(lessThanOrEqualToConstant: 150),

            animationView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            animationView.centerYAnchor.constraint(equalTo: view.centerYAnchor),

            animationView.leadingAnchor.constraint(greaterThanOrEqualTo: view.leadingAnchor, constant: AppPadding.padding16),
            view.trailingAnchor.constraint(greaterThanOrEqualTo: animationView.trailingAnchor, constant: AppPadding.padding16),
            animationView.topAnchor.constraint(greaterThanOrEqualTo: view.topAnchor, constant: AppPadding.padding16),

            progressIndicator.topAnchor.constraint(equalTo: animationView.bottomAnchor, constant: AppPadding.padding48),
            progressIndicator.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            view.bottomAnchor.constraint(greaterThanOrEqualTo: progressIndicator.bottomAnchor, constant: AppPadding.padding16)
        ])
    }

    private func loadAndPlayAnimation() {
        Task {
            let dotLottieFile = try await DotLottieFile.loadedFromBundle(forResource: "kdrive-loader-light")
            animationView.loadAnimation(from: dotLottieFile)

            animationView.loopMode = .loop
            animationView.play()
        }
    }
}
