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

import SwiftUI

/// Grouped form usable on every supported macOS version.
///
/// macOS 13+ renders a native `Form` with the `.grouped` style. `.grouped` is unavailable on macOS 12, where a `Form`
/// falls back to a vertically centered column layout without section backgrounds; a top-aligned scrollable stack
/// of `IKSection` cards emulates the grouped layout instead.
///
/// Rows must be wrapped in an `IKSection` to get a card on macOS 12.
public struct IKForm<Content: View>: View {
    private let content: Content

    public init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    public var body: some View {
        if #available(macOS 13.0, *) {
            Form {
                content
            }
            .formStyle(.grouped)
        } else {
            ScrollView {
                VStack(alignment: .leading, spacing: AppPadding.padding24) {
                    content
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(AppPadding.page)
            }
        }
    }
}

/// Form section usable on every supported macOS version. See `IKForm`.
public struct IKSection<Content: View, Header: View, Footer: View>: View {
    private let content: Content
    private let header: Header
    private let footer: Footer

    private var hasContent: Bool {
        return Content.self != EmptyView.self
    }

    private var hasHeader: Bool {
        return Header.self != EmptyView.self
    }

    private var hasFooter: Bool {
        return Footer.self != EmptyView.self
    }

    public init(
        @ViewBuilder content: () -> Content,
        @ViewBuilder header: () -> Header,
        @ViewBuilder footer: () -> Footer
    ) {
        self.content = content()
        self.header = header()
        self.footer = footer()
    }

    public var body: some View {
        if #available(macOS 13.0, *) {
            nativeSection
        } else {
            legacySection
        }
    }

    @ViewBuilder
    private var nativeSection: some View {
        if hasHeader && hasFooter {
            Section {
                content
            } header: {
                header
            } footer: {
                footer
            }
        } else if hasHeader {
            Section {
                content
            } header: {
                header
            }
        } else if hasFooter {
            Section {
                content
            } footer: {
                footer
            }
        } else {
            Section {
                content
            }
        }
    }

    private var legacySection: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding8) {
            if hasHeader {
                header
                    .font(.headline)
            }

            if hasContent {
                VStack(alignment: .leading, spacing: AppPadding.padding12) {
                    content
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.vertical, AppPadding.padding12)
                .padding(.horizontal, AppPadding.padding16)
                .background(
                    // System colors mirror the native grouped form section background on later macOS versions.
                    RoundedRectangle(cornerRadius: AppRadius.radius8)
                        .fill(Color(nsColor: .controlBackgroundColor))
                )
                .overlay(
                    RoundedRectangle(cornerRadius: AppRadius.radius8)
                        .strokeBorder(Color(nsColor: .separatorColor))
                )
            }

            if hasFooter {
                footer
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }
        }
    }
}

public extension IKSection where Footer == EmptyView {
    init(@ViewBuilder content: () -> Content, @ViewBuilder header: () -> Header) {
        self.init(content: content, header: header) { EmptyView() }
    }
}

public extension IKSection where Header == EmptyView {
    init(@ViewBuilder content: () -> Content, @ViewBuilder footer: () -> Footer) {
        self.init(content: content, header: { EmptyView() }, footer: footer)
    }
}

public extension IKSection where Header == EmptyView, Footer == EmptyView {
    init(@ViewBuilder content: () -> Content) {
        self.init(content: content, header: { EmptyView() }, footer: { EmptyView() })
    }
}

#Preview {
    IKForm {
        IKSection {} header: {
            Text("Title")
        }

        IKSection {
            Text("First row")
            Text("Second row")
        } header: {
            Text("Header")
        }

        IKSection {
            Text("Row without header")
        }
    }
}
