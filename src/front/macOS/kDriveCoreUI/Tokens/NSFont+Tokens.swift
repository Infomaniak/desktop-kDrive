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

extension NSFont.Weight {
    public static let emphasized = NSFont.Weight.semibold
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
