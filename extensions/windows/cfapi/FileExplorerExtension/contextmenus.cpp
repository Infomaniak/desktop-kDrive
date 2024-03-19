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

#include "contextmenus.h"
#include "..\Common\pipeclient.h"

#include <guiddef.h>
#include <winrt\Windows.Storage.Provider.h>

namespace winrt {
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Provider;
}  // namespace winrt

IFACEMETHODIMP ExplorerCommandHandler::GetTitle(IShellItemArray *psiItemArray, LPWSTR *ppszName) {
    if (_contextMenuInfo._menuItem._root) {
        loadCommandItems(psiItemArray);
    }

    *ppszName = NULL;
    return SHStrDup(_contextMenuInfo._menuItem._title.c_str(), ppszName);
}

IFACEMETHODIMP ExplorerCommandHandler::GetState(IShellItemArray *psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE *pCmdState) {
    if (!_contextMenuInfo._menuItem._enabled) {
        // Following Windows11 update, we need to make sure it is really disabled
        loadCommandItems(psiItemArray);
    }
    *pCmdState = _contextMenuInfo._menuItem._enabled ? ECS_ENABLED : ECS_DISABLED;


    return S_OK;
}

IFACEMETHODIMP ExplorerCommandHandler::GetFlags(EXPCMDFLAGS *pFlags) {
    *pFlags = _contextMenuInfo._menuItem._root ? ECF_HASSUBCOMMANDS : ECF_DEFAULT;

    return S_OK;
}

inline IFACEMETHODIMP ExplorerCommandHandler::GetIcon(IShellItemArray *psiItemArray, LPWSTR *ppszIcon) {
    *ppszIcon = NULL;
    return SHStrDup(_contextMenuInfo._menuItem._iconPath, ppszIcon);
}

inline IFACEMETHODIMP ExplorerCommandHandler::GetToolTip(IShellItemArray *psiItemArray, LPWSTR *ppszInfotip) {
    *ppszInfotip = NULL;

    return S_OK;
}

inline IFACEMETHODIMP ExplorerCommandHandler::GetCanonicalName(GUID *pguidCommandName) {
    if (_contextMenuInfo._menuItem._root) {
        *pguidCommandName = __uuidof(ExplorerCommandHandler);
    }
    return S_OK;
}

inline IFACEMETHODIMP ExplorerCommandHandler::EnumSubCommands(IEnumExplorerCommand **ppEnum) {
    HRESULT hr = E_FAIL;
    ExplorerCommandHandlerEnumerator *pFVCommandEnum = new (std::nothrow) ExplorerCommandHandlerEnumerator(_contextMenuInfo);
    hr = pFVCommandEnum ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        hr = pFVCommandEnum->QueryInterface(IID_PPV_ARGS(ppEnum));
        pFVCommandEnum->Release();
    }
    return hr;
}

IFACEMETHODIMP ExplorerCommandHandler::Invoke(IShellItemArray *psiItemArray, IBindCtx *pbc) {
    HRESULT hr = S_OK;
    if (!_contextMenuInfo._menuItem._command.empty()) {
        DWORD dwNumItems;
        HRESULT hr = psiItemArray->GetCount(&dwNumItems);
        if (SUCCEEDED(hr) && dwNumItems > 0) {
            for (DWORD i = 0; i < dwNumItems; i++) {
                IShellItem *psiShellItem;
                hr = psiItemArray->GetItemAt(i, &psiShellItem);
                if (SUCCEEDED(hr)) {
                    LPWSTR ppszName;
                    hr = psiShellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEEDITING, &ppszName);
                    if (SUCCEEDED(hr)) {
                        if (!PipeClient::getInstance().sendMessageWithoutAnswer(_contextMenuInfo._menuItem._command.c_str(),
                                                                                ppszName)) {
                            TRACE_ERROR(L"Error in PipeClient::sendMessage!");
                            hr = E_FAIL;
                            break;
                        }
                    }
                }
            }
        }
    }

    return hr;
}

IFACEMETHODIMP ExplorerCommandHandler::SetSite(IUnknown *site) {
    _site.copy_from(site);
    return S_OK;
}
IFACEMETHODIMP ExplorerCommandHandler::GetSite(REFIID riid, void **site) {
    return _site->QueryInterface(riid, site);
}

ExplorerCommandHandler::ExplorerCommandHandler() {
    _contextMenuInfo._menuItem._root = true;
    _cRef = 0;
}

ExplorerCommandHandler::ExplorerCommandHandler(const MenuItem *menuItem) {
    _contextMenuInfo._menuItem = *menuItem;
}

void ExplorerCommandHandler::loadCommandItems(IShellItemArray *psiItemArray) {
    if (_contextMenuInfo._subMenuItems.empty()) {
        // Make file list
        std::wstring files;
        DWORD dwNumItems;
        HRESULT hr = psiItemArray->GetCount(&dwNumItems);
        if (SUCCEEDED(hr) && dwNumItems > 0) {
            for (DWORD i = 0; i < dwNumItems; i++) {
                IShellItem *psiShellItem;
                hr = psiItemArray->GetItemAt(i, &psiShellItem);
                if (SUCCEEDED(hr)) {
                    LPWSTR ppszName;
                    hr = psiShellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEEDITING, &ppszName);
                    if (SUCCEEDED(hr)) {
                        if (files.size() > 0) {
                            files += MSG_ARG_SEPARATOR;
                        }
                        files += ppszName;
                    }
                }
            }
        }

        if (files.size() == 0) {
            return;
        }

        // Get menu items
        LONGLONG itemsMsgId;
        if (!PipeClient::getInstance().sendMessageWithAnswer(L"GET_ALL_MENU_ITEMS", files, itemsMsgId)) {
            TRACE_ERROR(L"Error in PipeClient::sendMessage!");
            return;
        }

        std::wstring response = std::wstring();
        if (!PipeClient::getInstance().readMessage(itemsMsgId, response)) {
            TRACE_ERROR(L"Error in PipeClient::readMessage!");
            return;
        }

        // Read menu title
        std::wstring menuTitle;
        if (!Utilities::readNextValue(response, menuTitle) || menuTitle.empty()) {
            TRACE_ERROR(L"Incorrect message!");
            return;
        }
        _contextMenuInfo._menuItem._title = move(menuTitle);

        // Read VFS mode
        std::wstring vfsMode;
        if (!Utilities::readNextValue(response, vfsMode)) {
            TRACE_ERROR(L"Incorrect message!");
            return;
        }
        if (vfsMode.compare(L"wincfapi") != 0) {
            return;
        }

        do {
            // Read next menu item info
            std::wstring commandName;
            if (!Utilities::readNextValue(response, commandName)) {
                TRACE_ERROR(L"Incorrect message!");
                return;
            }
            std::wstring flags;
            if (!Utilities::readNextValue(response, flags)) {
                TRACE_ERROR(L"Incorrect message!");
                return;
            }
            std::wstring title;
            if (!Utilities::readNextValue(response, title)) {
                TRACE_ERROR(L"Incorrect message!");
                return;
            }

            // bool enabled = flags.find(L'd') == std::string::npos;
            MenuItem menuItem{false, title, L"", true /*enabled*/,
                              commandName};  // enabled flag not supported by new Windows 11 menu
            _contextMenuInfo._subMenuItems.push_back(menuItem);
        } while (!response.empty());

        GetModuleFileNameW(NULL, _contextMenuInfo._menuItem._iconPath, MAX_FULL_PATH);
        _contextMenuInfo._menuItem._enabled = _contextMenuInfo._subMenuItems.size() > 0;
    }
}

HRESULT ExplorerCommandHandlerEnumerator::createCommandFromCommandItem(const MenuItem *menuItem,
                                                                       IExplorerCommand **ppExplorerCommand) {
    ExplorerCommandHandler *pCommand = new (std::nothrow) ExplorerCommandHandler(menuItem);
    HRESULT hr = pCommand ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        hr = pCommand->QueryInterface(IID_PPV_ARGS(ppExplorerCommand));
        pCommand->Release();
    }
    return hr;
}

IFACEMETHODIMP ExplorerCommandHandlerEnumerator::Next(ULONG celt, IExplorerCommand **apUICommand, ULONG *pceltFetched) {
    HRESULT hr = S_FALSE;
    if (_ullCurrent <= _contextMenuInfo._subMenuItems.size()) {
        UINT uIndex = 0;
        HRESULT hrLocal = S_OK;
        while (uIndex < celt && _ullCurrent < _contextMenuInfo._subMenuItems.size() && SUCCEEDED(hrLocal)) {
            hrLocal = createCommandFromCommandItem(&_contextMenuInfo._subMenuItems[_ullCurrent], &(apUICommand[uIndex]));
            uIndex++;
            _ullCurrent++;
        }

        if (pceltFetched != NULL) {
            *pceltFetched = uIndex;
        }

        if (uIndex == celt) {
            hr = S_OK;
        }
    }
    return hr;
}

IFACEMETHODIMP ExplorerCommandHandlerEnumerator::Skip(ULONG celt) {
    _ullCurrent += celt;

    HRESULT hr = S_OK;
    if (_ullCurrent > _contextMenuInfo._subMenuItems.size()) {
        _ullCurrent = _contextMenuInfo._subMenuItems.size();
        hr = S_FALSE;
    }
    return hr;
}

IFACEMETHODIMP ExplorerCommandHandlerEnumerator::Reset() {
    _ullCurrent = 0;
    return S_OK;
}
