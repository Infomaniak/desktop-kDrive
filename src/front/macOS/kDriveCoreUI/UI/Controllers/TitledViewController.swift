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

public struct ResizableContainerView<Content: View>: View {
    let content: Content

    public var body: some View {
        VStack {
            content
        }
        .frame(minWidth: nil, maxWidth: .infinity, minHeight: nil, maxHeight: .infinity)
    }
}

open class TitledViewController<Content: View>: NSHostingController<ResizableContainerView<Content>> {
    public let toolbarTitle: String

    override open var acceptsFirstResponder: Bool {
        return true
    }

    public init(toolbarTitle: String, contentView: Content) {
        self.toolbarTitle = toolbarTitle
        super.init(rootView: ResizableContainerView(content: contentView))
    }

    public convenience init(toolbarTitle: String, @ViewBuilder contentView: () -> Content) {
        self.init(toolbarTitle: toolbarTitle, contentView: contentView())
    }

    @available(*, unavailable)
    public required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override open func viewDidAppear() {
        super.viewDidAppear()
        view.window?.title = toolbarTitle
    }
}
