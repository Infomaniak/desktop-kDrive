# Vulnerability scan exceptions

Exceptions require component-specific evidence. Do not ignore a package, severity,
or vulnerability globally to make the security gate pass.

## macOS: SQLite CLI `.recover` command injection

- **Advisory:** [CVE-2026-119586](https://nvd.nist.gov/vuln/detail/CVE-2026-119586)
- **Reviewed:** 2026-11-19
- **Decision:** Not affected; the vulnerable command-line component is not shipped.
- **Scope:** Only the macOS Conan-graph finding for `sqlite3` **3.54.1**, package type
  `conan`, vulnerability namespace `nvd:cpe`.
- **Policy:** [`grype-macos.yaml`](grype-macos.yaml), explicitly passed only to the
  macOS Conan dependency scan in [`vulnerability-scan.yml`](../workflows/vulnerability-scan.yml).

### Evidence

The advisory concerns SQLite's **command-line shell**: its `.recover` command can
emit attacker-controlled dot-commands from a crafted database. The reported
execution scenario pipes that output back into the `sqlite3` shell.

kDrive links the SQLite library. Its macOS requirement in
[`conanfile.py`](../../conanfile.py) sets `build_executable=False`, so Conan does not
build the SQLite shell. The macOS application SBOM from
[scan run 44537109645](https://github.com/Infomaniak/desktop-kDrive/actions/runs/44537109645)
contains no `sqlite3` command-line executable. A source search also found no use
of `.recover`.

The Conan graph identifies the SQLite product/version without distinguishing the
library from the absent shell, producing this non-applicable finding.

### Limits and review conditions

- No other SQLite CVE is ignored, and this exception does not match other versions
  or package types. Review it when changing SQLite's version or build options.
- Remove or reassess the exception if kDrive starts building, bundling or invoking
  the SQLite shell, or if the advisory expands to affect library code.
- The application-binary scan remains unfiltered by this policy. Windows and Linux
  scans, and all severity thresholds, are unchanged.
- Grype records the ignored match and its reason in the macOS Conan scan's JSON
  artifact, `macos-conan-deps-grype.json`. The SQLite component remains in the SBOM.

## macOS: Qt Image Formats ICNS reader crash

- **Advisory:** [CVE-2025-5683](https://nvd.nist.gov/vuln/detail/CVE-2025-5683)
- **Upstream fix:** [Qt 6.8 security patch](https://download.qt.io/archive/qt/6.8/CVE-2025-5683-qtimageformats-6.8.patch)
- **Reviewed:** 2026-11-19
- **Decision:** Not affected in the packaged macOS application; the vulnerable
  image-format plugin is not shipped.
- **Scope:** Only the macOS Conan-graph finding for `qt` **6.8.3**, package type
  `conan`, vulnerability namespace `nvd:cpe`.
- **Policy:** [`grype-macos.yaml`](grype-macos.yaml), explicitly passed only to the
  macOS Conan dependency scan.

### Evidence

The upstream fix changes `QICNSHandler` in
`qtimageformats/src/plugins/imageformats/icns/qicnshandler.cpp`. This handler is
provided by the separate ICNS image plugin, `libqicns.dylib`, not by Qt's general
image-loading API or Apple's AppKit ICNS decoder.

The macOS Qt installer selection in
[`recipes/qt/all/conanfile.py`](../../infomaniak-build-tools/conan/recipes/qt/all/conanfile.py)
requests the base Qt package, Qt Positioning and Qt 5 Compatibility, but not the
Qt Image Formats add-on. The installed Qt 6.8.3 package contains no ICNS plugin.

The application SBOM from
[scan run 44540879761](https://github.com/Infomaniak/desktop-kDrive/actions/runs/44540879761)
lists these image plugins under `Contents/PlugIns/imageformats`:

- `libqgif.dylib`
- `libqico.dylib`
- `libqjpeg.dylib`
- `libqsvg.dylib`

No `qicns` plugin is present in that SBOM or in the local release bundle. A local
runtime check using the bundled Qt frameworks and plugin directory confirmed
that `QImageReader::supportedImageFormats()` excludes `icns`. PNG decoding passed;
reading the application's own valid `.icns` resource failed with
`Unsupported image format`.

This is **not** an assertion that kDrive never decodes images with Qt.
[`ExtensionJob::commandSetThumbnail`](../../src/server/comm/extensionjob.cpp)
passes thumbnail responses to `QPixmap::loadFromData()` without an explicit
format, and [`CommonUtility::toQImage`](../../src/libcommon/utility/utility.cpp)
also uses automatic format detection. These APIs would warrant reassessment if
the affected plugin became available. Bundled `.icns` application and Finder
icons do not themselves establish use of Qt's ICNS handler.

### Limits and review conditions

- Before applying the policy, the macOS scan checks the actual app bundle and
  fails if a `qicns` plugin is present. A missing app directory also fails the check.
- Reassess the exception when changing the Qt version, installer add-ons, plugin
  deployment or plugin search paths, or if Qt gains a built-in/static ICNS reader.
- Remove the exception if the vulnerable ICNS reader is shipped or enabled.
  Upgrade to a fixed Qt version or rebuild the affected plugin with the upstream
  patch instead of suppressing an applicable vulnerability.
- Other Qt CVEs, versions, package types and platform scans remain reportable.
  The application-binary scan does not use this exception policy.
- The ignored Conan finding and its reason remain in
  `macos-conan-deps-grype.json`; Qt remains in the dependency SBOM.
