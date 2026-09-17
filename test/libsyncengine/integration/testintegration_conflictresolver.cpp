/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testintegration.h"
#include "propagation/executor/filerescuer.h"
#include "syncpal_test_helper/syncpaltesthelper.h"

namespace KDC {

void TestIntegration::testParentDeleteRescuesModifiedLocalChildren() {
    SyncpalTestHelper testHelper(_syncPal);

    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const Situation initialSituation{Str2SyncName(R"({
        "content" : [
            {
                "type" : "Directory",
                "name" : "A",
                "content" : [
                    {
                        "type" : "Directory",
                        "name" : "AA",
                        "content" : [
                            {"type" : "File", "name" : "AAA"},
                            {"type" : "File", "name" : "AAC"}
                        ]
                    },
                    {
                        "type" : "Directory",
                        "name" : "AB",
                        "content" : [ {"type" : "File", "name" : "ABB"} ]
                    }
                ]
            }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.setInitialSituation(initialSituation, initialSituation));
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    CPPUNIT_ASSERT(testHelper.pauseSync());

    const Operations localOperations{Str2SyncName(R"({
        "operations": [
            { "type": "Edit", "path": "A/AB/ABB", "newSize": 4567 },
            { "type": "Move", "fromPath": "A/AA/AAA", "toPath": "A/AB/AAA" },
            { "type": "Create", "itemType": "File", "path": "A/AA", "name": "AAD", "size": 3456 }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Local, localOperations));

    const Operations remoteOperations{Str2SyncName(R"({
        "operations": [
            { "type": "Delete", "path":"A" }
        ]
    })")};
    CPPUNIT_ASSERT(testHelper.execute(ReplicaSide::Remote, remoteOperations));

    CPPUNIT_ASSERT(testHelper.unpauseSync());
    CPPUNIT_ASSERT(testHelper.executeSyncUntilEnd());

    const SyncPath rescueFolderPath = _syncPal->localPath() / FileRescuer::rescueFolderName();
    CPPUNIT_ASSERT(std::filesystem::exists(rescueFolderPath));

    const auto countRescuedByPrefix = [&rescueFolderPath](const SyncName &prefix) {
        size_t count = 0;
        for (const auto &entry: std::filesystem::directory_iterator(rescueFolderPath)) {
            if (entry.path().filename().native().starts_with(prefix)) {
                count++;
            }
        }
        return count;
    };

    CPPUNIT_ASSERT_EQUAL(size_t(0), countRescuedByPrefix(Str("AAA"))); // AAA was only moved to A/AB/AAA, so it should not be rescued.
    CPPUNIT_ASSERT_EQUAL(size_t(1), countRescuedByPrefix(Str("ABB")));
    CPPUNIT_ASSERT_EQUAL(size_t(1), countRescuedByPrefix(Str("AAD")));

    logStep("testParentDeleteRescuesModifiedLocalChildren");
}

} // namespace KDC
