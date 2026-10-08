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

import Sentry

protocol SentryLogReporting {
    func addBreadcrumb(_ event: LogEvent)
    func capture(_ event: LogEvent)
}

struct SentryLogReporter: SentryLogReporting {
    func addBreadcrumb(_ event: LogEvent) {
        let breadcrumb = Breadcrumb(level: event.level.sentryLevel, category: event.category)
        breadcrumb.message = event.message
        breadcrumb.timestamp = event.date
        breadcrumb.type = "log"
        breadcrumb.data = event.sentryData

        SentrySDK.addBreadcrumb(breadcrumb)
    }

    func capture(_ event: LogEvent) {
        SentrySDK.capture(message: event.message) { scope in
            scope.setLevel(event.level.sentryLevel)
            scope.setContext(value: event.sentryData, key: "log")
        }
    }
}

private extension LogEvent {
    var sentryData: [String: Any] {
        [
            "category": category,
            "file": file,
            "line": line,
            "thread_id": threadID
        ]
    }
}
