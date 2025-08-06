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

#include "exclusiontemplate.h"

namespace KDC {

ExclusionTemplate::ExclusionTemplate() {
    evaluateComplexity();
}

ExclusionTemplate::ExclusionTemplate(const std::string &templ, bool warning, bool def, bool deleted) :
    _templ(templ),
    _warning(warning),
    _def(def),
    _deleted(deleted) {
    evaluateComplexity();
}

void ExclusionTemplate::evaluateComplexity() {
    std::string::size_type n;
    n = _templ.find('*');
    if (n == std::string::npos) {
        // Simplest pattern without variable part, do not use regex
        _complexity = ExclusionTemplateComplexity::Simplest;
        return;
    }

    if (n == _templ.length() - 1) {
        // OK, variable part is at the end only, do not use regex
        _complexity = ExclusionTemplateComplexity::Simple;
        return;
    } else if (n == 0) {
        // Variable part at beginning, check if there is another one
        n = _templ.find('*', n + 1);
        if (n == _templ.length() - 1 || n == std::string::npos) {
            // OK, variable part is at beginning and/or end only, do not use regex
            _complexity = ExclusionTemplateComplexity::Simple;
            return;
        }
    }

    // More complex template, use regex
    _complexity = ExclusionTemplateComplexity::Complex;
}

} // namespace KDC
