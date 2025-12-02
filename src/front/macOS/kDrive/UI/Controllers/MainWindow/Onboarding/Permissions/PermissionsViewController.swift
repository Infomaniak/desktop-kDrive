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
            instructionsStack.addArrangedSubview(PermissionInstructionCell(step: step, title: NSAttributedString(string: "")))
        }
    }

    private func updateUIForPermission(_ permission: MacOSPermission) {
        setupHeader(for: permission)
        setupInstructions(for: permission)
        setupButtons(for: permission)
    }

    private func setupHeader(for permission: MacOSPermission) {
        switch permission {
        case .endpointSecurityExtension:
            titleLabel.stringValue = "!Activez kDrive sur votre Mac"
            descriptionLabel.stringValue = "!Autorisez kDrive dans les réglages macOS :"
        case .fullDiskAccess:
            titleLabel.stringValue = "!Autorisez l’accès aux dossiers"
            descriptionLabel.stringValue = "!Pour terminer l’installation, vous devez autoriser kDrive à accéder à vos dossiers :"
        }
    }

    private func setupInstructions(for permission: MacOSPermission) {
        switch permission {
        case .endpointSecurityExtension:
            setupEndpointSecurityExtensionInstructions()
        case .fullDiskAccess:
            setupFullDiskAccessInstructions()
        }
    }

    private func setupEndpointSecurityExtensionInstructions() {
        let step1Label = NSMutableAttributedString(string: "!Ouvrez les Réglages système > Général")
        instruction(at: 0)?.title = step1Label

        let step2Label = NSMutableAttributedString(string: "!Sélectionnez Ouverture et extensions > Extensions de sécurité")
        instruction(at: 1)?.title = step2Label

        let step3Label = NSMutableAttributedString(string: "!Activez kDrive.app")
        instruction(at: 2)?.title = step3Label
    }

    private func setupFullDiskAccessInstructions() {
        let step1Label = NSMutableAttributedString(string: "!Allez dans Confidentialité et sécurité > Accès complet au disque")
        instruction(at: 0)?.title = step1Label

        let step2Label = NSMutableAttributedString(string: "!Activez kDrive et kDrive LiteSync Extension")
        instruction(at: 1)?.title = step2Label

        let step3Label = NSMutableAttributedString(string: "!Redémarrez l’application si demandé")
        instruction(at: 2)?.title = step3Label
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

    private func instruction(at index: Int) -> PermissionInstructionCell? {
        return instructionsStack.arrangedSubviews[index] as? PermissionInstructionCell
    }
}
