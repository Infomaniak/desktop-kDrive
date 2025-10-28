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

#include "testpipecomm.h"
#include "log/log.h"

namespace KDC {

// Mock implementation of readMessage and sendMessage for testing purpose
CommString PipeCommChannelTest::readMessage() {
    CommChar data[1024];
    (void) readData(data, 1024);
    return data;
}

bool PipeCommChannelTest::sendMessage(const CommString &message) {
    (void) writeData(message.c_str(), message.size());
    return true;
}

// TestPipeComm implementation
void TestPipeComm::setUp() {
    TestBase::start();
}

void TestPipeComm::tearDown() {
    TestBase::stop();
}

void TestPipeComm::testServer() {
#if defined(KD_WINDOWS)
    // Start the server
    auto pipeCommServerTest = std::make_unique<PipeCommServerTest>("TestPipeComm::testServerListen");
    pipeCommServerTest->listen();

    bool newConnectionCalled = false;
    bool lostConnectionCalled = false;
    std::shared_ptr<AbstractCommChannel> lostChannel = nullptr;
    pipeCommServerTest->setNewConnectionCbk([&newConnectionCalled]() { newConnectionCalled = true; });
    pipeCommServerTest->setLostConnectionCbk([&lostConnectionCalled, &lostChannel](std::shared_ptr<AbstractCommChannel> channel) {
        lostConnectionCalled = true;
        lostChannel = channel;
    });

    // Connect a client to the server and exchange some messages
    // Repeat N times (with N > PIPE_INSTANCES) to check connection reuse
    for (int i = 0; i < 20; i++) {
        // Connect a client to the server
        SyncPath pipePath = PipeCommServer::pipePath();

        HANDLE hPipe = CreateFile(pipePath.native().c_str(), // pipe name
                                  GENERIC_READ | GENERIC_WRITE,
                                  0, // no sharing
                                  NULL, // default security attributes
                                  OPEN_EXISTING, // opens existing pipe
                                  0, // default attributes
                                  NULL); // no template file

        CPPUNIT_ASSERT(hPipe != INVALID_HANDLE_VALUE);

        CPPUNIT_ASSERT(newConnectionCalled);
        CPPUNIT_ASSERT(!pipeCommServerTest->connections().empty());

        // Get the server side channel
        auto serverSidechannel = pipeCommServerTest->nextPendingConnection();
        CPPUNIT_ASSERT(serverSidechannel != nullptr);

        bool readyReadCalled = false;
        std::shared_ptr<AbstractCommChannel> readyReadChannel = nullptr;
        serverSidechannel->setReadyReadCbk([&readyReadCalled, &readyReadChannel](std::shared_ptr<AbstractCommChannel> channel) {
            readyReadCalled = true;
            readyReadChannel = channel;
        });

        // Send some messages from the client to the server
        std::array<CommString, 3> messages = {Str("Hello word"),
                                              Str("éè$²@à*-+/\\;,:!!<>😅🍃😎🫥😶‍🌫️😰🤑🦞"),
                                              Str("每个人都有他的作战策略")};

        for (const auto &msg: messages) {
            DWORD cbToWrite = (msg.size() + 1) * sizeof(TCHAR);
            DWORD cbWritten = 0;

            BOOL fSuccess = WriteFile(hPipe, // pipe handle
                                      msg.c_str(), // message
                                      cbToWrite, // message length
                                      &cbWritten, // bytes written
                                      NULL); // not overlapped

            CPPUNIT_ASSERT(fSuccess);
        }

        // Wait for the server to receive the messages
        int remainWait = 100; // wait max 1 second
        while (serverSidechannel->bytesAvailable() == 0 && remainWait-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CPPUNIT_ASSERT(serverSidechannel->bytesAvailable() > 0);
        CPPUNIT_ASSERT(readyReadCalled);

        // Read the messages on the server side
        for (const auto &msg: messages) {
            auto msgReceived = serverSidechannel->readMessage();
            CPPUNIT_ASSERT(msgReceived == msg);
        }

        // Send a message from the server to the client
        CommString msg = Str("😅🍃😎🫥😶‍🌫️😰🤑🦞éè$²@à*-+/\\;,:!!<>");
        CPPUNIT_ASSERT(serverSidechannel->sendMessage(msg));

        // Read the message on the client side
        TCHAR chBuf[1024];
        DWORD cbRead = 0;

        BOOL fSuccess = ReadFile(hPipe, // pipe handle
                                 chBuf, // buffer to receive reply
                                 1024 * sizeof(TCHAR), // size of buffer
                                 &cbRead, // number of bytes read
                                 NULL); // not overlapped

        CPPUNIT_ASSERT(fSuccess);

        CommString msgReceived(chBuf);
        CPPUNIT_ASSERT(msgReceived == msg);

        // Close client connection
        CloseHandle(hPipe);
        CPPUNIT_ASSERT(lostConnectionCalled);
    }
#endif
}

/*

void TestSocketComm::testChannelReadAndWriteData() {
    // Start the server
    auto _socketCommServerTest = std::make_unique<SocketCommServerTest>("TestSocketComm::testChannelReadAndWriteData");
    CPPUNIT_ASSERT_MESSAGE("Server failed to start listening", _socketCommServerTest->listen());

    // Create a client socket and connect to the server
    Poco::Net::StreamSocket clientSocket;
    clientSocket.connect(Poco::Net::SocketAddress("localhost", _socketCommServerTest->getPort()));
    auto clientSideChannel = std::make_shared<SocketCommChannelTest>(clientSocket);

    // Wait for the server to accept the connection
    int remainWait = 100; // wait max 1 second
    while (_socketCommServerTest->connections().empty() && remainWait-- > 0) {
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
    remainWait = 100; // wait max 1 second
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
*/
} // namespace KDC
