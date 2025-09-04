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
#include "server/comm/abstractcommchannel.h"

#include <log4cplus/logger.h>

namespace KDC {

class CommChannelTest : public AbstractCommChannel {
    public:
        ~CommChannelTest() {}
        uint64_t readData(CommChar *data, uint64_t maxSize) override {
            auto size = _inBuffer.copy(data, maxSize);
            _inBuffer.erase(0, size);
            return size;
        }
        uint64_t writeData(const CommChar *data, uint64_t maxSize) override {
            _inBuffer.append(data, maxSize);
            return maxSize;
        }
        uint64_t bytesAvailable() const override { return _inBuffer.size(); }
        bool canReadLine() const override { return _inBuffer.find('\n') != std::string::npos; }

    private:
        CommString _inBuffer; // Write & read to/from the same buffer
};

class TestAbstractCommChannel : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestAbstractCommChannel);
        CPPUNIT_TEST(testAll);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testAll();

    private:
        log4cplus::Logger _logger;
        std::unique_ptr<CommChannelTest> _commChannelTest;
};

} // namespace KDC
