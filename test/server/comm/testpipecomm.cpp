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
    CommChar data[1024] = {0};
    const auto readSize = readData(data, 1024);
    const CommString dataStr(data, readSize);
    return dataStr;
}

bool PipeCommChannelTest::sendMessage(const CommString &message) {
    (void) writeData(message.c_str(), message.size());
    return true;
}

// TestPipeComm implementation
void TestPipeComm::setUp() {
    TestBase::start();

    // Start the server
    _pipeCommServer = std::make_unique<PipeCommServerTest>("TestPipeComm::testServerListen");

    _pipeCommServer->setNewConnectionCbk([this]() {
        const auto channel = _pipeCommServer->nextPendingConnection();
        if (channel) {
            channel->setReadyReadCbk([this](std::shared_ptr<AbstractCommChannel> ch) { _lastReadyReadChannel = ch; });
        }
    });

    _pipeCommServer->setLostConnectionCbk(
            [this](std::shared_ptr<AbstractCommChannel> channel) { _lastLostConnectionChannel = channel; });
}

void TestPipeComm::tearDown() {
    TestBase::stop();
}

void TestPipeComm::testServer() {
#if defined(KD_WINDOWS)
    // Run server
    CPPUNIT_ASSERT(_pipeCommServer->listen());

    // Wait for the server initialization
    int waitCount = 100; // wait max 1 second
    while (!_pipeCommServer->isListening() && waitCount-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CPPUNIT_ASSERT(_pipeCommServer->isListening());

    // Connect a client to the server and exchange some messages
    // Repeat N times (with N > PipeCommServer::nbrOfpipeInstances()) to check connection reuse
    const SyncPath pipePath = PipeCommServer::pipePath();
    for (int i = 0; i < 2 * PipeCommServer::nbrOfpipeInstances(); i++) {
        // Connect a client to the server
        HANDLE hPipe = CreateFile(pipePath.native().c_str(), // pipe name
                                  GENERIC_READ | GENERIC_WRITE,
                                  0, // no sharing
                                  nullptr, // default security attributes
                                  OPEN_EXISTING, // opens existing pipe
                                  0, // default attributes
                                  nullptr); // no template file

        CPPUNIT_ASSERT(hPipe != INVALID_HANDLE_VALUE);

        // Send a message from the client to the server
        _lastReadyReadChannel = nullptr;

        const CommString msgClient = Str(R"(Hello word)");
        const DWORD cbToWrite = (msgClient.size() + 1) * sizeof(TCHAR);
        DWORD cbWritten = 0;

        BOOL fSuccess = WriteFile(hPipe, // pipe handle
                                  msgClient.c_str(), // message
                                  cbToWrite, // message length
                                  &cbWritten, // bytes written
                                  nullptr); // not overlapped

        CPPUNIT_ASSERT(fSuccess);
        CPPUNIT_ASSERT(cbWritten == cbToWrite);

        // Wait for the server to receive the message
        waitCount = 100; // wait max 1 second
        while (!_lastReadyReadChannel && waitCount-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CPPUNIT_ASSERT(_lastReadyReadChannel);
        CPPUNIT_ASSERT(_lastReadyReadChannel->bytesAvailable() > 0);

        // Read the message on the server side
        auto msgReceived = _lastReadyReadChannel->readMessage();
        CPPUNIT_ASSERT(msgReceived == msgClient);

        // Send a message from the server to the client
        const CommString msgServer = Str(R"(😅🍃😎🫥😶‍🌫️😰🤑🦞éè$²@à*-+/\;,:!!<>)");
        CPPUNIT_ASSERT(_lastReadyReadChannel->sendMessage(msgServer));

        // Read the message on the client side
        TCHAR chBuf[1024] = {0};
        DWORD cbRead = 0;

        fSuccess = ReadFile(hPipe, // pipe handle
                            chBuf, // buffer to receive reply
                            1024 * sizeof(TCHAR), // size of buffer
                            &cbRead, // number of bytes read
                            nullptr); // not overlapped

        CPPUNIT_ASSERT(fSuccess);

        msgReceived = CommString(chBuf, cbRead / sizeof(TCHAR));
        CPPUNIT_ASSERT(msgReceived == msgServer);

        // Close client connection
        _lastLostConnectionChannel = nullptr;

        CPPUNIT_ASSERT(CloseHandle(hPipe));

        // Wait for the server to process disconnection
        waitCount = 100; // wait max 1 second
        while (!_lastLostConnectionChannel && waitCount-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CPPUNIT_ASSERT(_lastLostConnectionChannel);
        CPPUNIT_ASSERT(_lastLostConnectionChannel->id() == _lastReadyReadChannel->id());
    }

    // Stop server
    _pipeCommServer->close();
    CPPUNIT_ASSERT(!_pipeCommServer->isListening());
    CPPUNIT_ASSERT(!_pipeCommServer->isRunning());
#endif
}

} // namespace KDC
