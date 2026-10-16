#!/usr/bin/env python3
"""Merge several Grype JSON results into a single file.

The report generator reads one ``<scope>-grype.json`` per scope; when a scope
is scanned from several SBOMs (app bundle and conan dependency graph on
macOS), the matches are concatenated here so the report stays unchanged.
"""

import argparse
import json
import sys


def main():
    parser = argparse.ArgumentParser(description="Merge Grype JSON results into a single file")
    parser.add_argument("--output", required=True, help="Path of the merged Grype JSON to write")
    parser.add_argument("inputs", nargs="+", help="Grype JSON files to merge, in order")
    args = parser.parse_args()

    merged = None
    for path in args.inputs:
        with open(path, encoding="utf-8-sig") as stream:
            data = json.load(stream)
        if merged is None:
            merged = data
            continue
        merged["matches"] = (merged.get("matches") or []) + (data.get("matches") or [])

    if merged is None:
        print("No input file given", file=sys.stderr)
        sys.exit(1)

    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(merged, stream, indent=2)
    print("Merged %d grype result files into %s" % (len(args.inputs), args.output))


if __name__ == "__main__":
    main()
