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

#include "cloudprovider.h"
#include "..\Common\utilities.h"
#include "..\Common\pipeclient.h"
#include "cloudproviderregistrar.h"
#include "placeholders.h"

#include <fstream>

#include <ntstatus.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl_core.h>
#include <shlobj_core.h>
#include <winrt\base.h>
#include <filesystem>

#define MSSEARCH_INDEX L"SystemIndex"

DEFINE_PROPERTYKEY(PKEY_StorageProviderTransferProgress, 0xE77E90DF, 0x6271, 0x4F5B, 0x83, 0x4F, 0x2D, 0xD1, 0xF2, 0x45, 0xDD,
                   0xA4, 4);

#define CHUNKBASESIZE 4096
#define MAXCHUNKS 1000

#define FIELD_SIZE(type, field) (sizeof(((type *) 0)->field))
#define CF_SIZE_OF_OP_PARAM(field) (FIELD_OFFSET(CF_OPERATION_PARAMETERS, field) + FIELD_SIZE(CF_OPERATION_PARAMETERS, field))

CF_CALLBACK_REGISTRATION CloudProvider::s_callbackTable[] = {
        {CF_CALLBACK_TYPE_FETCH_DATA, CloudProvider::onFetchData},
        //{ CF_CALLBACK_TYPE_VALIDATE_DATA, CloudProvider::onValidateData },
        {CF_CALLBACK_TYPE_CANCEL_FETCH_DATA, CloudProvider::onCancelFetchData},
        //{ CF_CALLBACK_TYPE_FETCH_PLACEHOLDERS, CloudProvider::onNotifyFetchPlaceholder },
        //{ CF_CALLBACK_TYPE_CANCEL_FETCH_PLACEHOLDERS, CloudProvider::onCancelFetchPlacehoders },
        //{ CF_CALLBACK_TYPE_NOTIFY_FILE_OPEN_COMPLETION, CloudProvider::onNotifyOpenCompletion },
        //{ CF_CALLBACK_TYPE_NOTIFY_FILE_CLOSE_COMPLETION, CloudProvider::onNotifyCloseCompletion },
        {CF_CALLBACK_TYPE_NOTIFY_DEHYDRATE, CloudProvider::onNotifyDehydrate},
        //{ CF_CALLBACK_TYPE_NOTIFY_DEHYDRATE_COMPLETION, CloudProvider::onNotifyDehydrateCompletion },
        //{ CF_CALLBACK_TYPE_NOTIFY_DELETE, CloudProvider::onNotifyDelete },
        //{ CF_CALLBACK_TYPE_NOTIFY_DELETE_COMPLETION, CloudProvider::onNotifyDeleteCompletion },
        //{ CF_CALLBACK_TYPE_NOTIFY_RENAME, CloudProvider::onNotifyRename },
        //{ CF_CALLBACK_TYPE_NOTIFY_RENAME_COMPLETION, CloudProvider::onNotifyRenameCompletion },
        CF_CALLBACK_REGISTRATION_END};

CloudProvider::CloudProvider(const LPCWSTR id, const LPCWSTR driveId, const LPCWSTR folderId, const LPCWSTR userId,
                             const LPCWSTR folderName, const LPCWSTR folderPath) {
    _transferCallbackConnectionKey = CF_CONNECTION_KEY();
    _synRootID = std::wstring();
    _providerInfo = new ProviderInfo(id, driveId, folderId, userId, folderName, folderPath);
}

CloudProvider::~CloudProvider() {
    delete _providerInfo;
}

bool CloudProvider::start(wchar_t *namespaceCLSID, DWORD *namespaceCLSIDSize) {
    if (!_providerInfo) {
        TRACE_ERROR(L"Not initialized!");
        return FALSE;
    }

    // The client folder (syncroot) must be indexed in order for states to properly display
    TRACE_DEBUG(L"Calling Utilities::addFolderToSearchIndexer: path = %ls", _providerInfo->folderPath());
    if (!addFolderToSearchIndexer(_providerInfo->folderPath())) {
        TRACE_ERROR(L"Error in Utilities::addFolderToSearchIndexer!");
        return false;
    }

    // Register the provider with the shell so that the Sync Root shows up in File Explorer
    TRACE_DEBUG(L"Calling CloudProviderRegistrar::registerWithShell");
    _synRootID = CloudProviderRegistrar::registerWithShell(_providerInfo, namespaceCLSID, namespaceCLSIDSize);
    if (_synRootID.empty()) {
        TRACE_ERROR(L"Error in CloudProviderRegistrar::registerWithShell!");
        return false;
    }
    TRACE_DEBUG(L"CloudProviderRegistrar::registerWithShell done: syncRootID = %ls", _synRootID.c_str());

    // Hook up callback methods for transferring files between client and server
    TRACE_DEBUG(L"Calling connectSyncRootTransferCallbacks");
    if (!connectSyncRootTransferCallbacks()) {
        TRACE_ERROR(L"Error in connectSyncRootTransferCallbacks!");
        return false;
    }
    return true;
}

bool CloudProvider::stop() {
    if (!_providerInfo) {
        TRACE_ERROR(L"Not initialized!");
        return false;
    }

    // Unhook up callback methods
    if (!disconnectSyncRootTransferCallbacks()) {
        TRACE_ERROR(L"Error in disconnectSyncRootTransferCallbacks!");
    }

    TRACE_DEBUG(L"Calling CloudProviderRegistrar::unregister");
    if (!CloudProviderRegistrar::unregister(_synRootID)) {
        TRACE_ERROR(L"Error in CloudProviderRegistrar::Unregister!");
    }

    TRACE_DEBUG(L"Cloud provider stopped");

    return true;
}

bool CloudProvider::dehydrate(const wchar_t *path) {
    bool res = true;

    winrt::handle fileHandle(CreateFile(path, WRITE_DAC, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_NO_RECALL, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile: %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    LARGE_INTEGER offset = {};
    LARGE_INTEGER length;
    length.QuadPart = MAXLONGLONG;
    try {
        TRACE_DEBUG(L"Dehydrating placeholder: path = %ls", path);
        winrt::check_hresult(CfDehydratePlaceholder(fileHandle.get(), offset, length, CF_DEHYDRATE_FLAG_NONE, nullptr));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        if (ex.code() == HRESULT_FROM_WIN32(ERROR_CLOUD_FILE_IN_USE)) {
            // Workaround to fix a MS Photos issue that leads to locking pictures
            CF_PLACEHOLDER_STANDARD_INFO info;
            DWORD retLength = 0;
            try {
                TRACE_DEBUG(L"Get placeholder info: path = %ls", path);
                winrt::check_hresult(
                        CfGetPlaceholderInfo(fileHandle.get(), CF_PLACEHOLDER_INFO_STANDARD, &info, sizeof(info), &retLength));
            } catch (winrt::hresult_error const &ex) {
                if (ex.code() != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
                    TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()),
                                ex.message().c_str());
                    res = false;
                }
            }

            if (res) {
                try {
                    TRACE_DEBUG(L"Reverting placeholder: path = %ls", path);
                    winrt::check_hresult(CfRevertPlaceholder(fileHandle.get(), CF_REVERT_FLAG_NONE, nullptr));

                    TRACE_DEBUG(L"Converting to placeholder: path = %ls", path);
                    winrt::check_hresult(CfConvertToPlaceholder(fileHandle.get(), info.FileIdentity, info.FileIdentityLength,
                                                                CF_CONVERT_FLAG_MARK_IN_SYNC, nullptr, nullptr));

                    TRACE_DEBUG(L"Dehydrating placeholder: path = %ls", path);
                    winrt::check_hresult(
                            CfDehydratePlaceholder(fileHandle.get(), offset, length, CF_DEHYDRATE_FLAG_NONE, nullptr));
                } catch (winrt::hresult_error const &ex) {
                    TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()),
                                ex.message().c_str());
                    res = false;
                }
            }
        } else {
            res = false;
        }
    }

    return res;
}

bool CloudProvider::hydrate(const wchar_t *path) {
    if (!_providerInfo) {
        TRACE_ERROR(L"Not initialized!");
        return FALSE;
    }

    if (_providerInfo->_fetchMap.find(path) != _providerInfo->_fetchMap.end()) {
        TRACE_DEBUG(L"Fetch already in progress: path = %ls", path);
        return true;
    }

    winrt::handle fileHandle(CreateFile(path, WRITE_DAC, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_NO_RECALL, nullptr));
    if (fileHandle.get() == INVALID_HANDLE_VALUE) {
        TRACE_ERROR(L"Error in CreateFile: %ls", Utilities::getLastErrorMessage().c_str());
        return false;
    }

    LARGE_INTEGER offset = {};
    LARGE_INTEGER length;
    length.QuadPart = MAXLONGLONG;
    bool res = true;

    try {
        TRACE_DEBUG(L"Hydrating placeholder: path = %ls", path);
        winrt::check_hresult(CfHydratePlaceholder(fileHandle.get(), offset, length, CF_HYDRATE_FLAG_NONE, nullptr));
    } catch (winrt::hresult_error const &ex) {
        std::lock_guard<std::mutex> lk(_providerInfo->_fetchMapMutex);
        const auto &fetchInfoIt = _providerInfo->_fetchMap.find(path);
        if (fetchInfoIt == _providerInfo->_fetchMap.end() || fetchInfoIt->second.getCancel()) {
            // Hydration has been cancelled
            TRACE_DEBUG(L"Hydration has been cancelled: path = %ls", path);
            res = true;
        } else {
            TRACE_ERROR(L"WinRT error caught : %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
            if (ex.code() == 0x800701aa) {
                // Timeout
                res = false;
            } else {
                res = false;
            }
        }
    }

    return res;
}

bool CloudProvider::updateTransfer(const wchar_t *filePath, const wchar_t *fromFilePath, LONGLONG completed, bool *canceled,
                                   bool *finished) {
    if (!_providerInfo) {
        TRACE_ERROR(L"Not initialized!");
        return FALSE;
    }

    if (!filePath || !fromFilePath || !canceled) {
        TRACE_DEBUG(L"Invalid parameters");
        return false;
    }

    std::unique_lock<std::mutex> lck(_providerInfo->_fetchMapMutex);
    if (_providerInfo->_fetchMap.find(filePath) == _providerInfo->_fetchMap.end()) {
        TRACE_DEBUG(L"No fetch in progress: path = %ls", filePath);
        return true;
    }

    FetchInfo &fetchInfo = _providerInfo->_fetchMap[filePath];
    if (fetchInfo.getUpdating()) {
        TRACE_DEBUG(L"Update already in progress: path = %ls", filePath);
        return true;
    }

    if (completed == 0 || completed > fetchInfo._length.QuadPart) {
        TRACE_ERROR(L"Invalid completed size: path = %ls", filePath);
        return false;
    } else if (completed == fetchInfo._offset.QuadPart) {
        TRACE_DEBUG(L"Nothing to do: path = %ls", filePath);
        return true;
    } else if (completed > fetchInfo._offset.QuadPart && completed < fetchInfo._offset.QuadPart + CHUNKBASESIZE &&
               completed != fetchInfo._length.QuadPart) {
        TRACE_DEBUG(L"Not enough to transfer: path = %ls, completed = %lld", filePath, completed);
        return true;
    } else if (completed < fetchInfo._offset.QuadPart) {
        TRACE_DEBUG(L"Restart transfer: path = %ls", filePath);
        fetchInfo._offset.QuadPart = 0;
    }

    fetchInfo.setUpdating(true);
    lck.unlock();

    // Fetch data from temporary file to destination file
    bool res = true;
    std::ifstream fromFilePathStream(fromFilePath, std::ios::binary);
    if (fromFilePathStream.is_open()) {
        CF_OPERATION_INFO opInfo = {0};
        CF_OPERATION_PARAMETERS opParams = {0};

        opInfo.StructSize = sizeof(opInfo);
        opInfo.Type = CF_OPERATION_TYPE_TRANSFER_DATA;
        opInfo.ConnectionKey = fetchInfo._connectionKey;
        opInfo.TransferKey = fetchInfo._transferKey;
        opParams.ParamSize = CF_SIZE_OF_OP_PARAM(TransferData);
        opParams.TransferData.CompletionStatus = STATUS_SUCCESS;

        LARGE_INTEGER totalLengthToRead;
        LARGE_INTEGER lengthToRead;
        totalLengthToRead.QuadPart = completed - fetchInfo._offset.QuadPart;
        size_t chunkSize = (totalLengthToRead.QuadPart > CHUNKBASESIZE
                                    ? min(MAXCHUNKS, totalLengthToRead.QuadPart / CHUNKBASESIZE) * CHUNKBASESIZE
                                    : totalLengthToRead.QuadPart);
        char *transferData = new char[chunkSize];
        fromFilePathStream.seekg(fetchInfo._offset.QuadPart);
        while (((completed == fetchInfo._length.QuadPart && totalLengthToRead.QuadPart > 0) ||
                (completed < fetchInfo._length.QuadPart && totalLengthToRead.QuadPart >= LONGLONG(chunkSize)))) {
            if (fetchInfo.getCancel()) {
                TRACE_DEBUG(L"Update canceled!");
                break;
            }

            // Transfer a chunck
            lengthToRead.QuadPart = min(LONGLONG(chunkSize), totalLengthToRead.QuadPart);
            fromFilePathStream.read(transferData, lengthToRead.QuadPart);
            if ((fromFilePathStream.rdstate() & std::ifstream::failbit) != 0) {
                TRACE_DEBUG(L"Error reading tmp file!");
                break;
            }

            opParams.TransferData.Buffer = transferData;
            opParams.TransferData.Offset = fetchInfo._offset;
            opParams.TransferData.Length = lengthToRead;

            try {
                winrt::check_hresult(CfExecute(&opInfo, &opParams));
            } catch (winrt::hresult_error const &ex) {
                TRACE_ERROR(L"Error caught : hr %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
                res = false;
                break;
            }

            fetchInfo._offset.QuadPart += lengthToRead.QuadPart;
            totalLengthToRead.QuadPart -= lengthToRead.QuadPart;

            if (fetchInfo._offset.QuadPart == fetchInfo._length.QuadPart) {
                TRACE_DEBUG(L"Update finished!");
                if (finished) {
                    *finished = true;
                }
            }

            Sleep(0);
        }

        fromFilePathStream.close();
        delete[] transferData;
    } else {
        TRACE_ERROR(L"Unable to open temporary file: path = %ls", fromFilePath);
        res = false;
    }

    if (fetchInfo.getCancel()) {
        // Update canceled, notify
        TRACE_DEBUG(L"Update canceled: path = %ls", fromFilePath);
        *canceled = true;
        _providerInfo->_cancelFetchCV.notify_all();
    } else {
        lck.lock();
        if (res) {
            if (completed == fetchInfo._length.QuadPart) {
                // Transfer ended
                _providerInfo->_fetchMap.erase(filePath);
            } else {
                fetchInfo.setUpdating(false);
            }
        } else {
            fetchInfo.setUpdating(false);
        }
    }

    return res;
}

bool CloudProvider::cancelTransfer(ProviderInfo *providerInfo, const wchar_t *filePath, bool updateStatus) {
    if (!providerInfo || !filePath) {
        TRACE_ERROR(L"Invalid parameters");
        return false;
    }

    if (wcsncmp(filePath, providerInfo->folderPath(), wcslen(providerInfo->folderPath()))) {
        TRACE_DEBUG(L"File not synchronized: path = %lls", filePath);
        return true;
    }

    std::unique_lock<std::mutex> lck(providerInfo->_fetchMapMutex);
    if (providerInfo->_fetchMap.find(filePath) == providerInfo->_fetchMap.end()) {
        TRACE_DEBUG(L"No fetch in progress: path = %lls", filePath);
        return true;
    }

    FetchInfo &fetchInfo = providerInfo->_fetchMap[filePath];
    if (fetchInfo.getUpdating()) {
        // If updating, set cancel indicator and wait for notification
        TRACE_DEBUG(L"Cancel updating: path = %lls", filePath);
        fetchInfo.setCancel();
        providerInfo->_cancelFetchCV.wait(lck);
    }

    if (updateStatus) {
        if (!cancelFetchData(fetchInfo._connectionKey, fetchInfo._transferKey, fetchInfo._length)) {
            TRACE_ERROR(L"Error in cancelFetchData: path = %ls", filePath);
            return false;
        }
    }

    providerInfo->_fetchMap.erase(filePath);
    lck.unlock();

    // Reset pin state
    TRACE_DEBUG(L"Set pin state to UNPINNED: path = %lls", filePath);
    if (!Placeholders::setPinState(filePath, CF_PIN_STATE_UNPINNED)) {
        TRACE_ERROR(L"Error in setPinState");
        return false;
    }

    // Dehydrate file
    TRACE_DEBUG(L"Dehydrate: path = %lls", filePath);
    if (!dehydrate(filePath)) {
        TRACE_ERROR(L"Error in dehydrate");
        return false;
    }

    return true;
}

void CALLBACK CloudProvider::onFetchData(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                         _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    if (!callbackInfo || !callbackParameters) {
        TRACE_ERROR(L"Invalid parameters");
        return;
    }

    ProviderInfo *providerInfo = (ProviderInfo *) callbackInfo->CallbackContext;
    if (providerInfo) {
        std::filesystem::path fullPath =
                std::filesystem::path(callbackInfo->VolumeDosName) / std::filesystem::path(callbackInfo->NormalizedPath);

        if (wcsncmp(fullPath.wstring().c_str(), providerInfo->folderPath(), wcslen(providerInfo->folderPath()))) {
            TRACE_DEBUG(L"File not synchronized: %ls", fullPath.wstring().c_str());
            return;
        }

        if (callbackParameters->FetchData.Flags & CF_CALLBACK_FETCH_DATA_FLAG_NONE) {
            if (!cancelFetchData(callbackInfo->ConnectionKey, callbackInfo->TransferKey,
                                 callbackParameters->FetchData.RequiredFileOffset)) {
                TRACE_ERROR(L"Error in cancelFetchData: path = %ls", fullPath.wstring().c_str());
            }
            return;
        }

        std::unique_lock<std::mutex> lck(providerInfo->_fetchMapMutex);
        if (providerInfo->_fetchMap.find(fullPath.wstring()) != providerInfo->_fetchMap.end()) {
            TRACE_DEBUG(L"Fetch already in progress: path = %ls", fullPath.wstring().c_str());
            return;
        }

        // Store fetch info
        FetchInfo fetchInfo;
        fetchInfo._connectionKey = callbackInfo->ConnectionKey;
        fetchInfo._transferKey = callbackInfo->TransferKey;
        fetchInfo._offset = {0}; // Restart always from 0
        fetchInfo._length = callbackParameters->FetchData.RequiredFileOffset;
        fetchInfo._length.QuadPart += callbackParameters->FetchData.RequiredLength.QuadPart;
        fetchInfo._updating = false;
        fetchInfo._cancel = false;
        providerInfo->_fetchMap[fullPath.wstring()] = fetchInfo;
        lck.unlock();

        if (callbackInfo->ProcessInfo->ProcessId == Utilities::s_processId) {
            TRACE_DEBUG(L"Hydration asked by app: path = %lls", fullPath.wstring().c_str());
        } else {
            TRACE_DEBUG(L"Hydration asked: processId = %ld, path = %lls", callbackInfo->ProcessInfo->ProcessId,
                        fullPath.wstring().c_str());
        }

        if (!PipeClient::getInstance().sendMessageWithoutAnswer(L"MAKE_AVAILABLE_LOCALLY_DIRECT", fullPath.wstring())) {
            TRACE_ERROR(L"Error in Utilities::writeMessage!");
            if (!cancelFetchData(callbackInfo->ConnectionKey, callbackInfo->TransferKey,
                                 callbackParameters->FetchData.RequiredFileOffset)) {
                TRACE_ERROR(L"Error in cancelFetchData: path = %ls", fullPath.wstring().c_str());
            }
        }
    } else {
        TRACE_ERROR(L"Empty CallbackContext");
    }
}

void CloudProvider::onValidateData(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                   _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onValidateData");
}

void CALLBACK CloudProvider::onCancelFetchData(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                               _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    if (!callbackInfo || !callbackParameters) {
        TRACE_ERROR(L"Invalid parameters");
        return;
    }

    ProviderInfo *providerInfo = (ProviderInfo *) callbackInfo->CallbackContext;
    if (providerInfo) {
        std::filesystem::path fullPath =
                std::filesystem::path(callbackInfo->VolumeDosName) / std::filesystem::path(callbackInfo->NormalizedPath);

        if (!cancelTransfer(providerInfo, fullPath.wstring().c_str(), false)) {
            TRACE_ERROR(L"Error in cancelTransfer: path = %lls", fullPath.wstring().c_str());
            return;
        }
    }
}

void CloudProvider::onNotifyDehydrate(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                      _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    if (!callbackInfo || !callbackParameters) {
        TRACE_ERROR(L"Invalid parameters");
        return;
    }

    ProviderInfo *providerInfo = (ProviderInfo *) callbackInfo->CallbackContext;
    if (providerInfo) {
        std::filesystem::path fullPath =
                std::filesystem::path(callbackInfo->VolumeDosName) / std::filesystem::path(callbackInfo->NormalizedPath);

        if (callbackInfo->ProcessInfo->ProcessId == Utilities::s_processId) {
            TRACE_DEBUG(L"Dehydration asked by app: path = %lls", fullPath.wstring().c_str());
        } else {
            TRACE_DEBUG(L"Dehydration asked: processId = %ld, path = %lls", callbackInfo->ProcessInfo->ProcessId,
                        fullPath.wstring().c_str());
        }

        if (!PipeClient::getInstance().sendMessageWithoutAnswer(L"MAKE_ONLINE_ONLY", fullPath.wstring())) {
            TRACE_ERROR(L"Error in Utilities::writeMessage!");
        }
    } else {
        TRACE_ERROR(L"Empty CallbackContext");
    }
}

void CloudProvider::onNotifyDehydrateCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                                _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyDehydrateCompletion");
}

void CloudProvider::onNotifyDelete(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                   _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyDelete");
}

void CloudProvider::onNotifyDeleteCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                             _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyDeleteCompletion");
}

void CloudProvider::onNotifyRename(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                   _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyRename");
}

void CloudProvider::onNotifyRenameCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                             _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyRenameCompletion");
}

void CloudProvider::onNotifyFetchPlaceholder(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                             _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyFetchPlaceholder");
}

void CloudProvider::onCancelFetchPlacehoders(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                             _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onCancelFetchPlacehoders");
}

void CloudProvider::onNotifyOpenCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                           _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyOpenCompletion");
}

void CloudProvider::onNotifyCloseCompletion(_In_ CONST CF_CALLBACK_INFO *callbackInfo,
                                            _In_ CONST CF_CALLBACK_PARAMETERS *callbackParameters) {
    TRACE_DEBUG(L"onNotifyCloseCompletion");
}

// Registers the callbacks in the table at the top of this file so that the methods above are called for the provider
bool CloudProvider::connectSyncRootTransferCallbacks() {
    if (!_providerInfo) {
        TRACE_ERROR(L"Not initialized!");
        return FALSE;
    }

    try {
        // Connect to the sync root using Cloud File API
        winrt::check_hresult(CfConnectSyncRoot(_providerInfo->folderPath(), s_callbackTable, _providerInfo,
                                               CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO | CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH,
                                               &_transferCallbackConnectionKey));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}

// Unregisters the callbacks in the table at the top of this file so that the client doesn't Hindenburg
bool CloudProvider::disconnectSyncRootTransferCallbacks() {
    try {
        winrt::check_hresult(CfDisconnectSyncRoot(_transferCallbackConnectionKey));
    } catch (...) {
        TRACE_ERROR(L"WinRT error caught : hr %08x", static_cast<HRESULT>(winrt::to_hresult()));
        return false;
    }

    return true;
}

// If the local (client) folder where the cloud file placeholders are created
// is not under the User folder (i.e. Documents, Photos, etc), then it is required
// to add the folder to the Search Indexer. This is because the properties for
// the cloud file state/progress are cached in the indexer, and if the folder isn't
// indexed, attempts to get the properties on items will not return the expected values.
bool CloudProvider::addFolderToSearchIndexer(const PCWSTR folder) {
    std::wstring url(L"file:///");
    url.append(folder);

    try {
        winrt::com_ptr<ISearchManager> searchManager;
        winrt::check_hresult(CoCreateInstance(__uuidof(CSearchManager), nullptr, CLSCTX_SERVER, __uuidof(&searchManager),
                                              searchManager.put_void()));

        winrt::com_ptr<ISearchCatalogManager> searchCatalogManager;
        winrt::check_hresult(searchManager->GetCatalog(MSSEARCH_INDEX, searchCatalogManager.put()));

        winrt::com_ptr<ISearchCrawlScopeManager> searchCrawlScopeManager;
        winrt::check_hresult(searchCatalogManager->GetCrawlScopeManager(searchCrawlScopeManager.put()));

        winrt::check_hresult(searchCrawlScopeManager->AddDefaultScopeRule(url.data(), TRUE, FOLLOW_FLAGS::FF_INDEXCOMPLEXURLS));
        winrt::check_hresult(searchCrawlScopeManager->SaveAll());
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught: hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}

bool CloudProvider::cancelFetchData(CF_CONNECTION_KEY connectionKey, CF_TRANSFER_KEY transferKey,
                                    LARGE_INTEGER requiredFileOffset) {
    // Update transfer status
    CF_OPERATION_INFO opInfo = {0};
    CF_OPERATION_PARAMETERS opParams = {0};

    opInfo.StructSize = sizeof(opInfo);
    opInfo.Type = CF_OPERATION_TYPE_TRANSFER_DATA;
    opInfo.ConnectionKey = connectionKey;
    opInfo.TransferKey = transferKey;
    opParams.ParamSize = CF_SIZE_OF_OP_PARAM(TransferData);
    opParams.TransferData.CompletionStatus = STATUS_UNSUCCESSFUL;
    opParams.TransferData.Buffer = nullptr;
    opParams.TransferData.Offset = {0}; // Cancel entire transfer
    opParams.TransferData.Length = requiredFileOffset;

    try {
        winrt::check_hresult(CfExecute(&opInfo, &opParams));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"Error catched: hr %08x - %s", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return false;
    }

    return true;
}
