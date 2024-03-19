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

#include "stdafx.h"

#include "KDContextMenuFactory.h"
#include "KDContextMenu.h"
#include <new>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")


extern long g_cDllRef;


KDContextMenuFactory::KDContextMenuFactory() : m_cRef(1) {
    InterlockedIncrement(&g_cDllRef);
}

KDContextMenuFactory::~KDContextMenuFactory() {
    InterlockedDecrement(&g_cDllRef);
}


// IUnknown methods

IFACEMETHODIMP KDContextMenuFactory::QueryInterface(REFIID riid, void **ppv) {
    static const QITAB qit[] = {
        QITABENT(KDContextMenuFactory, IClassFactory),
        {0},
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) KDContextMenuFactory::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) KDContextMenuFactory::Release() {
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef) {
        delete this;
    }
    return cRef;
}


// IClassFactory methods

IFACEMETHODIMP KDContextMenuFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) {
    HRESULT hr = CLASS_E_NOAGGREGATION;

    // pUnkOuter is used for aggregation. We do not support it in the sample.
    if (pUnkOuter == NULL) {
        hr = E_OUTOFMEMORY;

        // Create the COM component.
        KDContextMenu *pExt = new (std::nothrow) KDContextMenu();
        if (pExt) {
            // Query the specified interface.
            hr = pExt->QueryInterface(riid, ppv);
            pExt->Release();
        }
    }

    return hr;
}

IFACEMETHODIMP KDContextMenuFactory::LockServer(BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&g_cDllRef);
    } else {
        InterlockedDecrement(&g_cDllRef);
    }
    return S_OK;
}