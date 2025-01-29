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

#include <windows.h>
#include <Guiddef.h>
#include "KDContextMenuRegHandler.h"
#include "KDContextMenuFactory.h"

// {841A0AAD-AA11-4B50-84D9-7F8E727D77D7}
static const GUID CLSID_FileContextMenuExt = {0x841a0aad, 0xaa11, 0x4b50, {0x84, 0xd9, 0x7f, 0x8e, 0x72, 0x7d, 0x77, 0xd7}};

HINSTANCE g_hInst = nullptr;
long g_cDllRef = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            // Hold the instance of this DLL module, we will use it to get the
            // path of the DLL to register the component.
            g_hInst = hModule;
            DisableThreadLibraryCalls(hModule);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
    HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

    if (IsEqualCLSID(CLSID_FileContextMenuExt, rclsid)) {
        hr = E_OUTOFMEMORY;

        KDContextMenuFactory *pClassFactory = new KDContextMenuFactory();
        if (pClassFactory) {
            hr = pClassFactory->QueryInterface(riid, ppv);
            pClassFactory->Release();
        }
    }

    return hr;
}

STDAPI DllCanUnloadNow(void) {
    return g_cDllRef > 0 ? S_FALSE : S_OK;
}

STDAPI DllRegisterServer(void) {
    HRESULT hr;

    wchar_t szModule[MAX_PATH];
    if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Register the component.
    hr = KDContextMenuRegHandler::RegisterInprocServer(szModule, CLSID_FileContextMenuExt, L"KDContextMenuHandler Class",
                                                       L"Apartment");
    if (SUCCEEDED(hr)) {
        // Register the context menu handler. The context menu handler is
        // associated with the .cpp file class.
        hr = KDContextMenuRegHandler::RegisterShellExtContextMenuHandler(L"AllFileSystemObjects", CLSID_FileContextMenuExt,
                                                                         L"KDContextMenuHandler");
    }

    return hr;
}

STDAPI DllUnregisterServer(void) {
    HRESULT hr = S_OK;

    wchar_t szModule[MAX_PATH];
    if (GetModuleFileName(g_hInst, szModule, ARRAYSIZE(szModule)) == 0) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    // Unregister the component.
    hr = KDContextMenuRegHandler::UnregisterInprocServer(CLSID_FileContextMenuExt);
    if (SUCCEEDED(hr)) {
        // Unregister the context menu handler.
        hr = KDContextMenuRegHandler::UnregisterShellExtContextMenuHandler(L"AllFileSystemObjects", L"KDContextMenuHandler");
    }

    return hr;
}
