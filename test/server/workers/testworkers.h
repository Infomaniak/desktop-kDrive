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

#include "testincludes.h"
#include "comm/commmanager.h"

#if defined(__APPLE__)
#include "vfs/mac/vfs_mac.h"
#elif defined(_WIN32)
#include "vfs/win/vfs_win.h"
#else
#include "vfs/vfs.h"
#endif

#include "libsyncengine/propagation/executor/executorworker.h"
#include "test_utility/localtemporarydirectory.h"

namespace KDC {

class TestWorkers : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestWorkers);
        CPPUNIT_TEST(testStartVfs);
        CPPUNIT_TEST(testCreatePlaceholder);
        CPPUNIT_TEST(testConvertToPlaceholder);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) final;
        void tearDown() override;
        void testStartVfs();
        void testCreatePlaceholder();
        void testConvertToPlaceholder();

    protected:
        static bool startVfs();

        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal;
        Sync _sync;
        LocalTemporaryDirectory _localTempDir{"TestExecutorWorker"};

        std::unique_ptr<CommManager> _commManager;

#if defined(KD_MACOS)
        static std::shared_ptr<VfsMac> _vfsPtr;
#elif defined(KD_WINDOWS)
        static std::shared_ptr<VfsWin> _vfsPtr;
#else
        static std::shared_ptr<VfsOff> _vfsPtr;
#endif

        static bool _vfsInstallationDone;
        static bool _vfsActivationDone;
        static bool _vfsConnectionDone;
};

} // namespace KDC
