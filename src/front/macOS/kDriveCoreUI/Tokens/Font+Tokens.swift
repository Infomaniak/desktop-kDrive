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
import SwiftUI

// MARK: - Font

public extension Font {
    enum Tokens {
        public static let largeTitleEmphasized: Font = .largeTitle.weight(.emphasized)

        public static let titleEmphasized: Font = .title.weight(.emphasized)

        public static let title2: Font = .title2
        public static let title2Emphasized: Font = .title2.weight(.emphasized)

        public static let title3: Font = .title3
        public static let title3Emphasized: Font = .title3.weight(.emphasized)

        public static let headline: Font = .headline

        public static let body: Font = .body
        public static let bodyEmphasized: Font = .body.weight(.emphasized)

        public static let subheadline: Font = .subheadline
    }
}

extension Font.Weight {
    static let emphasized = Font.Weight.semibold
}

// MARK: - NSFont

public extension NSFont.Weight {
    static let emphasized = NSFont.Weight.semibold
}

public extension NSFont {
    enum Tokens {
        public static let largeTitleEmphasized: NSFont = .preferredFont(forTextStyle: .largeTitle, weight: .emphasized)

        public static let title3Emphasized: NSFont = .preferredFont(forTextStyle: .title3, weight: .emphasized)

        public static let headline: NSFont = .preferredFont(forTextStyle: .headline)

        public static let body: NSFont = .preferredFont(forTextStyle: .body)
        public static let bodyEmphasized: NSFont = .preferredFont(forTextStyle: .body, weight: .emphasized)

        public static let subheadline: NSFont = .preferredFont(forTextStyle: .subheadline)
    }
}

extension NSFont {
    static func preferredFont(forTextStyle textStyle: NSFont.TextStyle, weight: Weight) -> NSFont {
        return .systemFont(
            ofSize: NSFont.preferredFont(forTextStyle: textStyle).pointSize,
            weight: weight
        )
    }
}
