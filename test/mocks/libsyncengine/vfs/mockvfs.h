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

#include "testincludes.h"
#include "utility/types.h"
#include "libcommonserver/vfs/vfs.h"

namespace KDC {

template<class T>
concept VfsDerived = std::is_base_of_v<Vfs, T>;

/* MockVfs
 *
 * Mock class for Vfs, it can mock any Vfs function by setting a lambda function that will be called instead of the original
 * function. Any function that is not set will call the original function.
 * Compatible with any Vfs derived class.
 */
template<VfsDerived T>
class MockVfs : public T {
    public:
        using T::T;

        ExitInfo updateMetadata(const SyncPath &path, time_t creationTime, time_t modtime, int64_t size,
                                const NodeId &nodeId) override {
            return _updateMetadata ? _updateMetadata(path, creationTime, modtime, size, nodeId)
                                   : T::updateMetadata(path, creationTime, modtime, size, nodeId);
        }
        ExitInfo createPlaceholder(const KDC::SyncPath &path, const KDC::SyncFileItem &item) override {
            return _createPlaceholder ? _createPlaceholder(path, item) : T::createPlaceholder(path, item);
        }

        ExitInfo dehydratePlaceholder(const SyncPath &path) override {
            return _dehydratePlaceholder ? _dehydratePlaceholder(path) : T::dehydratePlaceholder(path);
        }
        ExitInfo convertToPlaceholder(const SyncPath &path, const KDC::SyncFileItem &item) override {
            return _convertToPlaceholder ? _convertToPlaceholder(path, item) : T::convertToPlaceholder(path, item);
        }
        ExitInfo updateFetchStatus(const SyncPath &path, const SyncPath &path2, int64_t received, bool &canceled,
                                   bool &finished) override {
            return _updateFetchStatus ? _updateFetchStatus(path, path2, received, canceled, finished)
                                      : T::updateFetchStatus(path, path2, received, canceled, finished);
        }

        ExitInfo forceStatus(const SyncPath &path, const VfsStatus &vfsStatus) override {
            return _forceStatus ? _forceStatus(path, vfsStatus) : T::forceStatus(path, vfsStatus);
        }

        ExitInfo isDehydratedPlaceholder(const SyncPath &path, bool &isDehydrated) override {
            return _isDehydratedPlaceholder ? _isDehydratedPlaceholder(path, isDehydrated)
                                            : T::isDehydratedPlaceholder(path, isDehydrated);
        }

        ExitInfo setPinState(const SyncPath &path, PinState state) override {
            return _setPinState ? _setPinState(path, state) : T::setPinState(path, state);
        }
        PinState pinState(const SyncPath &path) override { return _pinState ? _pinState(path) : T::pinState(path); }
        ExitInfo status(const SyncPath &path, VfsStatus &vfsStatus) override {
            return _status ? _status(path, vfsStatus) : T::status(path, vfsStatus);
        }
        ExitInfo setThumbnail(const SyncPath &path, const QPixmap &pixmap) override {
            return _setThumbnail ? _setThumbnail(path, pixmap) : T::setThumbnail(path, pixmap);
        }
        ExitInfo setAppExcludeList() override { return _setAppExcludeList ? _setAppExcludeList() : T::setAppExcludeList(); }
        ExitInfo getFetchingAppList(QHash<QString, QString> &appList) override {
            return _getFetchingAppList ? _getFetchingAppList(appList) : T::getFetchingAppList(appList);
        }
        void exclude(const SyncPath &path) override { return _exclude ? _exclude(path) : T::exclude(path); }
        bool isExcluded(const SyncPath &path) override { return _isExcluded ? _isExcluded(path) : T::isExcluded(path); }

        bool fileStatusChanged(const SyncPath &path, SyncFileStatus status) final {
            return _fileStatusChanged ? _fileStatusChanged(path, status) : T::fileStatusChanged(path, status);
        }

        void clearFileAttributes(const SyncPath &path) override {
            return _clearFileAttributes ? _clearFileAttributes(path) : T::clearFileAttributes(path);
        }
        void dehydrate(const SyncPath &path) override { return _dehydrate ? _dehydrate(path) : T::dehydrate(path); }
        void hydrate(const SyncPath &path) override { return _hydrate ? _hydrate(path) : T::hydrate(path); }
        void cancelHydrate(const SyncPath &path) override {
            return _cancelHydrate ? _cancelHydrate(path) : T::cancelHydrate(path);
        }

        // Mock functions setters
        void setMockMode(std::function<VirtualFileMode()> mode) { _mode = mode; }
        void setMockShowPinStateActions(std::function<bool()> showPinStateActions) { _showPinStateActions = showPinStateActions; }
        void setMockUpdateMetadata(
                std::function<ExitInfo(const SyncPath &, time_t, time_t, int64_t, const NodeId &)> updateMetadata) {
            _updateMetadata = updateMetadata;
        }
        void setMockCreatePlaceholder(std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> createPlaceholder) {
            _createPlaceholder = createPlaceholder;
        }
        void setMockDehydratePlaceholder(std::function<ExitInfo(const SyncPath &)> dehydratePlaceholder) {
            _dehydratePlaceholder = dehydratePlaceholder;
        }
        void setMockConvertToPlaceholder(std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> convertToPlaceholder) {
            _convertToPlaceholder = convertToPlaceholder;
        }
        void setMockUpdateFetchStatus(
                std::function<ExitInfo(const SyncPath &, const SyncPath &, int64_t, bool &, bool &)> updateFetchStatus) {
            _updateFetchStatus = updateFetchStatus;
        }
        void setMockForceStatus(std::function<ExitInfo(const SyncPath &, const VfsStatus &)> forceStatus) {
            _forceStatus = forceStatus;
        }
        void setMockIsDehydratedPlaceholder(std::function<ExitInfo(const SyncPath &, bool &)> isDehydratedPlaceholder) {
            _isDehydratedPlaceholder = isDehydratedPlaceholder;
        }
        void setMockSetPinState(std::function<ExitInfo(const SyncPath &, PinState)> setPinState) { _setPinState = setPinState; }
        void setMockPinState(std::function<PinState(const SyncPath &)> pinState) { _pinState = pinState; }
        void setMockStatus(std::function<ExitInfo(const SyncPath &, VfsStatus &)> status) { _status = status; }
        void setMockSetThumbnail(std::function<ExitInfo(const SyncPath &, const QPixmap &)> setThumbnail) {
            _setThumbnail = setThumbnail;
        }
        void setMockSetAppExcludeList(std::function<ExitInfo()> setAppExcludeList) { _setAppExcludeList = setAppExcludeList; }
        void setMockGetFetchingAppList(std::function<ExitInfo(QHash<QString, QString> &)> getFetchingAppList) {
            _getFetchingAppList = getFetchingAppList;
        }
        void setMockExclude(std::function<void(const SyncPath &)> exclude) { _exclude = exclude; }
        void setMockIsExcluded(std::function<bool(const SyncPath &)> isExcluded) { _isExcluded = isExcluded; }
        void setMockFileStatusChanged(std::function<bool(const SyncPath &, SyncFileStatus)> fileStatusChanged) {
            _fileStatusChanged = fileStatusChanged;
        }
        void setMockClearFileAttributes(std::function<void(const SyncPath &)> clearFileAttributes) {
            _clearFileAttributes = clearFileAttributes;
        }
        void setMockDehydrate(std::function<void(const SyncPath &)> dehydrate) { _dehydrate = dehydrate; }
        void setMockHydrate(std::function<void(const SyncPath &)> hydrate) { _hydrate = hydrate; }
        void setMockCancelHydrate(std::function<void(const SyncPath &)> cancelHydrate) { _cancelHydrate = cancelHydrate; }


        // Mock functions resetters
        void resetMockMode() { _mode = nullptr; }
        void resetMockShowPinStateActions() { _showPinStateActions = nullptr; }
        void resetMockUpdateMetadata() { _updateMetadata = nullptr; }
        void resetMockCreatePlaceholder() { _createPlaceholder = nullptr; }
        void resetMockDehydratePlaceholder() { _dehydratePlaceholder = nullptr; }
        void resetMockConvertToPlaceholder() { _convertToPlaceholder = nullptr; }
        void resetMockUpdateFetchStatus() { _updateFetchStatus = nullptr; }
        void resetMockForceStatus() { _forceStatus = nullptr; }
        void resetMockIsDehydratedPlaceholder() { _isDehydratedPlaceholder = nullptr; }
        void resetMockSetPinState() { _setPinState = nullptr; }
        void resetMockPinState() { _pinState = nullptr; }
        void resetMockStatus() { _status = nullptr; }
        void resetMockSetThumbnail() { _setThumbnail = nullptr; }
        void resetMockSetAppExcludeList() { _setAppExcludeList = nullptr; }
        void resetMockGetFetchingAppList() { _getFetchingAppList = nullptr; }
        void resetMockExclude() { _exclude = nullptr; }
        void resetMockIsExcluded() { _isExcluded = nullptr; }
        void resetMockFileStatusChanged() { _fileStatusChanged = nullptr; }
        void resetMockClearFileAttributes() { _clearFileAttributes = nullptr; }
        void resetMockDehydrate() { _dehydrate = nullptr; }
        void resetMockHydrate() { _hydrate = nullptr; }
        void resetMockCancelHydrate() { _cancelHydrate = nullptr; }

    private:
        std::function<VirtualFileMode()> _mode;
        std::function<bool()> _showPinStateActions;
        std::function<ExitInfo(const SyncPath &, time_t, time_t, int64_t, const NodeId &)> _updateMetadata;
        std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> _createPlaceholder;
        std::function<ExitInfo(const SyncPath &)> _dehydratePlaceholder;
        std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> _convertToPlaceholder;
        std::function<ExitInfo(const SyncPath &, const SyncPath &, int64_t, bool &, bool &)> _updateFetchStatus;
        std::function<ExitInfo(const SyncPath &, const VfsStatus &)> _forceStatus;
        std::function<ExitInfo(const SyncPath &, bool &)> _isDehydratedPlaceholder;
        std::function<ExitInfo(const SyncPath &, PinState)> _setPinState;
        std::function<PinState(const SyncPath &)> _pinState;
        std::function<ExitInfo(const SyncPath &, VfsStatus &)> _status;
        std::function<ExitInfo(const SyncPath &, const QPixmap &)> _setThumbnail;
        std::function<ExitInfo()> _setAppExcludeList;
        std::function<ExitInfo(QHash<QString, QString> &)> _getFetchingAppList;
        std::function<void(const SyncPath &)> _exclude;
        std::function<bool(const SyncPath &)> _isExcluded;
        std::function<bool(const SyncPath &, SyncFileStatus)> _fileStatusChanged;
        std::function<void(const SyncPath &)> _clearFileAttributes;
        std::function<void(const SyncPath &)> _dehydrate;
        std::function<void(const SyncPath &)> _hydrate;
        std::function<void(const SyncPath &)> _cancelHydrate;
};
} // namespace KDC
