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

#include "commjson.h"

#include "utility/utility.h"

#include <Poco/Exception.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>

namespace KDC::CommJson {

Poco::DynamicStruct parseObject(const std::string &json) {
    Poco::JSON::Parser parser;
    const Poco::Dynamic::Var parsedJson = parser.parse(json);
    const auto jsonObject = parsedJson.extract<Poco::JSON::Object::Ptr>();
    if (!jsonObject) {
        throw Poco::InvalidArgumentException("Expected a JSON object");
    }

    return *jsonObject;
}

Poco::DynamicStruct parseCommObject(const CommString &json) {
#ifdef KD_WINDOWS
    // CommString is std::wstring on Windows, while Poco::JSON::Parser consumes UTF-8 std::string input.
    return parseObject(CommonUtility::commString2Str(json));
#else
    // On Unix platforms CommString is already std::string, so avoid an unnecessary conversion/copy.
    return parseObject(json);
#endif
}

} // namespace KDC::CommJson
