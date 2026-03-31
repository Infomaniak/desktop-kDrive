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

#include "testinfodynamicstruct.h"

#include "libcommon/info/errorinfo.h"
#include "libcommon/info/nodeinfo.h"
#include "libcommon/info/syncfileiteminfo.h"

#include <cstdint>

namespace KDC {

void TestInfoDynamicStruct::testErrorInfoRoundTrip() {
    const ErrorInfo source(ErrorDbId{41}, int64_t{1712345678}, ErrorLevel::SyncPal, QString("syncStep"), SyncDbId{99},
                           QString("workerA"), ExitCode::NetworkError, ExitCause::NetworkTimeout, QString("local-1"),
                           QString("remote-2"), NodeType::File, QString("workspace/file.txt"), ConflictType::EditEdit,
                           InconsistencyType::ForbiddenChar, CancelType::Move, QString("workspace/file-copy.txt"));

    Poco::DynamicStruct dstruct;
    source.toDynamicStruct(dstruct);

    ErrorInfo parsed;
    parsed.fromDynamicStruct(dstruct);

    CPPUNIT_ASSERT_EQUAL(ErrorDbId{41}, parsed.dbId());
    CPPUNIT_ASSERT_EQUAL(qint64{1712345678}, parsed.getTime());
    CPPUNIT_ASSERT_EQUAL(ErrorLevel::SyncPal, parsed.level());
    CPPUNIT_ASSERT_EQUAL(std::string("syncStep"), parsed.functionName().toStdString());
    CPPUNIT_ASSERT_EQUAL(SyncDbId{99}, parsed.syncDbId());
    CPPUNIT_ASSERT_EQUAL(std::string("workerA"), parsed.workerName().toStdString());
    CPPUNIT_ASSERT_EQUAL(ExitCode::NetworkError, parsed.exitCode());
    CPPUNIT_ASSERT_EQUAL(ExitCause::NetworkTimeout, parsed.exitCause());
    CPPUNIT_ASSERT_EQUAL(std::string("local-1"), parsed.localNodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("remote-2"), parsed.remoteNodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(NodeType::File, parsed.nodeType());
    CPPUNIT_ASSERT_EQUAL(std::string("workspace/file.txt"), parsed.path().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("workspace/file-copy.txt"), parsed.destinationPath().toStdString());
    CPPUNIT_ASSERT_EQUAL(ConflictType::EditEdit, parsed.conflictType());
    CPPUNIT_ASSERT_EQUAL(InconsistencyType::ForbiddenChar, parsed.inconsistencyType());
    CPPUNIT_ASSERT_EQUAL(CancelType::Move, parsed.cancelType());
    CPPUNIT_ASSERT_EQUAL(false, parsed.autoResolved());
}

void TestInfoDynamicStruct::testNodeInfoRoundTrip() {
    const NodeInfo source(QString("node-123"), QString("FolderA"), int64_t{4096}, QString("parent-99"), SyncTime{123456789},
                          QString("workspace/FolderA"));

    Poco::DynamicStruct dstruct;
    source.toDynamicStruct(dstruct);

    NodeInfo parsed;
    parsed.fromDynamicStruct(dstruct);

    CPPUNIT_ASSERT_EQUAL(std::string("node-123"), parsed.nodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("FolderA"), parsed.name().toStdString());
    CPPUNIT_ASSERT_EQUAL(qint64{4096}, parsed.size());
    CPPUNIT_ASSERT_EQUAL(std::string("parent-99"), parsed.parentNodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(qint64{123456789}, parsed.modtime());
    CPPUNIT_ASSERT_EQUAL(std::string("workspace/FolderA"), parsed.path().toStdString());
    CPPUNIT_ASSERT_EQUAL(false, parsed.accessDenied());
}

void TestInfoDynamicStruct::testSyncFileItemInfoRoundTrip() {
    SyncFileItemInfo source(NodeType::Directory, QString("docs/report.txt"), QString("docs/report-final.txt"), QString("local-9"),
                            QString("remote-10"), SyncDirection::Down, SyncFileInstruction::Update, SyncFileStatus::Syncing,
                            ConflictType::MoveCreate, InconsistencyType::PathLength, CancelType::TmpBlacklisted);
    source.setError(QString("temporary error"));
    source.setSize(int64_t{2048});
    source.setProgress(int32_t{73});
    source.setOperationId(UniqueId{555});

    Poco::DynamicStruct dstruct;
    source.toDynamicStruct(dstruct);

    SyncFileItemInfo parsed;
    parsed.fromDynamicStruct(dstruct);

    CPPUNIT_ASSERT_EQUAL(NodeType::Directory, parsed.type());
    CPPUNIT_ASSERT_EQUAL(std::string("docs/report.txt"), parsed.path().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("docs/report-final.txt"), parsed.newPath().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("local-9"), parsed.localNodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(std::string("remote-10"), parsed.remoteNodeId().toStdString());
    CPPUNIT_ASSERT_EQUAL(SyncDirection::Down, parsed.direction());
    CPPUNIT_ASSERT_EQUAL(SyncFileInstruction::Update, parsed.instruction());
    CPPUNIT_ASSERT_EQUAL(SyncFileStatus::Syncing, parsed.status());
    CPPUNIT_ASSERT_EQUAL(ConflictType::MoveCreate, parsed.conflict());
    CPPUNIT_ASSERT_EQUAL(InconsistencyType::PathLength, parsed.inconsistency());
    CPPUNIT_ASSERT_EQUAL(CancelType::TmpBlacklisted, parsed.cancelType());
    CPPUNIT_ASSERT_EQUAL(std::string("temporary error"), parsed.error().toStdString());
    CPPUNIT_ASSERT_EQUAL(int64_t{2048}, parsed.size());
    CPPUNIT_ASSERT_EQUAL(int32_t{73}, static_cast<int32_t>(parsed.progress()));
    CPPUNIT_ASSERT_EQUAL(UniqueId{555}, parsed.operationId());
}

} // namespace KDC
