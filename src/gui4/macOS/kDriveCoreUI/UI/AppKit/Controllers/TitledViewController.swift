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
        content
            .frame(minWidth: nil, maxWidth: .infinity, minHeight: nil, maxHeight: .infinity)
    }
}

open class TitledViewController<Content: View>: NSHostingController<ResizableContainerView<Content>> {
    public let toolbarTitle: String
    public let navigableRouter: NavigableRouter?

    override open var acceptsFirstResponder: Bool {
        return true
    }

    public init(toolbarTitle: String, navigableRouter: NavigableRouter? = nil, contentView: Content) {
        self.toolbarTitle = toolbarTitle
        self.navigableRouter = navigableRouter

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

        updateTitle()
        appendNavigationToolbarItemIfNecessary()
    }

    override open func viewWillDisappear() {
        super.viewWillDisappear()
        removeNavigationToolbarItemIfNecessary()
    }

    private func updateTitle() {
        view.window?.title = toolbarTitle
        view.window?.titleVisibility = .hidden

        if let titleItem = view.window?.toolbar?.items.first(where: { $0.itemIdentifier == .titleLabel }) as? NSToolbarTitleItem {
            titleItem.stringValue = toolbarTitle
        }
    }

    private func appendNavigationToolbarItemIfNecessary() {
        guard navigableRouter?.hasDeepNavigated == true else {
            return
        }

        view.window?.toolbar?.validateVisibleItems()
        view.window?.toolbar?.insertItem(withItemIdentifier: .goBack, at: 2)

        if let goBackItem = view.window?.toolbar?.items.first(where: { $0.itemIdentifier == .goBack }) {
            goBackItem.target = self
            goBackItem.action = #selector(goBackInHistory)
        }
    }

    private func removeNavigationToolbarItemIfNecessary() {
        guard let goBackItemIndex = view.window?.toolbar?.items.firstIndex(where: { $0.itemIdentifier == .goBack }) else {
            return
        }
        view.window?.toolbar?.removeItem(at: goBackItemIndex)
    }

    @objc private func goBackInHistory() {
        navigableRouter?.removeLast(1)
    }
}
