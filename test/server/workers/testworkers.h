/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#if defined(__APPLE__)
#include "server/vfs/mac/vfs_mac.h"
#elif defined(__WIN32)
#include "server/vfs/win/vfs_win.h"
#else
#include "libcommonserver/vfs.h"
#endif

#include "propagation/executor/executorworker.h"
#include "test_utility/localtemporarydirectory.h"

namespace KDC {

class TestWorkers : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestWorkers);
        CPPUNIT_TEST(testCreatePlaceholder);
        CPPUNIT_TEST(testConvertToPlaceholder);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp(void) final;
        void tearDown() override;
        void testCreatePlaceholder();
        void testConvertToPlaceholder();

    protected:
        static bool createPlaceholder(int syncDbId, const SyncPath &relativeLocalPath, const SyncFileItem &item);
        static bool convertToPlaceholder(int syncDbId, const SyncPath &relativeLocalPath, const SyncFileItem &item);
        static bool setPinState(int syncDbId, const SyncPath &relativeLocalPath, PinState pinState);

        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal;
        Sync _sync;
        LocalTemporaryDirectory _localTempDir{"TestExecutorWorker"};

#if defined(__APPLE__)
        static std::unique_ptr<VfsMac> _vfsPtr;
#elif defined(__WIN32)
        static std::unique_ptr<VfsWin> _vfsPtr;
#else
        static std::unique_ptr<VfsOff> _vfsPtr;
#endif
};

} // namespace KDC
