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

#include "testabstractcommchannel.h"
#include "log/log.h"

namespace KDC {

void TestAbstractCommChannel::setUp() {
    TestBase::start();

    _logger = Log::instance()->getLogger();
    //_commChannelTest = std::make_unique<CommChannelTest>();
}

void TestAbstractCommChannel::tearDown() {
    TestBase::stop();
}

void TestAbstractCommChannel::testAll() {
    /* CPPUNIT_ASSERT(_commChannelTest->open());

    CommString message1(Str("Hello word"));
    _commChannelTest->sendMessage(message1);
    auto bytesWritten = message1.size() + 1; // +1 for message separator
    CPPUNIT_ASSERT(_commChannelTest->bytesAvailable() == bytesWritten);
    CPPUNIT_ASSERT(_commChannelTest->canReadMessage());

    CommString message2(Str("How are you ?"));
    _commChannelTest->sendMessage(message2);
    bytesWritten += message2.size() + 1; // +1 for message separator
    CPPUNIT_ASSERT(_commChannelTest->bytesAvailable() == bytesWritten);
    CPPUNIT_ASSERT(_commChannelTest->canReadMessage());

    // Write Chinese text
    CommString message3(Str("每个人都有他的作战策略"));
    _commChannelTest->sendMessage(message3);
    bytesWritten += message3.size() + 1; // +1 for message separator
    CPPUNIT_ASSERT(_commChannelTest->bytesAvailable() == bytesWritten);
    CPPUNIT_ASSERT(_commChannelTest->canReadMessage());

    // Write long text (> 1024 chars)
    CommString message4(100000, Str('x'));
    _commChannelTest->sendMessage(message4);
    bytesWritten += message4.size() + 1; // +1 for message separator
    CPPUNIT_ASSERT(_commChannelTest->bytesAvailable() == bytesWritten);
    CPPUNIT_ASSERT(_commChannelTest->canReadMessage());

    CommString line1 = _commChannelTest->readMessage();
    CPPUNIT_ASSERT(line1 == message1);
    CommString line2 = _commChannelTest->readMessage();
    CPPUNIT_ASSERT(line2 == message2);
    CommString line3 = _commChannelTest->readMessage();
    CPPUNIT_ASSERT(line3 == message3);
    CommString line4 = _commChannelTest->readMessage();
    CPPUNIT_ASSERT(line4 == message4);

    CPPUNIT_ASSERT(_commChannelTest->bytesAvailable() == 0);
    CPPUNIT_ASSERT(!_commChannelTest->canReadMessage());

    _commChannelTest->close();*/
}

} // namespace KDC
