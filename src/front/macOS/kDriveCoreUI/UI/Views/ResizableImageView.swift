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
    public var image: NSImage? {
        didSet {
            setImage(image)
        }
    }

    public var preferredRect: NSRect = .zero
    public var imageGravity: CALayerContentsGravity = .resizeAspectFill

    private var contentsScale: CGFloat {
        return window?.backingScaleFactor ?? NSScreen.main?.backingScaleFactor ?? 2.0
    }

    public init(image: NSImage? = nil) {
        self.image = image
        super.init(frame: .zero)
        setupView()
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setupView() {
        wantsLayer = true

        layer?.masksToBounds = true
        layer?.contentsGravity = imageGravity
        layer?.contentsScale = contentsScale

        setImage(image)
    }

    private func setImage(_ image: NSImage?) {
        guard let image else {
            return
        }

        if preferredRect == .zero {
            preferredRect = CGRect(origin: .zero, size: image.size)
        }

        var proposedRect = CGRect(
            x: 0,
            y: 0,
            width: preferredRect.width * contentsScale,
            height: preferredRect.height * contentsScale
        )
        if let cgImage = image.cgImage(forProposedRect: &proposedRect, context: nil, hints: nil) {
            layer?.contents = cgImage
        }
    }
}

@available(macOS 14.0, *)
#Preview {
    ResizableImageView(image: PreviewHelper.userImage)
}
