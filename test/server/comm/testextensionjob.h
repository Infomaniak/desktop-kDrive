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

#pragma once

#include "testincludes.h"
#include "server/comm/abstractcommchannel.h"

namespace KDC {

class MockCommChannel final : public AbstractCommChannel {
    public:
        void close() override {}
        bool sendMessage(const CommString &) override { return true; }
        bool canReadMessage() override { return false; }
        CommString readMessage() override { return {}; }
        uint64_t bytesAvailable() const override { return 0; }

    protected:
        uint64_t readData(CommChar *, uint64_t) override { return 0; }
        uint64_t writeData(const CommChar *, uint64_t) override { return 0; }
};

class TestExtensionJob : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestExtensionJob);
        CPPUNIT_TEST(testRunJobWithMalformedArguments);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() final;

        void testRunJobWithMalformedArguments();
};

} // namespace KDC
