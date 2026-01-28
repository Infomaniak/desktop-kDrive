// swiftlint:disable all
// Generated using SwiftGen — https://github.com/SwiftGen/SwiftGen

import Foundation

// swiftlint:disable superfluous_disable_command file_length implicit_return prefer_self_in_static_references

// MARK: - Strings

// swiftlint:disable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:disable nesting type_body_length type_name vertical_whitespace_opening_braces
public enum KDriveLocalizable {
  /// loco:691deb2291b20ac7fd045012
  public static let buttonAdvancedParameters = KDriveLocalizable.tr("Localizable", "buttonAdvancedParameters", fallback: "Advanced settings")
  /// loco:696a343b72b6dc32e00557a4
  public static let buttonClose = KDriveLocalizable.tr("Localizable", "buttonClose", fallback: "Close")
  /// loco:691deb0be15255c13908ba42
  public static let buttonContinue = KDriveLocalizable.tr("Localizable", "buttonContinue", fallback: "Continue")
  /// loco:68e673c48af12c42e80027c8
  public static let buttonCreateAccount = KDriveLocalizable.tr("Localizable", "buttonCreateAccount", fallback: "Create an account")
  /// loco:6930506962426d4ff30af473
  public static let buttonFinishInstallation = KDriveLocalizable.tr("Localizable", "buttonFinishInstallation", fallback: "Finish installation")
  /// loco:69304d546385bb9cfc03e524
  public static let buttonKDriveIsActivated = KDriveLocalizable.tr("Localizable", "buttonKDriveIsActivated", fallback: "I've activated kDrive")
  /// loco:695e58701870be5cd709d2a2
  public static let buttonKDriveOnline = KDriveLocalizable.tr("Localizable", "buttonKDriveOnline", fallback: "kDrive Online")
  /// loco:68e673b2042a15d8470f9452
  public static let buttonLogin = KDriveLocalizable.tr("Localizable", "buttonLogin", fallback: "Login")
  /// loco:697a2658a44fbad34f011962
  public static let buttonManage = KDriveLocalizable.tr("Localizable", "buttonManage", fallback: "Manage")
  /// loco:68b04d823f2b735c170caea2
  public static let buttonOpenInBrowser = KDriveLocalizable.tr("Localizable", "buttonOpenInBrowser", fallback: "Open in browser")
  /// loco:68b04d5a63085b7ab90cea63
  public static let buttonOpenInFinder = KDriveLocalizable.tr("Localizable", "buttonOpenInFinder", fallback: "Open in Finder")
  /// loco:6931a5687031691272002314
  public static let buttonOpenKDrive = KDriveLocalizable.tr("Localizable", "buttonOpenKDrive", fallback: "Open kDrive")
  /// loco:6964e1fd78f375ec03073c88
  public static let buttonRestartSynchro = KDriveLocalizable.tr("Localizable", "buttonRestartSynchro", fallback: "Restart synchronization")
  /// loco:6964e1dfed4b6acb0107409a
  public static let buttonSeeActivities = KDriveLocalizable.tr("Localizable", "buttonSeeActivities", fallback: "See activities")
  /// loco:69240f7b15d05a975c07dc13
  public static let buttonShowOffers = KDriveLocalizable.tr("Localizable", "buttonShowOffers", fallback: "Show offers")
  /// loco:69240f6d4433e24658054fd4
  public static let buttonStartForFree = KDriveLocalizable.tr("Localizable", "buttonStartForFree", fallback: "Get started for free")
  /// loco:695e57f549f806b5ef0a3ac2
  public static let folderFavorites = KDriveLocalizable.tr("Localizable", "folderFavorites", fallback: "Favorites")
  /// loco:695e584bbc7544361b08b832
  public static let folderShares = KDriveLocalizable.tr("Localizable", "folderShares", fallback: "Shares")
  /// loco:695e5889bc7544361b08b834
  public static let folderTrash = KDriveLocalizable.tr("Localizable", "folderTrash", fallback: "Trash")
  /// loco:6960b7720cbd5c07340af144
  public static func greetingLabel(_ p1: UnsafePointer<CChar>, _ p2: UnsafePointer<CChar>) -> String {
    return KDriveLocalizable.tr("Localizable", "greetingLabel", p1, p2, fallback: "Hello %s, %s")
  }
  /// loco:6964f0b575a31a8d9601a6d7
  public static func helpKDriveName(_ p1: UnsafePointer<CChar>) -> String {
    return KDriveLocalizable.tr("Localizable", "helpKDriveName", p1, fallback: "kDrive %s")
  }
  /// loco:6930595e7664999c6a08c8a4
  public static let instructionEnableKDrive = KDriveLocalizable.tr("Localizable", "instructionEnableKDrive", fallback: "Activate kDrive.app")
  /// loco:693059768c31ab63990a0c82
  public static let instructionEnableKDriveArgument = KDriveLocalizable.tr("Localizable", "instructionEnableKDriveArgument", fallback: "kDrive.app")
  /// loco:69314c10606d30eb26081742
  public static let instructionEnableKDriveHint = KDriveLocalizable.tr("Localizable", "instructionEnableKDriveHint", fallback: "You must activate kDrive.app before continuing")
  /// loco:69314cfb358e5c25a50b1fb3
  public static let instructionFullDisk = KDriveLocalizable.tr("Localizable", "instructionFullDisk", fallback: "Activate kDrive and kDrive LiteSync Extension")
  /// loco:69314c248389940c6c08d902
  public static let instructionFullDiskHint = KDriveLocalizable.tr("Localizable", "instructionFullDiskHint", fallback: "You must activate authorizations before continuing")
  /// loco:69314c4aba85c2962d068dc2
  public static let instructionOpenPrivacySecurity = KDriveLocalizable.tr("Localizable", "instructionOpenPrivacySecurity", fallback: "Go to Privacy & Security > Full disk access")
  /// loco:69314c54606d30eb26081743
  public static let instructionOpenPrivacySecurityArgument = KDriveLocalizable.tr("Localizable", "instructionOpenPrivacySecurityArgument", fallback: "Full disk access")
  /// loco:69314c62739c205c410aa082
  public static let instructionOpenPrivacySecurityLink = KDriveLocalizable.tr("Localizable", "instructionOpenPrivacySecurityLink", fallback: "Privacy & Security")
  /// loco:69305911b203efcf750e7bb4
  public static let instructionOpenSecurityExtensions = KDriveLocalizable.tr("Localizable", "instructionOpenSecurityExtensions", fallback: "Select Open & Extensions > Security extensions")
  /// loco:6930592bfd6c86f6690db6d5
  public static let instructionOpenSecurityExtensionsArgument = KDriveLocalizable.tr("Localizable", "instructionOpenSecurityExtensionsArgument", fallback: "Security extensions")
  /// loco:693059483fc5023f6a0f86b3
  public static let instructionOpenSecurityExtensionsLink = KDriveLocalizable.tr("Localizable", "instructionOpenSecurityExtensionsLink", fallback: "Open & Extensions")
  /// loco:69305849f1fb5be2c7093fa2
  public static let instructionOpenSystemSettings = KDriveLocalizable.tr("Localizable", "instructionOpenSystemSettings", fallback: "Open System Settings > General")
  /// loco:6930588953221ff00608e983
  public static let instructionOpenSystemSettingsLink = KDriveLocalizable.tr("Localizable", "instructionOpenSystemSettingsLink", fallback: "System Settings")
  /// loco:69314d1a739c205c410aa087
  public static let instructionRestartIfNecessary = KDriveLocalizable.tr("Localizable", "instructionRestartIfNecessary", fallback: "Restart the application if required")
  /// loco:693050ed6a0836d25507da22
  public static let onboardingAuthorizationExtensionDescription = KDriveLocalizable.tr("Localizable", "onboardingAuthorizationExtensionDescription", fallback: "Authorize kDrive in macOS settings :")
  /// loco:6930509e14b396d6fc04b0f2
  public static let onboardingAuthorizationExtensionTitle = KDriveLocalizable.tr("Localizable", "onboardingAuthorizationExtensionTitle", fallback: "Activate kDrive on your Mac")
  /// loco:6930510648920a67910d1094
  public static let onboardingAuthorizationFullDiskDescription = KDriveLocalizable.tr("Localizable", "onboardingAuthorizationFullDiskDescription", fallback: "To complete the installation, you must authorize kDrive to access your :")
  /// loco:693050bab2a2c1990a0d2f04
  public static let onboardingAuthorizationFullDiskTitle = KDriveLocalizable.tr("Localizable", "onboardingAuthorizationFullDiskTitle", fallback: "Authorize access to files")
  /// loco:692406b0258f5599bf01d113
  public static let onboardingDriveSelectionNoDriveDescription = KDriveLocalizable.tr("Localizable", "onboardingDriveSelectionNoDriveDescription", fallback: "Get started for free with my kSuite,\nor choose a package tailored to your needs.")
  /// loco:692405adc5c224ad2a07ff62
  public static let onboardingDriveSelectionNoDriveTitle = KDriveLocalizable.tr("Localizable", "onboardingDriveSelectionNoDriveTitle", fallback: "You don't have a kDrive yet.")
  /// loco:69242dd4299cf487060f2143
  public static let onboardingDriveSelectionSelectTitle = KDriveLocalizable.tr("Localizable", "onboardingDriveSelectionSelectTitle", fallback: "Select the kDrive to be synchronized on this computer:")
  /// loco:69271e987f61390c0e053442
  public static let onboardingDriveSelectionSyncInProgress = KDriveLocalizable.tr("Localizable", "onboardingDriveSelectionSyncInProgress", fallback: "Synchronization in progress")
  /// loco:691deaf47145f10ab4052cd2
  public static let onboardingDriveSelectionTitle = KDriveLocalizable.tr("Localizable", "onboardingDriveSelectionTitle", fallback: "Welcome back!")
  /// loco:68e6739c8af12c42e80027c5
  public static let onboardingLoginDescription = KDriveLocalizable.tr("Localizable", "onboardingLoginDescription", fallback: "The fast, secure private cloud, hosted in Switzerland.\n\nLog in and keep your documents synchronized on all your devices.")
  /// loco:68e8fa543a683192560af9e2
  public static let onboardingLoginErrorDescription = KDriveLocalizable.tr("Localizable", "onboardingLoginErrorDescription", fallback: "An error has occurred, please try again.")
  /// loco:68e8fa06c1b003290f0ccf42
  public static let onboardingLoginErrorTitle = KDriveLocalizable.tr("Localizable", "onboardingLoginErrorTitle", fallback: "Connection error")
  /// loco:691deb66e28b84c78e034353
  public static let onboardingLoginHintLoading = KDriveLocalizable.tr("Localizable", "onboardingLoginHintLoading", fallback: "Just a few more moments, and we'll load your account…")
  /// loco:691deb477145f10ab4052cd4
  public static let onboardingLoginHintWebAuth = KDriveLocalizable.tr("Localizable", "onboardingLoginHintWebAuth", fallback: "Login from your browser…")
  /// loco:68e673754d80559c460bdf02
  public static let onboardingLoginTitle = KDriveLocalizable.tr("Localizable", "onboardingLoginTitle", fallback: "Welcome to kDrive!")
  /// loco:6931a1769dc60cb1820f5fe3
  public static let onboardingSynchronizationAppReadyDescription = KDriveLocalizable.tr("Localizable", "onboardingSynchronizationAppReadyDescription", fallback: "Your files are ready to be securely synchronized on your computer.")
  /// loco:6931a16c3498d5205b0380d3
  public static let onboardingSynchronizationAppReadyTitle = KDriveLocalizable.tr("Localizable", "onboardingSynchronizationAppReadyTitle", fallback: "All set!")
  /// loco:6931a1582ee1a869eb0303f4
  public static let onboardingSynchronizationInProgressDescription = KDriveLocalizable.tr("Localizable", "onboardingSynchronizationInProgressDescription", fallback: "Your kDrive files are being synchronized.\nPlease wait a few moments.")
  /// loco:6931a14fa4031f6161097f42
  public static let onboardingSynchronizationInProgressTitle = KDriveLocalizable.tr("Localizable", "onboardingSynchronizationInProgressTitle", fallback: "Synchronization in progress..")
  /// loco:68e8fa27d09187683c0679b2
  public static let onboardingWindowTitle = KDriveLocalizable.tr("Localizable", "onboardingWindowTitle", fallback: "Welcome to kDrive")
  /// loco:68cd386502633dee14000352
  public static let sidebarItemAccounts = KDriveLocalizable.tr("Localizable", "sidebarItemAccounts", fallback: "Accounts")
  /// loco:68cd389fcc36238762040074
  public static let sidebarItemAdvanced = KDriveLocalizable.tr("Localizable", "sidebarItemAdvanced", fallback: "Advanced")
  /// loco:68cd3843fbf0b4c60d0c9c82
  public static let sidebarItemGeneral = KDriveLocalizable.tr("Localizable", "sidebarItemGeneral", fallback: "General")
  /// loco:68a70c92c32749426b06db02
  public static let sidebarItemKDriveTitle = KDriveLocalizable.tr("Localizable", "sidebarItemKDriveTitle", fallback: "kDrive folder")
  /// loco:697a077ee16575000806de62
  public static let storageMacFreeSpace = KDriveLocalizable.tr("Localizable", "storageMacFreeSpace", fallback: "Free space")
  /// loco:697a0768c39a2189f5022564
  public static let storageMacUsedByComputer = KDriveLocalizable.tr("Localizable", "storageMacUsedByComputer", fallback: "Other Mac files")
  /// loco:697a07436d99d0c2c00449e2
  public static let storageMacUsedByKDrive = KDriveLocalizable.tr("Localizable", "storageMacUsedByKDrive", fallback: "kDrive files stored on your Mac")
  /// loco:697a2640c1c052cea305d2d6
  public static let storageSyncBlockMacDescription = KDriveLocalizable.tr("Localizable", "storageSyncBlockMacDescription", fallback: "Manage synchronized folders to free up space on your Mac.")
  /// loco:697a26183f570e1362040242
  public static let storageSyncBlockTitle = KDriveLocalizable.tr("Localizable", "storageSyncBlockTitle", fallback: "Synchronization")
  /// loco:6979e265a8419bb7510c8122
  public static func storageUsageLabel(_ p1: Any, _ p2: Any) -> String {
    return KDriveLocalizable.tr("Localizable", "storageUsageLabel", String(describing: p1), String(describing: p2), fallback: "%@ on %@ used")
  }
  /// loco:6960b7decf8ffcd3950d2706
  public static let synchroInProgress = KDriveLocalizable.tr("Localizable", "synchroInProgress", fallback: "your files are synchronizing")
  /// loco:6960b7fd4e868a099e022dc5
  public static let synchroPaused = KDriveLocalizable.tr("Localizable", "synchroPaused", fallback: "synchronization is paused")
  /// loco:6960f0917cb01bc78104ed32
  public static let synchroStatusInProgressDescription = KDriveLocalizable.tr("Localizable", "synchroStatusInProgressDescription", fallback: "Your recent files are being updated.")
  /// loco:6960f01471cc26128d040b12
  public static let synchroStatusInProgressTitle = KDriveLocalizable.tr("Localizable", "synchroStatusInProgressTitle", fallback: "Synchronization in progress")
  /// loco:6960f77abfc1ab365409ead2
  public static let synchroStatusOfflineDescription = KDriveLocalizable.tr("Localizable", "synchroStatusOfflineDescription", fallback: "Your local files remain accessible. Synchronization will resume automatically as soon as you reconnect.")
  /// loco:6960f75e46598388a60c3022
  public static let synchroStatusOfflineTitle = KDriveLocalizable.tr("Localizable", "synchroStatusOfflineTitle", fallback: "Offline")
  /// loco:6960f0d67c633959570799d2
  public static let synchroStatusPausedDescription = KDriveLocalizable.tr("Localizable", "synchroStatusPausedDescription", fallback: "Synchronization is temporarily stopped.")
  /// loco:6960f0b8ef5d3700b0018933
  public static let synchroStatusPausedTitle = KDriveLocalizable.tr("Localizable", "synchroStatusPausedTitle", fallback: "Pause activated")
  /// loco:6960efeed6dfdcb3af05e2d2
  public static let synchroStatusUpToDateDescription = KDriveLocalizable.tr("Localizable", "synchroStatusUpToDateDescription", fallback: "Your files are accessible and synchronized.")
  /// loco:6960ef1d538ef75e5a0ad3e2
  public static let synchroStatusUpToDateTitle = KDriveLocalizable.tr("Localizable", "synchroStatusUpToDateTitle", fallback: "No activity in progress")
  /// loco:6960b7a6cf8ffcd3950d2704
  public static let synchroUpToDate = KDriveLocalizable.tr("Localizable", "synchroUpToDate", fallback: "your files are up to date")
  /// loco:68a70be4b749277aa1081dc2
  public static let tabTitleActivity = KDriveLocalizable.tr("Localizable", "tabTitleActivity", fallback: "Activity")
  /// loco:68a705225b0066f86e001d52
  public static let tabTitleHome = KDriveLocalizable.tr("Localizable", "tabTitleHome", fallback: "Home")
  /// loco:68a70c659c53feed83039c42
  public static let tabTitleStorage = KDriveLocalizable.tr("Localizable", "tabTitleStorage", fallback: "Storage")
}
// swiftlint:enable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:enable nesting type_body_length type_name vertical_whitespace_opening_braces

// MARK: - Implementation Details

extension KDriveLocalizable {
  private static func tr(_ table: String, _ key: String, _ args: CVarArg..., fallback value: String) -> String {
    let format = BundleToken.bundle.localizedString(forKey: key, value: value, table: table)
    return String(format: format, locale: Locale.current, arguments: args)
  }
}

// swiftlint:disable convenience_type
private final class BundleToken {
  static let bundle: Bundle = {
    #if SWIFT_PACKAGE
    return Bundle.module
    #else
    return Bundle(for: BundleToken.self)
    #endif
  }()
}
// swiftlint:enable convenience_type
