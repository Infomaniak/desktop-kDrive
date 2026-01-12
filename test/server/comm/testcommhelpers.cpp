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

#include "testcommhelpers.h"

#include "libcommon/utility/utility.h"

#include <Poco/JSON/Parser.h>

namespace KDC::testcommhelpers {
std::string toBase64(const CommString &input) {
    std::string output;
    CommonUtility::convertToBase64Str(input, output);

    return output;
}

CommString beautifulString(const Poco::JSON::Object &obj) {
    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(obj, oss, 1, 0);

    const auto answerStdStr = oss.str();
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var dynamicVar = parser.parse(answerStdStr);
    Poco::DynamicStruct paramsStruct = *dynamicVar.extract<Poco::JSON::Object::Ptr>();

    return CommonUtility::str2CommString(Poco::Dynamic::structToString(paramsStruct));
}

CommString stringifyQueryObj(const Poco::JSON::Object &obj) {
    return beautifulString(obj);
}

CommString stringifyAnswerObj(const Poco::JSON::Object &obj) {
    return beautifulString(obj);
}

CommString stringifyCbkAnswerObj(const Poco::JSON::Object &obj) {
    std::ostringstream json;
    Poco::JSON::Stringifier::stringify(obj, json, 0); // compact form

    return CommonUtility::str2CommString(json.str());
}


Poco::JSON::Object createSimpleQuery(const RequestNum requestNum) {
    Poco::JSON::Object queryObj;
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    (void) queryObj.set("id", 1);
#endif
    (void) queryObj.set("num", toInt(requestNum));
    Poco::JSON::Object queryParamsObj;
    (void) queryObj.set("params", queryParamsObj);

    return queryObj;
}

SimpleAnswers createSimpleAnswers(const RequestNum requestEnum) {
    SimpleAnswers simpleAnswers;

    (void) simpleAnswers.answer.set("cause", 0);
    (void) simpleAnswers.answer.set("code", 0);
    (void) simpleAnswers.answer.set("id", 1);

    Poco::JSON::Object paramsObj;
    (void) simpleAnswers.answer.set("params", paramsObj);

    simpleAnswers.answerWithNumAndType = simpleAnswers.answer;
    (void) simpleAnswers.answerWithNumAndType.set("num", toInt(requestEnum));
    (void) simpleAnswers.answerWithNumAndType.set("type", toInt(AbstractGuiJob::GuiJobType::Query));

    return simpleAnswers;
}

} // namespace KDC::testcommhelpers
