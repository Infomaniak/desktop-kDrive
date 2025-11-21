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

public final class ResizableImageView: NSView {
    public let image: NSImage

    public var preferredRect: NSRect
    public var imageGravity: CALayerContentsGravity = .resizeAspectFill

    public init(image: NSImage) {
        self.image = image

        preferredRect = CGRect(origin: .zero, size: image.size)
        super.init(frame: .zero)

        setupView()
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        let contentsScale = window?.backingScaleFactor ?? NSScreen.main?.backingScaleFactor ?? 2.0
        var imageRect = CGRect(
            x: 0,
            y: 0,
            width: preferredRect.width * contentsScale,
            height: preferredRect.height * contentsScale
        )

        guard let cgImage = image.cgImage(forProposedRect: &imageRect, context: nil, hints: nil) else {
            return
        }

        wantsLayer = true

        layer?.masksToBounds = true
        layer?.contents = cgImage
        layer?.contentsGravity = imageGravity
        layer?.contentsScale = contentsScale
    }
}

@available(macOS 14.0, *)
#Preview {
    ResizableImageView(image: PreviewHelper.userImage)
}
