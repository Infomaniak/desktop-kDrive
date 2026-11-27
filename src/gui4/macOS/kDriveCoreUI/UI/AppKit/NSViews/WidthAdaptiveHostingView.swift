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

import AppKit
import SwiftUI

/// An `NSHostingView` whose height adapts to the width imposed by Auto Layout.
///
/// `NSHostingView` reports the SwiftUI content's ideal size as its intrinsic content size, so multiline `Text`
/// is measured on a single line and gets truncated when the view is constrained to a narrower width.
/// This view measures the content for its current width instead, letting text wrap.
public final class WidthAdaptiveHostingView<Content: View>: NSHostingView<Content> {
    private var lastMeasuredWidth: CGFloat = 0

    override public var intrinsicContentSize: NSSize {
        guard bounds.width > 0 else { return super.intrinsicContentSize }

        let measuringController = NSHostingController(rootView: rootView)
        let fittingSize = measuringController.sizeThatFits(in: CGSize(width: bounds.width, height: .infinity))
        return NSSize(width: NSView.noIntrinsicMetric, height: ceil(fittingSize.height))
    }

    override public func layout() {
        super.layout()

        guard bounds.width != lastMeasuredWidth else { return }
        lastMeasuredWidth = bounds.width
        invalidateIntrinsicContentSize()
    }
}
