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

#include "testincludes.h"
#include "libcommon/comm.h"
#include "server/comm/guicommserver.h"
#include "server/comm/guijobs/abstractguijob.h"

#include <log4cplus/logger.h>

namespace KDC {

class GuiCommChannelTest2 : public GuiCommChannel {
    public:
        GuiCommChannelTest2() :
#if defined(KD_WINDOWS) || defined(KD_LINUX)
            GuiCommChannel(Poco::Net::StreamSocket()) {
        }
#else
            GuiCommChannel(nullptr) {
        }
#endif

        uint64_t readData(CommChar *, uint64_t) override { return 0; }
        uint64_t writeData(const CommChar *, uint64_t) override { return 0; }
        uint64_t bytesAvailable() const override { return 0; }
};

class GuiJobTest : public AbstractGuiJob {
    public:
        struct Dummy {
                std::string strValue;
                int intValue;
        };

        enum class DummyEnum {
            Unknown,
            Dummy1,
            Dummy2,
            Dummy3,
            EnumEnd
        };

        GuiJobTest(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                   const std::shared_ptr<AbstractCommChannel> channel) :
            AbstractGuiJob(commManager, requestId, inParams, channel) {
            _requestNum = RequestNum::Unknown;
        }

    private:
        std::string _strValue;
        std::wstring _wstrValue;
        std::string _strValue2;
        std::wstring _wstrValue2;
        int _intValue;
        bool _boolValue;
        CommBLOB _blobValue;
        DummyEnum _enumValue;
        std::vector<std::string> _strValues;
        std::vector<std::wstring> _wstrValues;
        std::vector<int> _intValues;
        Dummy _dummyValue;
        std::vector<Dummy> _dummyValues;

        bool deserializeInputParms() override;
        bool serializeOutputParms() override;
        bool process() override;

        friend class TestAbstractGuiJob;
};

class TestAbstractGuiJob : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestAbstractGuiJob);
        CPPUNIT_TEST(testAll);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() final;
        void tearDown() override;

        void testAll();

    private:
        log4cplus::Logger _logger;
        std::shared_ptr<GuiCommChannelTest2> _channel;
};

} // namespace KDC
