#!/usr/bin/env python3
"""Build the kChat notification of the nightly vulnerability scan.

Reads the JSON summary produced by vulnerability_report.py, classifies the
overall status, draws the matching badge PNG and writes the kChat markdown
message to a UTF-8 file:

  - red:    Critical or High finding on at least one scope
  - orange: only Medium/Low findings on any scope, or a failed/cancelled scan
  - green:  nothing to report

The badge URL is emitted as a {{BADGE_URL}} placeholder: the workflow uploads
the PNG first, then replaces the placeholder (falling back to a static
raster.shields.io URL if the upload fails, since kChat can only display
images from a public URL).
"""

import argparse
import json
import os
import sys
from datetime import datetime, timezone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_badge  # noqa: E402

SEVERITIES = ["Critical", "High", "Medium", "Low", "Unknown"]

COLORS = {
    "red": "#e05d44",
    "orange": "#fe7d37",
    "green": "#4c1",
    "grey": "#9f9f9f",
    "failed": "#fe7d37",
}

HEADLINES = {
    "red": "🔴 **Nightly vulnerability scan {date} — action required**",
    "orange": "🟠 **Nightly vulnerability scan {date} — low severity findings**",
    "failed": "🟠 **Nightly vulnerability scan {date} — scan failed, check the run**",
    "green": "✅ **Nightly vulnerability scan {date} — nothing to report**",
    "grey": "⚪ **Nightly vulnerability scan {date} — cancelled**",
}


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


def build_message(status, summary, date, run_url):
    lines = ["![Vulnerability scan]({{BADGE_URL}})", "", HEADLINES[status].format(date=date), ""]
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
    parser = argparse.ArgumentParser(description="Build the kChat message and badge of the vulnerability scan")
    parser.add_argument("--summary", help="Path of the JSON summary written by vulnerability_report.py")
    parser.add_argument("--result", default="", help="Result of the Build-report job")
    parser.add_argument("--run-url", default="", help="URL of the workflow run")
    parser.add_argument("--badge-out", required=True, help="Path of the badge PNG to draw")
    parser.add_argument("--badge-meta", required=True, help="Path of the JSON badge description to write")
    parser.add_argument("--out", required=True, help="Path of the UTF-8 kChat message file to write")
    args = parser.parse_args()

    summary = load_summary(args.summary) if args.summary else None
    if args.summary and summary is None:
        print("kChat notification: summary unreadable at %s" % args.summary, file=sys.stderr)

    status, total = classify(args.result, summary)
    date = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    right_text = badge_right_text(status, total, date)
    color = COLORS[status]

    make_badge.make_badge("VULNERABILITY SCAN", right_text, color, args.badge_out)
    meta_dir = os.path.dirname(os.path.abspath(args.badge_meta))
    os.makedirs(meta_dir, exist_ok=True)
    with open(args.badge_meta, "w", encoding="utf-8") as stream:
        json.dump({"color": color, "right_text": right_text}, stream, indent=2)
        stream.write("\n")

    message = build_message(status, summary, date, args.run_url)
    # Emojis would crash print() on the cp1252 console of the Windows runners:
    # the message goes to a UTF-8 file, stdout keeps an ASCII status line.
    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as stream:
        stream.write(message + "\n")
    print("kChat notification: %s, badge '%s' %s, message written to %s" % (status, right_text, color, args.out))


if __name__ == "__main__":
    main()
