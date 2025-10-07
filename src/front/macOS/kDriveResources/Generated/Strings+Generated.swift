// swiftlint:disable all
// Generated using SwiftGen — https://github.com/SwiftGen/SwiftGen

import Foundation

// swiftlint:disable superfluous_disable_command file_length implicit_return prefer_self_in_static_references

// MARK: - Strings

// swiftlint:disable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:disable nesting type_body_length type_name vertical_whitespace_opening_braces
public enum KDriveLocalizable {
  /// loco:68b04d823f2b735c170caea2
  public static let buttonOpenInBrowser = KDriveLocalizable.tr("Localizable", "buttonOpenInBrowser", fallback: "Open in browser")
  /// loco:68b04d5a63085b7ab90cea63
  public static let buttonOpenInFinder = KDriveLocalizable.tr("Localizable", "buttonOpenInFinder", fallback: "Open in Finder")
  /// loco:68cd386502633dee14000352
  public static let sidebarItemAccounts = KDriveLocalizable.tr("Localizable", "sidebarItemAccounts", fallback: "Accounts")
  /// loco:68cd389fcc36238762040074
  public static let sidebarItemAdvanced = KDriveLocalizable.tr("Localizable", "sidebarItemAdvanced", fallback: "Advanced")
  /// loco:68cd3843fbf0b4c60d0c9c82
  public static let sidebarItemGeneral = KDriveLocalizable.tr("Localizable", "sidebarItemGeneral", fallback: "General")
  /// loco:68a70c92c32749426b06db02
  public static let sidebarItemKDriveTitle = KDriveLocalizable.tr("Localizable", "sidebarItemKDriveTitle", fallback: "kDrive folder")
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
