/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "statusuisource.h"

#include "..\Common\pipeclient.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#include <shellapi.h>
#include <shlobj_core.h>

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::Storage::Provider;
} // namespace winrt

namespace {

constexpr wchar_t defaultColor[] = L"#0098FF"; // kDrive blue, used when the drive color is unknown
constexpr DWORD statusRequestTimeoutMs = 4000;

std::vector<std::wstring> splitArguments(const std::wstring &message) {
    std::vector<std::wstring> arguments;
    size_t begin = 0;
    size_t end = message.find(MSG_ARG_SEPARATOR, begin);
    while (end != std::wstring::npos) {
        arguments.push_back(message.substr(begin, end - begin));
        begin = end + 1;
        end = message.find(MSG_ARG_SEPARATOR, begin);
    }
    arguments.push_back(message.substr(begin));
    return arguments;
}

uint64_t toUInt64(const std::wstring &value) {
    try {
        return static_cast<uint64_t>(std::stoull(value));
    } catch (const std::exception &) {
        return 0;
    }
}

winrt::StorageProviderState toProviderState(const std::wstring &value) {
    int state = 0;
    try {
        state = std::stoi(value);
    } catch (const std::exception &) {
        return winrt::StorageProviderState::Offline;
    }

    switch (state) {
        case 0:
            return winrt::StorageProviderState::InSync;
        case 1:
            return winrt::StorageProviderState::Syncing;
        case 2:
            return winrt::StorageProviderState::Paused;
        case 3:
            return winrt::StorageProviderState::Error;
        case 4:
            return winrt::StorageProviderState::Warning;
        default:
            return winrt::StorageProviderState::Offline;
    }
}

bool toRgb(const std::wstring &color, uint8_t &red, uint8_t &green, uint8_t &blue) {
    std::wstring value = color;
    if (!value.empty() && value.front() == L'#') value.erase(0, 1);
    if (value.size() != 6) return false;

    try {
        const auto rgb = std::stoul(value, nullptr, 16);
        red = static_cast<uint8_t>((rgb >> 16) & 0xFF);
        green = static_cast<uint8_t>((rgb >> 8) & 0xFF);
        blue = static_cast<uint8_t>(rgb & 0xFF);
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

// Local folder in which the drive colored icons are generated.
std::filesystem::path iconFolderPath() {
    PWSTR localAppDataPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataPath))) {
        return std::filesystem::path();
    }

    std::filesystem::path path(localAppDataPath);
    CoTaskMemFree(localAppDataPath);

    path /= Utilities::s_appName;
    path /= L"statusui";
    return path;
}

// Generates (once per color) the sync root icon tinted with the drive color and returns its path.
std::wstring generateIcon(const std::wstring &color) {
    std::wstring normalizedColor = color.empty() ? defaultColor : color;
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    if (!toRgb(normalizedColor, red, green, blue)) {
        normalizedColor = defaultColor;
        (void) toRgb(normalizedColor, red, green, blue);
    }

    const std::filesystem::path folderPath = iconFolderPath();
    if (folderPath.empty()) return std::wstring();

    wchar_t fileName[64];
    (void) swprintf(fileName, 64, L"kdrive-%02x%02x%02x.svg", red, green, blue);
    const std::filesystem::path filePath = folderPath / fileName;

    std::error_code ec;
    if (std::filesystem::exists(filePath, ec)) return filePath.wstring();

    std::filesystem::create_directories(folderPath, ec);
    if (ec) {
        TRACE_WARNING(L"Could not create the status UI icon folder: %ls", folderPath.c_str());
        return std::wstring();
    }

    char svg[512];
    (void) sprintf_s(svg, 512,
                     "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\">"
                     "<path fill=\"#%02x%02x%02x\" d=\"M19.35 10.04A7.49 7.49 0 0 0 12 4C9.11 4 6.6 5.64 5.35 8.04"
                     "A5.994 5.994 0 0 0 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96z\"/></svg>",
                     red, green, blue);

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        TRACE_WARNING(L"Could not write the status UI icon: %ls", filePath.c_str());
        return std::wstring();
    }
    file << svg;
    file.close();

    return filePath.wstring();
}

// Retrieves the local folder of the sync root identified by `syncRootId`.
std::wstring syncRootPath(const winrt::hstring &syncRootId) {
    try {
        const auto syncRoots = winrt::StorageProviderSyncRootManager::GetCurrentSyncRoots();
        for (uint32_t index = 0; index < syncRoots.Size(); index++) {
            const auto syncRoot = syncRoots.GetAt(index);
            if (syncRoot.Id() == syncRootId && syncRoot.Path()) {
                return std::wstring(syncRoot.Path().Path());
            }
        }
    } catch (const winrt::hresult_error &ex) {
        TRACE_ERROR(L"WinRT error caught : hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
    }

    return std::wstring();
}

// Asks the kDrive server for the status of the sync root.
bool requestStatus(const winrt::hstring &syncRootId, SyncRootStatus &status) {
    const std::wstring path = syncRootPath(syncRootId);
    if (path.empty()) {
        TRACE_WARNING(L"Sync root not found: %ls", syncRootId.c_str());
        return false;
    }

    LONGLONG msgId = 0;
    if (!PipeClient::getInstance().sendMessageWithAnswer(L"GET_STATUS_UI", path, msgId)) {
        TRACE_WARNING(L"Error in PipeClient::sendMessageWithAnswer!");
        return false;
    }

    std::wstring response;
    if (!PipeClient::getInstance().readMessage(msgId, response, statusRequestTimeoutMs)) {
        TRACE_WARNING(L"Error in PipeClient::readMessage!");
        return false;
    }

    const auto arguments = splitArguments(response);
    if (arguments.size() < 9) {
        TRACE_WARNING(L"Invalid GET_STATUS_UI response!");
        return false;
    }

    status.state = toProviderState(arguments[0]);
    status.stateLabel = arguments[1];
    status.quotaTotal = toUInt64(arguments[2]);
    status.quotaUsed = toUInt64(arguments[3]);
    status.quotaLabel = arguments[4];
    status.color = arguments[5];
    status.driveName = arguments[6];
    status.driveUrl = arguments[7];
    status.primaryCommandLabel = arguments[8];
    status.primaryCommandDescription = arguments.size() > 9 ? arguments[9] : std::wstring();

    return true;
}

// Builds the icon Uri. The File Explorer expects a fully qualified local path but a raw Windows path is not always a valid
// Uri, in which case the file scheme is used instead.
winrt::Uri makeIconUri(const std::wstring &iconPath) {
    if (iconPath.empty()) return nullptr;

    try {
        return winrt::Uri(winrt::hstring(iconPath));
    } catch (const winrt::hresult_error &) {
        // Ignored, retry with the file scheme below.
    }

    std::wstring fileUri = L"file:///" + iconPath;
    std::replace(fileUri.begin(), fileUri.end(), L'\\', L'/');
    try {
        return winrt::Uri(winrt::hstring(fileUri));
    } catch (const winrt::hresult_error &ex) {
        TRACE_WARNING(L"Could not build the icon Uri: hr %08x - %s", static_cast<HRESULT>(winrt::to_hresult()),
                      ex.message().c_str());
    }

    return nullptr;
}

} // namespace

StorageProviderUICommand::StorageProviderUICommand(const std::wstring &label, const std::wstring &description,
                                                   const std::wstring &iconPath, const std::wstring &url,
                                                   const winrt::StorageProviderUICommandState state) :
    _label(label),
    _description(description),
    _url(url),
    _state(state) {
    _icon = makeIconUri(iconPath);
}

void StorageProviderUICommand::Invoke() const {
    if (_url.empty()) return;

    TRACE_DEBUG(L"Opening %ls", _url.c_str());
    (void) ShellExecuteW(nullptr, L"open", _url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

StatusUISource::StatusUISource(const winrt::hstring &syncRootId) :
    _syncRootId(syncRootId) {}

winrt::StorageProviderStatusUI StatusUISource::GetStatusUI() {
    SyncRootStatus status;
    const bool statusAvailable = requestStatus(_syncRootId, status);

    auto statusUI = winrt::StorageProviderStatusUI();

    // Provider state (mandatory)
    statusUI.ProviderState(statusAvailable ? status.state : winrt::StorageProviderState::Offline);
    if (!status.stateLabel.empty()) {
        statusUI.ProviderStateLabel(winrt::hstring(status.stateLabel));
    }

    const std::wstring iconPath = generateIcon(status.color);
    if (const auto iconUri = makeIconUri(iconPath)) {
        statusUI.ProviderStateIcon(iconUri);
    }

    // Quota (the section is hidden by the File Explorer when the label is empty)
    if (statusAvailable && status.quotaTotal > 0) {
        auto quotaUI = winrt::StorageProviderQuotaUI();
        quotaUI.QuotaTotalInBytes(status.quotaTotal);
        quotaUI.QuotaUsedInBytes(status.quotaUsed);
        quotaUI.QuotaUsedLabel(winrt::hstring(status.quotaLabel));

        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        if (toRgb(status.color, red, green, blue)) {
            quotaUI.QuotaUsedColor(winrt::Windows::UI::ColorHelper::FromArgb(255, red, green, blue));
        }

        statusUI.QuotaUI(quotaUI);
    }

    // Primary command
    if (statusAvailable && !status.primaryCommandLabel.empty() && !status.driveUrl.empty()) {
        statusUI.ProviderPrimaryCommand(
                winrt::make<StorageProviderUICommand>(status.primaryCommandLabel, status.primaryCommandDescription, iconPath,
                                                      status.driveUrl, winrt::StorageProviderUICommandState::Enabled));
    }

    return statusUI;
}

winrt::event_token StatusUISource::StatusUIChanged(
        const winrt::TypedEventHandler<winrt::IStorageProviderStatusUISource, winrt::IInspectable> &handler) {
    return _statusUIChanged.add(handler);
}

void StatusUISource::StatusUIChanged(const winrt::event_token &token) noexcept {
    _statusUIChanged.remove(token);
}

winrt::IStorageProviderStatusUISource StatusUISourceFactory::GetStatusUISource(const winrt::hstring &syncRootId) {
    TRACE_DEBUG(L"Status UI source requested for %ls", syncRootId.c_str());
    return winrt::make<StatusUISource>(syncRootId);
}
