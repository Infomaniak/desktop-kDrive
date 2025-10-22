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

#include "utility.h"

namespace KDC {

struct DigitalSignatureInfo {
        SyncName _programName;
        SyncName _serialNumber;
        SyncName _issuerName;
        SyncName _subject;
};

class DigitalSignatureChecker_win {
    public:
        explicit DigitalSignatureChecker_win(const SyncPath &packageAbsolutePath);

        bool isSignatureValid() const {
            return _signatureIsValid && CommonUtility::containsInsensitive(_signatureInfo._subject, Str("Infomaniak"));
        }

    private:
        bool extractSignatureInfo(DigitalSignatureInfo &signatureInfo, std::wstring &errorMsg);

        SyncPath _packageAbsolutePath;
        DigitalSignatureInfo _signatureInfo;
        bool _signatureIsValid{false};
};

} // namespace KDC
