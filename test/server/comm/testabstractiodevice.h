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
#include "server/comm/abstractiodevice.h"

namespace KDC {

class TestIODevice : public AbstractIODevice {
    public:
        uint64_t readData(char *data, uint64_t maxSize) override { return _buffer.copy(data, maxSize); }
        uint64_t writeData(const char *data, uint64_t maxSize) override {
            _buffer += CommString(data, maxSize);
            return maxSize;
        }
        uint64_t bytesAvailable() const override { return _buffer.size(); }
        bool canReadLine() const override { return _buffer.find('\n') != std::string::npos; }

    private:
        CommString _buffer;
};

class TestAbstractIODevice : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestAbstractIODevice);
        CPPUNIT_TEST(testAll);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testAll();

    private:
        TestIODevice *testIODevice;
};

} // namespace KDC
