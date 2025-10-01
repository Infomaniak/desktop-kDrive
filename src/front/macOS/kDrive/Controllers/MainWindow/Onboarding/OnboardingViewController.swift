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

final class OnboardingViewController: NSViewController {
    private let animationsView = OnboardingAnimationsView()

    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }

    override func viewDidAppear() {
        super.viewDidAppear()
        setupWindowAppearance()
    }

    private func setupWindowAppearance() {
        guard let window = view.window else { return }

        window.title = "!Bienvenue dans kDrive"
        window.titlebarAppearsTransparent = true
    }

    private func setupUI() {
        animationsView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(animationsView)

        NSLayoutConstraint.activate([
            animationsView.topAnchor.constraint(equalTo: view.topAnchor),
            animationsView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            animationsView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            animationsView.widthAnchor.constraint(equalTo: view.widthAnchor, multiplier: 0.33)
        ])
    }
}
