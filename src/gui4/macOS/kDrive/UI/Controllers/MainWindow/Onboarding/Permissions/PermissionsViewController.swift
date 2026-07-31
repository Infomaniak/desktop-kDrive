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
import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources

extension MacOSPermission {
    var title: String {
        switch self {
        case .endpointSecurityExtension:
            return KDriveLocalizable.onboardingAuthorizationExtensionTitle
        case .fullDiskAccess:
            return KDriveLocalizable.onboardingAuthorizationFullDiskTitle
        }
    }

    var description: String {
        switch self {
        case .endpointSecurityExtension:
            return KDriveLocalizable.onboardingAuthorizationExtensionDescription
        case .fullDiskAccess:
            return KDriveLocalizable.onboardingAuthorizationFullDiskDescription
        }
    }

    var majorCellIndex: Int {
        switch self {
        case .endpointSecurityExtension:
            return 2
        case .fullDiskAccess:
            return 1
        }
    }
}

extension MacOSPermission {
    var instructions: [Instruction] {
        switch self {
        case .endpointSecurityExtension:
            return [.openSystemSettings, .openSecurityExtensions, .enableKDrive]
        case .fullDiskAccess:
            return [.openPrivacySecurity, .enableFullDiskAccess]
        }
    }

    enum Instruction: Sendable, Identifiable {
        case openSystemSettings
        case openSecurityExtensions
        case enableKDrive

        case openPrivacySecurity
        case enableFullDiskAccess

        case openLoginItems
        case enableBackgroundActivity

        var id: String { value }

        var value: String {
            switch self {
            case .openSystemSettings:
                return KDriveLocalizable.instructionOpenSystemSettings
            case .openSecurityExtensions:
                if #available(macOS 15.0, *) {
                    return KDriveLocalizable.instructionOpenSecurityExtensions
                } else {
                    return KDriveLocalizable.instructionOpenSecurityExtensionsLegacy
                }
            case .enableKDrive:
                return KDriveLocalizable.instructionEnableKDrive
            case .openPrivacySecurity:
                return KDriveLocalizable.instructionOpenPrivacySecurity
            case .enableFullDiskAccess:
                return KDriveLocalizable.instructionFullDisk
            case .openLoginItems:
                return KDriveLocalizable.instructionOpenLoginItems
            case .enableBackgroundActivity:
                return KDriveLocalizable.instructionEnableBackgroundActivity
            }
        }

        var argument: String? {
            switch self {
            case .openSecurityExtensions:
                if #available(macOS 15.0, *) {
                    return KDriveLocalizable.instructionOpenSecurityExtensionsArgument
                } else {
                    return KDriveLocalizable.instructionOpenSecurityExtensionsArgumentLegacy
                }
            case .enableKDrive:
                return KDriveLocalizable.instructionEnableKDriveArgument
            case .openPrivacySecurity:
                return KDriveLocalizable.instructionOpenPrivacySecurityArgument
            case .enableBackgroundActivity:
                return KDriveLocalizable.instructionEnableBackgroundActivityArgument
            default:
                return nil
            }
        }

        var link: String? {
            switch self {
            case .openSystemSettings:
                return KDriveLocalizable.instructionOpenSystemSettingsLink
            case .openSecurityExtensions:
                if #available(macOS 15.0, *) {
                    return KDriveLocalizable.instructionOpenSecurityExtensionsLink
                } else {
                    return KDriveLocalizable.instructionOpenSecurityExtensionsLinkLegacy
                }
            case .openPrivacySecurity:
                return KDriveLocalizable.instructionOpenPrivacySecurityLink
            case .openLoginItems:
                return KDriveLocalizable.instructionOpenLoginItemsLink
            default:
                return nil
            }
        }

        var linkURL: URL? {
            @InjectService var permissionHandler: MacOSPermissionHandling

            switch self {
            case .openSystemSettings:
                return SystemPreferencesURL.general
            case .openSecurityExtensions:
                return permissionHandler.systemPreferencesURL(for: .endpointSecurityExtension)
            case .openPrivacySecurity:
                return permissionHandler.systemPreferencesURL(for: .fullDiskAccess)
            case .openLoginItems:
                return permissionHandler.systemPreferencesURL(for: .endpointSecurityExtension)
            default:
                return nil
            }
        }

        var attributedString: NSMutableAttributedString {
            let attributedString = NSMutableAttributedString(string: value)

            let paragraphStyle = NSMutableParagraphStyle()
            paragraphStyle.setParagraphStyle(.default)
            paragraphStyle.alignment = .left
            paragraphStyle.lineBreakMode = .byWordWrapping

            let basicAttributes: [NSAttributedString.Key: Any] = [
                .font: NSFont.Tokens.body,
                .foregroundColor: ColorToken.Text.secondary.asNSColor,
                .paragraphStyle: paragraphStyle,
                .cursor: NSCursor.arrow
            ]
            attributedString.addAttributes(basicAttributes, range: NSRange(location: 0, length: attributedString.length))

            if let argument {
                let range = (attributedString.string as NSString).range(of: argument)
                attributedString.addAttribute(.font, value: NSFont.Tokens.bodyEmphasized, range: range)
            }

            if let link, let linkURL {
                let range = (attributedString.string as NSString).range(of: link)
                let attributes: [NSAttributedString.Key: Any] = [
                    .font: NSFont.Tokens.bodyEmphasized,
                    .foregroundColor: ColorToken.Action.primary.asNSColor,
                    .link: linkURL,
                    .cursor: NSCursor.pointingHand
                ]
                attributedString.addAttributes(attributes, range: range)
            }

            return attributedString
        }

        var hint: String? {
            switch self {
            case .enableKDrive:
                return KDriveLocalizable.instructionEnableKDriveHint
            case .enableFullDiskAccess:
                return KDriveLocalizable.instructionFullDiskHint
            default:
                return nil
            }
        }
    }
}

final class PermissionsViewController: OnboardingStepViewController {
    private let viewModel: PermissionsViewModel

    private var bindStore = Set<AnyCancellable>()
    private var didBecomeActiveObserver: NSObjectProtocol?

    private lazy var instructionsStack: NSStackView = {
        let stackView = NSStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.spacing = 0
        stackView.orientation = .vertical
        stackView.alignment = .leading
        return stackView
    }()

    init(flowCoordinator: OnboardingFlowCoordinator) {
        viewModel = PermissionsViewModel(flowCoordinator: flowCoordinator)
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    deinit {
        if let didBecomeActiveObserver {
            NotificationCenter.default.removeObserver(didBecomeActiveObserver)
        }
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        setupUI()
        bindValues()

        viewModel.installLiteSyncExtensionIfNeeded()
    }

    override func viewWillAppear() {
        super.viewWillAppear()

        didBecomeActiveObserver = NotificationCenter.default
            .addObserver(forName: NSApplication.didBecomeActiveNotification, object: nil, queue: .main) { [weak self] _ in
                self?.checkPermission()
            }
    }

    private func bindValues() {
        viewModel.$currentPermission
            .receiveOnMain(store: &bindStore) { [weak self] permission in
                self?.updateUIForPermission(permission)
            }

        viewModel.$currentState
            .receiveOnMain(store: &bindStore) { [weak self] state in
                self?.updateUIForState(state)
            }
    }

    private func setupUI() {
        titleLabel.isHidden = false
        descriptionLabel.isHidden = false

        stackView.insertArrangedSubview(instructionsStack, at: 2)
        stackView.setCustomSpacing(AppPadding.padding24, after: instructionsStack)
    }

    private func updateUIForPermission(_ permission: MacOSPermission) {
        setupHeader(for: permission)
        setupInstructions(for: permission)
        setupButtons(for: permission)
    }

    private func updateUIForState(_ state: MacOSPermissionState) {
        guard let cell = instructionCell(at: viewModel.currentPermission.majorCellIndex) else {
            return
        }

        switch state {
        case .neutral:
            cell.state = .neutral
            cell.hintLabel.isHidden = true
            primaryButton.isEnabled = false
        case .warning:
            cell.state = .warning
            cell.hintLabel.isHidden = false
            primaryButton.isEnabled = false
        case .done:
            cell.state = .done
            cell.hintLabel.isHidden = true
            primaryButton.isEnabled = true
        }
    }

    private func setupHeader(for permission: MacOSPermission) {
        titleLabel.stringValue = permission.title
        descriptionLabel.stringValue = permission.description
    }

    private func setupInstructions(for permission: MacOSPermission) {
        for subview in instructionsStack.arrangedSubviews {
            instructionsStack.removeArrangedSubview(subview)
            subview.removeFromSuperview()
        }

        for (index, instruction) in permission.instructions.enumerated() {
            let instructionCell = PermissionInstructionCell(step: index + 1, title: instruction.attributedString)
            if let hint = instruction.hint {
                instructionCell.hint = hint
                instructionCell.hintLabel.textColor = ColorToken.Status.Strong.warning.asNSColor
            }
            instructionsStack.addArrangedSubview(instructionCell)
        }
    }

    private func setupButtons(for permission: MacOSPermission) {
        primaryButton.isHidden = false
        primaryButton.target = self
        primaryButton.action = #selector(didClickValidate)
        secondaryButton.isHidden = true

        switch permission {
        case .endpointSecurityExtension:
            primaryButton.title = KDriveLocalizable.buttonKDriveIsActivated
        case .fullDiskAccess:
            primaryButton.title = KDriveLocalizable.buttonFinishInstallation
        }
    }

    @objc private func didClickValidate() {
        viewModel.navigateIfPossible()
    }

    private func checkPermission() {
        viewModel.updatePermissionStatus()
    }

    private func instructionCell(at index: Int) -> PermissionInstructionCell? {
        guard instructionsStack.arrangedSubviews.indices.contains(index) else {
            return nil
        }
        return instructionsStack.arrangedSubviews[index] as? PermissionInstructionCell
    }
}
