// swiftlint:disable all
// Generated using SwiftGen — https://github.com/SwiftGen/SwiftGen

#if os(macOS)
  import AppKit
#elseif os(iOS)
  import UIKit
#elseif os(tvOS) || os(watchOS)
  import UIKit
#endif
#if canImport(SwiftUI)
  import SwiftUI
#endif

// Deprecated typealiases
@available(*, deprecated, renamed: "ImageAsset.Image", message: "This typealias will be removed in SwiftGen 7.0")
public typealias AssetImageTypeAlias = ImageAsset.Image

// swiftlint:disable superfluous_disable_command file_length implicit_return

// MARK: - Asset Catalogs

// swiftlint:disable identifier_name line_length nesting type_body_length type_name
public enum KDriveResources {
  public enum FileTypes {
    public static let fileArchive = ImageAsset(name: "FileTypes/fileArchive")
    public static let fileAudio = ImageAsset(name: "FileTypes/fileAudio")
    public static let fileCode = ImageAsset(name: "FileTypes/fileCode")
    public static let fileDoc = ImageAsset(name: "FileTypes/fileDoc")
    public static let fileFolder = ImageAsset(name: "FileTypes/fileFolder")
    public static let fileFont = ImageAsset(name: "FileTypes/fileFont")
    public static let fileGrid = ImageAsset(name: "FileTypes/fileGrid")
    public static let fileIcs = ImageAsset(name: "FileTypes/fileIcs")
    public static let fileImage = ImageAsset(name: "FileTypes/fileImage")
    public static let filePdf = ImageAsset(name: "FileTypes/filePdf")
    public static let filePoint = ImageAsset(name: "FileTypes/filePoint")
    public static let fileUnknown = ImageAsset(name: "FileTypes/fileUnknown")
    public static let fileVcard = ImageAsset(name: "FileTypes/fileVcard")
    public static let fileVideo = ImageAsset(name: "FileTypes/fileVideo")
  }
  public static let alert = ImageAsset(name: "alert")
  public static let checkmarkCircle = ImageAsset(name: "checkmark-circle")
  public static let checkmark = ImageAsset(name: "checkmark")
  public static let circularArrowsClockwise = ImageAsset(name: "circular-arrows-clockwise")
  public static let cloudArrowDown = ImageAsset(name: "cloud-arrow-down")
  public static let cloud = ImageAsset(name: "cloud")
  public static let cog = ImageAsset(name: "cog")
  public static let computerArrowUp = ImageAsset(name: "computer-arrow-up")
  public static let computer = ImageAsset(name: "computer")
  public static let doorArrowRight = ImageAsset(name: "door-arrow-right")
  public static let dotsVertical = ImageAsset(name: "dots-vertical")
  public static let finder = ImageAsset(name: "finder")
  public static let folderCircleArrowRight = ImageAsset(name: "folder-circle-arrow-right")
  public static let folderFilled = ImageAsset(name: "folder-filled")
  public static let folderShare = ImageAsset(name: "folder-share")
  public static let hammerWrench = ImageAsset(name: "hammer-wrench")
  public static let hardDiskDrive = ImageAsset(name: "hard-disk-drive")
  public static let headphones = ImageAsset(name: "headphones")
  public static let house = ImageAsset(name: "house")
  public static let kdriveFoldersStacked = ImageAsset(name: "kdrive-folders-stacked")
  public static let magnifyingGlass = ImageAsset(name: "magnifying-glass")
  public static let moonSleep = ImageAsset(name: "moon-sleep")
  public static let pause = ImageAsset(name: "pause")
  public static let pencil = ImageAsset(name: "pencil")
  public static let people = ImageAsset(name: "people")
  public static let person = ImageAsset(name: "person")
  public static let persons = ImageAsset(name: "persons")
  public static let play = ImageAsset(name: "play")
  public static let settings = ImageAsset(name: "settings")
  public static let squareArrowDiagonalUp = ImageAsset(name: "square-arrow-diagonal-up")
  public static let squareArrowUp = ImageAsset(name: "square-arrow-up")
  public static let star = ImageAsset(name: "star")
  public static let sun = ImageAsset(name: "sun")
  public static let trash = ImageAsset(name: "trash")
  public static let warning = ImageAsset(name: "warning")
  public static let wrench = ImageAsset(name: "wrench")
  public static let mountainsTreesSun = ImageAsset(name: "mountains-trees-sun")
  public static let volumeStrokeDots = ImageAsset(name: "volume-stroke-dots")
  public static let kdriveAppIcon = ImageAsset(name: "kdrive-app-icon")
  public static let kdriveCircleFilledArrowsClockwise = ImageAsset(name: "kdrive-circle-filled-arrows-clockwise")
  public static let kdriveCircleFilledExclamationMark = ImageAsset(name: "kdrive-circle-filled-exclamation-mark")
  public static let kdriveCircleFilledPause = ImageAsset(name: "kdrive-circle-filled-pause")
  public static let kdriveCircleFilled = ImageAsset(name: "kdrive-circle-filled")
  public static let kdriveNeutral = ImageAsset(name: "kdrive-neutral")
  public static let onboardingGradient = ImageAsset(name: "OnboardingGradient")
}
// swiftlint:enable identifier_name line_length nesting type_body_length type_name

// MARK: - Implementation Details

public struct ImageAsset {
  public fileprivate(set) var name: String

  #if os(macOS)
  public typealias Image = NSImage
  #elseif os(iOS) || os(tvOS) || os(watchOS)
  public typealias Image = UIImage
  #endif

  @available(iOS 8.0, tvOS 9.0, watchOS 2.0, macOS 10.7, *)
  public var image: Image {
    let bundle = BundleToken.bundle
    #if os(iOS) || os(tvOS)
    let image = Image(named: name, in: bundle, compatibleWith: nil)
    #elseif os(macOS)
    let name = NSImage.Name(self.name)
    let image = (bundle == .main) ? NSImage(named: name) : bundle.image(forResource: name)
    #elseif os(watchOS)
    let image = Image(named: name)
    #endif
    guard let result = image else {
      fatalError("Unable to load image asset named \(name).")
    }
    return result
  }

  #if os(iOS) || os(tvOS)
  @available(iOS 8.0, tvOS 9.0, *)
  public func image(compatibleWith traitCollection: UITraitCollection) -> Image {
    let bundle = BundleToken.bundle
    guard let result = Image(named: name, in: bundle, compatibleWith: traitCollection) else {
      fatalError("Unable to load image asset named \(name).")
    }
    return result
  }
  #endif

  #if canImport(SwiftUI)
  @available(iOS 13.0, tvOS 13.0, watchOS 6.0, macOS 10.15, *)
  public var swiftUIImage: SwiftUI.Image {
    SwiftUI.Image(asset: self)
  }
  #endif
}

public extension ImageAsset.Image {
  @available(iOS 8.0, tvOS 9.0, watchOS 2.0, *)
  @available(macOS, deprecated,
    message: "This initializer is unsafe on macOS, please use the ImageAsset.image property")
  convenience init?(asset: ImageAsset) {
    #if os(iOS) || os(tvOS)
    let bundle = BundleToken.bundle
    self.init(named: asset.name, in: bundle, compatibleWith: nil)
    #elseif os(macOS)
    self.init(named: NSImage.Name(asset.name))
    #elseif os(watchOS)
    self.init(named: asset.name)
    #endif
  }
}

#if canImport(SwiftUI)
@available(iOS 13.0, tvOS 13.0, watchOS 6.0, macOS 10.15, *)
public extension SwiftUI.Image {
  init(asset: ImageAsset) {
    let bundle = BundleToken.bundle
    self.init(asset.name, bundle: bundle)
  }

  init(asset: ImageAsset, label: Text) {
    let bundle = BundleToken.bundle
    self.init(asset.name, bundle: bundle, label: label)
  }

  init(decorative asset: ImageAsset) {
    let bundle = BundleToken.bundle
    self.init(decorative: asset.name, bundle: bundle)
  }
}
#endif

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
