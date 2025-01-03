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

#include "libcommon/info/parametersinfo.h"

#include <memory>

namespace KDC {

class ParametersCache {
    public:
        static std::shared_ptr<ParametersCache> instance() noexcept;
        static bool isExtendedLogEnabled() noexcept { return instance()->_parametersInfo.extendedLog(); };

        ParametersCache(ParametersCache const &) = delete;
        void operator=(ParametersCache const &) = delete;

        ParametersInfo &parametersInfo() { return _parametersInfo; }
        bool saveParametersInfo(bool displayMessageBoxOnError = true);

    private:
        static std::shared_ptr<ParametersCache> _instance;
        ParametersInfo _parametersInfo;

        ParametersCache();
};

} // namespace KDC
