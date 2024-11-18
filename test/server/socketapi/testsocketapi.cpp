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

#include "socketapi.h"
#include "testsocketapi.h"

#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#include <combaseapi.h>
#endif

namespace KDC {


void TestSocketApi::setUp() {}

void TestSocketApi::tearDown() {}

void TestSocketApi::testFileData() {
    FileData fileData;
    QString localPath = QString("/Users/hyphensmbp/kDrive Labör/Common documents/");

    QByteArray b1 = localPath.toLatin1();
    CPPUNIT_ASSERT(QString(b1) == localPath);

    // QString parentPath = QFileInfo(localPath).dir().path().toUtf8();

    // CPPUNIT_ASSERT(QString("/Users/hyphensmbp/kDrive Labör").toStdString() == parentPath.toStdString());

    // std::filesystem::path localPath_ = QStr2Path(parentPath);

    // CPPUNIT_ASSERT(localPath_ == SyncPath(QString("/Users/hyphensmbp/kDrive Labör").toStdString()));
}
} // namespace KDC
