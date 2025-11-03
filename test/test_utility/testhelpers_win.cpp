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

#include "testhelpers.h"
#include "localtemporarydirectory.h"
#include "io/iohelper.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"

#include <fstream>


#include <Poco/JSON/Object.h>

#include <shlobj_core.h> // SHCreateItemFromIDList
#include <atlbase.h> // CComPtr

#include <sys/utime.h>
#include <sys/types.h>


namespace KDC::testhelpers {


namespace {
HRESULT bindToCsidl(const int csidl, REFIID riid, void **ppv) {
    auto hr = S_OK;
    PIDLIST_ABSOLUTE pidl = nullptr;
    hr = SHGetSpecialFolderLocation(nullptr, csidl, &pidl);
    if (SUCCEEDED(hr)) {
        IShellFolder *psfDesktop = nullptr;
        hr = SHGetDesktopFolder(&psfDesktop);
        if (SUCCEEDED(hr)) {
            if (pidl->mkid.cb) {
                hr = psfDesktop->BindToObject(pidl, nullptr, riid, ppv);
            } else {
                hr = psfDesktop->QueryInterface(riid, ppv);
            }
            (void) psfDesktop->Release();
        }
        CoTaskMemFree(pidl);
    }
    return hr;
}

bool isFolder(IShellItem *item) {
    if (!item) return false;

    SFGAOF attributes = SFGAO_FOLDER;
    if (const HRESULT hr = item->GetAttributes(SFGAO_FOLDER, &attributes); SUCCEEDED(hr)) {
        return (attributes & SFGAO_FOLDER) != 0;
    }

    return false;
}

bool isInFolder(const std::list<SyncName> &relativePath, IShellFolder2 *folder) {
    IEnumIDList *peidl = nullptr;

    if (const auto enumHr = folder->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &peidl); SUCCEEDED(enumHr)) {
        PITEMID_CHILD pidlItem = nullptr;
        while (peidl->Next(1, &pidlItem, nullptr) == S_OK) {
            CComPtr<IShellItem> pItem;
            if (const auto createItemHr = SHCreateItemFromIDList(pidlItem, IID_PPV_ARGS(&pItem)); FAILED(createItemHr)) {
                CoTaskMemFree(pidlItem);
                continue;
            }

            STRRET strRet;
            ZeroMemory(&strRet, sizeof(strRet));
            strRet.uType = STRRET_WSTR;

            if (const auto displayNameHr =
                        folder->GetDisplayNameOf(pidlItem, SHGDN_FORADDRESSBAR | SHGDN_INFOLDER | SHGDN_FOREDITING, &strRet);
                FAILED(displayNameHr)) {
                CoTaskMemFree(pidlItem);
                continue;
            }

            LPTSTR lptstr = nullptr;
            if (const auto stringReturnToStrHr = StrRetToStr(&strRet, pidlItem, &lptstr); FAILED(stringReturnToStrHr)) {
                CoTaskMemFree(pidlItem);
                CoTaskMemFree(lptstr);
                continue;
            }

            const bool match = SyncPath(lptstr) == SyncPath(relativePath.front()).stem();
            if (relativePath.size() == 1 && match) {
                return true;
                CoTaskMemFree(pidlItem);
                CoTaskMemFree(lptstr);
                break;
            }

            if (match && isFolder(pItem)) {
                auto childRelativedPath = relativePath;
                childRelativedPath.pop_front();
                IShellFolder2 *psChildFolder = nullptr;
                (void) SHBindToObject(folder, pidlItem, nullptr, IID_IShellFolder2, reinterpret_cast<void **>(&psChildFolder));

                return isInFolder(childRelativedPath, psChildFolder);
            }
        }
    }

    if (peidl) (void) peidl->Release();

    return false;
}

bool isInFolder(const SyncPath &relativePath, IShellFolder2 *folder) {
    return isInFolder(CommonUtility::splitSyncPath(relativePath), folder);
}

} // namespace

bool isInTrash(const SyncPath &relativePath) {
    bool found = false;

    if (const auto coInitializeHr = CoInitialize(nullptr); SUCCEEDED(coInitializeHr)) {
        IShellFolder2 *psfRecycleBin = nullptr;
        if (const auto bindingHr = bindToCsidl(CSIDL_BITBUCKET, IID_PPV_ARGS(&psfRecycleBin)); SUCCEEDED(bindingHr)) {
            found = isInFolder(relativePath, psfRecycleBin);
            (void) psfRecycleBin->Release();
        }
        CoUninitialize();
    }

    return found;
}

} // namespace KDC::testhelpers
