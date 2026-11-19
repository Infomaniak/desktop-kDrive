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
