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

#include "server/comm/guijobs/abstractguijob.h"

#include "libcommon/info/exclusiontemplateinfo.h"

namespace KDC {

class ExclTemplSetListJob : public AbstractGuiJob {
    public:
        ExclTemplSetListJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                            std::shared_ptr<AbstractCommChannel> channel);

        // Setters for compatibility with legacy comm layer
        void setInParms(bool def, const std::vector<ExclusionTemplateInfo> &exclusionTemplateList) {
            _default = def;
            _exclusionTemplateList = exclusionTemplateList;
        }
        ExitInfo process() override;

    private:
        // Input parameters
        bool _default = false;

        // Output parameters
        std::vector<ExclusionTemplateInfo> _exclusionTemplateList;


        ExitInfo deserializeInputParms() override;
        ExitInfo serializeOutputParms() override { return ExitCode::Ok; }

        std::vector<ExclusionTemplateInfo> computeNormalizations(const std::vector<ExclusionTemplateInfo> &templateList);
        static std::unordered_set<SyncName> computeNormalizations(const SyncName &templateString);
        static bool canNormalize(const SyncName &template_);
        friend class TestGuiCommChannel;
};

} // namespace KDC
