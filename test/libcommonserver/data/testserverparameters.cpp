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

#include "testserverparameters.h"

namespace KDC {

void TestServerParameters::testApplyClientParametersPreservesServerOnlyFields() {
    ServerParameters serverParameters;
    serverParameters.setUpdateFileAvailable("3.9.0");
    serverParameters.setUpdateTargetVersion("3.9.0");
    serverParameters.setUpdateTargetVersionString("kDrive 3.9.0");
    serverParameters.setAutoUpdateAttempted(true);
    serverParameters.setSeenVersion("3.8.7");
    serverParameters.setUploadSessionParallelJobs(7);
    serverParameters.setDialogGeometry("preferencesWindow", QByteArray("blob1234"));
    serverParameters.setLanguage(Language::French);

    Parameters baseParameters;
    baseParameters.setLanguage(Language::English);
    CPPUNIT_ASSERT(baseParameters.dialogGeometry().isEmpty());

    serverParameters.applyClientParameters(baseParameters);

    // Server-only fields must be preserved by the update.
    CPPUNIT_ASSERT_EQUAL(std::string("3.9.0"), serverParameters.updateFileAvailable());
    CPPUNIT_ASSERT_EQUAL(std::string("3.9.0"), serverParameters.updateTargetVersion());
    CPPUNIT_ASSERT_EQUAL(std::string("kDrive 3.9.0"), serverParameters.updateTargetVersionString());
    CPPUNIT_ASSERT_EQUAL(true, serverParameters.autoUpdateAttempted());
    CPPUNIT_ASSERT_EQUAL(std::string("3.8.7"), serverParameters.seenVersion());
    CPPUNIT_ASSERT_EQUAL(7, serverParameters.uploadSessionParallelJobs());

    // Client-visible fields must be overwritten with the base Parameters values.
    CPPUNIT_ASSERT(serverParameters.language() == Language::English);

    // The previous non-empty dialog geometry must not be wiped by the empty incoming one.
    CPPUNIT_ASSERT(!serverParameters.dialogGeometry().isEmpty());
    CPPUNIT_ASSERT(serverParameters.dialogGeometry("preferencesWindow") == QByteArray("blob1234"));
}

void TestServerParameters::testApplyClientParametersReplacesDialogGeometry() {
    ServerParameters serverParameters;
    serverParameters.setUpdateFileAvailable("3.9.0");
    serverParameters.setUploadSessionParallelJobs(7);
    serverParameters.setDialogGeometry("preferencesWindow", QByteArray("blob1234"));

    Parameters baseParameters;
    baseParameters.setLanguage(Language::English);
    baseParameters.setDialogGeometry("preferencesWindow", QByteArray("newGeometry"));

    serverParameters.applyClientParameters(baseParameters);

    // Server-only fields must still be preserved by the update.
    CPPUNIT_ASSERT_EQUAL(std::string("3.9.0"), serverParameters.updateFileAvailable());
    CPPUNIT_ASSERT_EQUAL(7, serverParameters.uploadSessionParallelJobs());

    // A non-empty incoming dialog geometry replaces the previous one.
    CPPUNIT_ASSERT(serverParameters.dialogGeometry("preferencesWindow") == QByteArray("newGeometry"));
}

} // namespace KDC
