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

#include "syncenginelib.h"
#include "libparms/db/parameters.h"
#include "libcommon/utility/types.h"

#include <memory>

namespace KDC {

class SYNCENGINE_EXPORT ParametersCache {
    public:
        static std::shared_ptr<ParametersCache> instance(bool isTest = false);
        inline static bool isExtendedLogEnabled() noexcept { return instance()->_parameters.extendedLog(); };

        ParametersCache(ParametersCache const &) = delete;
        void operator=(ParametersCache const &) = delete;

        inline Parameters &parameters() { return _parameters; }
        ExitCode save();

        void setUploadSessionParallelThreads(int count); // For testing purpose
        void decreaseUploadSessionParallelThreads();

    private:
        static std::shared_ptr<ParametersCache> _instance;
        Parameters _parameters;

        ParametersCache(bool isTest = false);
};

} // namespace KDC
