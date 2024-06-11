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

#include "testselfrestarter.h"

#ifdef _WIN32
#include <Windows.h>
#include <tlhelp32.h> 
#endif

using namespace CppUnit;

namespace KDC {

void TestSelfRestarter::setUp() {}

void TestSelfRestarter::tearDown() {}

#ifdef _WIN32
DWORD GetPidByName(const WCHAR *szProcName, int &count) {
    DWORD dwPID = 0;
    PROCESSENTRY32 pe = {sizeof(PROCESSENTRY32)};

    if (HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); hSnap != INVALID_HANDLE_VALUE) {
        if (Process32First(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, szProcName) == 0) {
                    dwPID = pe.th32ProcessID;
                    count ++;
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    return dwPID;
}
#endif

void TestSelfRestarter::testServerCrash() {
#ifdef _WIN32
    // Create a process that will crash
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcess(
        L"kDrive.exe",  // Application name
        NULL,          // Command line
        NULL,          // Process handle not inheritable
        NULL,          // Thread handle not inheritable
        FALSE,         // Set handle inheritance to FALSE
        0,             // No creation flags
        NULL,          // Use parent's environment block
        NULL,          // Use parent's starting directory
        &si,           // Pointer to STARTUPINFO structure
        &pi            // Pointer to PROCESS_INFORMATION structure
    )) {
        CPPUNIT_FAIL("CreateProcess failed");
    }
    // Wait for the process to start
    Sleep(5000);

    // force the process to crash
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Check if the process has been restarted
    int count = 0;
    GetPidByName(L"kDrive.exe", count);
    if (count < 2) {
        CPPUNIT_FAIL("Process not restarted");
        GetPidByName(L"kDrive_client.exe", count);
    }
    CPPUNIT_ASSERT_EQUAL(2, count);


#endif  // _WIN32


}

}  // namespace KDC
