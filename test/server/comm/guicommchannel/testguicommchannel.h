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
#include "server/comm/guicommserver.h"
#include "server/comm/guijobs/guijobfactory.h"

#include <log4cplus/logger.h>

namespace KDC {
class GuiCommChannelTest : public GuiCommChannel {
    public:
        GuiCommChannelTest() :
#if defined(KD_WINDOWS) || defined(KD_LINUX)
            GuiCommChannel(Poco::Net::StreamSocket()) {
        }
#else
            GuiCommChannel(nullptr) {
        }
#endif
        ~GuiCommChannelTest() = default;

        // Abstract the underlying read and write data to be able to test it easily
        uint64_t readData(CommChar *data, uint64_t maxlen) override;
        uint64_t writeData(const CommChar *data, uint64_t len) override;
        uint64_t bytesAvailable() const override;

        mutable std::mutex _bufferMutex;
        CommString _buffer;
};

class TestGuiCommChannel : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestGuiCommChannel);
        CPPUNIT_TEST(testSendMessage);
        CPPUNIT_TEST(testReadMessage);
        CPPUNIT_TEST(testCanReadMessage);
        CPPUNIT_TEST(testLoginRequestTokenJob);
        CPPUNIT_TEST(testUserDbIdListJob);
        CPPUNIT_TEST(testUserInfoListJob);
        CPPUNIT_TEST(testUserDeleteJob);
        CPPUNIT_TEST(testUserAvailableDrivesJob);
        CPPUNIT_TEST(testAccountInfoListJob);
        CPPUNIT_TEST(testDriveInfoListJob);
        CPPUNIT_TEST(testDriveUpdateJob);
        CPPUNIT_TEST(testDriveDeleteJob);
        CPPUNIT_TEST(testDriveSearchJob);
        CPPUNIT_TEST(testSyncInfoListJob);
        CPPUNIT_TEST(testStartSyncJob);
        CPPUNIT_TEST(testStopSyncJob);
        CPPUNIT_TEST(testSyncStatusJob);
        CPPUNIT_TEST(testSyncAddJob);
        CPPUNIT_TEST(testSyncAdd2Job);
        CPPUNIT_TEST(testSyncStartAfterLoginJob);
        CPPUNIT_TEST(testSyncDeleteJob);
        CPPUNIT_TEST(testSyncGetPublicLinkUrlJob);
        CPPUNIT_TEST(testSyncGetPrivateLinkUrlJob);
        CPPUNIT_TEST(testSyncSetSupportsVirtualFilesJob);
        CPPUNIT_TEST(testSyncSetRootPinStateJob);
        /* Deactivated because string values are not yet base64-encoded
         * in these tests.
        CPPUNIT_TEST(testNodeInfoJob);
        CPPUNIT_TEST(testNodeSubFolderJob);
        CPPUNIT_TEST(testNodeFolderSizeJob);
        CPPUNIT_TEST(testSyncNodeListJob);
        CPPUNIT_TEST(testSyncNodeSetListJob);
         */
        CPPUNIT_TEST(testNodeSubFolders2Job);
        CPPUNIT_TEST(testUpdaterChangeChannelJob);
        CPPUNIT_TEST(testUpdaterVersionInfoJob);
        CPPUNIT_TEST(testUpdaterStateJob);
        CPPUNIT_TEST(testUpdaterStartInstallerJob);
        CPPUNIT_TEST(testUpdaterSkipVersionJob);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() final;

        void testSendMessage();
        void testReadMessage();
        void testCanReadMessage();
        void testLoginRequestTokenJob();
        void testUserDbIdListJob();
        void testUserInfoListJob();
        void testUserDeleteJob();
        void testUserAvailableDrivesJob();
        void testAccountInfoListJob();
        void testDriveInfoListJob();
        void testDriveUpdateJob();
        void testDriveDeleteJob();
        void testDriveSearchJob();
        void testSyncInfoListJob();
        void testStartSyncJob();
        void testStopSyncJob();
        void testSyncStatusJob();
        void testSyncAddJob();
        void testSyncAdd2Job();
        void testSyncStartAfterLoginJob();
        void testSyncDeleteJob();
        void testSyncGetPublicLinkUrlJob();
        void testSyncGetPrivateLinkUrlJob();
        void testSyncSetSupportsVirtualFilesJob();
        void testSyncSetRootPinStateJob();
        void testNodeInfoJob();
        void testNodeSubFolders2Job();
        void testNodeSubFolderJob();
        void testNodeFolderSizeJob();
        void testSyncNodeListJob();
        void testSyncNodeSetListJob();
        void testUpdaterChangeChannelJob();
        void testUpdaterVersionInfoJob();
        void testUpdaterStateJob();
        void testUpdaterStartInstallerJob();
        void testUpdaterSkipVersionJob();

    private:
        GuiJobFactory _guiJobFactory;

        void testGenericJob(const CommString &query, const CommString &answer, const CommString &cbkAnswer,
                            const std::function<void(std::shared_ptr<AbstractGuiJob>)> &processFct);
};
} // namespace KDC
