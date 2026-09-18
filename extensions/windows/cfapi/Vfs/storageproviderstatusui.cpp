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

#include "storageproviderstatusui.h"

StorageProviderStatusUISource::StorageProviderStatusUISource() :
    _quotaTotal(nullptr),
    _quotaUsed(nullptr),
    _primaryCommand(winrt::StorageProviderUICommand::None),
    _providerState(winrt::StorageProviderState::InSync),
    _stateIcon(nullptr),
    _stateLabel(L"") {
}

winrt::IReference<uint64_t> StorageProviderStatusUISource::QuotaUI_Total() {
    return _quotaTotal;
}

void StorageProviderStatusUISource::QuotaUI_Total(winrt::IReference<uint64_t> value) {
    _quotaTotal = value;
}

winrt::IReference<uint64_t> StorageProviderStatusUISource::QuotaUI_Used() {
    return _quotaUsed;
}

void StorageProviderStatusUISource::QuotaUI_Used(winrt::IReference<uint64_t> value) {
    _quotaUsed = value;
}

winrt::StorageProviderUICommand StorageProviderStatusUISource::ProviderPrimaryCommand() {
    return _primaryCommand;
}

void StorageProviderStatusUISource::ProviderPrimaryCommand(winrt::StorageProviderUICommand value) {
    _primaryCommand = value;
}

winrt::StorageProviderState StorageProviderStatusUISource::ProviderState() {
    return _providerState;
}

void StorageProviderStatusUISource::ProviderState(winrt::StorageProviderState value) {
    _providerState = value;
}

winrt::Uri StorageProviderStatusUISource::ProviderStateIcon() {
    return _stateIcon;
}

void StorageProviderStatusUISource::ProviderStateIcon(const winrt::Uri &value) {
    _stateIcon = value;
}

winrt::hstring StorageProviderStatusUISource::ProviderStateLabel() {
    return _stateLabel;
}

void StorageProviderStatusUISource::ProviderStateLabel(const winrt::hstring &value) {
    _stateLabel = value;
}
