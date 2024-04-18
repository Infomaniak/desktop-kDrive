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

#include "shellservices.h"
#include "classfactory.h"
#include "thumbnailprovider.h"
#include "customstateprovider.h"
#include "contextmenus.h"

#include <ppltasks.h>

using namespace concurrency;

bool ShellServices::initAndStartServiceTask() {
    auto task = concurrency::create_task([]() {
        TRACE_DEBUG(L"Task begin");

        try {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            winrt::check_hresult(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

            DWORD cookie;
            auto thumbnailProviderClassFactory = winrt::make<ClassFactory<ThumbnailProvider>>();
            winrt::check_hresult(CoRegisterClassObject(__uuidof(ThumbnailProvider), thumbnailProviderClassFactory.get(),
                                                       CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie));
            TRACE_DEBUG(L"ThumbnailProvider cookie = %ld", cookie);

            auto contextMenuClassFactory = winrt::make<ClassFactory<ExplorerCommandHandler>>();
            winrt::check_hresult(CoRegisterClassObject(__uuidof(ExplorerCommandHandler), contextMenuClassFactory.get(),
                                                       CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie));
            TRACE_DEBUG(L"ExplorerCommandHandler cookie = %ld", cookie);

            auto customStateProvider =
                winrt::make<ClassFactory<winrt::FileExplorerExtension::implementation::CustomStateProvider>>();
            winrt::check_hresult(CoRegisterClassObject(CLSID_CustomStateProvider, customStateProvider.get(), CLSCTX_LOCAL_SERVER,
                                                       REGCLS_MULTIPLEUSE, &cookie));
            TRACE_DEBUG(L"CustomStateProvider cookie = %ld", cookie);

            winrt::handle dummyEvent(CreateEvent(nullptr, FALSE, FALSE, nullptr));
            if (!dummyEvent) {
                winrt::throw_last_error();
            }
            DWORD index;
            HANDLE temp = dummyEvent.get();
            winrt::check_hresult(CoWaitForMultipleHandles(COWAIT_DISPATCH_CALLS, INFINITE, 1, &temp, &index));
        } catch (winrt::hresult_error const &ex) {
            TRACE_ERROR(L"WinRT error catched : hr %08x - %s!", static_cast<HRESULT>(winrt::to_hresult()), ex.message().c_str());
        }

        TRACE_DEBUG(L"Task end");
    });

    return true;
}
