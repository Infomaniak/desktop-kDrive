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

import CppInterop
import Foundation

/// The subscription a drive can be upgraded to, along with the shop page where the upgrade is ordered.
///
/// Computed in kDriveCore because members typed with `KDC.UpsellTarget` are not visible from other modules.
public struct UpsellOffer: Sendable, Hashable {
    public enum Plan: Sendable, Hashable {
        case myKSuitePlus
        case kSuitePro
    }

    public let plan: Plan
    public let url: URL

    public init(plan: Plan, url: URL) {
        self.plan = plan
        self.url = url
    }
}

public extension UpsellOffer {
    /// Only the administrators of a drive with a free pack are offered an upgrade.
    init?(drive: Drive, languageCode: String = Bundle.main.preferredLocalizations.first ?? "en") {
        guard drive.admin, drive.packInfo.isFree else { return nil }

        switch drive.packInfo.upsellTarget {
        case .StandAloneToMykSuitePlus:
            self.init(
                plan: .myKSuitePlus,
                url: URLConstants.standAloneDriveToKSuiteMigration(driveId: Int(drive.driveId), languageCode: languageCode)
            )
        case .StandAloneTokSuitePro:
            self.init(
                plan: .kSuitePro,
                url: URLConstants.standAloneDriveToKSuiteMigration(driveId: Int(drive.driveId), languageCode: languageCode)
            )
        case .MykSuiteToMykSuitePlus:
            self.init(plan: .myKSuitePlus, url: URLConstants.myKSuiteUpgrade)
        case .kSuiteUpgrade:
            self.init(plan: .kSuitePro, url: URLConstants.kSuiteUpgrade(kSuiteId: drive.packInfo.kSuiteId))
        case .Unknown:
            return nil
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UpsellOffer init received KDC.UpsellTarget.EnumEnd case")
            return nil
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "UpsellOffer init received @unknown case")
            return nil
        }
    }
}
