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

#include "testincludes.h"
#include "server/comm/pipecommserver.h"

#include <log4cplus/logger.h>

namespace KDC {

class PipeCommChannelTest : public PipeCommChannel {
    public:
        using PipeCommChannel::PipeCommChannel;

        bool canReadMessage() override { return true; }
        CommString readMessage() override;
        bool sendMessage(const CommString &message) override;
};

class PipeCommServerTest : public PipeCommServer {
    public:
        using PipeCommServer::PipeCommServer;

        std::shared_ptr<PipeCommChannel> makeCommChannel() const override { return std::make_shared<PipeCommChannelTest>(); }
};

class TestPipeComm : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestPipeComm);
        CPPUNIT_TEST(testServer);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testServer();

    private:
        std::unique_ptr<PipeCommServerTest> _pipeCommServer = nullptr;
        std::shared_ptr<AbstractCommChannel> _lastReadyReadChannel = nullptr;
        std::shared_ptr<AbstractCommChannel> _lastLostConnectionChannel = nullptr;
};
} // namespace KDC
