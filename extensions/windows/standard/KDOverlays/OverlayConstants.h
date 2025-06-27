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


#define OVERLAY_GUID_ERROR L"{0960F090-F328-48A3-B746-276B1E3C3722}"
#define OVERLAY_GUID_OK L"{0960F092-F328-48A3-B746-276B1E3C3722}"
#define OVERLAY_GUID_SYNC L"{0960F094-F328-48A3-B746-276B1E3C3722}"
#define OVERLAY_GUID_WARNING L"{0960F096-F328-48A3-B746-276B1E3C3722}"

#define OVERLAY_GENERIC_NAME L"Infomaniak Drive overlay handler"

/*
    With Windows only taking up to 15 overlay icons, we want our Overlay icons to come before concurrents in alphabetical order
    For now, three spaces will be added before the name of the Registry Key
    Later plan is to count how many spaces the first concurrent installed has, and add one more space
    This could be done either each start of the kDrive App, or during installation
    source : https://devblogs.microsoft.com/oldnewthing/20190313-00/?p=101094
*/
#define OVERLAY_APP_NAME L"kDrive"
#define OVERLAY_NAME_ERROR L"Error"
#define OVERLAY_NAME_OK L"OK"
#define OVERLAY_NAME_SYNC L"Sync"
#define OVERLAY_NAME_WARNING L"Warning"

#define REGISTRY_OVERLAY_KEY L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers"
#define REGISTRY_CLSID L"CLSID"
#define REGISTRY_IN_PROCESS L"InprocServer32"
#define REGISTRY_THREADING L"ThreadingModel"
#define REGISTRY_APARTMENT L"Apartment"
#define REGISTRY_VERSION L"Version"
#define REGISTRY_VERSION_NUMBER L"1.0"

// Registry values for running
#define REGISTRY_ENABLE_OVERLAY L"EnableOverlay"

#define GET_FILE_OVERLAY_ID L"getFileIconId"

#define PORT 34001
