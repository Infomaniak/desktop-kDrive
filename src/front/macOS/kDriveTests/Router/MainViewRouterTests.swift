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

import Foundation
@testable import kDrive
import kDriveCoreUI
import Testing

struct MainViewRouterTests {
    @Test()
    func initialization() {
        // WHEN
        let router = MainViewRouter()

        // THEN
        #expect(router.currentPath.mainTab == .home)
        #expect(router.currentPath.details == [.home])
        #expect(router.currentModal == nil)
    }

    @Test()
    func setCurrentTabToActivity() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN
        await router.setCurrentTab(.activities)

        // THEN
        #expect(router.currentPath.mainTab == .activities)
        #expect(router.currentPath.details == [.activities])
    }

    @Test()
    func setCurrentTabToStorage() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN
        await router.setCurrentTab(.storage)

        // THEN
        #expect(router.currentPath.mainTab == .storage)
        #expect(router.currentPath.details == [.storage])
    }

    @Test()
    func setCurrentTabToBlockingError() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN
        await router.setCurrentTab(.blockingError)

        // THEN
        #expect(router.currentPath.mainTab == .blockingError)
        #expect(router.currentPath.details == [.blockingError])
    }

    @Test()
    func pathCacheIsUsedWhenSwitchingTabs() async {
        // GIVEN
        let router = MainViewRouter()
        await router.append(.activityError)

        // WHEN - switch to activity and set some details
        await router.setCurrentTab(.activities)
        await router.append(.versionConflict)

        // WHEN - switch back to home (should use cache)
        await router.setCurrentTab(.home)

        // THEN - should have the cached path with activityError
        #expect(router.currentPath.mainTab == .home)
        #expect(router.currentPath.details == [.home, .activityError])

        // WHEN - switch back to activity (should use cache)
        await router.setCurrentTab(.activities)

        // THEN - should have the cached path with versionConflict
        #expect(router.currentPath.mainTab == .activities)
        #expect(router.currentPath.details == [.activities, .versionConflict])
    }

    @Test()
    func appendAddsDetailToCurrentPath() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN
        await router.append(.activityError)

        // THEN
        #expect(router.currentPath.details == [.home, .activityError])
    }

    @Test()
    func appendingUpdatesPathCache() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN
        await router.append(.activityError)

        // WHEN - switch away and back to verify cache
        await router.setCurrentTab(.activities)
        await router.setCurrentTab(.home)

        // THEN
        #expect(router.currentPath.details == [.home, .activityError])
    }

    @Test()
    func appendMultipleDetails() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN
        await router.append(.activityError)
        await router.append(.versionConflict)

        // THEN
        #expect(router.currentPath.details == [.home, .activityError, .versionConflict])
    }

    @Test()
    func removeLastRemovesSingleElement() async {
        // GIVEN
        let router = MainViewRouter()
        await router.append(.activityError)
        #expect(router.currentPath.details == [.home, .activityError])

        // WHEN
        await router.removeLast()

        // THEN
        #expect(router.currentPath.details == [.home])
    }

    @Test()
    func removeLastRemovesMultipleElements() async {
        // GIVEN
        let router = MainViewRouter()
        await router.append(.activityError)
        await router.append(.versionConflict)
        #expect(router.currentPath.details == [.home, .activityError, .versionConflict])

        // WHEN
        await router.removeLast(2)

        // THEN
        #expect(router.currentPath.details == [.home])
    }

    @Test()
    func removeLastDoesNotRemoveWhenElementsToRemoveExceedsCount() async {
        // GIVEN
        let router = MainViewRouter()
        #expect(router.currentPath.details == [.home])

        // WHEN - try to remove more elements than exist (should be a no-op)
        await router.removeLast(5)

        // THEN - should still have the original path
        #expect(router.currentPath.details == [.home])
    }

    @Test()
    func removeLastUpdatesPathCache() async {
        // GIVEN
        let router = MainViewRouter()
        await router.append(.activityError)
        await router.append(.versionConflict)

        // WHEN
        await router.removeLast()

        // WHEN - switch away and back to verify cache
        await router.setCurrentTab(.activities)
        await router.setCurrentTab(.home)

        // THEN
        #expect(router.currentPath.details == [.home, .activityError])
    }

    @Test()
    func setCurrentModalToValue() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN - ModalPath has no cases yet; verify setter is safe with nil
        await router.setCurrentModal(nil)

        // THEN
        #expect(router.currentModal == nil)
    }

    @Test()
    func switchingTabsDoesNotAffectModal() async {
        // GIVEN
        let router = MainViewRouter()

        // WHEN
        await router.setCurrentTab(.activities)

        // THEN - modal should remain nil regardless of tab switches
        #expect(router.currentModal == nil)
    }
}
