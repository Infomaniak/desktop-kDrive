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
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#include <TlHelp32.h>
#endif

using namespace CppUnit;

namespace KDC {

void TestSelfRestarter::setUp() {}

void TestSelfRestarter::tearDown() {
    killAllInstances();
}

#ifdef _WIN32
void GetPidByName(const WCHAR *szProcName, std::vector<DWORD> &pids) {
    PROCESSENTRY32 pe = {sizeof(PROCESSENTRY32)};

    if (HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); hSnap != INVALID_HANDLE_VALUE) {
        if (Process32First(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, szProcName) == 0) {
                    pids.push_back(pe.th32ProcessID);
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
}
#endif

void TestSelfRestarter::testServerCrash() {
    CPPUNIT_ASSERT(killAllInstances());
    CPPUNIT_ASSERT(startApp("kDrive.exe"));

    // Wait for both processes to start (server and client) max 10s
    auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(10);
    while (std::chrono::system_clock::now() < timeout) {
        if (isAppRunningByName(L"kDrive.exe") && isAppRunningByName(L"kDrive_client.exe")) {
            break;
        }
    }
    CPPUNIT_ASSERT_LESS(timeout, std::chrono::system_clock::now());
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // force the server to crash
    killApp(L"kDrive.exe");

    // Wait for the server to restart
    timeout = std::chrono::system_clock::now() + std::chrono::seconds(10);
    while (std::chrono::system_clock::now() < timeout) {
        if (isAppRunningByName(L"kDrive.exe") && isAppRunningByName(L"kDrive_client.exe")) {
            break;
        }
    }
    CPPUNIT_ASSERT_LESS(timeout, std::chrono::system_clock::now());
    std::this_thread::sleep_for(std::chrono::seconds(10));


    // Kill the server again
    killApp(L"kDrive.exe");

    // Check that the client app close in the 10 seconds
    timeout = std::chrono::system_clock::now() + std::chrono::seconds(10);
    while (std::chrono::system_clock::now() < timeout) {
        if (!isAppRunningByName(L"kDrive_client.exe")) {
            break;
        }
    }
    CPPUNIT_ASSERT_LESS(timeout, std::chrono::system_clock::now());

    // Check that the client close do not restart in the 10 seconds (server still running because of the crash pop-up)
    timeout = std::chrono::system_clock::now() + std::chrono::seconds(10);
    while (std::chrono::system_clock::now() < timeout) {
        if (isAppRunningByName(L"kDrive_client.exe")) {
            break;
        }
    }
    CPPUNIT_ASSERT_GREATER(timeout, std::chrono::system_clock::now());
    CPPUNIT_ASSERT(killAllInstances());
}

void TestSelfRestarter::testClientCrash() {
    CPPUNIT_ASSERT(killAllInstances());
    CPPUNIT_ASSERT(startApp("kDrive.exe"));

    // Wait for both processes to start (server and client) max 10s
    auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(10);
    while (std::chrono::system_clock::now() < timeout) {
        if (isAppRunningByName(L"kDrive.exe") && isAppRunningByName(L"kDrive_client.exe")) {
            break;
        }
    }
    CPPUNIT_ASSERT_LESS(timeout, std::chrono::system_clock::now());
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // force the client to crash
    CPPUNIT_ASSERT(killApp(L"kDrive_client.exe"));

    // Wait for the client to restart
    timeout = std::chrono::system_clock::now() + std::chrono::seconds(10);
    while (std::chrono::system_clock::now() < timeout) {
        if (isAppRunningByName(L"kDrive_client.exe") && isAppRunningByName(L"kDrive.exe")) {
            break;
        }
    }
    CPPUNIT_ASSERT_LESS(timeout, std::chrono::system_clock::now());
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Kill the client again
    CPPUNIT_ASSERT(killApp(L"kDrive_client.exe"));

    // Check that the client do not restart in the 10 seconds
    timeout = std::chrono::system_clock::now() + std::chrono::seconds(10);
    while (std::chrono::system_clock::now() < timeout) {
        if (isAppRunningByName(L"kDrive_client.exe")) {
            break;
        }
    }
    CPPUNIT_ASSERT_GREATER(timeout, std::chrono::system_clock::now());
    CPPUNIT_ASSERT(killAllInstances());
}


bool TestSelfRestarter::killAllInstances() const {
    return killApp(L"kDrive.exe") && killApp(L"kDrive_client.exe");
}

bool TestSelfRestarter::isAppRunningByName(std::wstring name) const {
#ifdef _WIN32
    std::vector<DWORD> pids;
    GetPidByName(std::wstring(name.begin(), name.end()).c_str(), pids);
    return !pids.empty();
#endif
}

bool TestSelfRestarter::startApp(std::filesystem::path appPath) const {
    std::cout << "Starting " << appPath << std::endl;
#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = FALSE;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcess(appPath.c_str(),   // Application name
                       NULL,              // Command line
                       NULL,              // Process handle not inheritable
                       NULL,              // Thread handle not inheritable
                       FALSE,             // Set handle inheritance to FALSE
                       DETACHED_PROCESS,  // No creation flags
                       NULL,              // Use parent's environment block
                       NULL,              // Use parent's starting directory
                       &si,               // Pointer to STARTUPINFO structure
                       &pi                // Pointer to PROCESS_INFORMATION structure
                       )) {
        std::cout << "Start process failed with error " << GetLastError() << std::endl;
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#endif

    std::cout << appPath << " started" << std::endl;
    return true;
}

bool TestSelfRestarter::killApp(std::wstring name) const {
    std::cout << "Killing " << std::string(name.begin(), name.end()) << std::endl;
#ifdef _WIN32
    std::vector<DWORD> pids;
    GetPidByName(name.c_str(), pids);
    if (pids.empty()) {
        std::cout << "No process to kill" << std::endl;
        return true;  // Already closed
    }
    for (DWORD pid : pids) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess == nullptr) {
            return false;
        }
        auto res = TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);
        if (res == 0) {
            std::cout << "TerminateProcess failed with error " << GetLastError() << std::endl;
            return false;
        }
    }
#endif
    std::cout << "Killed" << std::endl;
    return true;
}
}  // namespace KDC
