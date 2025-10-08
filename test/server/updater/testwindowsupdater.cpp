/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

#include "testwindowsupdater.h"

#include "io/iohelper.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "updater/windowsupdater.h"

namespace KDC {

void TestWindowsUpdater::testOnUpdateFound() {
    class WindowsUpdaterMock final : public WindowsUpdater {
        public:
            void setInstallerPath(const SyncPath &installerPath) { _installerPath = installerPath; }

        private:
            [[nodiscard]] bool getInstallerPath(SyncPath &path) const override {
                path = _installerPath;
                return true;
            }

            std::streamsize getExpectedInstallerSize([[maybe_unused]] const std::string &downloadUrl) override { return 10; }

            SyncPath _installerPath;
    };

    LocalTemporaryDirectory tempDir("TestWindowsUpdater");
    WindowsUpdaterMock testObj;
    const auto dummyInstallerPath = tempDir.path() / "TestWindowsUpdater";
    testObj.setInstallerPath(dummyInstallerPath);

    // Installer is not yet downloaded.
    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Downloading, testObj.state());

    // Installer exist locally but is empty.
    testhelpers::generateTestFile(dummyInstallerPath);

    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Downloading, testObj.state());
    CPPUNIT_ASSERT(!std::filesystem::exists(dummyInstallerPath)); // Local file has been removed.

    // Installer exist locally but incomplete.
    testhelpers::generateOrEditTestFile(dummyInstallerPath);

    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Downloading, testObj.state());
    CPPUNIT_ASSERT(!std::filesystem::exists(dummyInstallerPath)); // Local file has been removed.

    // Installer exist locally and is valid.
    testhelpers::generateTestFile(dummyInstallerPath, 10);

    testObj.onUpdateFound();
    CPPUNIT_ASSERT_EQUAL(UpdateState::Ready, testObj.state());
    CPPUNIT_ASSERT(std::filesystem::exists(dummyInstallerPath));
}

} // namespace KDC
