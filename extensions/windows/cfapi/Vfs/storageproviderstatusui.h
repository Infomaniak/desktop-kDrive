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

#include "..\Common\framework.h"
#include "..\Common\utilities.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Provider.h>
#include <winrt/Windows.Graphics.h>

namespace winrt {
using namespace Windows::Foundation;
using namespace Windows::Storage::Provider;
using namespace Windows::Graphics;
} // namespace winrt

class StorageProviderStatusUISource : public winrt::implements<StorageProviderStatusUISource, 
                                                                winrt::StorageProviderStatusUISource> {
    public:
        StorageProviderStatusUISource();
        
        // QuotaUI
        winrt::IReference<uint64_t> QuotaUI_Total();
        void QuotaUI_Total(winrt::IReference<uint64_t> value);
        
        winrt::IReference<uint64_t> QuotaUI_Used();
        void QuotaUI_Used(winrt::IReference<uint64_t> value);
        
        // ProviderPrimaryCommand
        winrt::StorageProviderUICommand ProviderPrimaryCommand();
        void ProviderPrimaryCommand(winrt::StorageProviderUICommand value);
        
        // ProviderState
        winrt::StorageProviderState ProviderState();
        void ProviderState(winrt::StorageProviderState value);
        
        // ProviderStateIcon
        winrt::Uri ProviderStateIcon();
        void ProviderStateIcon(const winrt::Uri &value);
        
        // ProviderStateLabel
        winrt::hstring ProviderStateLabel();
        void ProviderStateLabel(const winrt::hstring &value);

    private:
        winrt::IReference<uint64_t> _quotaTotal{nullptr};
        winrt::IReference<uint64_t> _quotaUsed{nullptr};
        winrt::StorageProviderUICommand _primaryCommand{winrt::StorageProviderUICommand::None};
        winrt::StorageProviderState _providerState{winrt::StorageProviderState::InSync};
        winrt::Uri _stateIcon{nullptr};
        winrt::hstring _stateLabel{};
};
