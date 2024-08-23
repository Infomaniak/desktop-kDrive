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

#include "libcommonserver/log/log.h"

#include <Poco/JSON/Object.h>
#include <Poco/Dynamic/Var.h>

#include <log4cplus/loggingmacros.h>

namespace KDC {

struct COMMONSERVER_EXPORT JsonParserUtility {
        template <typename T>
        static bool extractValue(const Poco::JSON::Object::Ptr obj, const std::string &key, T &val, bool mandatory = true) {
            if (!obj) {
                LOG_WARN(Log::instance()->getLogger(), "JSON object is NULL");
                return false;
            }  // namespace KDC

            if (obj->has(key) && obj->isNull(key)) {
                // Item exist in JSON but is null, this is ok
                return true;
            }

            Poco::Dynamic::Var var = obj->get(key);
            try {
                var.convert(val);
            } catch (...) {
                if (mandatory) {
                    std::string msg = "Fail to extract value for key=" + key;
                    LOG_WARN(Log::instance()->getLogger(), msg.c_str());
#ifdef NDEBUG
                    sentry_capture_event(
                        sentry_value_new_message_event(SENTRY_LEVEL_ERROR, "JsonParserUtility::extractValue", msg.c_str()));
#endif

                    return false;
                }
            }

            return true;
        }

        static Poco::JSON::Object::Ptr extractJsonObject(const Poco::JSON::Object::Ptr parentObj, const std::string &key) {
            if (!parentObj) {
                LOG_WARN(Log::instance()->getLogger(), "Parent object is NULL.");
                return nullptr;
            }

            Poco::JSON::Object::Ptr obj = parentObj->getObject(key);
            if (!obj) {
                LOG_WARN(Log::instance()->getLogger(), "Missing JSON object: " << key.c_str());
            }

            return obj;
        }

        static Poco::JSON::Array::Ptr extractArrayObject(const Poco::JSON::Object::Ptr parentObj, const std::string &key) {
            if (!parentObj) {
                LOG_WARN(Log::instance()->getLogger(), "Parent object is NULL.");
                return nullptr;
            }

            Poco::JSON::Array::Ptr array = parentObj->getArray(key);
            if (!array) {
                LOG_WARN(Log::instance()->getLogger(), "Missing JSON array: " << key.c_str());
            }

            return array;
        }
};

}  // namespace KDC
