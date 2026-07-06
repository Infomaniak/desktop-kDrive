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

#include "testsituationgeneratortest.h"

#include "test_utility/testhelpers.h"

namespace KDC {

void TestSituationGeneratorTest::setUp() {
    TestBase::start();
}

void TestSituationGeneratorTest::tearDown() {
    TestBase::stop();
}

void TestSituationGeneratorTest::testLegacyFormat() {
    _gen.generateInitialSituation(R"({
        "a": {
            "aa": {
                "aaa": 1
            }
        },
        "b": {},
        "c": 1
    })");

    std::cout << "\n";
    _gen.printTree(ReplicaSide::Local);

    CPPUNIT_ASSERT(_gen.getNode(ReplicaSide::Local, "a") != nullptr);
    CPPUNIT_ASSERT(_gen.getNode(ReplicaSide::Local, "aa") != nullptr);
    CPPUNIT_ASSERT(_gen.getNode(ReplicaSide::Local, "aaa") != nullptr);
    CPPUNIT_ASSERT(_gen.getNode(ReplicaSide::Local, "b") != nullptr);
    CPPUNIT_ASSERT(_gen.getNode(ReplicaSide::Local, "c") != nullptr);
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, _gen.getNode(ReplicaSide::Local, "a")->type());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, _gen.getNode(ReplicaSide::Local, "aaa")->type());
}

void TestSituationGeneratorTest::testExtendedFormat() {
    _gen.generateInitialSituation(R"({
        "content": [
            {
                "type": "Directory",
                "name": "MyDir",
                "createdAt": 1000,
                "lastModifiedAt": 2000,
                "content": [
                    { "type": "File", "name": "MyFile", "size": 4242 },
                    { "type": "File", "name": "OtherFile", "size": 99, "lastModifiedAt": 5000 }
                ]
            },
            { "type": "Directory", "name": "EmptyDir" },
            { "type": "File", "name": "RootFile", "size": 1234 }
        ]
    })");

    std::cout << "\n";
    _gen.printTree(ReplicaSide::Local);
    _gen.printTree(ReplicaSide::Remote);

    const auto dir = _gen.getNode(ReplicaSide::Local, "mydir");
    CPPUNIT_ASSERT(dir != nullptr);
    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, dir->type());
    CPPUNIT_ASSERT_EQUAL(SyncTime(2000), dir->modificationTime().value());

    const auto file = _gen.getNode(ReplicaSide::Local, "myfile");
    CPPUNIT_ASSERT(file != nullptr);
    CPPUNIT_ASSERT_EQUAL(NodeType::File, file->type());
    CPPUNIT_ASSERT_EQUAL(int64_t(4242), file->size());

    const auto otherFile = _gen.getNode(ReplicaSide::Remote, "otherfile");
    CPPUNIT_ASSERT(otherFile != nullptr);
    CPPUNIT_ASSERT_EQUAL(SyncTime(5000), otherFile->modificationTime().value());

    const auto rootFile = _gen.getNode(ReplicaSide::Local, "rootfile");
    CPPUNIT_ASSERT(rootFile != nullptr);
    CPPUNIT_ASSERT_EQUAL(int64_t(1234), rootFile->size());
}

} // namespace KDC
