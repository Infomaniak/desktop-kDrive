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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "app/mainwindow/searchresultmodel.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <string>

namespace KDC {

class CommService;
struct DriveSearchResult;
class MainSelectionStore;

/**
 * QML-facing state of the search dialog: searches the drive of the selected synchronization and opens a result.
 *
 * Typing is debounced, and every response is matched against the request generation it answers, since an IPC request
 * cannot be cancelled: a response to an outdated query, page, or synchronization is dropped. Pages are fetched with the
 * cursor returned by the server.
 */
class SearchController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
        Q_PROPERTY(State state READ state NOTIFY stateChanged)
        Q_PROPERTY(SearchResultModel *model READ model CONSTANT)
        Q_PROPERTY(bool available READ available NOTIFY availableChanged)
        Q_PROPERTY(QString driveName READ driveName NOTIFY driveNameChanged)
        Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY paginationChanged)
        Q_PROPERTY(bool loadingMore READ loadingMore NOTIFY paginationChanged)
        Q_PROPERTY(bool nextPageFailed READ nextPageFailed NOTIFY paginationChanged)

    public:
        enum class State : uint8_t {
            Idle,
            Loading,
            Results,
            Empty,
            Error,
        };
        Q_ENUM(State)

        SearchController(const CommService &commService, const MainSelectionStore &selectionStore, QObject *parent = nullptr);

        [[nodiscard]] const QString &query() const { return _query; }
        void setQuery(const QString &query);
        [[nodiscard]] State state() const { return _state; }
        [[nodiscard]] SearchResultModel *model() { return &_model; }
        [[nodiscard]] bool available() const { return _available; }
        [[nodiscard]] const QString &driveName() const { return _driveName; }
        [[nodiscard]] bool canLoadMore() const;
        [[nodiscard]] bool loadingMore() const { return _loadingMore; }
        [[nodiscard]] bool nextPageFailed() const { return _nextPageFailed; }

        // Both start from an empty query, so that a reopened dialog never shows a previous search.
        Q_INVOKABLE void open();
        Q_INVOKABLE void close();
        // Repeats the failed request: the first page in the Error state, otherwise the next page.
        Q_INVOKABLE void retry();
        // Requests the next page. Ignored while a page loads or after a failed page, which only `retry()` requests again.
        Q_INVOKABLE void loadMore();
        // Opens a local result with the desktop services, any other result in the web app. With `revealInFolder`, a local
        // result opens its parent folder instead; a remote result ignores it.
        Q_INVOKABLE void openResult(int32_t row, bool revealInFolder = false) const;

        void retranslate();

    signals:
        void queryChanged();
        void stateChanged();
        void availableChanged();
        void driveNameChanged();
        void paginationChanged();

    private:
        void reset();
        void startSearch();
        void requestPage(bool firstPage);
        void handlePageResponse(uint64_t generation, SyncDbId syncDbId, bool firstPage, const ExitInfo &exitInfo,
                                const DriveSearchResult &searchResult);
        void handleSelectionChanged();
        void refreshSelection();
        void setState(State state);

        const CommService &_commService;
        const MainSelectionStore &_selectionStore;
        SearchResultModel _model;
        QTimer _debounceTimer;

        QString _query;
        State _state{State::Idle};
        bool _available{false};
        QString _driveName;

        // Request of the displayed results. Next pages repeat it unchanged, as the cursor belongs to it.
        SyncDbId _activeSyncDbId{0};
        QString _activeQuery;
        uint64_t _requestGeneration{0};

        bool _hasMore{false};
        std::string _cursor;
        bool _loadingMore{false};
        bool _nextPageFailed{false};
};

} // namespace KDC
