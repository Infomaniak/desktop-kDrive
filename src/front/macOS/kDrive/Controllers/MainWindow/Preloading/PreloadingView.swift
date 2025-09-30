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
import Lottie

class PreloadingView: NSView {
    private var animationView: LottieAnimationView!

    init() {
        super.init(frame: .zero)

        wantsLayer = true
        layer?.backgroundColor = NSColor.surfaceSecondary.cgColor

        animationView = LottieAnimationView()
        animationView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(animationView)

        let progressIndicator = NSProgressIndicator()
        progressIndicator.translatesAutoresizingMaskIntoConstraints = false
        progressIndicator.style = .spinning
        progressIndicator.isIndeterminate = true
        progressIndicator.controlSize = .small
        progressIndicator.startAnimation(nil)
        addSubview(progressIndicator)

        loadAndPlayAnimation()

        NSLayoutConstraint.activate([
            animationView.widthAnchor.constraint(lessThanOrEqualToConstant: 150),
            animationView.heightAnchor.constraint(lessThanOrEqualToConstant: 150),

            animationView.centerXAnchor.constraint(equalTo: centerXAnchor),
            animationView.centerYAnchor.constraint(equalTo: centerYAnchor),

            animationView.leadingAnchor.constraint(greaterThanOrEqualTo: leadingAnchor, constant: 16),
            trailingAnchor.constraint(greaterThanOrEqualTo: animationView.trailingAnchor, constant: 16),
            animationView.topAnchor.constraint(greaterThanOrEqualTo: topAnchor, constant: 16),

            progressIndicator.topAnchor.constraint(equalTo: animationView.bottomAnchor, constant: 48),
            progressIndicator.centerXAnchor.constraint(equalTo: centerXAnchor),
            bottomAnchor.constraint(greaterThanOrEqualTo: progressIndicator.bottomAnchor, constant: 16)
        ])
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func loadAndPlayAnimation() {
        Task {
            guard let dotLottieFileURL = Bundle.main.url(forResource: "kdrive-loader-light", withExtension: "lottie") else {
                return
            }

            let dotLottieFile = try await DotLottieFile.loadedFrom(url: dotLottieFileURL)
            animationView.animation = dotLottieFile.animations.first?.animation

            animationView.loopMode = .loop
            animationView.play()
        }
    }
}
