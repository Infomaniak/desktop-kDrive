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

#pragma once
#include <shlobj.h> // For IShellExtInit and IContextMenu
#include <string>
#include "KDClientInterface.h"

class KDContextMenu : public IShellExtInit, public IContextMenu {
    public:
        // IUnknown
        IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
        IFACEMETHODIMP_(ULONG) AddRef();
        IFACEMETHODIMP_(ULONG) Release();

        // IShellExtInit
        IFACEMETHODIMP Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID);

        // IContextMenu
        IFACEMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
        IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici);
        IFACEMETHODIMP GetCommandString(UINT_PTR idCommand, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax);

        KDContextMenu();

    protected:
        ~KDContextMenu();

    private:
        // Reference count of component.
        long m_cRef;

        // The name of the selected files (separated by '\x1e')
        std::wstring m_selectedFiles;
        KDClientInterface::ContextMenuInfo m_info;
};
