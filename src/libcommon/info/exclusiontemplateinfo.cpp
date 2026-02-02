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

#include "exclusiontemplateinfo.h"

#include "utility/utility.h"

static const auto exclusionTemplateInfoString = "template";
static const auto exclusionTemplateInfoWarning = "warning";
static const auto exclusionTemplateInfoDefault = "default";

namespace KDC {

ExclusionTemplateInfo::ExclusionTemplateInfo(const QString &templ, const bool warning, const bool def) :
    _templ(templ),
    _warning(warning),
    _def(def) {}


void ExclusionTemplateInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateInfoString, CommonUtility::qStr2CommString(_templ));
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateInfoWarning, _warning);
    CommonUtility::writeValueToStruct(dstruct, exclusionTemplateInfoDefault, _def);
}

void ExclusionTemplateInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommString templateCommStr;
    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateInfoString, templateCommStr);
    _templ = CommonUtility::commString2QStr(templateCommStr);

    CommonUtility::readValueFromStruct(dstruct, exclusionTemplateInfoWarning, _warning);
    try {
        CommonUtility::readValueFromStruct(dstruct, exclusionTemplateInfoDefault, _def);
    } catch (Poco::NotFoundException&) {
        _def = false;
    }
}

void ExclusionTemplateInfo::normalizeExclusionTemplateInfoList(std::vector<ExclusionTemplateInfo> &templateList) {
    SyncNameSet uniqueTemplSet; // Unique template names up to NFC-encoding.
    for (auto it = templateList.begin(); it != templateList.end();) {
        SyncName normalizedTempl;
        if (const auto nfcSuccess =
                    CommonUtility::normalizedSyncName(QStr2SyncName(it->templ()), normalizedTempl, UnicodeNormalization::NFC);
            !nfcSuccess)
            normalizedTempl = QStr2SyncName(it->templ());
        else
            it->setTempl(QString::fromStdString(SyncName2Str(normalizedTempl)));

        if (uniqueTemplSet.emplace(normalizedTempl).second)
            ++it;
        else
            (void) templateList.erase(it);
    }
}

void ExclusionTemplateInfo::updateExclusionTemplateInfoList(std::vector<ExclusionTemplateInfo> &templateList) {
    std::vector<ExclusionTemplateInfo> newTemplateList;
    for (const auto &templateInfo: templateList) {
        const auto normalizations = computeNormalizations(QStr2SyncName(templateInfo.templ()));
        for (const auto &normalization: normalizations)
            newTemplateList.push_back(ExclusionTemplateInfo{QString::fromStdString(SyncName2Str(normalization)),
                                                            templateInfo.warning(), templateInfo.def()});
    }
    templateList = newTemplateList;
}

SyncNameSet ExclusionTemplateInfo::computeNormalizations(const SyncName &template_) {
    if (const auto normalizations = CommonUtility::computePathNormalizations(template_); !normalizations.empty()) {
        SyncNameSet result;
        for (const SyncName &normalization: normalizations) (void) result.emplace(normalization);
        return result;
    }

    return {template_};
}

QDataStream &operator>>(QDataStream &in, ExclusionTemplateInfo &exclusionTemplateInfo) {
    in >> exclusionTemplateInfo._templ >> exclusionTemplateInfo._warning >> exclusionTemplateInfo._def;
    return in;
}

QDataStream &operator<<(QDataStream &out, const ExclusionTemplateInfo &exclusionTemplateInfo) {
    out << exclusionTemplateInfo._templ << exclusionTemplateInfo._warning << exclusionTemplateInfo._def;
    return out;
}

QDataStream &operator<<(QDataStream &out, const QList<ExclusionTemplateInfo> &list) {
    int count = static_cast<int>(list.size());
    out << count;
    for (int i = 0; i < count; i++) {
        ExclusionTemplateInfo exclusionTemplateInfo = list[i];
        out << exclusionTemplateInfo;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QList<ExclusionTemplateInfo> &list) {
    auto count = 0;
    in >> count;
    for (int i = 0; i < count; i++) {
        ExclusionTemplateInfo *exclusionTemplateInfo = new ExclusionTemplateInfo();
        in >> *exclusionTemplateInfo;
        list.push_back(*exclusionTemplateInfo);
    }
    return in;
}

} // namespace KDC
