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
import kDriveCoreUI
import kDriveResources
import Lottie

final class PreloadingViewController: NSViewController {
    private lazy var animationView: NSThemedAnimationView = {
        let view = NSThemedAnimationView()
        view.translatesAutoresizingMaskIntoConstraints = false
        return view
    }()

    private lazy var errorLabel: NSTextField = {
        let textField = NSTextField(labelWithString: KDriveLocalizable.errorConnectingToXPCServer)
        textField.translatesAutoresizingMaskIntoConstraints = false
        textField.textColor = ColorToken.Text.tertiary.asNSColor
        textField.font = NSFont.Tokens.body
        return textField
    }()

    init(isShowingError: Bool) {
        super.init(nibName: nil, bundle: nil)
        errorLabel.isHidden = !isShowingError
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        setupView()
        loadAndPlayAnimation()
    }

    override func viewDidAppear() {
        super.viewDidAppear()
        configureWindowAppearance()
    }

    private func configureWindowAppearance() {
        guard let window = view.window else { return }

        window.titlebarAppearsTransparent = true
        window.isMovableByWindowBackground = true
        window.toolbar = NSToolbar()
    }

    private func setupView() {
        view.wantsLayer = true
        view.layer?.backgroundColor = ColorToken.Surface.secondary.asCGColor

        view.addSubview(animationView)

        let progressIndicator = NSProgressIndicator()
        progressIndicator.translatesAutoresizingMaskIntoConstraints = false
        progressIndicator.style = .spinning
        progressIndicator.isIndeterminate = true
        progressIndicator.controlSize = .small
        progressIndicator.startAnimation(nil)
        view.addSubview(progressIndicator)

        view.addSubview(errorLabel)

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

            errorLabel.topAnchor.constraint(equalTo: progressIndicator.bottomAnchor, constant: AppPadding.padding16),
            errorLabel.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            view.bottomAnchor.constraint(greaterThanOrEqualTo: errorLabel.bottomAnchor, constant: AppPadding.padding16)
        ])
    }

    private func loadAndPlayAnimation() {
        Task {
            try await animationView.loadAnimation(themedAnimation: .kDriveLoader)

            animationView.loopMode = .loop
            animationView.play()
        }
    }
}

@available(macOS 14.0, *)
#Preview("Loading") {
    PreloadingViewController(isShowingError: false)
}

@available(macOS 14.0, *)
#Preview("With Error") {
    PreloadingViewController(isShowingError: true)
}
