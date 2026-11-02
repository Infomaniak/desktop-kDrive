#!/usr/bin/env python3
"""Report formatting differences in server-side files changed by a PR."""

import argparse
import difflib
import os
from pathlib import Path
import subprocess


SOURCE_ROOTS = (
    "src/server/",
    "src/libcommon/",
    "src/libcommonserver/",
    "src/libsyncengine/",
    "src/libparms/",
    "test/",
)
SOURCE_EXTENSIONS = {".ino", ".cpp", ".cc", ".c", ".h", ".hpp", ".hh", ".m", ".mm"}


def annotation_escape(text):
    return text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A").replace(",", "%2C").replace(":", "%3A")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base")
    parser.add_argument("head")
    parser.add_argument("--clang-format", default="clang-format-18")
    args = parser.parse_args()

    changed = subprocess.check_output(
        ["git", "diff", "--name-only", "-z", "--diff-filter=ACMR", f"{args.base}...{args.head}"]
    )
    files = [
        os.fsdecode(name)
        for name in changed.split(b"\0")
        if name
        and os.fsdecode(name).startswith(SOURCE_ROOTS)
        and Path(os.fsdecode(name)).suffix in SOURCE_EXTENSIONS
        and Path(os.fsdecode(name)).is_file()
    ]

    summary = ["## Server formatting (advisory)", ""]
    if files:
        version = subprocess.check_output([args.clang_format, "--version"], text=True).strip()
        print(version)
        summary.extend([f"Formatter: `{version}`", ""])

    differences = []
    for filename in files:
        original = Path(filename).read_bytes()
        formatted = subprocess.check_output([args.clang_format, "--style=file", filename])
        if original == formatted:
            continue

        print(
            f"::warning file={annotation_escape(filename)}::"
            "Formatting differs from .clang-format (advisory). "
            f"Run {args.clang_format} -i --style=file on this file."
        )
        diff = "".join(
            line if line.endswith("\n") else line + "\n\\ No newline at end of file\n"
            for line in difflib.unified_diff(
                original.decode("utf-8", errors="replace").splitlines(keepends=True),
                formatted.decode("utf-8", errors="replace").splitlines(keepends=True),
                fromfile=f"a/{filename}",
                tofile=f"b/{filename}",
            )
        )
        print(diff)
        differences.append(diff)

    result = f"Checked {len(files)} changed server-side file(s); {len(differences)} need formatting."
    summary.append(result)
    summary.extend(["", "Formatting differences are advisory and do not block CI."])
    if differences:
        summary.extend(["", "Suggested changes (full diff in the step log):", "", "````diff"])
        # Keep the summary comfortably below GitHub's 1 MiB limit.
        summary.append("\n".join(differences)[:100_000])
        summary.append("````")

    print(result)
    if summary_path := os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(summary_path, "a", encoding="utf-8") as output:
            output.write("\n".join(summary) + "\n")


if __name__ == "__main__":
    main()
