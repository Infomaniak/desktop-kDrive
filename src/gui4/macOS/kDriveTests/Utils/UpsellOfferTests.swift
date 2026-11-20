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
@testable import kDriveCore
import OrderedCollections
import Testing

struct UpsellOfferTests {
    private static let driveId: Int32 = 42
    private static let kSuiteId: UInt64 = 1234

    private func makeDrive(admin: Bool = true, isFree: Bool = true, upsellTarget: KDC.UpsellTarget) -> Drive {
        Drive(driveDbId: 1,
              driveId: Self.driveId,
              accountDbId: 1,
              accountId: 1,
              userDbId: 1,
              name: "drive",
              color: nil,
              accessDenied: false,
              admin: admin,
              locked: false,
              maintenance: false,
              notifications: false,
              size: 0,
              usedSize: 0,
              packInfo: PackInfo(
                  id: 1,
                  name: "",
                  displayName: "",
                  isFree: isFree,
                  kSuiteId: Self.kSuiteId,
                  upsellTarget: upsellTarget
              ),
              synchros: [:])
    }

    @Test("Only the administrators of a drive with a free pack get an offer", arguments: [
        (admin: false, isFree: true),
        (admin: true, isFree: false),
        (admin: false, isFree: false)
    ])
    func noOfferWhenNotAllowed(admin: Bool, isFree: Bool) {
        let drive = makeDrive(admin: admin, isFree: isFree, upsellTarget: .kSuiteUpgrade)

        #expect(UpsellOffer(drive: drive) == nil)
    }

    @Test func noOfferForUnknownTarget() {
        #expect(UpsellOffer(drive: makeDrive(upsellTarget: .Unknown)) == nil)
    }

    @Test func standAloneDriveToMyKSuitePlusOpensDriveMigration() throws {
        let offer = try #require(UpsellOffer(drive: makeDrive(upsellTarget: .StandAloneToMykSuitePlus), languageCode: "fr"))

        #expect(offer.plan == .myKSuitePlus)
        #expect(offer.url.absoluteString == "https://shop.infomaniak.com/order/ksuite?migration=drive&drive_id=42&lang=fr")
    }

    @Test func standAloneDriveToKSuiteProOpensDriveMigration() throws {
        let offer = try #require(UpsellOffer(drive: makeDrive(upsellTarget: .StandAloneTokSuitePro), languageCode: "de"))

        #expect(offer.plan == .kSuitePro)
        #expect(offer.url.absoluteString == "https://shop.infomaniak.com/order/ksuite?migration=drive&drive_id=42&lang=de")
    }

    @Test func driveMigrationFallsBackToEnglishForUnsupportedSiteLanguage() throws {
        let offer = try #require(UpsellOffer(drive: makeDrive(upsellTarget: .StandAloneTokSuitePro), languageCode: "sv"))

        #expect(offer.url.absoluteString == "https://shop.infomaniak.com/order/ksuite?migration=drive&drive_id=42&lang=en")
    }

    @Test func myKSuiteToMyKSuitePlusOpensMyKSuiteOrder() throws {
        let offer = try #require(UpsellOffer(drive: makeDrive(upsellTarget: .MykSuiteToMykSuitePlus)))

        #expect(offer.plan == .myKSuitePlus)
        #expect(offer.url.absoluteString == "https://shop.infomaniak.com/order/myksuite")
    }

    @Test func kSuiteUpgradeOpensKSuiteOrder() throws {
        let offer = try #require(UpsellOffer(drive: makeDrive(upsellTarget: .kSuiteUpgrade)))

        #expect(offer.plan == .kSuitePro)
        #expect(offer.url.absoluteString == "https://shop.infomaniak.com/order2/ksuite/1234")
    }
}
