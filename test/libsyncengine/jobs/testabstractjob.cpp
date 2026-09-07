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

#include "testabstractjob.h"

#include "jobs/abstractjob.h"
#include "libcommon/utility/utility.h"
#include "requests/parameterscache.h"

#include <cstdlib>

namespace KDC {

namespace {

class TestJob final : public AbstractJob {
    public:
        ExitInfo runJob() override { return ExitCode::Ok; }
};

} // namespace

void TestAbstractJob::setUp() {
    TestBase::start();
    _previousExtendedLogEnvVarValue = CommonUtility::envVarValue("KDRIVE_FORCE_EXTENDED_LOG", _isExtendedLogEnvVarSet);
}

void TestAbstractJob::tearDown() {
    if (_isExtendedLogEnvVarSet) {
        (void) CommonUtility::setenv("KDRIVE_FORCE_EXTENDED_LOG", _previousExtendedLogEnvVarValue.c_str(), 1);
    } else {
#if defined(KD_WINDOWS)
        _putenv_s("KDRIVE_FORCE_EXTENDED_LOG", "");
#else
        (void) unsetenv("KDRIVE_FORCE_EXTENDED_LOG");
#endif
    }
    ParametersCache::reset();
    TestBase::stop();
}

void TestAbstractJob::testIsExtendedLogEnabledByEnvironmentVariable() {
    (void) CommonUtility::setenv("KDRIVE_FORCE_EXTENDED_LOG", "1", 1);
    ParametersCache::reset();

    auto parametersCache = ParametersCache::instance(true);
    CPPUNIT_ASSERT(parametersCache != nullptr);
    parametersCache->parameters().setExtendedLog(false);

    TestJob job;
    CPPUNIT_ASSERT(job.isExtendedLog());
}

} // namespace KDC
