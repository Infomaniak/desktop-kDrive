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

#include "..\Common\framework.h"
#include "providerinfo.h"

#include <cfapi.h>

class CloudProvider {
    public:
        CloudProvider(const LPCWSTR id, const LPCWSTR driveId, const LPCWSTR folderId, const LPCWSTR userId,
                      const LPCWSTR folderName, const LPCWSTR folderPath);
        ~CloudProvider();

        inline ProviderInfo *getProviderInfo() const { return _providerInfo; }
        bool start(wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize);
        bool stop();
        static bool dehydrate(const wchar_t *path);
        bool hydrate(const wchar_t *path);
        bool updateTransfer(const wchar_t *filePath, const wchar_t *fromFilePath, LONGLONG completed, bool *canceled,
                            bool *finished);
        static bool cancelTransfer(ProviderInfo *providerInfo, const wchar_t *filePath, bool updateStatus);

    private:
        bool connectSyncRootTransferCallbacks();
        bool disconnectSyncRootTransferCallbacks();

        static void CALLBACK onFetchData(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                         _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onValidateData(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                            _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onCancelFetchData(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                               _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyFetchPlaceholder(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                      _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onCancelFetchPlacehoders(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                      _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyOpenCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                    _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyCloseCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                     _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyDehydrate(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                               _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyDehydrateCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                         _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyDelete(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                            _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyDeleteCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                      _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyRename(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                            _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

        static void CALLBACK onNotifyRenameCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                      _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters);

    private:
        static CF_CALLBACK_REGISTRATION s_callbackTable[];
        CF_CONNECTION_KEY _transferCallbackConnectionKey;
        ProviderInfo *_providerInfo;
        std::wstring _synRootID;

        static bool addFolderToSearchIndexer(const PCWSTR folder);
        static bool cancelFetchData(CF_CONNECTION_KEY connectionKey, CF_TRANSFER_KEY transferKey,
                                    LARGE_INTEGER requiredFileOffset);
};
