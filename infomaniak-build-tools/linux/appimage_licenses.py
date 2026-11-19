#!/usr/bin/env python3
#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2026 Infomaniak Network SA
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""
Ships the license notices of every component bundled in the Linux AppImage, in usr/share/doc/kDrive.

Each ELF file of the AppDir is traced back to its origin: kDrive itself, a Qt module (Qt SBOM), a Conan package
(manifest written by conanfile.py) or an Ubuntu package of the build system (dpkg). The build fails when a file has no
known origin or when a license text is missing, so that no component is shipped without its notice.

Runs with the Python of the build image (3.10) and the standard library only.
"""

import argparse
import fnmatch
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
VENDORED_DIR = SCRIPT_DIR / "licenses"
DOC_RELATIVE_DIR = Path("usr/share/doc/kDrive")
OWN_BINARIES = {"bin/kDrive", "bin/kdrive_qml"}
QT_DATA_DIRS = ("plugins", "qml", "translations")
SPDX_OPERATORS = {"AND", "OR", "WITH"}
UBUNTU_COMMON_LICENSES = Path("/usr/share/common-licenses")

# Components shipped without their own notice, matched by file name. "package" tells where they come from: the Qt
# package (Qt installer) or /usr/local/lib (built from source in the build image). Update the vendored notice and the
# version together: the build fails when the bundled version differs.
EXTRA_COMPONENTS = [
    {
        "id": "icu",
        "name": "ICU",
        "version": "73.2",
        "license": "Unicode-DFS-2016 AND ICU (third-party terms in LICENSE)",
        "patterns": ["libicu*.so*"],
        "package": "qt",
        "license_dir": "components/icu",
        "source": "https://github.com/unicode-org/icu/releases/tag/release-73-2",
    },
    {
        "id": "libzip",
        "name": "libzip",
        "version": "1.10.1",
        "license": "BSD-3-Clause",
        "patterns": ["libzip.so*"],
        "package": "local",
        "pkgconfig": "libzip.pc",
        "license_dir": "components/libzip",
        "source": "https://github.com/nih-at/libzip/releases/tag/v1.10.1",
    },
]
LOCAL_LIB_DIR = Path("/usr/local/lib")

APPIMAGE_RUNTIME = {
    "name": "AppImage type 2 runtime",
    "license": "MIT; statically links squashfuse (BSD-2-Clause), zstd (BSD-3-Clause), zlib (Zlib), "
    "libfuse 3 (LGPL-2.1-only), mimalloc (MIT) and musl (MIT)",
    "license_dir": "components/appimage-runtime",
    "license_ids": ["MIT", "BSD-2-Clause", "BSD-3-Clause", "Zlib", "LGPL-2.1-only"],
    "source": "https://github.com/AppImage/type2-runtime (the commit is printed by `--appimage-version`)",
}


class LicenseError(Exception):
    pass


@dataclass
class Component:
    name: str
    version: str
    license: str
    origin: str
    source: str
    files: list = field(default_factory=list)
    note: str = ""  # Shown instead of the files when the component ships no ELF file.


def tool_env():
    """Host tools must not load the AppDir libraries."""
    env = dict(os.environ)
    env.pop("LD_LIBRARY_PATH", None)
    env["LC_ALL"] = "C"
    return env


def is_elf(path: Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    with path.open("rb") as file:
        return file.read(4) == b"\x7fELF"


_build_ids = {}


def build_id(path: Path):
    """GNU build ID: kept by patchelf and strip, it identifies the file a bundled copy comes from."""
    real_path = path.resolve()
    if real_path not in _build_ids:
        result = subprocess.run(["readelf", "-n", str(real_path)], capture_output=True, text=True, env=tool_env())
        match = re.search(r"Build ID: ([0-9a-f]+)", result.stdout)
        _build_ids[real_path] = match.group(1) if match else None
    return _build_ids[real_path]


def license_ids(expression: str) -> set:
    tokens = re.findall(r"[A-Za-z0-9.+\-]+", expression or "")
    return {token for token in tokens if token not in SPDX_OPERATORS and token not in {"NOASSERTION", "NONE"}}


def load_manifest(path: Path) -> list:
    dependencies = json.loads(path.read_text())
    for dependency in dependencies:
        folder = dependency.get("package_folder")
        dependency["package_folder"] = Path(folder) if folder else None
    return dependencies


def index_conan_files(dependencies: list) -> dict:
    """Maps the name of every shared library or executable of the Conan packages to its candidates."""
    index = {}
    for dependency in dependencies:
        folder = dependency["package_folder"]
        if folder is None or not folder.is_dir():
            continue
        for root, _dirs, files in os.walk(folder):
            for name in files:
                path = Path(root) / name
                if ".so" in name or Path(root).name == "bin":
                    index.setdefault(name, []).append((dependency, path))
    return index


def load_qt_sbom(qt_folder: Path, qt_version: str):
    """Returns the SBOM of every Qt module, and the module and license of every installed Qt file."""
    sboms = {}
    files = {}
    for sbom_path in sorted((qt_folder / "sbom").glob(f"*-{qt_version}.spdx.json")):
        sbom = json.loads(sbom_path.read_text())
        sboms[sbom_path.name] = (sbom_path, sbom)
        for entry in sbom.get("files", []):
            files[entry["fileName"].removeprefix("./")] = (sbom_path.name, entry.get("licenseConcluded"))
    if not sboms:
        raise LicenseError(f"No Qt SBOM found in '{qt_folder / 'sbom'}'")
    return sboms, files


def dpkg_candidates(name: str) -> list:
    result = subprocess.run(["dpkg", "-S", f"*/{name}"], capture_output=True, text=True, env=tool_env())
    candidates = []
    for line in result.stdout.splitlines():
        packages, _, path = line.partition(": ")
        if not path.endswith("/" + name) or not Path(path).exists():
            continue
        for package in packages.split(", "):
            candidates.append((package.split(":")[0], Path(path)))
    return candidates


def dpkg_info(package: str) -> dict:
    result = subprocess.run(
        ["dpkg-query", "-W", "-f=${Version}\t${source:Package}\t${source:Version}", package],
        capture_output=True,
        text=True,
        env=tool_env(),
        check=True,
    )
    version, source_package, source_version = result.stdout.split("\t")
    return {"version": version, "source_package": source_package, "source_version": source_version}


def match_build_id(file: Path, candidates: list):
    """Returns the candidate the bundled file was copied from, identified by its build ID."""
    file_id = build_id(file)
    if file_id is None:
        return None
    for candidate in candidates:
        if build_id(candidate[1]) == file_id:
            return candidate
    return None


def extra_component_for(name: str, package_name: str):
    for component in EXTRA_COMPONENTS:
        if component["package"] == package_name and any(fnmatch.fnmatch(name, p) for p in component["patterns"]):
            return component
    return None


def source_commit(source_dir: Path) -> str:
    """Commit of the built sources, read from .git without requiring git in the build image."""
    git_dir = source_dir / ".git"
    if git_dir.is_file():
        git_dir = (source_dir / git_dir.read_text().split("gitdir:", 1)[1].strip()).resolve()
    head = (git_dir / "HEAD").read_text().strip()
    if not head.startswith("ref: "):
        return head
    ref = head[len("ref: "):]
    common_dir = git_dir
    if (git_dir / "commondir").is_file():
        common_dir = (git_dir / (git_dir / "commondir").read_text().strip()).resolve()
    for base in (git_dir, common_dir):
        if (base / ref).is_file():
            return (base / ref).read_text().strip()
    packed_refs = common_dir / "packed-refs"
    if packed_refs.is_file():
        for line in packed_refs.read_text().splitlines():
            if line.endswith(" " + ref):
                return line.split(" ", 1)[0]
    raise LicenseError(f"Unable to resolve '{ref}' in '{git_dir}'")


class Collector:
    def __init__(self, app_dir: Path, manifest: Path, source_dir: Path):
        self.app_dir = app_dir
        self.usr_dir = app_dir / "usr"
        self.doc_dir = app_dir / DOC_RELATIVE_DIR
        self.source_dir = source_dir
        self.dependencies = load_manifest(manifest)
        self.conan_index = index_conan_files(self.dependencies)
        qt = [d for d in self.dependencies if d["name"] == "qt"]
        if len(qt) != 1 or qt[0]["package_folder"] is None:
            raise LicenseError("The Conan manifest must list the Qt package with its folder")
        self.qt = qt[0]
        self.qt_sboms, self.qt_files = load_qt_sbom(self.qt["package_folder"], self.qt["version"])
        self.components = {}
        self.qt_modules = set()
        self.ubuntu_packages = {}
        self.extra_components = set()
        self.errors = []

    def component(self, key: str, **fields) -> Component:
        if key not in self.components:
            self.components[key] = Component(**fields)
        return self.components[key]

    def run(self):
        for path in sorted(self.usr_dir.rglob("*")):
            relative = path.relative_to(self.usr_dir).as_posix()
            if relative.startswith(DOC_RELATIVE_DIR.relative_to("usr").as_posix()):
                continue
            if relative.split("/", 1)[0] in QT_DATA_DIRS and relative in self.qt_files:
                self.qt_component(*self.qt_files[relative]).note = "data files (translations, QML)"
            if is_elf(path):
                self.classify(path, relative)
        if self.errors:
            raise LicenseError(
                "Bundled files without a known origin:\n  "
                + "\n  ".join(self.errors)
                + "\nDeclare a library from /usr/local or the Qt installer in EXTRA_COMPONENTS, with its license"
                " under infomaniak-build-tools/linux/licenses/components/."
            )
        self.write()

    def classify(self, path: Path, relative: str):
        if relative in OWN_BINARIES:
            self.component(
                "kdrive",
                name="kDrive",
                version="",
                license="GPL-3.0-or-later",
                origin="Infomaniak",
                source="see SOURCES.md",
            ).files.append(relative)
            return

        conan_candidates = self.conan_index.get(path.name, [])
        conan_choice = match_build_id(path, conan_candidates)
        ubuntu_choice = None
        if conan_choice is None:
            ubuntu_candidates = dpkg_candidates(path.name)
            ubuntu_choice = match_build_id(path, ubuntu_candidates)
            if ubuntu_choice is None:
                # Without a matching build ID, the origin is only certain when a single source has this file name.
                if conan_candidates and not ubuntu_candidates:
                    conan_choice = conan_candidates[0]
                elif ubuntu_candidates and not conan_candidates:
                    ubuntu_choice = ubuntu_candidates[0]
                elif conan_candidates:
                    self.errors.append(f"{relative} (found in Conan and Ubuntu packages, build IDs do not match)")
                    return

        if conan_choice is not None:
            dependency, package_file = conan_choice
            if dependency is self.qt:
                self.classify_qt(path, relative, package_file)
            else:
                self.component(
                    "conan:" + dependency["name"],
                    name=dependency["name"],
                    version=dependency["version"],
                    license=dependency.get("license") or "",
                    origin="Conan",
                    source=dependency.get("homepage") or dependency.get("url") or "",
                ).files.append(relative)
            return

        if ubuntu_choice is None and self.classify_local(path, relative):
            return

        if ubuntu_choice is not None:
            package = ubuntu_choice[0]
            if package not in self.ubuntu_packages:
                self.ubuntu_packages[package] = dpkg_info(package)
            info = self.ubuntu_packages[package]
            self.component(
                "ubuntu:" + package,
                name=package,
                version=info["version"],
                license="see ubuntu/" + package + "/copyright",
                origin="Ubuntu package",
                source=f"https://launchpad.net/ubuntu/+source/{info['source_package']}/{info['source_version']}",
            ).files.append(relative)
            return

        self.errors.append(relative)

    def classify_local(self, path: Path, relative: str) -> bool:
        """Libraries built from source into /usr/local by the build image, known by EXTRA_COMPONENTS."""
        extra = extra_component_for(path.name, "local")
        local_file = LOCAL_LIB_DIR / path.name
        if extra is None or not local_file.exists():
            return False
        file_id = build_id(path)
        if file_id is not None and build_id(local_file) != file_id:
            return False
        pkgconfig = LOCAL_LIB_DIR / "pkgconfig" / extra["pkgconfig"]
        version = re.search(r"^Version:\s*(\S+)", pkgconfig.read_text(), re.M) if pkgconfig.is_file() else None
        if version is None or version.group(1) != extra["version"]:
            found = version.group(1) if version else "unknown"
            self.errors.append(f"{relative} ({extra['name']} {found} bundled, vendored notice is for {extra['version']})")
            return True
        self.add_extra(extra, relative, "Build image (/usr/local)")
        return True

    def add_extra(self, extra: dict, relative: str, origin: str):
        self.extra_components.add(extra["id"])
        self.component(
            "extra:" + extra["id"],
            name=extra["name"],
            version=extra["version"],
            license=extra["license"],
            origin=origin,
            source=extra["source"],
        ).files.append(relative)

    def classify_qt(self, path: Path, relative: str, package_file: Path):
        package_relative = package_file.resolve().relative_to(self.qt["package_folder"].resolve()).as_posix()
        if package_relative in self.qt_files:
            self.qt_component(*self.qt_files[package_relative]).files.append(relative)
            return

        extra = extra_component_for(path.name, "qt")
        if extra is None:
            self.errors.append(f"{relative} (Qt package file missing from the Qt SBOM)")
            return
        bundled_major = re.search(r"\.so\.(\d+)", path.name)
        if bundled_major and not extra["version"].startswith(bundled_major.group(1) + "."):
            self.errors.append(f"{relative} (vendored {extra['name']} {extra['version']} notice is outdated)")
            return
        self.add_extra(extra, relative, "Qt installer")

    def qt_component(self, module: str, license_expression) -> Component:
        """The first file of a Qt module sets its license; the SBOM gives the license of every file."""
        self.qt_modules.add(module)
        module_name = module.removesuffix(f"-{self.qt['version']}.spdx.json")
        return self.component(
            "qt:" + module_name,
            name=f"Qt ({module_name})",
            version=self.qt["version"],
            license=license_expression or "",
            origin="Qt",
            source=f"https://download.qt.io/archive/qt/{self.qt_series()}/{self.qt['version']}/submodules/",
        )

    def qt_series(self) -> str:
        return ".".join(self.qt["version"].split(".")[:2])

    def write(self):
        if self.doc_dir.exists():
            shutil.rmtree(self.doc_dir)
        self.doc_dir.mkdir(parents=True)
        shutil.copy(self.source_dir / "LICENSE", self.doc_dir / "LICENSE")
        shutil.copy(self.source_dir / "THIRD_PARTY_NOTICES.md", self.doc_dir / "THIRD_PARTY_NOTICES.md")
        self.write_qt()
        self.write_conan()
        self.write_ubuntu()
        self.write_vendored()
        self.write_components()
        self.write_sources()

    def write_license_texts(self, ids: set, extracted: dict):
        target = self.doc_dir / "licenses"
        target.mkdir(exist_ok=True)
        missing = []
        for license_id in sorted(ids):
            if license_id.startswith("LicenseRef-"):
                if license_id not in extracted:
                    missing.append(license_id)
                    continue
                (target / f"{license_id}.txt").write_text(extracted[license_id] + "\n")
                continue
            text = VENDORED_DIR / "spdx" / f"{license_id}.txt"
            if not text.is_file():
                missing.append(license_id)
                continue
            shutil.copy(text, target / text.name)
        if missing:
            raise LicenseError(
                "License texts missing from infomaniak-build-tools/linux/licenses/spdx: " + ", ".join(missing)
            )

    def write_qt(self):
        target = self.doc_dir / "qt"
        target.mkdir()
        ids = set()
        extracted = {}
        for module in sorted(self.qt_modules):
            sbom_path, sbom = self.qt_sboms[module]
            shutil.copy(sbom_path, target / module)
            for entry in sbom.get("packages", []) + sbom.get("files", []):
                ids |= license_ids(entry.get("licenseConcluded")) | license_ids(entry.get("licenseDeclared"))
            for info in sbom.get("hasExtractedLicensingInfos", []):
                extracted[info["licenseId"]] = info.get("extractedText", "")
        ids |= set(APPIMAGE_RUNTIME["license_ids"])
        self.write_license_texts(ids, extracted)

    def write_conan(self):
        """Every Conan host package: a static library leaves no file in the AppDir but still needs its notice."""
        target = self.doc_dir / "conan"
        missing = []
        for dependency in self.dependencies:
            if dependency is self.qt:
                continue
            folder = dependency["package_folder"]
            licenses = folder / "licenses" if folder else None
            if licenses is None or not licenses.is_dir() or not any(licenses.iterdir()):
                missing.append(f"{dependency['name']}/{dependency['version']}")
                continue
            shutil.copytree(licenses, target / dependency["name"])
            self.component(
                "conan:" + dependency["name"],
                name=dependency["name"],
                version=dependency["version"],
                license=dependency.get("license") or "",
                origin="Conan",
                source=dependency.get("homepage") or dependency.get("url") or "",
            ).note = "no separate file (static library or dependency of another package)"
        if missing:
            raise LicenseError("Conan packages without license files: " + ", ".join(missing))

    def write_ubuntu(self):
        target = self.doc_dir / "ubuntu"
        missing = []
        for package in sorted(self.ubuntu_packages):
            copyright_file = Path("/usr/share/doc") / package / "copyright"
            if not copyright_file.is_file():
                missing.append(package)
                continue
            (target / package).mkdir(parents=True)
            shutil.copy(copyright_file, target / package / "copyright")
        if missing:
            raise LicenseError("Ubuntu packages without copyright file: " + ", ".join(missing))
        if self.ubuntu_packages:
            # Debian copyright files refer to these texts instead of repeating them.
            shutil.copytree(UBUNTU_COMMON_LICENSES, target / "common-licenses")

    def write_vendored(self):
        for component in EXTRA_COMPONENTS:
            if component["id"] in self.extra_components:
                shutil.copytree(VENDORED_DIR / component["license_dir"], self.doc_dir / component["id"])
        shutil.copytree(VENDORED_DIR / APPIMAGE_RUNTIME["license_dir"], self.doc_dir / "appimage-runtime")

    def sorted_components(self) -> list:
        return sorted(self.components.values(), key=lambda c: (c.origin != "Infomaniak", c.origin, c.name))

    def write_components(self):
        lines = [
            "# Bundled components",
            "",
            "kDrive is licensed under the GNU General Public License v3 (`LICENSE`). This AppImage also bundles the",
            "components below. Their license texts are in this folder: `licenses/` (SPDX texts), `qt/` (Qt SBOMs, with",
            "the license of every Qt file and third-party component), `conan/`, `ubuntu/`, and one folder for each",
            "other component. Qt is used under the LGPL v3, except Qt Lottie Animation and Qt Quick Timeline, which",
            "are available under the GPL v3 only.",
            "",
            "| Component | Version | License | Origin | Bundled files |",
            "|---|---|---|---|---|",
        ]
        for component in self.sorted_components():
            files = ", ".join(f"`{f}`" for f in sorted(component.files)) or component.note
            lines.append(
                f"| {component.name} | {component.version} | {component.license} | {component.origin} | {files} |"
            )
        lines += [
            f"| {APPIMAGE_RUNTIME['name']} | | {APPIMAGE_RUNTIME['license']} | AppImage | runtime of the AppImage file |",
            "",
        ]
        (self.doc_dir / "BUNDLED_COMPONENTS.md").write_text("\n".join(lines))

    def write_sources(self):
        try:
            commit = source_commit(self.source_dir)
        except OSError as error:
            raise LicenseError(f"Unable to read the commit of '{self.source_dir}': {error}") from error
        lines = [
            "# Sources",
            "",
            "The source code of this AppImage and of the components it bundles is available at the following",
            "locations.",
            "",
            f"- kDrive: https://github.com/Infomaniak/desktop-kDrive/tree/{commit}",
        ]
        for component in self.sorted_components():
            if component.origin == "Infomaniak":
                continue
            version = f" {component.version}" if component.version else ""
            lines.append(f"- {component.name}{version}: {component.source}")
        lines += [f"- {APPIMAGE_RUNTIME['name']}: {APPIMAGE_RUNTIME['source']}", ""]
        (self.doc_dir / "SOURCES.md").write_text("\n".join(lines))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("--app-dir", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path, help="kdrive_third_party.json written by Conan")
    parser.add_argument("--source-dir", required=True, type=Path)
    arguments = parser.parse_args()

    try:
        collector = Collector(arguments.app_dir, arguments.manifest, arguments.source_dir)
        collector.run()
    except LicenseError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(f"License notices written to '{collector.doc_dir}' for {len(collector.components)} components.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
