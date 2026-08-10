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

#include "exclusiontemplate.h"

#include "utility/utility.h"

static const auto exclusionTemplateString = "template";
static const auto exclusionTemplateWarning = "warning";
static const auto exclusionTemplateDefault = "default";

namespace KDC {

ExclusionTemplate::ExclusionTemplate() {
    evaluateComplexity();
}

ExclusionTemplate::ExclusionTemplate(const std::string &templ, const bool warning, const bool def) :
    _templ(templ),
    _warning(warning),
    _def(def) {
    evaluateComplexity();
}

void ExclusionTemplate::evaluateComplexity() {
    std::string::size_type n = 0;
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

void ExclusionTemplate::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateString, CommonUtility::str2CommString(_templ));
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateWarning, _warning);
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateDefault, _def);
}

void ExclusionTemplate::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommString templateCommStr;
    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateString, templateCommStr);
    _templ = CommonUtility::commString2Str(templateCommStr);
    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateWarning, _warning);
    try {
        CommonUtility::readValueFromStruct(dstruct, exclusionTemplateDefault, _def);
    } catch (Poco::NotFoundException &) {
        _def = false;
    }
}

void ExclusionTemplate::normalizeList(std::vector<ExclusionTemplate> &templateList) {
    SyncNameSet uniqueTemplSet; // Unique template names up to NFC-encoding.
    for (auto it = templateList.begin(); it != templateList.end();) {
        SyncName normalizedTempl;
        if (const auto nfcSuccess =
                    CommonUtility::normalizedSyncName(Str2SyncName(it->templ()), normalizedTempl, UnicodeNormalization::NFC);
            !nfcSuccess)
            normalizedTempl = Str2SyncName(it->templ());
        else
            it->setTempl(SyncName2Str(normalizedTempl));

        if (uniqueTemplSet.emplace(normalizedTempl).second)
            ++it;
        else
            it = templateList.erase(it);
    }
}

void ExclusionTemplate::updateList(std::vector<ExclusionTemplate> &templateList) {
    std::vector<ExclusionTemplate> newTemplateList;
    for (const auto &exclusionTemplate: templateList) {
        const auto normalizations = computeNormalizations(Str2SyncName(exclusionTemplate.templ()));
        for (const auto &normalization: normalizations)
            (void) newTemplateList.push_back(
                    ExclusionTemplate{SyncName2Str(normalization), exclusionTemplate.warning(), exclusionTemplate.def()});
    }
    templateList = newTemplateList;
}

SyncNameSet ExclusionTemplate::computeNormalizations(const SyncName &templateString) {
    if (const auto normalizations = CommonUtility::computePathNormalizations(templateString); !normalizations.empty()) {
        SyncNameSet result;
        for (const SyncName &normalization: normalizations) (void) result.emplace(normalization);
        return result;
    }

    return {templateString};
}

} // namespace KDC
