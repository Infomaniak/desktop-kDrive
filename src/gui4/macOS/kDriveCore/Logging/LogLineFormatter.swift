/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation

struct LogLineFormatter {
    private let dateFormatter: DateFormatter

    init(timeZone: TimeZone = .current) {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = timeZone
        formatter.dateFormat = "yyyy-MM-dd HH:mm:ss:SSS"
        dateFormatter = formatter
    }

    func format(_ event: LogEvent) -> String {
        let date = dateFormatter.string(from: event.date)
        return "\(date) [\(event.level.logLetter)] (\(event.threadID)) \(event.file):\(event.line) - \(event.message)"
    }
}
