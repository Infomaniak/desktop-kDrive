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

#include "utility/types.h"
#include <mutex>
#include <string>

namespace KDC {

// TODO : this is a utility class with only static members and should not be a singleton...

class PlatformInconsistencyCheckerUtility {
    public:
        enum class SuffixType {
            Conflict,
            Orphan,
            Blacklisted
        };

        static std::shared_ptr<PlatformInconsistencyCheckerUtility> instance();

        bool isNameTooLong(const SyncName &name) const;
        bool isPathTooLong(size_t pathSize);
        bool nameHasForbiddenChars(const SyncPath &name);
        static bool isNameOnlySpaces(const SyncName &name);
        static bool nameEndWithForbiddenSpace(const SyncName &name);

#if defined(KD_WINDOWS)
        bool fixNameWithBackslash(const SyncName &name, SyncName &newName);
#endif
        bool checkReservedNames(const SyncName &name);
        SyncName generateNewValidName(const SyncPath &name, SuffixType suffixType);
        static ExitInfo renameLocalFile(const SyncPath &absoluteLocalPath, SuffixType suffixType, SyncPath *newPathPtr = nullptr);

    private:
        PlatformInconsistencyCheckerUtility();

        SyncName charToHex(unsigned int c);
        void setMaxPath();
        SyncName generateSuffix(SuffixType suffixType);

        static std::shared_ptr<PlatformInconsistencyCheckerUtility> _instance;
        static size_t _maxPathLength;

        friend class TestPlatformInconsistencyCheckerWorker;
};

} // namespace KDC
