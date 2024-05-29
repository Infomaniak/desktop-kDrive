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

#include "libcommon/utility/types.h"

#include <string>

namespace KDC {

class ExclusionTemplate {
    public:
        ExclusionTemplate();
        ExclusionTemplate(const std::string &templ, bool warning = false, bool def = false, bool deleted = false);

        inline void setTempl(const std::string &templ) {
            _templ = templ;
            evaluateComplexity();
        }
        inline const std::string &templ() const { return _templ; }
        inline void setWarning(bool warning) { _warning = warning; }
        inline bool warning() const { return _warning; }
        inline void setDef(bool def) { _def = def; }
        inline bool def() const { return _def; }
        inline void setDeleted(bool deleted) { _deleted = deleted; }
        inline bool deleted() const { return _deleted; }
        inline ExclusionTemplateComplexity complexity() const { return _complexity; }

    private:
        std::string _templ;
        bool _warning {false};
        bool _def {false};
        bool _deleted {false};
        ExclusionTemplateComplexity _complexity = ExclusionTemplateComplexityComplex;

        void evaluateComplexity();
};

}  // namespace KDC
