// swiftlint:disable all
// Generated using SwiftGen — https://github.com/SwiftGen/SwiftGen

import Foundation

// swiftlint:disable superfluous_disable_command file_length implicit_return prefer_self_in_static_references

// MARK: - Strings

// swiftlint:disable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:disable nesting type_body_length type_name vertical_whitespace_opening_braces
internal enum KDriveLocalizable {
  /// loco:68a7228730d0f594cc03e4b3
  internal static let menuItemSearch = KDriveLocalizable.tr("Localizable", "menuItemSearch", fallback: "Search")
  /// loco:68a70c92c32749426b06db02
  internal static let sidebarItemKDriveTitle = KDriveLocalizable.tr("Localizable", "sidebarItemKDriveTitle", fallback: "kDrive folder")
  /// loco:68a8031da6bad911b30ec9c2
  internal static let sidebarSectionAdvancedSync = KDriveLocalizable.tr("Localizable", "sidebarSectionAdvancedSync", fallback: "Advanced sync.")
  /// loco:68a70be4b749277aa1081dc2
  internal static let tabTitleActivity = KDriveLocalizable.tr("Localizable", "tabTitleActivity", fallback: "Activity")
  /// loco:68a705225b0066f86e001d52
  internal static let tabTitleHome = KDriveLocalizable.tr("Localizable", "tabTitleHome", fallback: "Home")
  /// loco:68a70c659c53feed83039c42
  internal static let tabTitleStorage = KDriveLocalizable.tr("Localizable", "tabTitleStorage", fallback: "Storage")
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
