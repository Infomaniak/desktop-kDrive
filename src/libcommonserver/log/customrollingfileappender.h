/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>

namespace KDC {

class CustomRollingFileAppender : public log4cplus::RollingFileAppender {
    public:
        CustomRollingFileAppender(const log4cplus::tstring &filename,
                                  long maxFileSize = 10 * 1024 * 1024,  // 10 MB
                                  int maxBackupIndex = 1, bool immediateFlush = true, bool createDirs = false);
        CustomRollingFileAppender(const log4cplus::helpers::Properties &properties);

        inline int expire() const { return _expire; }
        inline void setExpire(int newExpire) {
            _expire = newExpire;
            _lastExpireCheck = std::chrono::system_clock::time_point(); // Force check on next append
        }

    protected:
        void append(const log4cplus::spi::InternalLoggingEvent &event) override;
        void rollover(bool alreadyLocked = false);

    private:
        int _expire = 0;
        std::chrono::time_point<std::chrono::system_clock> _lastExpireCheck;

        void checkForExpiredFiles() noexcept(false);
};

}  // namespace KDC
