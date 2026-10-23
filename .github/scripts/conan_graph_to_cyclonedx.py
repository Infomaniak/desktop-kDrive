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
import urllib.parse

# Conan recipe names that build a product known to NVD under a different
# identifier (the recipe name does not always match the CPE product name);
# grype matches components by CPE when their purl is unknown.
CPE_PRODUCT_ALIASES = {
    "openssl-macos": "openssl",
    "sqlite3": "sqlite",
}


def conan_purl(name, version, user=None, channel=None):
    # pkg:conan/<name>@<version>, as defined by the purl-spec conan type:
    # the conan user and channel are optional percent-encoded qualifiers.
    purl = "pkg:conan/%s@%s" % (name, version)
    qualifiers = [(key, value) for key, value in (("user", user), ("channel", channel))
                  if value and value != "_"]
    if qualifiers:
        purl += "?" + "&".join(
            "%s=%s" % (key, urllib.parse.quote(value, safe="")) for key, value in sorted(qualifiers))
    return purl


def conan_cpe(name, version):
    # Emit the same CPE form as syft (vendor = product name), using the
    # product identifier instead of the recipe name.
    product = CPE_PRODUCT_ALIASES.get(name, name)
    return "cpe:2.3:a:%s:%s:%s:*:*:*:*:*:*:*" % (product, product, version)


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
        user = node.get("user") or None
        channel = node.get("channel") or None
        name = node.get("name")
        version = node.get("version")
        if not name or not version:
            parsed = parse_ref(ref)
            if parsed is None:
                continue
            name, version, ref_user_channel = parsed
            if ref_user_channel and not user:
                user, _, channel = ref_user_channel.partition("/")
        packages.setdefault((name, version), (user, channel))
    return packages


def cyclonedx(packages):
    components = []
    for (name, version), (user, channel) in sorted(packages.items()):
        components.append({"type": "library", "name": name, "version": version,
                           "purl": conan_purl(name, version, user, channel), "cpe": conan_cpe(name, version)})
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
