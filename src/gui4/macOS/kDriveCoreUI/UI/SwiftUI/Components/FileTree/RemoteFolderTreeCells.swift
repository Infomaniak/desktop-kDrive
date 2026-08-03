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
import kDriveResources

final class RemoteFolderNameCell: NSTableCellView {
    private let icon = NSImageView()
    private let label = NSTextField(labelWithString: "")
    private let createButton = NSButton()

    private var trackingArea: NSTrackingArea?
    private var onCreateFolder: (() -> Void)?

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)

        icon.imageScaling = .scaleProportionallyUpOrDown
        icon.translatesAutoresizingMaskIntoConstraints = false

        label.lineBreakMode = .byTruncatingMiddle
        label.translatesAutoresizingMaskIntoConstraints = false
        label.font = .systemFont(ofSize: NSFont.systemFontSize)

        createButton.translatesAutoresizingMaskIntoConstraints = false
        createButton.isBordered = false
        createButton.bezelStyle = .regularSquare
        createButton.imageScaling = .scaleProportionallyUpOrDown
        createButton.image = NSImage(systemSymbolName: "folder.badge.plus", accessibilityDescription: nil)
        createButton.contentTintColor = ColorToken.Action.primary.asNSColor
        createButton.isHidden = true
        createButton.target = self
        createButton.action = #selector(createButtonTapped)

        addSubview(icon)
        addSubview(label)
        addSubview(createButton)

        imageView = icon
        textField = label

        NSLayoutConstraint.activate([
            icon.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 2),
            icon.centerYAnchor.constraint(equalTo: centerYAnchor),
            icon.widthAnchor.constraint(equalToConstant: AppIconSize.iconSize16.width),
            icon.heightAnchor.constraint(equalToConstant: AppIconSize.iconSize16.height),

            label.leadingAnchor.constraint(equalTo: icon.trailingAnchor, constant: AppPadding.padding8),
            label.centerYAnchor.constraint(equalTo: centerYAnchor),

            createButton.centerYAnchor.constraint(equalTo: centerYAnchor),
            createButton.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -AppPadding.padding8),
            createButton.widthAnchor.constraint(equalToConstant: AppIconSize.iconSize16.width),
            createButton.heightAnchor.constraint(equalToConstant: AppIconSize.iconSize16.height),

            label.trailingAnchor.constraint(lessThanOrEqualTo: createButton.leadingAnchor, constant: -AppPadding.padding4)
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func configure(with item: FileTreeItem, onCreateFolder: @escaping () -> Void) {
        self.onCreateFolder = onCreateFolder

        icon.image = KDriveResources.folderFilled.image
        label.stringValue = item.name
        label.textColor = item.isEnabled ? .labelColor : .disabledControlTextColor
        alphaValue = item.isEnabled ? 1 : 0.5

        createButton.isEnabled = item.isEnabled
        createButton.isHidden = true
    }

    @objc private func createButtonTapped() {
        onCreateFolder?()
    }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        if let trackingArea {
            removeTrackingArea(trackingArea)
        }
        let area = NSTrackingArea(
            rect: .zero,
            options: [.mouseEnteredAndExited, .activeInActiveApp, .inVisibleRect],
            owner: self,
            userInfo: nil
        )
        addTrackingArea(area)
        trackingArea = area
    }

    override func mouseEntered(with event: NSEvent) {
        if createButton.isEnabled {
            createButton.isHidden = false
        }
    }

    override func mouseExited(with event: NSEvent) {
        createButton.isHidden = true
    }
}

/// A temporary row holding a text field used to name a folder being created.
final class RemoteFolderEditCell: NSTableCellView, NSTextFieldDelegate {
    private let icon = NSImageView()
    private let nameField = NSTextField()

    private var onCommit: ((String) -> Void)?
    private var onCancel: (() -> Void)?
    private var didFinishEditing = false

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)

        icon.imageScaling = .scaleProportionallyUpOrDown
        icon.translatesAutoresizingMaskIntoConstraints = false
        icon.image = KDriveResources.folderFilled.image

        nameField.translatesAutoresizingMaskIntoConstraints = false
        nameField.font = .systemFont(ofSize: NSFont.systemFontSize)
        nameField.isEditable = true
        nameField.isBordered = true
        nameField.bezelStyle = .roundedBezel
        nameField.focusRingType = .default
        nameField.delegate = self

        addSubview(icon)
        addSubview(nameField)

        imageView = icon
        textField = nameField

        NSLayoutConstraint.activate([
            icon.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 2),
            icon.centerYAnchor.constraint(equalTo: centerYAnchor),
            icon.widthAnchor.constraint(equalToConstant: AppIconSize.iconSize16.width),
            icon.heightAnchor.constraint(equalToConstant: AppIconSize.iconSize16.height),

            nameField.leadingAnchor.constraint(equalTo: icon.trailingAnchor, constant: AppPadding.padding8),
            nameField.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -AppPadding.padding8),
            nameField.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func configure(onCommit: @escaping (String) -> Void, onCancel: @escaping () -> Void) {
        self.onCommit = onCommit
        self.onCancel = onCancel
        didFinishEditing = false
        nameField.stringValue = ""
    }

    func beginEditing() {
        window?.makeFirstResponder(nameField)
    }

    // MARK: - NSTextFieldDelegate

    func control(_ control: NSControl, textView: NSTextView, doCommandBy commandSelector: Selector) -> Bool {
        if commandSelector == #selector(NSResponder.insertNewline(_:)) {
            guard !didFinishEditing else { return true }
            didFinishEditing = true
            onCommit?(nameField.stringValue)
            return true
        }
        if commandSelector == #selector(NSResponder.cancelOperation(_:)) {
            guard !didFinishEditing else { return true }
            didFinishEditing = true
            onCancel?()
            return true
        }
        return false
    }

    func controlTextDidEndEditing(_ obj: Notification) {
        guard !didFinishEditing else { return }
        didFinishEditing = true

        if nameField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            onCancel?()
        } else {
            onCommit?(nameField.stringValue)
        }
    }
}
