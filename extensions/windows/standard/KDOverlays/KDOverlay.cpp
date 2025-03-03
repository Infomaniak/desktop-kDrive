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

#include "stdafx.h"

#include "KDOverlay.h"

#include "KDOverlayFactory.h"
#include "StringUtil.h"

#include "RemotePathChecker.h"

#include "resource.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>

using namespace std;

#pragma comment(lib, "shlwapi.lib")

extern HINSTANCE instanceHandle;

#define IDM_DISPLAY 0
#define IDB_OK 101

namespace {

unique_ptr<RemotePathChecker> s_instance;

RemotePathChecker *getGlobalChecker() {
    // On Vista we'll run into issue #2680 if we try to create the thread+pipe connection
    // on any DllGetClassObject of our registered classes.
    // Work around the issue by creating the static RemotePathChecker only once actually needed.
    static once_flag s_onceFlag;
    call_once(s_onceFlag, [] { s_instance.reset(new RemotePathChecker); });

    return s_instance.get();
}

} // namespace
KDOverlay::KDOverlay(int state) : _referenceCount(1), _state(state) {}

KDOverlay::~KDOverlay(void) {}


IFACEMETHODIMP_(ULONG) KDOverlay::AddRef() {
    return InterlockedIncrement(&_referenceCount);
}

IFACEMETHODIMP KDOverlay::QueryInterface(REFIID riid, void **ppv) {
    HRESULT hr = S_OK;

    if (IsEqualIID(IID_IUnknown, riid) || IsEqualIID(IID_IShellIconOverlayIdentifier, riid)) {
        *ppv = static_cast<IShellIconOverlayIdentifier *>(this);
    } else {
        hr = E_NOINTERFACE;
        *ppv = nullptr;
    }

    if (*ppv) {
        AddRef();
    }

    return hr;
}

IFACEMETHODIMP_(ULONG) KDOverlay::Release() {
    ULONG cRef = InterlockedDecrement(&_referenceCount);
    if (0 == cRef) {
        delete this;
    }

    return cRef;
}

IFACEMETHODIMP KDOverlay::GetPriority(int *pPriority) {
    // this defines which handler has prededence, so
    // we order this in terms of likelyhood
    switch (_state) {
        case State_OK:
            *pPriority = 0;
            break;
        case State_OKShared:
            *pPriority = 1;
            break;
        case State_Warning:
            *pPriority = 2;
            break;
        case State_Sync:
            *pPriority = 3;
            break;
        case State_Error:
            *pPriority = 4;
            break;
        default:
            *pPriority = 5;
            break;
    }

    return S_OK;
}

IFACEMETHODIMP KDOverlay::IsMemberOf(PCWSTR pwszPath, DWORD dwAttrib) {
    RemotePathChecker *checker = getGlobalChecker();
    std::shared_ptr<const std::vector<std::wstring>> watchedDirectories = checker->WatchedDirectories();

    if (watchedDirectories->empty()) {
        return MAKE_HRESULT(S_FALSE, 0, 0);
    }

    bool watched = false;
    size_t pathLength = wcslen(pwszPath);
    for (auto it = watchedDirectories->begin(); it != watchedDirectories->end(); ++it) {
        if (StringUtil::isDescendantOf(pwszPath, pathLength, *it)) {
            watched = true;
        }
    }

    if (!watched) {
        return MAKE_HRESULT(S_FALSE, 0, 0);
    }

    int state = 0;
    if (!checker->IsMonitoredPath(pwszPath, &state)) {
        return MAKE_HRESULT(S_FALSE, 0, 0);
    }
    return MAKE_HRESULT(state == _state ? S_OK : S_FALSE, 0, 0);
}

IFACEMETHODIMP KDOverlay::GetOverlayInfo(PWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags) {
    *pIndex = 0;
    *pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
    *pIndex = _state;

    if (GetModuleFileName(instanceHandle, pwszIconFile, cchMax) == 0) {
        HRESULT hResult = HRESULT_FROM_WIN32(GetLastError());
        wcerr << L"IsOK? " << (hResult == S_OK) << L" with path " << pwszIconFile << L", index " << *pIndex << endl;
        return hResult;
    }

    return S_OK;
}
