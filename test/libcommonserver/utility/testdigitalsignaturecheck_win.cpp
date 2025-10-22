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

#include "testdigitalsignaturecheck_win.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "utility/digitalsignaturechecker_win.h"

namespace KDC {

void TestDigitalSignatureCheck_win::testIsSignatureValid() {
    // Empty path.
    CPPUNIT_ASSERT(!DigitalSignatureChecker_win({}).isSignatureValid());
    // Path to non existing file.
    CPPUNIT_ASSERT(!DigitalSignatureChecker_win(SyncPath("A/B/C")).isSignatureValid());
    // Path to existing file but not signed.
    LocalTemporaryDirectory tmpDir;
    const auto testPath = tmpDir.path() / "testSignature.txt";
    testhelpers::generateOrEditTestFile(testPath);
    CPPUNIT_ASSERT(!DigitalSignatureChecker_win(SyncPath(testPath)).isSignatureValid());
}

} // namespace KDC
