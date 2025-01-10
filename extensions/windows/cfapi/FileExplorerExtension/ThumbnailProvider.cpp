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

#include "..\Common\utilities.h"
#include "..\Common\pipeclient.h"
#include "thumbnailprovider.h"

#include <filesystem>
#include <fstream>

#include <shlobj_core.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <minwindef.h>
#include <urlmon.h>

#define THUMBNAIL_MIME_TYPE_LIST {L"image", L"video"}

// IInitializeWithItem
IFACEMETHODIMP ThumbnailProvider::Initialize(_In_ IShellItem *item, _In_ DWORD mode) {
    try {
        winrt::check_hresult(item->QueryInterface(__uuidof(_item), _item.put_void()));
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return winrt::to_hresult();
    }

    return S_OK;
}

// IThumbnailProvider
IFACEMETHODIMP ThumbnailProvider::GetThumbnail(_In_ UINT width, _Out_ HBITMAP *bitmap, _Out_ WTS_ALPHATYPE *alphaType) {
    if (!_item) {
        TRACE_ERROR(L"Not initialized!");
        return E_UNEXPECTED;
    }

    // Retrieve thumbnails of the placeholders on demand by delegating to the thumbnail of the source items.
    *bitmap = nullptr;
    *alphaType = WTSAT_UNKNOWN;

    try {
        // Check the file status
        winrt::com_array<wchar_t> path;
        winrt::check_hresult(_item->GetDisplayName(SIGDN_FILESYSPATH, winrt::put_abi(path)));
        std::wstring fullPath(path.data());

        winrt::handle fileHandle(
                CreateFile(fullPath.c_str(), WRITE_DAC, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_NO_RECALL, nullptr));
        if (fileHandle.get() == INVALID_HANDLE_VALUE) {
            TRACE_ERROR(L"Error in CreateFile : %ls", Utilities::getLastErrorMessage().c_str());
            return E_UNEXPECTED;
        }

        FILE_ATTRIBUTE_TAG_INFO fileInformation;
        if (!GetFileInformationByHandleEx(fileHandle.get(), FileAttributeTagInfo, &fileInformation, sizeof(fileInformation))) {
            TRACE_ERROR(L"Error in GetFileInformationByHandleEx : %ls", Utilities::getLastErrorMessage().c_str());
            return E_UNEXPECTED;
        }

        bool offlineThumbnail = (fileInformation.FileAttributes & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);

        // Check the file type
        if (offlineThumbnail) {
            offlineThumbnail &= getOffline(fullPath);
        }

        if (offlineThumbnail) {
            // Ask the thumbnail to the server
            std::wostringstream message;
            message << width << MSG_ARG_SEPARATOR << fullPath;
            LONGLONG msgId;
            if (!PipeClient::getInstance().sendMessageWithAnswer(L"GET_THUMBNAIL", message.str(), msgId)) {
                TRACE_ERROR(L"Error in Utilities::writeMessage!");
                return E_UNEXPECTED;
            }

            BYTE *pixmapBuffer = nullptr;
            DWORD bufferLength = 0;
            bool vfsModeCompatible = false;
            std::wstring response = std::wstring();
            if (!PipeClient::getInstance().readMessage(msgId, response)) {
                TRACE_ERROR(L"Error in PipeClient::readMessage!");
                return E_UNEXPECTED;
            }

            // Calculate buffer length
            if (!CryptStringToBinaryW(response.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &bufferLength, nullptr, nullptr)) {
                TRACE_ERROR(L"Error in CryptStringToBinaryW!");
                return E_UNEXPECTED;
            }

            // Load buffer
            pixmapBuffer = new BYTE[bufferLength];
            if (!CryptStringToBinaryW(response.c_str(), 0, CRYPT_STRING_BASE64, pixmapBuffer, &bufferLength, nullptr, nullptr)) {
                TRACE_ERROR(L"Error in CryptStringToBinaryW!");
                return E_UNEXPECTED;
            }

            TRACE_DEBUG(L"Thumbnail received");

            if (pixmapBuffer) {
                BITMAPFILEHEADER *bmfh = (BITMAPFILEHEADER *) pixmapBuffer;
                BITMAPINFOHEADER *bmih = (BITMAPINFOHEADER *) (pixmapBuffer + sizeof(BITMAPFILEHEADER));
                BITMAPINFO *bmi = (BITMAPINFO *) bmih;
                void *bits = (void *) (pixmapBuffer + bmfh->bfOffBits);
                HDC hdc = ::GetDC(nullptr);

                TRACE_DEBUG(L"Create bitmap");
                *bitmap = CreateDIBitmap(hdc, bmih, CBM_INIT, bits, bmi, DIB_RGB_COLORS);
                if (*bitmap == nullptr) {
                    TRACE_DEBUG(L"Error in CreateDIBitmap %ls", Utilities::getLastErrorMessage().c_str());
                }

                TRACE_DEBUG(L"Free memory");
                ReleaseDC(nullptr, hdc);
                delete[] pixmapBuffer;
            }
        } else {
            // Get default thumbnail
            TRACE_DEBUG(L"Get default thumbnail for %ls - width %d", path.data(), width);
            winrt::com_ptr<IThumbnailProvider> thumbnailProviderSource;
            winrt::check_hresult(_item->BindToHandler(nullptr, BHID_ThumbnailHandler, __uuidof(thumbnailProviderSource),
                                                      thumbnailProviderSource.put_void()));
            winrt::check_hresult(thumbnailProviderSource->GetThumbnail(width, bitmap, alphaType));
        }
    } catch (winrt::hresult_error const &ex) {
        TRACE_ERROR(L"WinRT error caught : hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        return E_UNEXPECTED;
    }

    return S_OK;
}

bool ThumbnailProvider::getOffline(const std::wstring &path) {
    std::list<std::wstring> mimeTypeList(THUMBNAIL_MIME_TYPE_LIST);
    std::wstring ext(PathFindExtensionW(path.c_str()));
    std::wstring mimeType = mimeTypeFromString(ext);
    std::wstring mimeTypeCategory = mimeType.substr(0, mimeType.find(L'/'));
    auto it = std::find(mimeTypeList.begin(), mimeTypeList.end(), mimeTypeCategory);
    if (it != mimeTypeList.end()) {
        return true;
    }
    return false;
}

std::wstring ThumbnailProvider::mimeTypeFromString(const std::wstring &ext) {
    LPWSTR pwzMimeOut = NULL;
    HRESULT hr = FindMimeFromData(NULL, ext.c_str(), NULL, 0, NULL, FMFD_URLASFILENAME, &pwzMimeOut, 0x0);
    if (SUCCEEDED(hr)) {
        std::wstring strResult(pwzMimeOut);
        // Despite the documentation stating to call operator delete, the returned string must be cleaned up using CoTaskMemFree
        CoTaskMemFree(pwzMimeOut);
        return strResult;
    }

    return L"";
}
