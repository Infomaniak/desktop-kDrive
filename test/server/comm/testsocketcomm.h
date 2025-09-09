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
#include "server/comm/socketcommserver.h"

#include <log4cplus/logger.h>

namespace KDC {

class SocketCommChannelTest : public SocketCommChannel {
    public:
        ~SocketCommChannelTest() {}
        bool canReadMessage() const override { return true; }
        CommString readMessage() override;
        void sendMessage(const CommString &message) override;
};

class SocketCommServerTest : public SocketCommServer {
    public:
        SocketCommServerTest(const std::string &name) :
            SocketCommServer(name) {}
        std::shared_ptr<SocketCommChannel> makeCommChannel() const override { return std::make_unique<SocketCommChannelTest>(); }
};

class TestSocketComm : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestSocketComm);
        CPPUNIT_TEST(testServerListen);
        CPPUNIT_TEST(testServerCallbacks);
        CPPUNIT_TEST(testChannelReadyReadCallback);
        CPPUNIT_TEST(testChannelReadAndWriteData);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testServerListen();
        void testServerCallbacks();
        void testChannelReadyReadCallback();
        void testChannelReadAndWriteData();
};
} // namespace KDC
