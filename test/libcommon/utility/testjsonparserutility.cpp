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

#include "testjsonparserutility.h"

#include "libcommon/utility/jsonparserutility.h"
#include "libsyncengine/jobs/network/networkjobsparams.h"

#include <Poco/JSON/Parser.h>

namespace KDC {


void TestJsonParserUtility::testExtractValue() {
    {
        // The json object has not "created_at" key
        std::string json = "{ \"no_created_at_key\" : { \"added_at\" : \"null\" } }";
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(json);

        SyncTime creationTimeOut{100};
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();
        // If missing but not mandatory, the parsed date value defaults to zero.
        CPPUNIT_ASSERT(JsonParserUtility::extractValue(object, KDC::createdAtKey, creationTimeOut, false));
        CPPUNIT_ASSERT_EQUAL(SyncTime{0}, creationTimeOut);
        // If the key is missing and mandatory, the `extractValue` function returns `false` (error).
        CPPUNIT_ASSERT(!JsonParserUtility::extractValue(object, KDC::createdAtKey, creationTimeOut));
    }

    {
        // The json object has a "created_at" key with an associated "null" value.
        std::string json = "{ \"created_at\" : \"\" }";
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var result = parser.parse(json);

        SyncTime creationTimeOut{100};
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();

        // If "created_at" is not mandatory and has an empty string value, the resulting parsed value is 0.
        CPPUNIT_ASSERT(JsonParserUtility::extractValue(object, KDC::createdAtKey, creationTimeOut, false));
        CPPUNIT_ASSERT_EQUAL(SyncTime{0}, creationTimeOut);

        // Otherwise, the function `extractValue` returns `false` (error).
        CPPUNIT_ASSERT(!JsonParserUtility::extractValue(object, KDC::createdAtKey, creationTimeOut));
    }
}

} // namespace KDC
