// Infomaniak kDrive - Desktop
// Copyright (C) 2023-2025 Infomaniak Network SA
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "benchupdatetreeworker.h"

#include "gui/parameterscache.h"
#include "test_utility/testhelpers.h"
#include "update_detection/update_detector/updatetreeworker.h"
#include "utility/types.h"

#include <version.h>
#include <mocks/libcommonserver/db/mockdb.h>

namespace KDC {

void BenchUpdateTreeWorker::setUp() {
    TestBase::start();

    bool alreadyExists = false;
    const auto parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    const std::filesystem::path syncDbPath = MockDb::makeDbName(alreadyExists);
    _syncDb = std::make_shared<SyncDb>(syncDbPath.string(), KDRIVE_VERSION_STRING);
    (void) _syncDb->init(KDRIVE_VERSION_STRING);
    _syncDb->setAutoDelete(true);

    _updateTree = std::make_shared<UpdateTree>(ReplicaSide::Local, SyncDb::driveRootNode());
    _fsOpSet = std::make_shared<FSOperationSet>(ReplicaSide::Unknown);

    _testObj = std::make_shared<UpdateTreeWorker>(_syncDb->cache(), _fsOpSet, _updateTree, "BenchUpdateTreeWorker", "BUTW",
                                                  ReplicaSide::Local);

    _situationGenerator.setSyncDb(_syncDb);
    _situationGenerator.setLocalUpdateTree(_updateTree);

    // Generate initial situation
    // .
    // ├── A
    // │   ├── AA
    // │   └── AB
    // └── B
    //     ├── BA
    //     └── BB
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    const std::vector<std::string> dirs = {"a", "b", "c", "d", "e", "f"}; //, "e", "f", "g", "h", "i", "j"};
    for (const auto &dirId: dirs) {
        _situationGenerator.addItem(NodeType::Directory, dirId, "");
        for (const auto &tmpId: dirs) {
            const auto subDirId = dirId + tmpId;
            _situationGenerator.addItem(NodeType::Directory, subDirId, dirId);
            for (const auto &tmpId2: dirs) {
                const auto subSubDirId = subDirId + tmpId2;
                _situationGenerator.addItem(NodeType::Directory, subSubDirId, subDirId);
                for (auto i = 0; i < 1000; i++) {
                    _situationGenerator.addItem(NodeType::File, subSubDirId + std::to_string(i), subSubDirId);
                }
            }
        }
    }
    const std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    std::cout << std::endl;
    std::cout << "Initial situation generated in " << elapsed_seconds.count() << "s for " << _situationGenerator.size()
              << " items" << std::endl;
}

void BenchUpdateTreeWorker::tearDown() {
    ParmsDb::instance()->close();
    // The singleton ParmsDb calls KDC::Log()->instance() in its destructor.
    // As the two singletons are instantiated in different translation units, the order of their destruction is unknown.
    ParmsDb::reset();
    TestBase::stop();
}

void BenchUpdateTreeWorker::measureUpdateTreeGenerationFromScratch() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ clean update tree");

    _updateTree->clear();
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    _testObj->execute();
    const std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    std::cout << "Update tree worker executed in " << elapsed_seconds.count() << "s" << std::endl;
}

void BenchUpdateTreeWorker::measureUpdateTreeGenerationFromExisting() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ keep update tree");

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    _testObj->execute();
    const std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    std::cout << "Update tree worker executed in " << elapsed_seconds.count() << "s" << std::endl;
}

void BenchUpdateTreeWorker::measureUpdateTreeGenerationFromScratchWithFsOps() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ clean update tree - 100'000 CREATE ops");

    for (auto i = 0; i < 100000; i++) {
        const auto fsOp = std::make_shared<FSOperation>(OperationType::Create, "l_" + std::to_string(i), NodeType::File,
                                                        testhelpers::defaultTime, testhelpers::defaultTime,
                                                        testhelpers::defaultFileSize, "A/" + std::to_string(i));
        _fsOpSet->insertOp(fsOp);
    }

    _updateTree->clear();
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    _testObj->execute();
    const std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    std::cout << "Update tree worker executed in " << elapsed_seconds.count() << "s" << std::endl;
}

void BenchUpdateTreeWorker::measureUpdateTreeGenerationFromExistingWithFsOps() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ keep update tree - 100'000 CREATE ops");

    for (auto i = 0; i < 100000; i++) {
        const auto fsOp = std::make_shared<FSOperation>(OperationType::Create, "l_" + std::to_string(i), NodeType::File,
                                                        testhelpers::defaultTime, testhelpers::defaultTime,
                                                        testhelpers::defaultFileSize, "A/" + std::to_string(i));
        _fsOpSet->insertOp(fsOp);
    }

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    _testObj->execute();
    const std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    std::cout << "Update tree worker executed in " << elapsed_seconds.count() << "s" << std::endl;
}

} // namespace KDC
