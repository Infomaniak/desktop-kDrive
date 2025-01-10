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

#include "KDOverlayFactory.h"
#include "KDOverlay.h"

extern long dllReferenceCount;

KDOverlayFactory::KDOverlayFactory(int state) : _referenceCount(1), _state(state) {
    InterlockedIncrement(&dllReferenceCount);
}

KDOverlayFactory::~KDOverlayFactory() {
    InterlockedDecrement(&dllReferenceCount);
}

IFACEMETHODIMP KDOverlayFactory::QueryInterface(REFIID riid, void **ppv) {
    HRESULT hResult = S_OK;

    if (IsEqualIID(IID_IUnknown, riid) || IsEqualIID(IID_IClassFactory, riid)) {
        *ppv = static_cast<IUnknown *>(this);
        AddRef();
    } else {
        hResult = E_NOINTERFACE;
        *ppv = nullptr;
    }

    return hResult;
}

IFACEMETHODIMP_(ULONG) KDOverlayFactory::AddRef() {
    return InterlockedIncrement(&_referenceCount);
}

IFACEMETHODIMP_(ULONG) KDOverlayFactory::Release() {
    ULONG cRef = InterlockedDecrement(&_referenceCount);

    if (0 == cRef) {
        delete this;
    }
    return cRef;
}

IFACEMETHODIMP KDOverlayFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) {
    HRESULT hResult = CLASS_E_NOAGGREGATION;

    if (pUnkOuter != nullptr) {
        return hResult;
    }

    hResult = E_OUTOFMEMORY;
    KDOverlay *lrOverlay = new (std::nothrow) KDOverlay(_state);
    if (!lrOverlay) {
        return hResult;
    }

    hResult = lrOverlay->QueryInterface(riid, ppv);
    lrOverlay->Release();

    return hResult;
}

IFACEMETHODIMP KDOverlayFactory::LockServer(BOOL fLock) {
    if (fLock) {
        InterlockedIncrement(&dllReferenceCount);
    } else {
        InterlockedDecrement(&dllReferenceCount);
    }
    return S_OK;
}
