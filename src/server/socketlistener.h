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

#pragma once

#include <QBitArray>
#include <QIODevice>
#include <QPointer>
#include <thread>

namespace KDC {

class BloomFilter {
        // Initialize with m=1024 bits and k=2 (high and low 16 bits of a qHash).
        // For a client navigating in less than 100 directories, this gives us a probability less than (1-e^(-2*100/1024))^2 =
        // 0.03147872136 false positives.
        const static int NumBits = 1024;

    public:
        BloomFilter();

        void storeHash(uint hash);
        bool isHashMaybeStored(uint hash) const;

    private:
        QBitArray hashBits;
};

class SocketListener {
    public:
        QPointer<QIODevice> socket;

        explicit SocketListener(QIODevice *socket);

        void sendMessage(const QString &message, bool doWait = false) const;

        void sendMessageIfDirectoryMonitored(const QString &message, uint systemDirectoryHash) const;

        void registerMonitoredDirectory(uint systemDirectoryHash);

    private:
        BloomFilter _monitoredDirectoriesBloomFilter;
        std::thread::id _threadId;
};

} // namespace KDC
