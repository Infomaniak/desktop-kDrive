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

#include "dummyguijob.h"

namespace KDC {

DummyGuiJob::DummyGuiJob(std::shared_ptr<CommManager> commManager, const CommString &inputParmsStr,
                         const std::shared_ptr<AbstractCommChannel> &channel) :
    AbstractGuiJob(commManager, inputParmsStr, channel) {}

bool DummyGuiJob::deserializeInputParms() {
    if (!AbstractGuiJob::deserializeInputParms()) {
        return false;
    }

    // Deserialization code

    return true;
}

bool DummyGuiJob::serializeOutputParms() {
    // Serialization code

    if (!AbstractGuiJob::serializeOutputParms()) {
        return false;
    }

    return true;
}

bool DummyGuiJob::process() {
    if (!AbstractGuiJob::process()) {
        return false;
    }

    // Processing code

    return true;
}

} // namespace KDC
