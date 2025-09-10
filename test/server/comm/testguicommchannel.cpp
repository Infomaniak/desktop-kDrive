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

#include "testguicommchannel.h"
#include "log/log.h"

namespace KDC {

uint64_t GuiCommChannelTest::readData(CommChar *data, uint64_t maxlen) {
    std::scoped_lock lock(_bufferMutex);
    uint64_t toRead = (std::min)(maxlen, static_cast<uint64_t>(_buffer.size()));
    if (toRead > 0) {
        std::memcpy(data, _buffer.data(), toRead * sizeof(CommChar));
        _buffer.erase(0, toRead);
        return toRead;
    }
    return toRead;
}

uint64_t GuiCommChannelTest::writeData(const CommChar *data, uint64_t len) {
    std::scoped_lock lock(_bufferMutex);
    _buffer.append(data, len);
    return len;
}

uint64_t GuiCommChannelTest::bytesAvailable() const {
    std::scoped_lock lock(_bufferMutex);
    return _buffer.size();
}

void TestGuiCommChannel::setUp() {
    TestBase::start();
}

void TestGuiCommChannel::tearDown() {
    TestBase::stop();
}

void TestGuiCommChannel::testSendMessage() {
    GuiCommChannelTest channelTest;

    CommString message = Str("{\"type\":\"test\",\"content\":\"Hello, World!\"}");
    CPPUNIT_ASSERT(channelTest.sendMessage(message)); // Valid JSON

    message = Str("{\"type\":\"test\",\"content\":\"");
    CPPUNIT_ASSERT(!channelTest.sendMessage(message)); // Invalid JSON

    message = Str(""); // Empty message
    CPPUNIT_ASSERT(!channelTest.sendMessage(message)); // Invalid JSON
}

void TestGuiCommChannel::testReadMessage() {
    std::shared_ptr<GuiCommChannelTest> channelTest = std::make_shared<GuiCommChannelTest>();
    bool readyReadCalled = false;
    std::shared_ptr<AbstractCommChannel> readyReadChannel;
    channelTest->setReadyReadCbk([&readyReadCalled, &readyReadChannel](std::shared_ptr<AbstractCommChannel> channel) {
        readyReadCalled = true;
        readyReadChannel = channel;
    });

    // Read a single JSON
    CommString message[3] = {Str("{\"type\":\"test\",\"content\":\"Hello, World!\"}"),
                             Str("{\"type\":\"test\",\"content\":\"Another message\"}"),
                             Str("{\"type\":\"test\",\"content\":\"Final message\"}")};

    CPPUNIT_ASSERT(channelTest->sendMessage(message[0]));

    CommString readMessage = channelTest->readMessage();
    CPPUNIT_ASSERT(message[0] == readMessage);

    // Read with multiple JSON in buffer
    CPPUNIT_ASSERT(channelTest->sendMessage(message[0]));
    CPPUNIT_ASSERT(channelTest->sendMessage(message[1]));
    CPPUNIT_ASSERT(channelTest->sendMessage(message[2]));

    readyReadCalled = false;
    readyReadChannel = nullptr;
    for (int i = 0; i < 3; ++i) {
        readMessage = channelTest->readMessage();
        CPPUNIT_ASSERT(message[i] == readMessage);
        if (i < 2) {
            CPPUNIT_ASSERT(readyReadCalled); // Should have been called for the first two messages
            CPPUNIT_ASSERT(readyReadChannel == channelTest);
            readyReadCalled = false; // Reset for next iteration
            readyReadChannel = nullptr;
        } else {
            CPPUNIT_ASSERT(!readyReadCalled); // Should not be called after the last message
            CPPUNIT_ASSERT(readyReadChannel == nullptr);
        }
    }

    // Read partial JSON
    CPPUNIT_ASSERT(
            channelTest->writeData(message[0].substr(0, 10).c_str(), 10)); // Partial (use write data to bypass sendMessage check)
    readMessage = channelTest->readMessage();
    CPPUNIT_ASSERT(readMessage.empty()); // Should be empty as JSON is incomplete

    CPPUNIT_ASSERT(channelTest->writeData(message[0].substr(10).c_str(), message[0].size() - 10)); // Complete the JSON
    readMessage = channelTest->readMessage();
    CPPUNIT_ASSERT(message[0] == readMessage); // Now should be complete
}

void TestGuiCommChannel::testCanReadMessage() {
    GuiCommChannelTest channelTest;
    CommString message = Str("{\"type\":\"test\",\"content\":\"Hello, World!\"}");
    CPPUNIT_ASSERT(channelTest.sendMessage(message)); // Valid JSON
    CPPUNIT_ASSERT(channelTest.canReadMessage()); // Should be able to read a message
    CommString readMessage = channelTest.readMessage();
    CPPUNIT_ASSERT(message == readMessage);
    CPPUNIT_ASSERT(!channelTest.canReadMessage()); // No more messages to read

    // Send partial JSON
    CPPUNIT_ASSERT(
            channelTest.writeData(message.substr(0, 10).c_str(), 10)); // Partial (use write data to bypass sendMessage check)
    CPPUNIT_ASSERT(!channelTest.canReadMessage()); // Should not be able to read a message
    CPPUNIT_ASSERT(channelTest.writeData(message.substr(10).c_str(), message.size() - 10)); // Complete the JSON
    CPPUNIT_ASSERT(channelTest.canReadMessage()); // Now should be able to read a message
}
} // namespace KDC
