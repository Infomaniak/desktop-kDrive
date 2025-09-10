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

#include "testsocketcomm.h"
#include "log/log.h"

namespace KDC {

// Mock implementation of readMessage and sendMessage for testing purpose
CommString SocketCommChannelTest::readMessage() {
    CommChar data[1024];
    (void) readData(data, 1024);
    return data;
}

void SocketCommChannelTest::sendMessage(const CommString &message) {
    (void) writeData(message.c_str(), message.size());
}

// TestSocketComm implementation
void TestSocketComm::setUp() {
    TestBase::start();
}

void TestSocketComm::tearDown() {
    TestBase::stop();
}

void TestSocketComm::testServerListen() {
    // Start the server
    auto _socketCommServerTest = std::make_unique<SocketCommServerTest>("TestSocketComm::testServerListen");
    _socketCommServerTest->listen(SyncPath());

    // Create a client socket and connect to the server
    Poco::Net::StreamSocket clientSocket;
    clientSocket.connect(Poco::Net::SocketAddress("localhost", _socketCommServerTest->getPort()));
    auto clientSideChannel = std::make_shared<SocketCommChannelTest>(clientSocket);

    // Wait for the server to accept the connection
    while (_socketCommServerTest->connections().empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get the server side channel
    auto serverSidechannel = _socketCommServerTest->nextPendingConnection();
    CPPUNIT_ASSERT(serverSidechannel != nullptr);

    // Send a message from the client to the server
    clientSideChannel->sendMessage(Str("Hello world"));

    // Wait for the server to receive the message
    while (serverSidechannel->bytesAvailable() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Read the message on the server side
    auto message = serverSidechannel->readMessage();
    CPPUNIT_ASSERT(message.starts_with(CommString(Str("Hello world"))));
}

void TestSocketComm::testServerCallbacks() {
    // Start the server
    auto _socketCommServerTest = std::make_unique<SocketCommServerTest>("TestSocketComm::testServerCallbacks");
    _socketCommServerTest->listen(SyncPath());

    bool newConnectionCalled = false;
    bool lostConnectionCalled = false;
    std::shared_ptr<AbstractCommChannel> lostChannel = nullptr;
    _socketCommServerTest->setNewConnectionCbk([&newConnectionCalled]() { newConnectionCalled = true; });
    _socketCommServerTest->setLostConnectionCbk(
            [&lostConnectionCalled, &lostChannel](std::shared_ptr<AbstractCommChannel> channel) {
                lostConnectionCalled = true;
                lostChannel = channel;
            });

    // Create a client socket and connect to the server
    Poco::Net::StreamSocket clientSocket;
    clientSocket.connect(Poco::Net::SocketAddress("localhost", _socketCommServerTest->getPort()));
    auto clientSideChannel = std::make_shared<SocketCommChannelTest>(clientSocket);

    // Wait for the server to accept the connection
    while (_socketCommServerTest->connections().empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CPPUNIT_ASSERT_MESSAGE("New connection callback not called", newConnectionCalled);

    // Get the server side channel
    auto serverSidechannel = _socketCommServerTest->nextPendingConnection();
    CPPUNIT_ASSERT(serverSidechannel != nullptr);

    // Close the client side channel to trigger lost connection callback
    clientSideChannel->close();

    // Wait for the lost connection callback to be called
    int remainWait = 400; // wait max 4 seconds
    while (!lostConnectionCalled && remainWait-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CPPUNIT_ASSERT_MESSAGE("Lost connection callback not called", lostConnectionCalled);
    CPPUNIT_ASSERT_MESSAGE("Lost connection callback channel mismatch", lostChannel == serverSidechannel);
}

void TestSocketComm::testChannelReadyReadCallback() {
    // Start the server
    auto _socketCommServerTest = std::make_unique<SocketCommServerTest>("TestSocketComm::testChannelReadyReadCallback");
    _socketCommServerTest->listen(SyncPath());

    // Create a client socket and connect to the server
    Poco::Net::StreamSocket clientSocket;
    clientSocket.connect(Poco::Net::SocketAddress("localhost", _socketCommServerTest->getPort()));
    auto clientSideChannel = std::make_shared<SocketCommChannelTest>(clientSocket);

    // Wait for the server to accept the connection
    int remainWait = 100; // wait max 1 second
    while (_socketCommServerTest->connections().empty() && remainWait-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT_MESSAGE("Server did not accept the connection in time", _socketCommServerTest->connections().empty());

    // Get the server side channel
    auto serverSidechannel = _socketCommServerTest->nextPendingConnection();
    CPPUNIT_ASSERT(serverSidechannel != nullptr);

    bool readyReadCalled = false;
    std::shared_ptr<AbstractCommChannel> readyReadChannel = nullptr;
    serverSidechannel->setReadyReadCbk([&readyReadCalled, &readyReadChannel](std::shared_ptr<AbstractCommChannel> channel) {
        readyReadCalled = true;
        readyReadChannel = channel;
    });

    // Send a message from the client to the server
    clientSideChannel->sendMessage(Str("Hello world"));

    // Wait for the server to receive the message and trigger the ready read callback
    remainWait = 100; // wait max 1 second
    while (!readyReadCalled && remainWait-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT_MESSAGE("Ready read callback not called", readyReadCalled);
    CPPUNIT_ASSERT_MESSAGE("Ready read callback channel mismatch", readyReadChannel == serverSidechannel);
}

void TestSocketComm::testChannelReadAndWriteData() {
    // Start the server
    auto _socketCommServerTest = std::make_unique<SocketCommServerTest>("TestSocketComm::testChannelReadAndWriteData");
    CPPUNIT_ASSERT_MESSAGE("Server failed to start listening", _socketCommServerTest->listen(SyncPath()));

    // Create a client socket and connect to the server
    Poco::Net::StreamSocket clientSocket;
    clientSocket.connect(Poco::Net::SocketAddress("localhost", _socketCommServerTest->getPort()));
    auto clientSideChannel = std::make_shared<SocketCommChannelTest>(clientSocket);

    // Wait for the server to accept the connection
    while (_socketCommServerTest->connections().empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get the server side channel
    auto serverSidechannel = _socketCommServerTest->nextPendingConnection();
    CPPUNIT_ASSERT(serverSidechannel != nullptr);

    // Test messages with various characters
    std::array<CommString, 3> messages = {Str("Hello word"),
                                          Str("éè$²@à*-+/\\;,:!!<>😅🍃😎🫥😶‍🌫️😰🤑🦞"),
                                          Str("每个人都有他的作战策略")};
    for (const auto &msg: messages) {
        // Send a message from the client to the server
        clientSideChannel->sendMessage(msg);
        // Wait for the server to receive the message
        int remainWait = 100; // wait max 1 second
        while (serverSidechannel->bytesAvailable() == 0 && remainWait-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CPPUNIT_ASSERT_MESSAGE("Server did not receive the message in time", serverSidechannel->bytesAvailable() > 0);
        // Read the message on the server side
        auto message = serverSidechannel->readMessage();
        CPPUNIT_ASSERT(message.starts_with(msg)); // The mock readMessage always return a 1024 CommChar string.
    }

    // Test reading a long message in several calls to readData
    CommString longMessage;
    longMessage.reserve(200);
    for (int i = 0; i < 200; ++i) {
        longMessage += Str2SyncName(std::to_string(rand() % 10));
    }

    clientSideChannel->sendMessage(longMessage);

    // Wait for the server to receive the message
    int remainWait = 100; // wait max 1 second
    while (serverSidechannel->bytesAvailable() == 0 && remainWait-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT_MESSAGE("Server did not receive the long message in time", serverSidechannel->bytesAvailable() > 0);

    // Read the message on the server side
    CommChar data[101];
    serverSidechannel->readData(data, 99);
    auto message = CommString(data);
    CPPUNIT_ASSERT(message.starts_with(longMessage.substr(0, 99)));

    serverSidechannel->readData(data, 101);
    message = CommString(data);
    CPPUNIT_ASSERT(message.starts_with(longMessage.substr(99, 101)));
    CPPUNIT_ASSERT_EQUAL(uint64_t(0), serverSidechannel->bytesAvailable());
}
} // namespace KDC
