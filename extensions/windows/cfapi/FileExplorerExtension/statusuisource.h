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

#pragma once

#include "..\Common\utilities.h"

#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Provider.h>
#include <winrt/Windows.UI.h>

// CLSID of the status UI source factory, it must match the value declared in
// FileExplorerExtensionPackage\Package.appxmanifest and the one written in the sync root registry key by the Vfs library.
// {0BA42370-2A5A-4F30-AA40-620F870E44DF}
constexpr CLSID CLSID_StatusUISourceFactory = {0x0ba42370, 0x2a5a, 0x4f30, {0xaa, 0x40, 0x62, 0x0f, 0x87, 0x0e, 0x44, 0xdf}};

// Status of a sync root, as reported by the kDrive server.
struct SyncRootStatus {
        winrt::Windows::Storage::Provider::StorageProviderState state{
                winrt::Windows::Storage::Provider::StorageProviderState::Offline};
        std::wstring stateLabel;
        uint64_t quotaTotal{0};
        uint64_t quotaUsed{0};
        std::wstring quotaLabel;
        std::wstring color; // #RRGGBB
        std::wstring driveName;
        std::wstring driveUrl;
        std::wstring primaryCommandLabel;
        std::wstring primaryCommandDescription;
};

// Command displayed by the File Explorer status UI. Invoking it opens `url` in the default browser.
struct StorageProviderUICommand
    : winrt::implements<StorageProviderUICommand, winrt::Windows::Storage::Provider::IStorageProviderUICommand> {
        StorageProviderUICommand(const std::wstring &label, const std::wstring &description, const std::wstring &iconPath,
                                 const std::wstring &url, winrt::Windows::Storage::Provider::StorageProviderUICommandState state);

        winrt::hstring Label() const { return _label; }
        winrt::hstring Description() const { return _description; }
        winrt::Windows::Foundation::Uri Icon() const { return _icon; }
        winrt::Windows::Storage::Provider::StorageProviderUICommandState State() const { return _state; }
        void Invoke() const;

    private:
        winrt::hstring _label;
        winrt::hstring _description;
        winrt::Windows::Foundation::Uri _icon{nullptr};
        std::wstring _url;
        winrt::Windows::Storage::Provider::StorageProviderUICommandState _state{
                winrt::Windows::Storage::Provider::StorageProviderUICommandState::Enabled};
};

// Provides the File Explorer status UI (quota, provider state, primary command) of a given sync root.
struct StatusUISource : winrt::implements<StatusUISource, winrt::Windows::Storage::Provider::IStorageProviderStatusUISource> {
        explicit StatusUISource(const winrt::hstring &syncRootId);

        winrt::Windows::Storage::Provider::StorageProviderStatusUI GetStatusUI();

        winrt::event_token StatusUIChanged(const winrt::Windows::Foundation::TypedEventHandler<
                                           winrt::Windows::Storage::Provider::IStorageProviderStatusUISource,
                                           winrt::Windows::Foundation::IInspectable> &handler);
        void StatusUIChanged(const winrt::event_token &token) noexcept;

    private:
        winrt::hstring _syncRootId;
        winrt::event<winrt::Windows::Foundation::TypedEventHandler<
                winrt::Windows::Storage::Provider::IStorageProviderStatusUISource, winrt::Windows::Foundation::IInspectable>>
                _statusUIChanged;
};

// Factory instantiated by the File Explorer to retrieve the status UI source of a sync root.
struct __declspec(uuid("0BA42370-2A5A-4F30-AA40-620F870E44DF")) StatusUISourceFactory
    : winrt::implements<StatusUISourceFactory, winrt::Windows::Storage::Provider::IStorageProviderStatusUISourceFactory> {
        winrt::Windows::Storage::Provider::IStorageProviderStatusUISource GetStatusUISource(const winrt::hstring &syncRootId);
};
