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

namespace KDC {

// Mock implementation of readMessage and sendMessage for testing purpose
CommString PipeCommChannelTest::readMessage() {
    CommChar data[1024];
    auto readSize = readData(data, 1024);
    CommString dataStr(data, readSize);
    return dataStr;
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
    auto pipeCommServer = std::make_unique<PipeCommServerTest>("TestPipeComm::testServerListen");

    bool newConnectionCalled = false;
    bool lostConnectionCalled = false;
    std::shared_ptr<AbstractCommChannel> lostChannel = nullptr;
    pipeCommServer->setNewConnectionCbk([&newConnectionCalled]() { newConnectionCalled = true; });
    pipeCommServer->setLostConnectionCbk([&lostConnectionCalled, &lostChannel](std::shared_ptr<AbstractCommChannel> channel) {
        lostConnectionCalled = true;
        lostChannel = channel;
    });

    CPPUNIT_ASSERT(pipeCommServer->listen());
    // Wait for the server initialization
    int waitCount = 100; // wait max 1 second
    while (!pipeCommServer->isListening() && waitCount-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT(pipeCommServer->isListening());

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

        // Wait for the server to process the connection
        waitCount = 100; // wait max 1 second
        while (pipeCommServer->connections().size() == 0 && waitCount-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CPPUNIT_ASSERT(newConnectionCalled);

        // Get the server side connected channel list
        auto channelList = pipeCommServer->connections();
        CPPUNIT_ASSERT(channelList.size() > 0);

        bool readyReadCalled = false;
        std::shared_ptr<AbstractCommChannel> readyReadChannel = nullptr;
        for (auto &channel: channelList) {
            channel->setReadyReadCbk([&readyReadCalled, &readyReadChannel](std::shared_ptr<AbstractCommChannel> channel) {
                readyReadCalled = true;
                readyReadChannel = channel;
            });
        }

        // Send a message from the client to the server
        CommString msgClient = Str(R"(Hello word)");
        DWORD cbToWrite = (msgClient.size() + 1) * sizeof(TCHAR);
        DWORD cbWritten = 0;

        BOOL fSuccess = WriteFile(hPipe, // pipe handle
                                  msgClient.c_str(), // message
                                  cbToWrite, // message length
                                  &cbWritten, // bytes written
                                  NULL); // not overlapped

        CPPUNIT_ASSERT(fSuccess);
        CPPUNIT_ASSERT(cbWritten == cbToWrite);

        // Wait for the server to receive the message
        waitCount = 100; // wait max 1 second
        while (!readyReadCalled && waitCount-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CPPUNIT_ASSERT(readyReadCalled);
        CPPUNIT_ASSERT(readyReadChannel->bytesAvailable() > 0);

        // Read the message on the server side
        auto msgReceived = readyReadChannel->readMessage();
        CPPUNIT_ASSERT(msgReceived == msgClient);

        // Send a message from the server to the client
        CommString msgServer = Str(R"(😅🍃😎🫥😶‍🌫️😰🤑🦞éè$²@à*-+/\;,:!!<>)");
        CPPUNIT_ASSERT(readyReadChannel->sendMessage(msgServer));

        // Read the message on the client side
        TCHAR chBuf[1024] = {0};
        DWORD cbRead = 0;

        fSuccess = ReadFile(hPipe, // pipe handle
                            chBuf, // buffer to receive reply
                            1024 * sizeof(TCHAR), // size of buffer
                            &cbRead, // number of bytes read
                            NULL); // not overlapped

        CPPUNIT_ASSERT(fSuccess);

        msgReceived = CommString(chBuf, cbRead / sizeof(TCHAR));
        CPPUNIT_ASSERT(msgReceived == msgServer);

        // Close client connection
        CloseHandle(hPipe);

        // Wait for the server to process disconnection
        waitCount = 100; // wait max 1 second
        while (!lostConnectionCalled && waitCount-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CPPUNIT_ASSERT(lostConnectionCalled);
    }
#endif
}

} // namespace KDC
