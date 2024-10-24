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

#pragma once

#include "abstracttokennetworkjob.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

namespace KDC {

class SnapshotItemHandler {
    public:
        explicit SnapshotItemHandler(log4cplus::Logger logger);
        enum CsvIndex {
            CsvIndexId = 0,
            CsvIndexParentId,
            CsvIndexName,
            CsvIndexType,
            CsvIndexSize,
            CsvIndexCreatedAt,
            CsvIndexModtime,
            CsvIndexCanWrite,
            CsvIndexIsLink,
            CsvIndexEnd
        };

        inline static void incrementCsvIndex(CsvIndex &index) { index = static_cast<CsvIndex>(static_cast<int>(index) + 1); };

        struct ParsingState {
                CsvIndex index{CsvIndexId}; // The index of the column that is currently read.
                bool readingDoubleQuotedValue{
                        false}; // True if an opening double quote is encountered with no closing counter-part at this stage.
                bool prevCharDoubleQuotes{false};
                bool readNextLine{true}; // If true, read the next line, stop item parsing otherwise.
                std::string tmp;
                int doubleQuoteCount{0};
        };

        bool updateSnapshotItem(const std::string &str, CsvIndex index, SnapshotItem &item);
        /**
         * @brief Extract one snapshot item from the full listing CSV file. An item can spread on several lines.
         * @param item output value
         * @param ss stringstream containg the CSV file
         * @param error `true` if parsing fails
         * @param ignore `true` if a line is ignored due to a non critical parsing issue
         * @return `true` if there are more lines to be read
         */
        bool getItem(SnapshotItem &item, std::stringstream &ss, bool &error, bool &ignore);

    private:
        bool _ignoreFirstLine = true;
        log4cplus::Logger _logger;
        void logError(const std::wstring &methodName, const std::wstring &stdErrorType, const std::string &str,
                      const std::exception &exc);
        void readSnapshotItemFields(SnapshotItem &item, const std::string &line, bool &error, ParsingState &state);
};


class CsvFullFileListWithCursorJob : public AbstractTokenNetworkJob {
    public:
        CsvFullFileListWithCursorJob(int driveDbId, const NodeId &dirId, std::unordered_set<NodeId> blacklist = {},
                                     bool zip = true);

        /**
         * @brief getItem
         * @param item : item extracted from line of the CSV file
         * @param error : blocking error, stop the process
         * @param ignore : parsing issue, non blocking, just ignore the item
         * @return if return == true, continue parsing
         */
        bool getItem(SnapshotItem &item, bool &error, bool &ignore);
        std::string getCursor();

    private:
        virtual std::string getSpecificUrl() override;
        virtual void setQueryParameters(Poco::URI &uri, bool &) override;
        inline virtual ExitInfo setData() override { return ExitCode::Ok; }

        virtual bool handleResponse(std::istream &is) override;

        NodeId _dirId;
        std::unordered_set<NodeId> _blacklist;
        bool _zip = true;

        std::stringstream _ss;
        SnapshotItemHandler _snapshotItemHandler;
};

} // namespace KDC
