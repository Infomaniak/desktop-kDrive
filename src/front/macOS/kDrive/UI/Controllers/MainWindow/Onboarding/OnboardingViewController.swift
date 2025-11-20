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
import kDriveCore
import kDriveResources

final class OnboardingViewController: NSViewController {
    private let flowCoordinator: OnboardingFlowCoordinator

    private var currentContentViewController: NSViewController?

    private let contentView: NSView
    private let animationsView: OnboardingAnimationsView

    private var bindStore = Set<AnyCancellable>()

    init() {
        flowCoordinator = OnboardingFlowCoordinator()

        contentView = NSView()
        animationsView = OnboardingAnimationsView(flowCoordinator: flowCoordinator)

        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()

        setupUI()
        bindCoordinator()
    }

    override func viewDidAppear() {
        super.viewDidAppear()
        setupWindowAppearance()
    }

    private func setupWindowAppearance() {
        guard let window = view.window else { return }

        window.title = KDriveLocalizable.onboardingWindowTitle
        window.titlebarAppearsTransparent = true
        window.isMovableByWindowBackground = false
    }

    private func setupUI() {
        let gradient = NSImageView(image: KDriveResources.onboardingGradient.image)
        gradient.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(gradient)

        animationsView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(animationsView)

        contentView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(contentView)

        NSLayoutConstraint.activate([
            gradient.widthAnchor.constraint(equalToConstant: 200),
            gradient.heightAnchor.constraint(equalToConstant: 200),
            gradient.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            gradient.bottomAnchor.constraint(equalTo: view.bottomAnchor),

            animationsView.topAnchor.constraint(equalTo: view.topAnchor),
            animationsView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            animationsView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            animationsView.widthAnchor.constraint(equalTo: view.widthAnchor, multiplier: 0.33),

            contentView.topAnchor.constraint(equalTo: view.topAnchor),
            contentView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            contentView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            contentView.trailingAnchor.constraint(equalTo: animationsView.leadingAnchor)
        ])
    }

    private func bindCoordinator() {
        transition(toStep: flowCoordinator.currentStep)
        flowCoordinator.$currentStep
            .receiveOnMain(store: &bindStore) { [weak self] step in
                self?.transition(toStep: step)
            }
    }

    private func transition(toStep step: OnboardingStep) {
        removeCurrentContentViewController()

        let viewController = getViewController(forStep: step)
        addContentViewController(viewController)
    }

    private func getViewController(forStep step: OnboardingStep) -> NSViewController {
        switch step {
        case .login:
            return LoginViewController(flowCoordinator: flowCoordinator)
        case .driveSelection:
            return DriveSelectionViewController()
        case .permissions:
            fatalError("Not Implemented Yet")
        case .synchronisation:
            fatalError("Not Implemented Yet")
        }
    }

    private func addContentViewController(_ viewController: NSViewController) {
        addChild(viewController)

        viewController.view.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(viewController.view)

        NSLayoutConstraint.activate([
            viewController.view.topAnchor.constraint(equalTo: contentView.topAnchor),
            viewController.view.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            viewController.view.bottomAnchor.constraint(equalTo: contentView.bottomAnchor),
            viewController.view.trailingAnchor.constraint(equalTo: contentView.trailingAnchor)
        ])

        currentContentViewController = viewController
    }

    private func removeCurrentContentViewController() {
        guard let currentContentViewController else { return }

        currentContentViewController.view.removeFromSuperview()
        currentContentViewController.removeFromParent()

        self.currentContentViewController = nil
    }
}
