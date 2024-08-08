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

#include "..\Common\utilities.h"

#include <vector>

#include <shobjidl_core.h>
#include <shlwapi.h>
#include <winrt\base.h>

struct MenuItem {
        bool _root = false;
        std::wstring _title;
        WCHAR _iconPath[MAX_FULL_PATH] = L"";
        bool _enabled = false;
        std::wstring _command;
};

struct ContextMenuInfo {
        MenuItem _menuItem;
        std::vector<MenuItem> _subMenuItems;
};

class __declspec(uuid("A18E9226-E817-4958-BD21-D6967DBD1921")) ExplorerCommandHandler
    : public winrt::implements<ExplorerCommandHandler, IExplorerCommand, IObjectWithSite> {
    public:
        // IExplorerCommand
        IFACEMETHODIMP GetTitle(IShellItemArray *psiItemArray, LPWSTR *ppszName);
        IFACEMETHODIMP GetState(IShellItemArray *psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE *pCmdState);
        IFACEMETHODIMP Invoke(IShellItemArray *psiItemArray, IBindCtx *pbc);
        IFACEMETHODIMP GetFlags(EXPCMDFLAGS *pFlags);
        IFACEMETHODIMP GetIcon(IShellItemArray *psiItemArray, LPWSTR *ppszIcon);
        IFACEMETHODIMP GetToolTip(IShellItemArray *psiItemArray, LPWSTR *ppszInfotip);
        IFACEMETHODIMP GetCanonicalName(GUID *pguidCommandName);
        IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand **ppEnum);

        // IObjectWithSite
        IFACEMETHODIMP SetSite(IUnknown *site);
        IFACEMETHODIMP GetSite(REFIID riid, void **site);

        explicit ExplorerCommandHandler();
        explicit ExplorerCommandHandler(const MenuItem *menuItem);

    private:
        long _cRef;
        winrt::com_ptr<IUnknown> _site;
        ContextMenuInfo _contextMenuInfo;

        void loadCommandItems(IShellItemArray *psiItemArray);
};

class ExplorerCommandHandlerEnumerator : public IEnumExplorerCommand {
    public:
        // IUnknown
        IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) {
            static const QITAB qit[] = {
                QITABENT(ExplorerCommandHandlerEnumerator, IEnumExplorerCommand),
                {0},
            };
            return QISearch(this, qit, riid, ppv);
        }

        IFACEMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_cRef); }
        IFACEMETHODIMP_(ULONG) Release() {
            long cRef = InterlockedDecrement(&_cRef);
            if (!cRef) {
                delete this;
            }
            return cRef;
        }

        // IEnumExplorerCommand
        IFACEMETHODIMP Next(ULONG celt, IExplorerCommand **apUICommand, ULONG *pceltFetched);
        IFACEMETHODIMP Skip(ULONG celt);
        IFACEMETHODIMP Reset();
        IFACEMETHODIMP Clone(IEnumExplorerCommand **ppenum) {
            *ppenum = nullptr;
            return E_NOTIMPL;
        }

        ExplorerCommandHandlerEnumerator(const ContextMenuInfo &contextMenuInfo)
            : _cRef(1), _ullCurrent(0), _contextMenuInfo(contextMenuInfo) {}

    private:
        ~ExplorerCommandHandlerEnumerator() {}

        HRESULT createCommandFromCommandItem(const MenuItem *menuItem, IExplorerCommand **ppExplorerCommand);

        long _cRef;
        size_t _ullCurrent;
        const ContextMenuInfo &_contextMenuInfo;
};
