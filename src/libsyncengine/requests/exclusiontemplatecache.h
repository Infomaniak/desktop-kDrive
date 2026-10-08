/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
#include "libcommon/data/exclusiontemplate.h"
#include "libcommon/utility/types.h"

#include <vector>
#include <regex>
#include <mutex>

namespace KDC {

class SYNCENGINE_EXPORT ExclusionTemplateCache {
    public:
        static std::shared_ptr<ExclusionTemplateCache> instance();
        static void reset();

        ExclusionTemplateCache(ExclusionTemplateCache const &) = delete;

        void operator=(ExclusionTemplateCache const &) = delete;

        [[nodiscard]] const std::vector<ExclusionTemplate> &exclusionTemplates() const { return _exclusionTemplates; }
        [[nodiscard]] const std::vector<ExclusionTemplate> &exclusionTemplates(const bool selectDefault) const {
            return selectDefault ? _defaultExclusionTemplates : _userExclusionTemplates;
        }

        ExitCode update(bool def, const std::vector<ExclusionTemplate> &exclusionTemplates);

        bool isExcluded(const SyncPath &relativePath) noexcept;
        bool isExcluded(const SyncPath &relativePath, bool &isWarning) noexcept;

    private:
        friend class TestExclusionTemplateCache;
        static std::shared_ptr<ExclusionTemplateCache> _instance;
        std::vector<ExclusionTemplate> _exclusionTemplates;
        std::vector<ExclusionTemplate> _defaultExclusionTemplates;
        std::vector<ExclusionTemplate> _userExclusionTemplates;
        std::vector<std::pair<std::regex, ExclusionTemplate>> _regexPatterns;

        std::mutex _mutex;

        ExclusionTemplateCache();

        void populateExclusionTemplates();

        void updateRegexPatterns();

        static void escapeRegexSpecialChar(std::string &in);

        void addRegexForAllNormalizationForms(const std::string &regexPattern, ExclusionTemplate exclusionTemplate);
};

} // namespace KDC
