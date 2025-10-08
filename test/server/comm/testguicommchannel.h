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
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() final;

        void testSendMessage();
        void testReadMessage();
        void testCanReadMessage();
        void testLoginRequestTokenJob();
        void testUserDbIdListJob();

    private:
        GuiJobFactory _guiJobFactory;
};
} // namespace KDC
