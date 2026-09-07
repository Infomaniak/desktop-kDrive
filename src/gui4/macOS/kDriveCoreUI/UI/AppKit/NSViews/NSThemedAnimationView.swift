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

import Cocoa
import kDriveResources
import Lottie

public final class NSThemedAnimationView: LottieAnimationView {
    private var themedAnimation: ThemedAnimation?

    override public func viewDidChangeEffectiveAppearance() {
        super.viewDidChangeEffectiveAppearance()

        guard let themedAnimation else { return }

        Task {
            let oldLoopMode = loopMode
            try await loadAnimation(themedAnimation: themedAnimation)
            loopMode = oldLoopMode
            play()
        }
    }

    public func loadAnimation(themedAnimation: ThemedAnimation) async throws {
        self.themedAnimation = themedAnimation

        let animationName = themedAnimation.animation(forAppearance: effectiveAppearance)

        let dotLottieFile = try await DotLottieFile.loadedFromBundle(forResource: animationName)
        loadAnimation(from: dotLottieFile)
    }
}
