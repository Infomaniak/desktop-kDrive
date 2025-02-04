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
#include <libcommonserver/vfs/vfs.h>

namespace KDC {

template<class T>
concept VfsDerived = std::is_base_of_v<KDC::Vfs, T>;

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

        ExitInfo forceStatus(const SyncPath &path, bool isSyncing, int progress, bool isHydrated = false) override {
            return _forceStatus ? _forceStatus(path, isSyncing, progress, isHydrated)
                                : T::forceStatus(path, isSyncing, progress, isHydrated);
        }

        ExitInfo isDehydratedPlaceholder(const SyncPath &path, bool &isDehydrated) override {
            return _isDehydratedPlaceholder ? _isDehydratedPlaceholder(path, isDehydrated)
                                            : T::isDehydratedPlaceholder(path, isDehydrated);
        }

        ExitInfo setPinState(const SyncPath &path, KDC::PinState state) override {
            return _setPinState ? _setPinState(path, state) : T::setPinState(path, state);
        }
        KDC::PinState pinState(const SyncPath &path) override { return _pinState ? _pinState(path) : T::pinState(path); }
        ExitInfo status(const SyncPath &path, bool &isPlaceHolder, bool &isHydrated, bool &isSyncing, int &progress) override {
            return _status ? _status(path, isPlaceHolder, isHydrated, isSyncing, progress)
                           : T::status(path, isPlaceHolder, isHydrated, isSyncing, progress);
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
        bool fileStatusChanged(const SyncPath &path, KDC::SyncFileStatus status) final {
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
        void setModeMock(std::function<VirtualFileMode()> mode) { _mode = mode; }
        void setSocketApiPinStateActionsShownMock(std::function<bool()> socketApiPinStateActionsShown) {
            _socketApiPinStateActionsShown = socketApiPinStateActionsShown;
        }
        void setUpdateMetadataMock(
                std::function<ExitInfo(const SyncPath &, time_t, time_t, int64_t, const NodeId &)> updateMetadata) {
            _updateMetadata = updateMetadata;
        }
        void setCreatePlaceholderMock(std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> createPlaceholder) {
            _createPlaceholder = createPlaceholder;
        }
        void setDehydratePlaceholderMock(std::function<ExitInfo(const SyncPath &)> dehydratePlaceholder) {
            _dehydratePlaceholder = dehydratePlaceholder;
        }
        void setConvertToPlaceholderMock(std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> convertToPlaceholder) {
            _convertToPlaceholder = convertToPlaceholder;
        }
        void setUpdateFetchStatusMock(
                std::function<ExitInfo(const SyncPath &, const SyncPath &, int64_t, bool &, bool &)> updateFetchStatus) {
            _updateFetchStatus = updateFetchStatus;
        }
        void setForceStatusMock(std::function<ExitInfo(const SyncPath &, bool, int, bool)> forceStatus) {
            _forceStatus = forceStatus;
        }
        void setIsDehydratedPlaceholderMock(std::function<ExitInfo(const SyncPath &, bool &)> isDehydratedPlaceholder) {
            _isDehydratedPlaceholder = isDehydratedPlaceholder;
        }
        void setSetPinStateMock(std::function<ExitInfo(const SyncPath &, KDC::PinState)> setPinState) {
            _setPinState = setPinState;
        }
        void setPinStateMock(std::function<KDC::PinState(const SyncPath &)> pinState) { _pinState = pinState; }
        void setStatusMock(std::function<ExitInfo(const SyncPath &, bool &, bool &, bool &, int &)> status) { _status = status; }
        void setSetThumbnailMock(std::function<ExitInfo(const SyncPath &, const QPixmap &)> setThumbnail) {
            _setThumbnail = setThumbnail;
        }
        void setSetAppExcludeListMock(std::function<ExitInfo()> setAppExcludeList) { _setAppExcludeList = setAppExcludeList; }
        void setGetFetchingAppListMock(std::function<ExitInfo(QHash<QString, QString> &)> getFetchingAppList) {
            _getFetchingAppList = getFetchingAppList;
        }
        void setExcludeMock(std::function<void(const SyncPath &)> exclude) { _exclude = exclude; }
        void setIsExcludedMock(std::function<bool(const SyncPath &)> isExcluded) { _isExcluded = isExcluded; }
        void setFileStatusChangedMock(std::function<bool(const SyncPath &, KDC::SyncFileStatus)> fileStatusChanged) {
            _fileStatusChanged = fileStatusChanged;
        }
        void setClearFileAttributesMock(std::function<void(const SyncPath &)> clearFileAttributes) {
            _clearFileAttributes = clearFileAttributes;
        }
        void setDehydrateMock(std::function<void(const SyncPath &)> dehydrate) { _dehydrate = dehydrate; }
        void setHydrateMock(std::function<void(const SyncPath &)> hydrate) { _hydrate = hydrate; }
        void setCancelHydrateMock(std::function<void(const SyncPath &)> cancelHydrate) { _cancelHydrate = cancelHydrate; }


        // Mock functions resetters
        void resetModeMock() { _mode = nullptr; }
        void resetSocketApiPinStateActionsShownMock() { _socketApiPinStateActionsShown = nullptr; }
        void resetUpdateMetadataMock() { _updateMetadata = nullptr; }
        void resetCreatePlaceholderMock() { _createPlaceholder = nullptr; }
        void resetDehydratePlaceholderMock() { _dehydratePlaceholder = nullptr; }
        void resetConvertToPlaceholderMock() { _convertToPlaceholder = nullptr; }
        void resetUpdateFetchStatusMock() { _updateFetchStatus = nullptr; }
        void resetForceStatusMock() { _forceStatus = nullptr; }
        void resetIsDehydratedPlaceholderMock() { _isDehydratedPlaceholder = nullptr; }
        void resetSetPinStateMock() { _setPinState = nullptr; }
        void resetPinStateMock() { _pinState = nullptr; }
        void resetStatusMock() { _status = nullptr; }
        void resetSetThumbnailMock() { _setThumbnail = nullptr; }
        void resetSetAppExcludeListMock() { _setAppExcludeList = nullptr; }
        void resetGetFetchingAppListMock() { _getFetchingAppList = nullptr; }
        void resetExcludeMock() { _exclude = nullptr; }
        void resetIsExcludedMock() { _isExcluded = nullptr; }
        void resetFileStatusChangedMock() { _fileStatusChanged = nullptr; }
        void resetClearFileAttributesMock() { _clearFileAttributes = nullptr; }
        void resetDehydrateMock() { _dehydrate = nullptr; }
        void resetHydrateMock() { _hydrate = nullptr; }
        void resetCancelHydrateMock() { _cancelHydrate = nullptr; }

    private:
        std::function<VirtualFileMode()> _mode;
        std::function<bool()> _socketApiPinStateActionsShown;
        std::function<ExitInfo(const SyncPath &, time_t, time_t, int64_t, const NodeId &)> _updateMetadata;
        std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> _createPlaceholder;
        std::function<ExitInfo(const SyncPath &)> _dehydratePlaceholder;
        std::function<ExitInfo(const SyncPath &, const SyncFileItem &)> _convertToPlaceholder;
        std::function<ExitInfo(const SyncPath &, const SyncPath &, int64_t, bool &, bool &)> _updateFetchStatus;
        std::function<ExitInfo(const SyncPath &, bool, int, bool)> _forceStatus;
        std::function<ExitInfo(const SyncPath &, bool &)> _isDehydratedPlaceholder;
        std::function<ExitInfo(const SyncPath &, KDC::PinState)> _setPinState;
        std::function<KDC::PinState(const SyncPath &)> _pinState;
        std::function<ExitInfo(const SyncPath &, bool &, bool &, bool &, int &)> _status;
        std::function<ExitInfo(const SyncPath &, const QPixmap &)> _setThumbnail;
        std::function<ExitInfo()> _setAppExcludeList;
        std::function<ExitInfo(QHash<QString, QString> &)> _getFetchingAppList;
        std::function<void(const SyncPath &)> _exclude;
        std::function<bool(const SyncPath &)> _isExcluded;
        std::function<bool(const SyncPath &, KDC::SyncFileStatus)> _fileStatusChanged;
        std::function<void(const SyncPath &)> _clearFileAttributes;
        std::function<void(const SyncPath &)> _dehydrate;
        std::function<void(const SyncPath &)> _hydrate;
        std::function<void(const SyncPath &)> _cancelHydrate;
};
} // namespace KDC
