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

#include "stdafx.h"

class __declspec(dllexport) KDContextMenuRegHandler {
    public:
        static HRESULT MakeRegistryEntries(const CLSID& clsid, PCWSTR fileType);
        static HRESULT RegisterCOMObject(PCWSTR modulePath, PCWSTR friendlyName, const CLSID& clsid);
        static HRESULT RemoveRegistryEntries(PCWSTR friendlyName);
        static HRESULT UnregisterCOMObject(const CLSID& clsid);

        static HRESULT RegisterInprocServer(PCWSTR pszModule, const CLSID& clsid, PCWSTR pszFriendlyName, PCWSTR pszThreadModel);
        static HRESULT UnregisterInprocServer(const CLSID& clsid);

        static HRESULT RegisterShellExtContextMenuHandler(PCWSTR pszFileType, const CLSID& clsid, PCWSTR pszFriendlyName);
        static HRESULT UnregisterShellExtContextMenuHandler(PCWSTR pszFileType, PCWSTR pszFriendlyName);
};
