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

#include "libcommon/utility/types.h"

#include "libcommon/comm.h"
#include "server/comm/guijobs/abstractguijob.h"

#include <Poco/JSON/Object.h>

namespace KDC::testcommhelpers {
std::string toBase64(const CommString &input);

CommString beautifulString(const Poco::JSON::Object &obj);
CommString stringifyQueryObj(const Poco::JSON::Object &obj);
CommString stringifyAnswerObj(const Poco::JSON::Object &obj);
CommString stringifyCbkAnswerObj(const Poco::JSON::Object &obj);

struct SimpleAnswers {
        Poco::JSON::Object answer;
        Poco::JSON::Object answerWithNumAndType;
};

Poco::JSON::Object createSimpleQuery(RequestNum requestEnum);
SimpleAnswers createSimpleAnswers(RequestNum requestEnum);

} // namespace KDC::testcommhelpers
