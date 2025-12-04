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

#include "testguicommchannel.h"

#include "comm/guijobs/updaterchangechanneljob.h"


namespace KDC {

void TestGuiCommChannel::testUpdaterChangeChannelJob() {
#if defined(KD_WINDOWS) || defined(KD_LINUX)
    const auto queryStr {
        R"({ "id": 1, "num": )" + std::to_string(toInt(RequestNum::UPDATER_CHANGE_CHANNEL)) + R"(, "params": { "channel": )" +
                std::to_string(toInt(VersionChannel::Prod)) + R"( } })";
#else
    const auto queryStr{R"({ "num": )" + std::to_string(toInt(RequestNum::UPDATER_CHANGE_CHANNEL)) +
                        R"(, "params": { "channel": )" + std::to_string(toInt(VersionChannel::Prod)) + R"( } })"};
    const auto cbkAnswerStr{R"({"cause":0,"code":0,"id":1,"params":{}})"};
#endif
        const auto answerStr{R"({ "cause": 0, "code": 0, "id": 1, "num": )" +
                             std::to_string(toInt(RequestNum::UPDATER_CHANGE_CHANNEL)) + R"(, "params": {  }, "type": )" +
                             std::to_string(toInt(AbstractGuiJob::GuiJobType::Query)) + R"( })"};

        auto processFct = [](std::shared_ptr<AbstractGuiJob> job) {
            auto updaterChangeChannelJob = std::dynamic_pointer_cast<UpdaterChangeChannelJob>(job);
            CPPUNIT_ASSERT(updaterChangeChannelJob);
            CPPUNIT_ASSERT_EQUAL(VersionChannel::Prod, updaterChangeChannelJob->_channel);
        };
#if defined(KD_WINDOWS) || defined(KD_LINUX)
        testGenericJob(CommonUtility::str2CommString(queryStr), CommonUtility::str2CommString(answerStr), {}, processFct);
#else
    testGenericJob(queryStr, answerStr, cbkAnswerStr, processFct);
#endif
    }


} // namespace KDC
