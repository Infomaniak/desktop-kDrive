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

#include "..\Common\utilities.h"
#include "shellservices.h"

#include <string>

#include <ntstatus.h>

#define APPNAME L"kDrive"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    // Create stop event
    HANDLE stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (stopEvent == NULL) {
        TRACE_ERROR(L"Error in CreateEvent!");
        return -1;
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        TRACE_DEBUG(L"Event already exists!");
    }

    // Initialize
    Utilities::s_appName = APPNAME;

    Utilities::initPipeName(APPNAME);

    if (!ShellServices::initAndStartServiceTask()) {
        TRACE_DEBUG(L"ShellServices::initAndStartServiceTask");
        return -1;
    }

    // Main code
    while (WaitForSingleObject(stopEvent, 1000) != WAIT_OBJECT_0) {
        // Do nothing
    }

    // Finalize
    CloseHandle(stopEvent);

    return 0;
}
