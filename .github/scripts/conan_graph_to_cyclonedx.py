#!/usr/bin/env python3
"""Convert the dependency graph resolved by Conan into a CycloneDX SBOM.

Some dependencies are statically linked into the shipped binaries (conan zlib
is for example linked into libcrypto/libssl on macOS), so a binary scan of the
app bundle cannot fingerprint them. The dependency graph resolved by conan for
the build is converted to CycloneDX instead, so Grype can scan it like any
other SBOM.

The input is the JSON output of `conan graph info . --format=json` (conan 2
does not write the resolved graph to disk during `conan install`). The
graph_info.json layout is also accepted for convenience.
"""

import argparse
import json
import sys


def parse_ref(ref):
    # "name/version[@user/channel]#revision" -> (name, version, user_channel or None)
    base = ref.split("#", 1)[0]
    name_version, _, user_channel = base.partition("@")
    parts = name_version.split("/")
    if len(parts) < 2 or not parts[1]:
        return None
    return parts[0], parts[1], user_channel or None


def load_graph(path):
    with open(path, encoding="utf-8-sig") as stream:
        return json.load(stream)


def graph_nodes(data):
    # `conan graph info --format=json` nests the graph under the "graph" key,
    # while graph_info.json nests it under "serialized"; accept both plus a
    # bare "nodes" mapping.
    for container in (data, data.get("graph"), data.get("serialized")):
        if isinstance(container, dict) and isinstance(container.get("nodes"), dict):
            return container["nodes"]
    return {}


def graph_packages(data):
    nodes = graph_nodes(data)
    packages = {}
    for node in nodes.values():
        if not isinstance(node, dict):
            continue
        ref = node.get("ref") or ""
        if not ref or node.get("recipe") == "Consumer":
            continue  # root/consumer node, not a dependency
        if node.get("context", "host") != "host":
            continue  # build tool_requires (cmake, ninja, ...) are not shipped
        user_channel = None
        if node.get("user"):
            user_channel = node["user"]
            if node.get("channel"):
                user_channel += "/" + node["channel"]
        name = node.get("name")
        version = node.get("version")
        if not name or not version:
            parsed = parse_ref(ref)
            if parsed is None:
                continue
            name, version, ref_user_channel = parsed
            user_channel = user_channel or ref_user_channel
        packages.setdefault((name, version), user_channel)
    return packages


def cyclonedx(packages):
    components = []
    for (name, version), user_channel in sorted(packages.items()):
        purl = "pkg:conan/%s/%s" % (name, version)
        if user_channel:
            purl += "@%s" % user_channel
        # Grype matches binaries by the CPE syft generates for them; emit the
        # same CPE form so the conan packages are matched the same way.
        cpe = "cpe:2.3:a:%s:%s:%s:*:*:*:*:*:*:*" % (name, name, version)
        components.append({"type": "library", "name": name, "version": version, "purl": purl, "cpe": cpe})
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.4",
        "version": 1,
        "metadata": {
            "tools": [{"vendor": "Infomaniak", "name": "conan-graph-to-cyclonedx", "version": "1.0"}]
        },
        "components": components,
    }


def main():
    parser = argparse.ArgumentParser(description="Convert a Conan resolved graph to a CycloneDX SBOM")
    parser.add_argument("graph", help="Path of the Conan graph JSON (conan graph info --format=json)")
    parser.add_argument("output", help="Path of the CycloneDX JSON to write")
    args = parser.parse_args()

    packages = graph_packages(load_graph(args.graph))
    if not packages:
        print("No conan package found in %s" % args.graph, file=sys.stderr)
        sys.exit(1)

    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(cyclonedx(packages), stream, indent=2)
    print("Wrote %d conan packages to %s" % (len(packages), args.output))


if __name__ == "__main__":
    main()
