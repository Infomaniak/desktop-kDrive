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

#include "testsocketcommserver.h"
#include "log/log.h"

namespace KDC {

// Mock implementation of readMessage and sendMessage for testing purpose
CommString SocketCommChannelTest::readMessage() {
    CommChar data[1024];
    readData(data, 1024);
    return data;
}

void SocketCommChannelTest::sendMessage(const CommString &message) {
    writeData(message.c_str(), message.size());
}

// TestSocketCommServer implementation
void TestSocketCommServer::setUp() {
    TestBase::start();
}

void TestSocketCommServer::tearDown() {
    TestBase::stop();
}

void TestSocketCommServer::testListen() {
    // Start the server
    std::unique_ptr<SocketCommServerTest> _socketCommServerTest =
            std::make_unique<SocketCommServerTest>("TestSocketCommServer::testListen");
    _socketCommServerTest->listen(SyncPath());

    // Create a client socket and connect to the server
    Poco::Net::StreamSocket clientSocket;
    clientSocket.connect(Poco::Net::SocketAddress("localhost", 12345));
    SocketCommChannelTest clientSideChannel;
    clientSideChannel.setSocket(std::move(clientSocket));

    // Wait for the server to accept the connection
    while (_socketCommServerTest->connections().size() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get the server side channel
    auto serverSidechannel = _socketCommServerTest->nextPendingConnection();
    CPPUNIT_ASSERT(serverSidechannel != nullptr);

    // Send a message from the client to the server
    clientSideChannel.sendMessage(Str("Hello world"));

    // Wait for the server to receive the message
    while (serverSidechannel->bytesAvailable() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Read the message on the server side
    auto message = serverSidechannel->readMessage();
    CPPUNIT_ASSERT(message.starts_with(CommString(Str("Hello world"))));

    clientSideChannel.close();
    serverSidechannel->close();
    _socketCommServerTest->close();
}

void TestSocketCommServer::testServerCbk() {
    // Start the server
    std::unique_ptr<SocketCommServerTest> _socketCommServerTest =
            std::make_unique<SocketCommServerTest>("TestSocketCommServer::testServerCbk");
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
    clientSocket.connect(Poco::Net::SocketAddress("localhost", 12345));
    SocketCommChannelTest clientSideChannel;
    clientSideChannel.setSocket(std::move(clientSocket));

    // Wait for the server to accept the connection
    while (_socketCommServerTest->connections().size() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CPPUNIT_ASSERT_MESSAGE("New connection callback not called", newConnectionCalled);

    // Get the server side channel
    auto serverSidechannel = _socketCommServerTest->nextPendingConnection();
    CPPUNIT_ASSERT(serverSidechannel != nullptr);

    // Close the client side channel to trigger lost connection callback
    clientSideChannel.close();
    auto _ = serverSidechannel->readMessage(); // Force read to detect closed connection

    CPPUNIT_ASSERT_MESSAGE("Lost connection callback not called", lostConnectionCalled);
    CPPUNIT_ASSERT_MESSAGE("Lost connection callback channel mismatch", lostChannel == serverSidechannel);
}

void TestSocketCommServer::testReadyReadCbkHandler() {
    // Start the server
    std::unique_ptr<SocketCommServerTest> _socketCommServerTest =
            std::make_unique<SocketCommServerTest>("TestSocketCommServer::testReadyReadCbkHandler");
    _socketCommServerTest->listen(SyncPath());

    // Create a client socket and connect to the server
    Poco::Net::StreamSocket clientSocket;
    clientSocket.connect(Poco::Net::SocketAddress("localhost", 12345));
    SocketCommChannelTest clientSideChannel;
    clientSideChannel.setSocket(std::move(clientSocket));

    // Wait for the server to accept the connection
    int remainWait = 100; // wait max 1 second
    while (_socketCommServerTest->connections().size() == 0 && remainWait-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT_MESSAGE("Server did not accept the connection in time", _socketCommServerTest->connections().size() > 0);

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
    clientSideChannel.sendMessage(Str("Hello world"));

    // Wait for the server to receive the message and trigger the ready read callback
    remainWait = 100; // wait max 1 second
    while (!readyReadCalled && remainWait-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT_MESSAGE("Ready read callback not called", readyReadCalled);
    CPPUNIT_ASSERT_MESSAGE("Ready read callback channel mismatch", readyReadChannel == serverSidechannel);
}
} // namespace KDC
