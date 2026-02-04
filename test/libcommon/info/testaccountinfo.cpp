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

#include "testaccountinfo.h"
#include "libcommon/info/accountinfo.h"
#include <QBuffer>

namespace KDC {

void TestAccountInfo::testConstructors() {
    // Test default constructor
    AccountInfo info1;
    CPPUNIT_ASSERT_EQUAL(0, info1.dbId());
    CPPUNIT_ASSERT_EQUAL(0, info1.userDbId());
    CPPUNIT_ASSERT_EQUAL(std::string{}, info1.name());

    // Test parameterized constructor
    AccountInfo info2(42, 84);
    CPPUNIT_ASSERT_EQUAL(42, info2.dbId());
    CPPUNIT_ASSERT_EQUAL(std::string{}, info1.name());
}

void TestAccountInfo::testGettersSetters() {
    AccountInfo info;

    info.setDbId(100);
    CPPUNIT_ASSERT_EQUAL(100, info.dbId());

    info.setUserDbId(200);
    CPPUNIT_ASSERT_EQUAL(200, info.userDbId());

    info.setName("filename");
    CPPUNIT_ASSERT_EQUAL(std::string{"filename"}, info.name());
}

void TestAccountInfo::testDynamicStruct() {
    AccountInfo info(123, 456);
    info.setName("filename");

    Poco::DynamicStruct dstruct;
    info.toDynamicStruct(dstruct);

    AccountInfo readInfo;
    readInfo.fromDynamicStruct(dstruct);

    CPPUNIT_ASSERT_EQUAL(123, readInfo.dbId());
    CPPUNIT_ASSERT_EQUAL(456, readInfo.userDbId());
    CPPUNIT_ASSERT_EQUAL(std::string{"filename"}, info.name());
}

void TestAccountInfo::testDataStream() {
    AccountInfo original(111, 222);
    original.setName("file1.txt");

    QByteArray dataArray;
    QBuffer buffer(&dataArray);
    (void) buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer);

    out << original;

    buffer.close();
    buffer.setData(dataArray);
    (void) buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);

    AccountInfo readInfo;
    in >> readInfo;

    CPPUNIT_ASSERT_EQUAL(111, readInfo.dbId());
    CPPUNIT_ASSERT_EQUAL(222, readInfo.userDbId());
    CPPUNIT_ASSERT_EQUAL(std::string("file1.txt"), readInfo.name());
}

void TestAccountInfo::testDataStreamList() {
    QList<AccountInfo> originalList;
    originalList.append(AccountInfo(1, 2));
    originalList.append(AccountInfo(4, 5));
    originalList[1].setName("filename2");

    QByteArray dataArray;
    QBuffer buffer(&dataArray);
    (void) buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer);

    out << originalList;

    buffer.close();
    buffer.setData(dataArray);
    (void) buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);

    QList<AccountInfo> readList;
    in >> readList;

    CPPUNIT_ASSERT_EQUAL(qsizetype{2}, readList.size());
    CPPUNIT_ASSERT_EQUAL(1, readList[0].dbId());
    CPPUNIT_ASSERT_EQUAL(2, readList[0].userDbId());
    CPPUNIT_ASSERT_EQUAL(std::string{}, readList[0].name());

    CPPUNIT_ASSERT_EQUAL(4, readList[1].dbId());
    CPPUNIT_ASSERT_EQUAL(5, readList[1].userDbId());
    CPPUNIT_ASSERT_EQUAL(std::string{"filename2"}, readList[1].name());
}

} // namespace KDC
