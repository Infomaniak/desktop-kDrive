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

#pragma once

#include "..\Common\utilities.h"

#include <winrt\base.h>

template<typename T>
class ClassFactory : public winrt::implements<ClassFactory<T>, IClassFactory> {
    public:
        // IClassFactory
        IFACEMETHODIMP CreateInstance(_In_opt_ IUnknown *unkOuter, REFIID riid, _COM_Outptr_ void **object) {
            try {
                auto provider = winrt::make<T>();
                winrt::com_ptr<IUnknown> unkn{provider.as<IUnknown>()};
                winrt::check_hresult(unkn->QueryInterface(riid, object));
                return S_OK;
            } catch (winrt::hresult_error const &ex) {
                TRACE_ERROR(L"WinRT error caught : hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()),
                            ex.message().c_str());
                return winrt::to_hresult();
            }
        }

        IFACEMETHODIMP LockServer(BOOL lock) { return S_OK; }
};
