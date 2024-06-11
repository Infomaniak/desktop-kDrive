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

#include "cppunit_test_template.h"

using namespace CppUnit;

namespace KDC {

void TestClass::setUp() {
    _myClass = new MyClass();
}

void TestClass::tearDown() {
    delete _myClass;
}

void TestClass::testMyFunc() {
    // Add test here
    ... CPPUNIT_ASSERT(testResult);
}

}  // namespace KDC
