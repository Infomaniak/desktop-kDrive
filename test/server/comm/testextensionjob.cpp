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

#include "testextensionjob.h"

#include "server/comm/extensionjob.h"
#include "io/iohelper.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include <version.h>

namespace KDC {

void TestExtensionJob::setUp() {
    TestBase::start();

    // Create parmsDb (needed by AbstractJob through ParametersCache)
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) IoHelper::deleteItem(parmsDbPath);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
}

void TestExtensionJob::tearDown() {
    TestBase::stop();
}

void TestExtensionJob::testRunJobWithMalformedArguments() {
    // A malformed STATUS command sent by an extension must not throw inside the noexcept runJob(), as it would
    // terminate the server, and must be silently discarded.
    const CommString argSeparator = messageArgSeparator;
    const CommString cdeSeparator = messageCdeSeparator;
    const CommString argument =
            Str("not_a_number") + argSeparator + Str("50") + argSeparator + Str("not_a_number") + argSeparator + Str("/path");
    const CommString commandLine = Str("STATUS") + cdeSeparator + argument;

    ExtensionJob job(nullptr, commandLine, {std::make_shared<MockCommChannel>()});
    const ExitInfo exitInfo = job.runSynchronously();

    CPPUNIT_ASSERT(exitInfo);
}

} // namespace KDC
