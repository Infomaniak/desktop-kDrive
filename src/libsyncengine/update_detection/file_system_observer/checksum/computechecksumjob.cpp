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

#include "computechecksumjob.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"
#include "utility/timerutility.h"

#include <log4cplus/loggingmacros.h>

#include <xxhash.h>

namespace KDC {

ComputeChecksumJob::ComputeChecksumJob(const NodeId &nodeId, const SyncPath &filepath,
                                       const std::shared_ptr<LiveSnapshot> localSnapshot) :
    _logger(Log::instance()->getLogger()),
    _nodeId(nodeId),
    _filePath(filepath),
    _localSnapshot(localSnapshot) {}

ExitInfo ComputeChecksumJob::runJob() {
    if (isExtendedLog()) {
        LOGW_DEBUG(_logger, L"Checksum job started: id: " << jobId() << L", path: " << Path2WStr(_filePath));
    }

    const TimerUtility timer;
    bool stopped = false;

    // Create a hash stat
    XXH3_state_t *const state = XXH3_createState();
    if (!state) {
        LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath));
        return {};
    }

    // Initialize state with selected seed
    if (XXH3_64bits_reset(state) == XXH_ERROR) {
        LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath));
        return {};
    }

    try {
        FILE *f = fopen(SyncName2Str(_filePath.native()).c_str(), "rb");
        if (f) {
            std::size_t len;
            unsigned char buf[BUFSIZ];
            do {
                len = fread(buf, 1, BUFSIZ, f);
                if (XXH3_64bits_update(state, buf, len) == XXH_ERROR) {
                    LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath));
                    return {};
                }
            } while (len == BUFSIZ);

            if (!stopped) {
                // Produce the final hash value
                XXH64_hash_t const hash = XXH3_64bits_digest(state);
                _localSnapshot->setContentChecksum(_nodeId, Utility::xxHashToStr(hash));
            }

            XXH3_freeState(state);

            fclose(f);

            if (stopped) {
                LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" aborted for file " << Path2WStr(_filePath));
            } else {
                if (isExtendedLog()) {
                    LOGW_DEBUG(_logger, L"Checksum computation " << jobId() << L" for file " << Path2WStr(_filePath) << L" took "
                                                                 << timer.elapsed<DoubleSeconds>().count() << L"s");
                }
            }
        } else {
            LOGW_DEBUG(_logger, L"Item does not exist anymore - path=" << Path2WStr(_filePath));
        }
    } catch (...) {
        LOGW_DEBUG(_logger, L"File " << Path2WStr(_filePath) << L" is not readable");
    }

    if (isExtendedLog()) {
        LOG_DEBUG(_logger, "Checksum job finished: id=" << jobId());
    }
    return ExitCode::Ok;
}

} // namespace KDC
