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

#include <functional>

#if defined(_WINDLL)
#define DLL_EXP __declspec(dllexport)
#else
#define DLL_EXP __declspec(dllimport)
#endif

extern "C" {
// CF_PIN_STATE clone
typedef enum {
    VFS_PIN_STATE_UNSPECIFIED = 0,
    VFS_PIN_STATE_PINNED = 1,
    VFS_PIN_STATE_UNPINNED = 2,
    VFS_PIN_STATE_EXCLUDED = 3,
    VFS_PIN_STATE_INHERIT = 4,
    VFS_PIN_STATE_UNKNOWN
} VfsPinState;

DLL_EXP int __cdecl vfsInit(TraceCbk debugCallback, const wchar_t *appName, DWORD processId, const wchar_t *version,
                            const wchar_t *trashURI);

DLL_EXP int __cdecl vfsStart(const wchar_t *driveId, const wchar_t *userId, const wchar_t *folderId, const wchar_t *folderName,
                             const wchar_t *folderPath, wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize);

DLL_EXP int __cdecl vfsStop(const wchar_t *driveId, const wchar_t *folderId, bool unregister);

DLL_EXP int __cdecl vfsGetPlaceHolderStatus(const wchar_t *filePath, bool *isPlaceholder, bool *isDehydrated, bool *isSynced);

DLL_EXP int __cdecl vfsSetPlaceHolderStatus(const wchar_t *path, bool syncOngoing);

DLL_EXP int __cdecl vfsDehydratePlaceHolder(const wchar_t *path);

DLL_EXP int __cdecl vfsHydratePlaceHolder(const wchar_t *driveId, const wchar_t *folderId, const wchar_t *path);

DLL_EXP int __cdecl vfsCreatePlaceHolder(const wchar_t *fileId, const wchar_t *relativePath, const wchar_t *destPath,
                                         const WIN32_FIND_DATA *findData);

DLL_EXP int __cdecl vfsConvertToPlaceHolder(const wchar_t *fileId, const wchar_t *filePath);

DLL_EXP int __cdecl vfsRevertPlaceHolder(const wchar_t *filePath);

DLL_EXP int __cdecl vfsUpdatePlaceHolder(const wchar_t *filePath, const WIN32_FIND_DATA *findData);

DLL_EXP int __cdecl vfsUpdateFetchStatus(const wchar_t *driveId, const wchar_t *folderId, const wchar_t *filePath,
                                         const wchar_t *fromFilePath, LONGLONG completed, bool *canceled, bool *finished);

DLL_EXP int __cdecl vfsCancelFetch(const wchar_t *driveId, const wchar_t *folderId, const wchar_t *filePath);

DLL_EXP int __cdecl vfsGetPinState(const wchar_t *path, VfsPinState *state);

DLL_EXP int __cdecl vfsSetPinState(const wchar_t *path, VfsPinState state);
}
