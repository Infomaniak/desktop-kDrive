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
std::string toBase64(const std::string &input) {
    std::string output;
    CommonUtility::convertToBase64Str(input, output);
    return output;
}

CommString beautifulString(const Poco::JSON::Object &obj) {
    std::ostringstream oss;
    Poco::JSON::Stringifier::stringify(obj, oss, 1, 0);

    const CommString _answerStr = oss.str();
    Poco::JSON::Parser parser;
    Poco::Dynamic::Var dynamicVar = parser.parse(CommonUtility::commString2Str(_answerStr));
    Poco::DynamicStruct paramsStruct = *dynamicVar.extract<Poco::JSON::Object::Ptr>();

    return Poco::Dynamic::structToString(paramsStruct);
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

    return json.str();
}
} // namespace KDC::testcommhelpers
