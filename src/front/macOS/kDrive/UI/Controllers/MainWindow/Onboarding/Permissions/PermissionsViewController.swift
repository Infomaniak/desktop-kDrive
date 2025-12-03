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
import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI

extension MacOSPermission {
    var title: String {
        switch self {
        case .endpointSecurityExtension:
            return "!Activez kDrive sur votre Mac"
        case .fullDiskAccess:
            return "!Autorisez l’accès aux dossiers"
        }
    }

    var subtitle: String {
        switch self {
        case .endpointSecurityExtension:
            return "!Autorisez kDrive dans les réglages macOS :"
        case .fullDiskAccess:
            return "!Pour terminer l’installation, vous devez autoriser kDrive à accéder à vos dossiers :"
        }
    }
}

extension MacOSPermission {
    var instructions: [Instruction] {
        switch self {
        case .endpointSecurityExtension:
            return [.openSystemSettings, .openSecurityExtensions, .enableKDrive]
        case .fullDiskAccess:
            return [.openPrivacySecurity, .enableFullDiskAccess, .restartAppIfNecessary]
        }
    }

    enum Instruction: Sendable {
        case openSystemSettings
        case openSecurityExtensions
        case enableKDrive

        case openPrivacySecurity
        case enableFullDiskAccess
        case restartAppIfNecessary

        var value: String {
            switch self {
            case .openSystemSettings:
                return "!Ouvrez les Réglages système > Général"
            case .openSecurityExtensions:
                return "!Sélectionnez Ouverture et extensions > Extensions de sécurité"
            case .enableKDrive:
                return "!Activez kDrive.app"
            case .openPrivacySecurity:
                return "!Allez dans Confidentialité et sécurité > Accès complet au disque"
            case .enableFullDiskAccess:
                return "!Activez kDrive et kDrive LiteSync Extension"
            case .restartAppIfNecessary:
                return "!Redémarrez l’application si demandé"
            }
        }

        var argument: String? {
            switch self {
            case .openSecurityExtensions:
                return "Extensions de sécurité"
            case .enableKDrive:
                return "kDrive.app"
            case .openPrivacySecurity:
                return "Accès complet au disque"
            default:
                return nil
            }
        }

        var link: String? {
            switch self {
            case .openSystemSettings:
                return "Réglages système"
            case .openSecurityExtensions:
                return "Ouverture et extensions"
            case .openPrivacySecurity:
                return "Confidentialité et sécurité"
            default:
                return nil
            }
        }

        var linkURL: URL? {
            @InjectService var permissionHandler: MacOSPermissionHandling

            switch self {
            case .openSystemSettings:
                return URL(string: "x-apple.systempreferences:com.apple.preference")!
            case .openSecurityExtensions:
                return permissionHandler.systemPreferencesURL(for: .endpointSecurityExtension)
            case .openPrivacySecurity:
                return permissionHandler.systemPreferencesURL(for: .fullDiskAccess)
            default:
                return nil
            }
        }
    }
}

final class PermissionsViewController: OnboardingStepViewController {
    private let viewModel: PermissionsViewModel

    private var bindStore = Set<AnyCancellable>()

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

    override func viewDidLoad() {
        super.viewDidLoad()

        setupUI()
        bindValues()
    }

    private func bindValues() {
        viewModel.$currentPermission
            .receiveOnMain(store: &bindStore) { [weak self] permission in
                self?.updateUIForPermission(permission)
            }
    }

    private func setupUI() {
        titleLabel.isHidden = false
        descriptionLabel.isHidden = false

        stackView.insertArrangedSubview(instructionsStack, at: 2)
        stackView.setCustomSpacing(AppPadding.padding24, after: instructionsStack)

        for step in 1 ... 3 {
            instructionsStack.addArrangedSubview(PermissionInstructionCell(step: step, title: .init(string: "")))
        }
    }

    private func updateUIForPermission(_ permission: MacOSPermission) {
        setupHeader(for: permission)
        setupInstructions(for: permission)
        setupButtons(for: permission)
    }

    private func setupHeader(for permission: MacOSPermission) {
        titleLabel.stringValue = permission.title
        descriptionLabel.stringValue = permission.subtitle
    }

    private func setupInstructions(for permission: MacOSPermission) {
        for index in 0 ..< permission.instructions.count {
            let instruction = permission.instructions[index]
            let attributedString = createAttributedString(for: instruction)

            getInstruction(at: index)?.title = attributedString
        }
    }

    private func createAttributedString(for instruction: MacOSPermission.Instruction) -> NSMutableAttributedString {
        let attributedString = NSMutableAttributedString(string: instruction.value)

        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.setParagraphStyle(.default)
        paragraphStyle.alignment = .left
        paragraphStyle.lineBreakMode = .byWordWrapping

        let basicAttributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.Tokens.body,
            .foregroundColor: NSColor.Tokens.Text.secondary,
            .paragraphStyle: paragraphStyle,
            .cursor: NSCursor.arrow
        ]
        attributedString.addAttributes(basicAttributes, range: NSRange(location: 0, length: attributedString.length))

        if let argument = instruction.argument {
            let range = (attributedString.string as NSString).range(of: argument)
            attributedString.addAttribute(.font, value: NSFont.Tokens.bodyEmphasized, range: range)
        }

        if let link = instruction.link, let linkURL = instruction.linkURL {
            let range = (attributedString.string as NSString).range(of: link)
            let attributes: [NSAttributedString.Key: Any] = [
                .font: NSFont.Tokens.bodyEmphasized,
                .foregroundColor: NSColor.Tokens.Action.primary,
                .link: linkURL,
                .cursor: NSCursor.pointingHand
            ]
            attributedString.addAttributes(attributes, range: range)
        }

        return attributedString
    }

    private func setupButtons(for permission: MacOSPermission) {
        primaryButton.isHidden = false
        primaryButton.target = self
        primaryButton.action = #selector(validatePermission)
        secondaryButton.isHidden = true

        switch permission {
        case .endpointSecurityExtension:
            primaryButton.title = "!J'ai activé kDrive"
        case .fullDiskAccess:
            primaryButton.title = "!Terminer l'installation"
        }
    }

    private func updateButtonStatus() {}

    @objc private func validatePermission() {}

    private func getInstruction(at index: Int) -> PermissionInstructionCell? {
        return instructionsStack.arrangedSubviews[index] as? PermissionInstructionCell
    }
}
