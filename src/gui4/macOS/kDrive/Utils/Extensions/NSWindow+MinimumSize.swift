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

extension NSWindow {
    /// Sets `minSize` and grows the current frame if it is smaller (e.g. a frame restored from autosave).
    ///
    /// AppKit resets `minSize` to zero when a window's `contentView` that is an `NSHostingView` gets replaced,
    /// so this must be called again after each `contentView` swap.
    func applyMinimumSize(_ size: NSSize) {
        minSize = size

        let currentFrame = frame
        let newSize = NSSize(width: max(currentFrame.width, size.width), height: max(currentFrame.height, size.height))
        guard newSize != currentFrame.size else { return }

        let newOrigin = NSPoint(x: currentFrame.minX, y: currentFrame.maxY - newSize.height)
        setFrame(NSRect(origin: newOrigin, size: newSize), display: true)
    }
}
