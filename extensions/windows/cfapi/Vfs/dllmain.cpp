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

#include "..\Common\utilities.h"
#include "vfs.h"
#include "cloudprovider.h"
#include "placeholders.h"

#define PROVIDERID(driveId, folderId) std::wstring(driveId) + L"-" + std::wstring(folderId)
#define PROVIDERID_CSTR(driveId, folderId) std::wstring(PROVIDERID(driveId, folderId)).c_str()

static std::unordered_map<std::wstring, CloudProvider *> s_cloudProviders;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    auto returnCode{FALSE};

    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            returnCode = TRUE;
            break;
    }

    return returnCode;
}

DLL_EXP int __cdecl vfsInit(TraceCbk debugCallback, const wchar_t *appName, DWORD processId, const wchar_t *version,
                            const wchar_t *trashURI) {
    if (debugCallback) {
        Utilities::s_traceCbk = debugCallback;
    }

    if (appName) {
        Utilities::s_appName = appName;

        // Init sync engine pipe name
        Utilities::initPipeName(appName);
    }

    Utilities::s_processId = processId;

    if (version) {
        Utilities::s_version = version;
    }

    if (trashURI) {
        Utilities::s_trashURI = trashURI;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsStart(const wchar_t *driveId, const wchar_t *userId, const wchar_t *folderId, const wchar_t *folderName,
                             const wchar_t *folderPath, wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize) {
    CloudProvider *cloudProvider = nullptr;
    try {
        cloudProvider = new CloudProvider(PROVIDERID_CSTR(driveId, folderId), driveId, folderId, userId, folderName, folderPath);
    } catch (...) {
        TRACE_ERROR(L"CloudProvider construction error!");
        return E_ABORT;
    }

    if (cloudProvider) {
        if (s_cloudProviders.size() == 0) {
            if (!Utilities::connectToPipeServer()) {
                TRACE_ERROR(L"Error in connectToPipeServer!");
                delete cloudProvider;
                return E_ABORT;
            }
        }

        s_cloudProviders[PROVIDERID(driveId, folderId)] = cloudProvider;
        if (!cloudProvider->start(namespaceCLSID, namespaceCLSIDSize)) {
            TRACE_ERROR(L"Start failed!");
            return E_ABORT;
        }
    } else {
        TRACE_ERROR(L"Provider not found : %ls", PROVIDERID_CSTR(driveId, folderId));
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsStop(const wchar_t *driveId, const wchar_t *folderId, bool unregister) {
    TRACE_DEBUG(L"Stop VFS");

    int ret = S_OK;

    CloudProvider *cloudProvider = s_cloudProviders[PROVIDERID(driveId, folderId)];
    if (cloudProvider) {
        if (unregister) {
            TRACE_DEBUG(L"Stop cloud provider");
            if (!cloudProvider->stop()) {
                TRACE_ERROR(L"Stop failed!");
                ret = E_ABORT;
            }
            TRACE_DEBUG(L"Stop cloud provider done");
        }

        delete cloudProvider;
        s_cloudProviders.erase(PROVIDERID(driveId, folderId));

        if (s_cloudProviders.size() == 0) {
            if (!Utilities::disconnectFromPipeServer()) {
                TRACE_ERROR(L"Error in disconnectFromPipeServer!");
                ret = E_ABORT;
            }
        }
    } else {
        TRACE_ERROR(L"Provider not found : %ls", PROVIDERID_CSTR(driveId, folderId));
        ret = E_ABORT;
    }

    TRACE_DEBUG(L"Stop VFS done");

    Utilities::s_traceCbk = nullptr;

    return ret;
}

DLL_EXP int __cdecl vfsGetPlaceHolderStatus(const wchar_t *filePath, bool *isPlaceholder, bool *isDehydrated, bool *isSynced) {
    if (!Placeholders::getStatus(filePath, isPlaceholder, isDehydrated, isSynced)) {
        TRACE_ERROR(L"Error in Placeholders::getPlaceholderStatus : %ls", filePath);
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsSetPlaceHolderStatus(const wchar_t *path, bool syncOngoing) {
    if (!Placeholders::setStatus(path, syncOngoing)) {
        TRACE_ERROR(L"Error in Placeholders::setStatus : %ls, %d", path, syncOngoing);
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsCreatePlaceHolder(const wchar_t *fileId, const wchar_t *relativePath, const wchar_t *destPath,
                                         const WIN32_FIND_DATA *findData) {
    if (!Placeholders::create(fileId, relativePath, destPath, findData)) {
        TRACE_ERROR(L"Error in Placeholders::create : %ls, %ls", relativePath, destPath);
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsConvertToPlaceHolder(const wchar_t *fileId, const wchar_t *filePath) {
    if (!Placeholders::convert(fileId, filePath)) {
        TRACE_ERROR(L"Error in Placeholders::convert : %ls, %ls", fileId, filePath);
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsRevertPlaceHolder(const wchar_t *filePath) {
    if (!Placeholders::revert(filePath)) {
        TRACE_ERROR(L"Error in Placeholders::revert : %ls", filePath);
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsUpdatePlaceHolder(const wchar_t *filePath, const WIN32_FIND_DATA *findData) {
    if (!Placeholders::update(filePath, findData)) {
        TRACE_ERROR(L"Error in Placeholders::update : %ls", filePath);
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsDehydratePlaceHolder(const wchar_t *path) {
    if (!CloudProvider::dehydrate(path)) {
        TRACE_ERROR(L"Error in CloudProvider::dehydrate : %ls", path);
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsHydratePlaceHolder(const wchar_t *driveId, const wchar_t *folderId, const wchar_t *path) {
    CloudProvider *cloudProvider = s_cloudProviders[PROVIDERID(driveId, folderId)];
    if (cloudProvider) {
        if (!cloudProvider->hydrate(path)) {
            TRACE_ERROR(L"Error in CloudProvider::hydrate : %ls", path);
            return E_ABORT;
        }
    } else {
        TRACE_ERROR(L"Provider not found : %ls", PROVIDERID_CSTR(driveId, folderId));
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsUpdateFetchStatus(const wchar_t *driveId, const wchar_t *folderId, const wchar_t *filePath,
                                         const wchar_t *fromFilePath, LONGLONG completed, bool *canceled, bool *finished) {
    CloudProvider *cloudProvider = s_cloudProviders[PROVIDERID(driveId, folderId)];
    if (cloudProvider) {
        if (!cloudProvider->updateTransfer(filePath, fromFilePath, completed, canceled, finished)) {
            TRACE_ERROR(L"Error in CloudProvider::updateTransfer!");
            return E_ABORT;
        }
    } else {
        TRACE_ERROR(L"Provider not found : %ls", PROVIDERID_CSTR(driveId, folderId));
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsCancelFetch(const wchar_t *driveId, const wchar_t *folderId, const wchar_t *filePath) {
    CloudProvider *cloudProvider = s_cloudProviders[PROVIDERID(driveId, folderId)];
    if (cloudProvider) {
        if (!CloudProvider::cancelTransfer(cloudProvider->getProviderInfo(), filePath, true)) {
            TRACE_ERROR(L"Error in CloudProvider::cancelTransfer!");
            return E_ABORT;
        }
    } else {
        TRACE_ERROR(L"Provider not found : %ls", PROVIDERID_CSTR(driveId, folderId));
        return E_ABORT;
    }

    return S_OK;
}

DLL_EXP int __cdecl vfsGetPinState(const wchar_t *path, VfsPinState *state) {
    CF_PLACEHOLDER_STANDARD_INFO info{};

    if (!Placeholders::getInfo(path, info)) {
        TRACE_ERROR(L"Error in Placeholders::getInfo : %ls", path);
        return E_ABORT;
    }

    *state = static_cast<VfsPinState>(info.PinState);

    return S_OK;
}

DLL_EXP int __cdecl vfsSetPinState(const wchar_t *path, VfsPinState state) {
    if (!Placeholders::setPinState(path, static_cast<CF_PIN_STATE>(state))) {
        TRACE_ERROR(L"Error in Placeholders::setPinState : %ls", path);
        return E_ABORT;
    }

    return S_OK;
}
