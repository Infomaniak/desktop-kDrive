#!/usr/bin/env python3

#  Infomaniak kDrive - Desktop
#  Copyright (C) 2023-2026 Infomaniak Network SA
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Build the kChat notification of the nightly vulnerability scan.

Reads the JSON summary produced by vulnerability_report.py, classifies the
overall status and writes the kChat markdown message to a UTF-8 file:

  - red:    Critical or High finding on at least one scope
  - orange: only Medium/Low findings on any scope, or a failed/cancelled scan
  - green:  nothing to report — only when every scope reported clean results
            and its scan job succeeded (a missing, failed or cancelled scope
            is never reported green)

The badge is a static shields.io URL rendered by raster.shields.io as a PNG
(kChat cannot display SVGs), colored by severity: every message keeps its
own result since the status and date are baked into the URL.
"""

import argparse
import json
import os
import sys
from datetime import datetime, timezone

SEVERITIES = ["Critical", "High", "Medium", "Low", "Unknown"]

# shields.io named colors of the badges endpoint
SHIELDS_COLORS = {
    "red": "red",
    "orange": "orange",
    "failed": "orange",
    "green": "brightgreen",
    "grey": "lightgrey",
}

HEADLINES = {
    "red": "🔴 **Nightly vulnerability scan {date} — action required**",
    "orange": "🟠 **Nightly vulnerability scan {date} — low severity findings**",
    "failed": "🟠 **Nightly vulnerability scan {date} — scan failed, check the run**",
    "green": "✅ **Nightly vulnerability scan {date} — nothing to report**",
    "grey": "⚪ **Nightly vulnerability scan {date} — cancelled**",
}

# Scope keys written by vulnerability_report.py: the green status requires all
# of them to be present, successful and clean, so that a truncated summary or
# a failed/missing scan result is never reported as "nothing to report".
EXPECTED_SCOPE_KEYS = ["windows-server", "macos-app", "linux-appimage", "windows-client-nuget"]


def load_summary(path):
    try:
        with open(path, encoding="utf-8-sig") as stream:
            data = json.load(stream)
        return data if isinstance(data, dict) and "scopes" in data else None
    except (OSError, json.JSONDecodeError, UnicodeDecodeError):
        return None


def totals(summary):
    total = {severity: 0 for severity in SEVERITIES}
    for scope in (summary or {}).get("scopes", []):
        counts = scope.get("counts") or {}
        for severity in SEVERITIES:
            total[severity] += counts.get(severity) or 0
    return total


def classify(report_result, summary):
    total = totals(summary)
    if report_result == "cancelled":
        return "grey", total
    if total["Critical"] or total["High"]:
        return "red", total
    if report_result != "success":
        return "failed", total
    scopes = {scope.get("key"): scope for scope in (summary or {}).get("scopes") or []}
    for key in EXPECTED_SCOPE_KEYS:
        scope = scopes.get(key)
        if scope is None or scope.get("counts") is None or scope.get("result") != "success":
            # An incomplete or partially failed scan must not be reported green
            return "failed", total
    if total["Medium"] or total["Low"] or total["Unknown"]:
        return "orange", total
    return "green", total


def badge_right_text(status, total, date):
    if status == "green":
        return "PASS " + date
    if status == "red":
        parts = [(total["Critical"], "CRITICAL"), (total["High"], "HIGH")]
    elif status == "orange":
        parts = [(total["Medium"], "MEDIUM"), (total["Low"], "LOW"), (total["Unknown"], "UNKNOWN")]
    else:
        return "SCAN FAILED"
    return " ".join("%d %s" % (count, word) for count, word in parts if count) or "SCAN FAILED"


def badge_url(status, total, date):
    right = badge_right_text(status, total, date)
    # Static badge as documented on https://shields.io/badges: in the path,
    # a literal dash must be written as "--" and a space as "_"; the query
    # parameters control the style and the logo.
    escaped = right.replace("-", "--").replace(" ", "_")
    color = SHIELDS_COLORS[status]
    return ("https://raster.shields.io/badge/vulnerability_scan-%s-%s.png"
            "?style=for-the-badge&logo=githubactions&logoColor=white" % (escaped, color)), right


def scope_line(scope):
    title = scope.get("title") or scope.get("key") or "?"
    counts = scope.get("counts")
    if counts is None:
        result = scope.get("result") or "missing"
        detail = "scan job %s" % result if result != "success" else "no results available"
        return "- ⚠️ **%s** — %s" % (title, detail)
    parts = ["%d %s" % (counts[severity], severity) for severity in SEVERITIES if counts.get(severity)]
    if not parts:
        return "- ✅ %s — clean" % title
    emoji = "🔴" if (counts.get("Critical") or counts.get("High")) else "🟠"
    return "- %s **%s** — %s" % (emoji, title, ", ".join(parts))


def build_message(status, badge, summary, date, run_url):
    lines = ["![Vulnerability scan](%s)" % badge, "", HEADLINES[status].format(date=date), ""]
    if status in ("red", "orange", "failed"):
        scopes = (summary or {}).get("scopes")
        if scopes:
            lines.extend(scope_line(scope) for scope in scopes)
        else:
            lines.append("- ⚠️ results unavailable")
        lines.append("")
    lines.append("[Open the run](%s) for the full report (step summary and vulnerability-report artifact)" % run_url)
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Build the kChat message of the vulnerability scan")
    parser.add_argument("--summary", help="Path of the JSON summary written by vulnerability_report.py")
    parser.add_argument("--result", default="", help="Result of the Build-report job")
    parser.add_argument("--run-url", default="", help="URL of the workflow run")
    parser.add_argument("--out", required=True, help="Path of the UTF-8 kChat message file to write")
    args = parser.parse_args()

    summary = load_summary(args.summary) if args.summary else None
    if args.summary and summary is None:
        print("kChat notification: summary unreadable at %s" % args.summary, file=sys.stderr)

    status, total = classify(args.result, summary)
    date = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    badge, right_text = badge_url(status, total, date)

    message = build_message(status, badge, summary, date, args.run_url)
    # Emojis would crash print() on the cp1252 console of the Windows runners:
    # the message goes to a UTF-8 file, stdout keeps an ASCII status line.
    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as stream:
        stream.write(message + "\n")
    print("kChat notification: %s, badge '%s', message written to %s" % (status, right_text, args.out))


if __name__ == "__main__":
    main()
