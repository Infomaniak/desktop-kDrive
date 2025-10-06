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

#include "testabstractguijob.h"
#include "log/log.h"
#include "test_utility/testhelpers.h"
#include "mocks/libcommonserver/db/mockdb.h"

// Input parameters keys
static const auto inParamsStrValue = "strValue";
static const auto inParamsStrValue2 = "strValue2";
static const auto inParamsIntValue = "intValue";
static const auto inParamsBoolValue = "boolValue";
static const auto inParamsBLOBValue = "blobValue";
static const auto inParamsEnumValue = "enumValue";
static const auto inParamsDummyValue = "dummyValue";
static const auto inParamsStrValues = "strValues";
static const auto inParamsIntValues = "intValues";
static const auto inParamsDummyValues = "dummyValues";
static const auto inParamsDummyStrValue = "strDummyValue";
static const auto inParamsDummyIntValue = "intDummyValue";

// Output parameters keys
static const auto outParamsStrValue = "strValue";
static const auto outParamsStrValue2 = "strValue2";
static const auto outParamsWStrValue = "wstrValue";
static const auto outParamsWStrValue2 = "wstrValue2";
static const auto outParamsIntValue = "intValue";
static const auto outParamsBoolValue = "boolValue";
static const auto outParamsBLOBValue = "blobValue";
static const auto outParamsEnumValue = "enumValue";
static const auto outParamsStrValues = "strValues";
static const auto outParamsWStrValues = "wstrValues";
static const auto outParamsIntValues = "intValues";
static const auto outParamsDummyValue = "dummyValue";
static const auto outParamsDummyValues = "dummyValues";
static const auto outParamsDummyStrValue = "strDummyValue";
static const auto outParamsDummyIntValue = "intDummyValue";

namespace KDC {

const auto inputParamsStr{Str(
        R"({ "id": 999,)"
        R"( "num": 0,)"
        R"( "params": {)"
        R"( "strValue": "aGVsbG8=",)"
        R"( "strValue2": "5q+P5Liq5Lq66YO95pyJ5LuW55qE5L2c5oiY562W55Wl",)"
        R"( "intValue": 1234,)"
        R"( "boolValue": 0,)"
        R"( "blobValue": "MDEyMzQ1Njc4OWFiY2RlZmdoaWprbG1ub3BxcnRzdXZ3eHl6",)"
        R"( "enumValue": 2,)"
        R"( "strValues": [ "YWFh", "YmJi", "Y2Nj" ],)"
        R"( "intValues": [ 10, 11, 12, 13, 14],)"
        R"( "dummyValue": { "strDummyValue": "YWFhYQ==", "intDummyValue": 1111 },)"
        R"( "dummyValues": [ { "strDummyValue": "YWFhYQ==", "intDummyValue": 1111 }, { "strDummyValue": "YmJiYg==", "intDummyValue": 2222 } ] } })")};

const auto outputParamsStr{Str(
        R"({ "cause": 0,)"
        R"( "code": 0,)"
        R"( "id": 999,)"
        R"( "num": 0,)"
        R"( "params": {)"
        R"( "blobValue": "MDEyMzQ1Njc4OWFiY2RlZmdoaWprbG1ub3BxcnRzdXZ3eHl6",)"
        R"( "boolValue": true,)"
        R"( "dummyValue": { "intDummyValue": 888, "strDummyValue": "ZGRkZA==" },)"
        R"( "dummyValues": [ { "intDummyValue": 888, "strDummyValue": "ZGRkZA==" }, { "intDummyValue": 7777, "strDummyValue": "ZWVlZQ==" } ],)"
        R"( "enumValue": 3,)"
        R"( "intValue": 999,)"
        R"( "intValues": [ 20, 21, 22 ],)"
        R"( "strValue": "cXdlcnR6",)"
        R"( "strValue2": "5q+P5Liq5Lq66YO95pyJ5LuW55qE5L2c5oiY562W55Wl",)"
        R"( "strValues": [ "enp6eg==", "eXl5eQ==" ],)"
        R"( "wstrValue": "YXNkZmdo",)"
        R"( "wstrValue2": "5q+P5Liq5Lq66YO95pyJ5LuW55qE5L2c5oiY562W55Wl",)"
        R"( "wstrValues": [ "eHh4eA==", "d3d3dw==" ] },)"
        R"( "type": 1 })")};

void TestAbstractGuiJob::setUp() {
    TestBase::start();

    _logger = Log::instance()->getLogger();

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    _channel = std::make_shared<GuiCommChannelTest2>();
}

void TestAbstractGuiJob::tearDown() {
    TestBase::stop();
}

void TestAbstractGuiJob::testAll() {
    // convertToBase64Str
    std::string blobStr("0123456789abcdefghijklmnopqrtsuvwxyz");
    CommBLOB blob;
    (void) std::copy(blobStr.begin(), blobStr.end(), std::back_inserter(blob));

    std::string base64Str;
    CommonUtility::convertToBase64Str(blob, base64Str);
    CPPUNIT_ASSERT(base64Str == "MDEyMzQ1Njc4OWFiY2RlZmdoaWprbG1ub3BxcnRzdXZ3eHl6");

    // convertFromBase64Str
    CommBLOB blob2;
    CommonUtility::convertFromBase64Str(base64Str, blob2);
    CPPUNIT_ASSERT(blob2 == blob);

    // deserializeGenericInputParms
    int requestId = 0;
    RequestNum requestNum = RequestNum::Unknown;
    Poco::DynamicStruct inParams;
    CPPUNIT_ASSERT(AbstractGuiJob::deserializeGenericInputParms(inputParamsStr, requestId, requestNum, inParams));
    CPPUNIT_ASSERT(requestId == 999);
    CPPUNIT_ASSERT(requestNum == RequestNum::Unknown);

    // deserializeInputParms
    auto job = std::make_unique<GuiJobTest>(nullptr, requestId, inParams, _channel);
    CPPUNIT_ASSERT(job->deserializeInputParms());
    CPPUNIT_ASSERT(job->exitInfo());

    CPPUNIT_ASSERT(job->_strValue == "hello");
    CPPUNIT_ASSERT(job->_wstrValue == L"hello");
    CPPUNIT_ASSERT(job->_strValue2 == "每个人都有他的作战策略");
    CPPUNIT_ASSERT(job->_wstrValue2 == L"每个人都有他的作战策略");
    CPPUNIT_ASSERT(job->_intValue == 1234);
    CPPUNIT_ASSERT(job->_boolValue == false);
    CPPUNIT_ASSERT(job->_blobValue == blob);
    CPPUNIT_ASSERT(job->_enumValue == GuiJobTest::DummyEnum::Dummy2);
    CPPUNIT_ASSERT(job->_strValues.size() == 3);
    CPPUNIT_ASSERT(job->_strValues[0] == "aaa");
    CPPUNIT_ASSERT(job->_strValues[1] == "bbb");
    CPPUNIT_ASSERT(job->_strValues[2] == "ccc");
    CPPUNIT_ASSERT(job->_wstrValues.size() == 3);
    CPPUNIT_ASSERT(job->_wstrValues[0] == L"aaa");
    CPPUNIT_ASSERT(job->_wstrValues[1] == L"bbb");
    CPPUNIT_ASSERT(job->_wstrValues[2] == L"ccc");
    CPPUNIT_ASSERT(job->_intValues.size() == 5);
    CPPUNIT_ASSERT(job->_intValues[0] == 10);
    CPPUNIT_ASSERT(job->_intValues[1] == 11);
    CPPUNIT_ASSERT(job->_intValues[2] == 12);
    CPPUNIT_ASSERT(job->_intValues[3] == 13);
    CPPUNIT_ASSERT(job->_intValues[4] == 14);
    CPPUNIT_ASSERT(job->_dummyValue.intValue == 1111);
    CPPUNIT_ASSERT(job->_dummyValue.strValue == "aaaa");
    CPPUNIT_ASSERT(job->_dummyValues.size() == 2);
    CPPUNIT_ASSERT(job->_dummyValues[0].intValue == 1111);
    CPPUNIT_ASSERT(job->_dummyValues[0].strValue == "aaaa");
    CPPUNIT_ASSERT(job->_dummyValues[1].intValue == 2222);
    CPPUNIT_ASSERT(job->_dummyValues[1].strValue == "bbbb");

    // serializeOutputParms
    CPPUNIT_ASSERT(job->serializeOutputParms());
    CPPUNIT_ASSERT(job->exitInfo());

    // serializeGenericOutputParms
    CPPUNIT_ASSERT(job->serializeGenericOutputParms(ExitCode::Ok));
    CPPUNIT_ASSERT(job->_outputParamsStr == outputParamsStr);
}

ExitInfo GuiJobTest::deserializeInputParms() {
    try {
        readParamValue(inParamsStrValue, _strValue);
        readParamValue(inParamsStrValue, _wstrValue);
        readParamValue(inParamsStrValue2, _strValue2);
        readParamValue(inParamsStrValue2, _wstrValue2);
        readParamValue(inParamsIntValue, _intValue);
        readParamValue(inParamsBoolValue, _boolValue);
        readParamValue(inParamsBLOBValue, _blobValue);
        readParamValue(inParamsEnumValue, _enumValue);
        readParamValues(inParamsStrValues, _strValues);
        readParamValues(inParamsStrValues, _wstrValues);
        readParamValues(inParamsIntValues, _intValues);

        std::function<Dummy(const Poco::Dynamic::Var &)> dynamicVar2Dummy = [](const Poco::Dynamic::Var &value) {
            assert(value.isStruct());
            const auto &structValue = value.extract<Poco::DynamicStruct>();
            Dummy dummy;
            CommonUtility::readValueFromStruct(structValue, inParamsDummyStrValue, dummy.strValue);
            CommonUtility::readValueFromStruct(structValue, inParamsDummyIntValue, dummy.intValue);
            return dummy;
        };
        readParamValue(inParamsDummyValue, _dummyValue, dynamicVar2Dummy);
        readParamValues(inParamsDummyValues, _dummyValues, dynamicVar2Dummy);
    } catch (std::exception &) {
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo GuiJobTest::serializeOutputParms([[maybe_unused]] bool hasError /*= false*/) {
    writeParamValue(outParamsStrValue, "qwertz");
    writeParamValue(outParamsStrValue2, "每个人都有他的作战策略");
    writeParamValue(outParamsWStrValue, L"asdfgh");
    writeParamValue(outParamsWStrValue2, L"每个人都有他的作战策略");
    writeParamValue(outParamsIntValue, 999);
    writeParamValue(outParamsBoolValue, true);

    std::string blobStr("0123456789abcdefghijklmnopqrtsuvwxyz");
    CommBLOB blob;
    (void) std::copy(blobStr.begin(), blobStr.end(), std::back_inserter(blob));
    writeParamValue(outParamsBLOBValue, blob);

    writeParamValue(outParamsEnumValue, GuiJobTest::DummyEnum::Dummy3);

    std::vector<std::string> strValues{"zzzz", "yyyy"};
    writeParamValues(outParamsStrValues, strValues);

    std::vector<std::wstring> wstrValues{L"xxxx", L"wwww"};
    writeParamValues(outParamsWStrValues, wstrValues);

    std::vector<int> intValues{20, 21, 22};
    writeParamValues(outParamsIntValues, intValues);

    std::function<Poco::Dynamic::Var(const Dummy &)> dummy2DynamicVar = [](const Dummy &value) {
        Poco::DynamicStruct structValue;
        CommonUtility::writeValueToStruct(structValue, outParamsDummyStrValue, value.strValue);
        CommonUtility::writeValueToStruct(structValue, outParamsDummyIntValue, value.intValue);
        return structValue;
    };
    Dummy dummy{"dddd", 888};
    writeParamValue(outParamsDummyValue, dummy, dummy2DynamicVar);
    std::vector<Dummy> dummyValues{{"dddd", 888}, {"eeee", 7777}};
    writeParamValues(outParamsDummyValues, dummyValues, dummy2DynamicVar);

    return ExitCode::Ok;
}

ExitInfo GuiJobTest::process() {
    return ExitCode::Ok;
}

} // namespace KDC
