---
name: update-translations
description: "Update kDrive app translations: GUI strings (macOS and Windows redesigns) are imported from the Loco platform with Infomaniak's import_loco CLI (one config per GUI is already committed in the repo); the mcp-loco MCP server stays available for Loco-side inspection and fixes. Server / legacy Qt GUI strings are refreshed with Qt lupdate from the Conan-managed Qt. By default the skill is scoped to the current PR: lupdate runs over every file, but only the strings the PR adds or changes are translated or imported, and out-of-scope unfinished entries are left unfinished; translating every entry runs only when explicitly requested. This skill is never triggered automatically: invoke it only when the user explicitly asks to use the translation skill (e.g. \"utilise le skill de traduction\" or \"use the update-translations skill\"). Use when the user explicitly requests it: update or refresh translations, extract new translatable strings, complete unfinished or missing translations, ensure all supported languages are translated, sync Localizable.strings, Resources.resw or client_*.ts files, or run lupdate."
---

# Update translations

## Invocation: explicit only

**Never trigger this skill automatically.** Run it only when the user explicitly asks to use the translation skill —
e.g. "utilise le skill de traduction", "use the update-translations skill". A PR that touches translation files, a
noticed missing or `unfinished` translation, or a generic "update the translations" request is **not** an explicit
invocation: in those cases, do not run the skill (mention it is available if useful, then stop).

The app has two independent translation pipelines. Determine the scope first: the user may ask for one part only, or
both.

| Part | Where | Files | Tool |
| --- | --- | --- | --- |
| GUI (macOS v4 redesign) | `src/gui4/macOS/` | `src/gui4/macOS/kDriveResources/Localizable/<lang>.lproj/Localizable.strings` (+ `.stringsdict`) | **import_loco** ([Infomaniak/importloco](https://github.com/Infomaniak/importloco)); mcp-loco MCP server only for Loco-side inspection and fixes |
| GUI (Windows WinUI3 redesign) | `src/gui4/windows/` | `src/gui4/windows/kDrive client/kDrive client/Strings/<locale>/Resources.resw` | **import_loco**; mcp-loco MCP server only for Loco-side inspection and fixes |
| Server + legacy Qt GUI | `src/server/`, `src/gui/`, common libs | `translations/client_<lang>.ts` | **lupdate** from the Conan-managed Qt |

Do not mix the pipelines: Loco owns both GUI redesigns, Qt tooling owns the C++ strings. All Loco exports carry a
header comment (`Loco ios export` / `Loco xml export`, project `kDrive Desktop`) and per-entry Loco asset IDs
(`/* loco:<id> */` in `.strings`); macOS strings are tagged `macOS`, Windows strings are tagged `windows`.

## Scope: the current PR only

**Default: modify only the translations related to the current PR.** The Qt `lupdate` pass itself always runs over
every file (it is a mechanical refresh, see Part 2), but only the strings of the current PR are translated:
out-of-scope `unfinished` entries stay unfinished. Full Loco imports and full translation passes run **only when the
user explicitly asks** for them — e.g. "update all translations", "translate everything", "make sure all languages
are translated". Any request about the PR, its new strings or its changed files means the scoped run.

Determine the scope from the diff against the base branch (usually `develop`):

```bash
BASE="$(git merge-base HEAD develop)"
git diff --name-only "$BASE...HEAD"   # committed
{
    git diff --name-only "$BASE"
    git ls-files --others --exclude-standard
} | sort -u                           # committed + tracked/untracked worktree changes
```

| Diff contains | Work to do |
| --- | --- |
| New/changed user-facing keys in GUI code under `src/gui4/macOS/` or `src/gui4/windows/` | Part 1, scoped |
| `Localizable.strings` / `Resources.resw` export files, `.import_loco.yml` | Part 1, scoped (config changes only if the PR itself changes language coverage) |
| C++ sources under `src/` with new/changed user-facing strings (`tr()`, …) | Part 2: full lupdate pass, translation scoped to the PR |
| `translations/client_*.ts` | Part 2: full lupdate pass, translation scoped to the PR (translation-only PRs included) |
| Nothing that adds or changes user-facing strings | Nothing: report that there is no translation work for this PR and stop |

In a scoped run: never run the full Loco import over every GUI, and never translate out-of-scope `unfinished` entries
or Loco gaps unrelated to the PR — report them instead. The language rules below
still apply to whatever the PR does touch (a new key must exist in every locale, for example).

## Language list

The authoritative list of supported languages is the `Language` enum in `src/libcommon/utility/cstypes.h` (ignore
`Default` and `EnumEnd`). English is the source language. Read the enum at the start of every run and derive each
language's code with `CommonUtility::languageCode()` in `src/libcommon/utility/utility.cpp`; do not rely on a
hardcoded list. The current mapping is French `fr`, German `de`, Spanish `es`, Italian `it`, Dutch `nl`, Swedish `sv`,
Portuguese `pt`, Polish `pl`, Norwegian `nb`, Finnish `fi`, Danish `da`, Greek `el`.

**A language added to the enum is not automatically supported end-to-end.** Create its `client_<code>.ts` target and
Loco locale, then verify that the pinned `import_loco` release supports its Windows language-to-country mapping before
adding it to the GUI import configs (see Part 1 for the exact changes).

**Scoped-run invariant (default).** Every string the PR adds or changes must be translated in every language of the
pipelines the PR touches (each `client_<code>.ts` for Part 2, every Loco locale for Part 1). Nothing else is
required: out-of-scope `unfinished` entries and untranslated Loco assets stay as they are and are reported, not
fixed — a scoped run is not expected to end with zero unfinished work.

**Full-run invariants (only when the user explicitly asks for a global refresh — "update all translations", "make
sure every language is translated", or similar):**
- Qt: for every non-English language, `client_<code>.ts` must exist and contain no `unfinished` entry; create a missing
  file by running lupdate on it (it generates the skeleton). English is the source language and must not get a
  `client_en.ts` file.
- Loco: every locale of the Loco project must have zero untranslated assets. If a language of the enum is missing from
  `list_locales`, report it: the locale itself must be added in the Loco project, which the MCP server cannot do.
- GUI imports: each GUI imports the languages of its own `.import_loco.yml` config (see Part 1): the Windows config
  lists every language of the enum, the macOS config uses the importloco default (today `de, en, es, fr, it`, the
  locales the macOS app ships). When the enum grows, extend the configs as described in Part 1.

## Part 1 — GUI redesigns: Loco via import_loco

GUI strings are **imported from Loco** with [`import_loco`](https://github.com/Infomaniak/importloco), Infomaniak's
import CLI. One config per GUI is already committed to the repository; run `import_loco` from the config's directory:

| GUI | Config directory | Imported files | Languages |
| --- | --- | --- | --- |
| macOS | `src/gui4/macOS/` | `kDriveResources/Localizable/<code>.lproj/Localizable.strings` (+ `.stringsdict`) | config default: `de, en, es, fr, it` |
| Windows | `src/gui4/windows/kDrive client/kDrive client/` | `Strings/<locale>/Resources.resw` | every language of the `Language` enum, listed in the config |

Setup:
- Install: `pipx install pipx:infomaniak/importloco` (the macOS project pins the version in `src/gui4/macOS/mise.toml`).
- API key: export `LOCO_API_KEY`, or write the key into a `.import_loco_api` file (gitignored). A read-only key is
  enough: importing only reads from Loco.

### Scoped run (default)

Work only on the keys the PR adds or changes:

1. List the user-facing keys from the GUI diff: new string literals, and keys whose English text changed.
2. Map each key to its Loco asset: reuse the `/* loco:<id> */` comment already present in the export files for
   existing keys; find or create the asset for new keys (mcp-loco `list_assets` / `create_asset`), tagged like the
   existing assets of that platform (`macOS` or `windows`).
3. For each of these assets: write the missing translations **in Loco** with `update_translation`, one call at a time
   (rules of Part 2). If the PR changed an asset's English source text, update the source text in Loco first
   (`update_asset` or `update_translation` on the source locale), then refresh the other locales.
4. Run `import_loco` from the config directory of each GUI the PR touches (quote the Windows path).
5. Review `git diff` on the imported files: keep only the PR's entries. If the import brought unrelated entries
   (Loco drift since the last import), revert the files that contain no PR key and report the drift; if a file mixes
   PR keys with unrelated changes, tell the user before keeping it.

Exception: if the PR itself adds a language to the `Language` enum or changes an `.import_loco.yml`, the config and
Loco-locale work of full-run step 3 is part of the PR scope.

### Full run (only when explicitly requested)

1. Import (quote the Windows path, it contains spaces):
   ```bash
   import_loco                 # import every resource type of the platform
   import_loco -r strings      # macOS only: just Localizable.strings
   import_loco --check         # validate the local files, import nothing
   ```
   `import_loco` rewrites the exported files wholesale: each entry keeps its `loco:<id>` comment and `.resw` files
   get a fresh export header. The manual web-export header and the extended asset notes are not carried over. Never
   hand-edit these files: every entry is anchored to a Loco asset ID that only Loco provides, and the next import
   would overwrite local edits. Never rename, reorder or delete existing loco IDs.
2. The **mcp-loco MCP server remains available if needed**; it is the only way this skill can inspect or change Loco
   itself, and writing requires a full-access key (a read-only key answers 403 on any write):
    - Inspect: `list_locales` shows per-locale `untranslated` counts; `list_assets` and `get_translations` identify
      exactly which assets are missing in which locale.
    - Fill missing translations **in Loco**, never in the exported files: write each one with mcp-loco
      `update_translation`, following the translation rules of Part 2, then re-run `import_loco` to refresh the local
      files. Send Loco write calls one at a time: Loco rate-limits concurrent requests (HTTP 429).
    - New keys used by the GUI code but absent from Loco must first be created as Loco assets (Loco dashboard, or
      mcp-loco `create_asset` when available) with the English source text, tagged like the existing assets of that
      platform (`macOS` or `windows`), then translated and imported.
3. When a language is added to the `Language` enum: add its code to the `languages` list of the Windows config, and
   add a `languages` key to the macOS config if the macOS app is to ship that language; the Loco locale itself must
   exist (create it in the Loco dashboard, which the MCP server cannot do).
4. Report, per GUI, what was imported, which translations were still missing in Loco, and what (if anything) was
   written to Loco.

## Part 2 — Server + legacy Qt GUI: lupdate

Locate `lupdate` from the Conan Qt package (it is usually not in `PATH`):

```bash
find ~/.conan2 -path "*/p/bin/lupdate" -type f 2>/dev/null | head -n 1
```

If the Conan cache is empty, install the dependencies first with
`infomaniak-build-tools/conan/build_dependencies.sh Debug`, then search again.

### lupdate pass (always)

Update every checked-in Qt translation file (run from the repository root). Do this **whenever the skill runs**,
whichever files the PR touches, including PRs that only change `translations/client_*.ts`:
```bash
LUPDATE="$(find ~/.conan2 -path "*/p/bin/lupdate" -type f 2>/dev/null | head -n 1)"
for f in translations/client_*.ts; do "$LUPDATE" src -no-obsolete -ts "$f"; done
```
- `src` is scanned recursively and covers the server, the legacy Qt GUI and the common libraries.
- `-no-obsolete` drops entries for removed strings, matching `translations/updateTool/update-translation-files.sh`.
- New strings appear as `<translation type="unfinished">`.
- The pass refreshes the `<location>` line numbers in every file: that mechanical diff is expected and harmless, and
  is not limited to the PR's files.

### Translation scope (default: the PR only)

By default, translate only the `unfinished` entries that belong to the current PR, directly in the file (no DeepL, no
external service): entries whose `<location>` file is changed by the PR and whose line falls inside one of the PR's
diff hunks for that file (the string is new, or its English text was changed).

**Leave every other `unfinished` entry exactly as it is** — pre-existing gaps, entries from files the PR does not
touch, strings from other in-flight work — and list them in the report. Do not clean them up: a scoped run is not
expected to end with zero `unfinished` entries, and that is the correct outcome. Translate *every*
`unfinished` entry, in every `client_*.ts`, only when the user explicitly asks for a full translation pass.

Translation rules (apply to whichever entries are translated):
- The target language is the file name: `client_da.ts` = Danish, `de` = German, `el` = Greek, `es` = Spanish,
  `fi` = Finnish, `fr` = French, `it` = Italian, `nb` = Norwegian Bokmål, `nl` = Dutch, `pl` = Polish,
  `pt` = Portuguese, `sv` = Swedish.
- Translate the `<source>` text, write it into the `<translation>` element and remove `type="unfinished"` so the
  entry is marked as finished.
- For plural entries, translate each `<numerusform>` and keep the number of forms lupdate generated.
- Keep placeholders exactly as-is (`%1`, `%2`, `%n`, `%Ln`), HTML/XML tags, `&` mnemonics and leading/trailing
  spaces; never translate format specifiers, paths or identifiers.
- Match the tone and terminology of the existing translations in the same file, and keep UI labels concise.
- When writing Loco translations for a language, check `client_<code>.ts` first for established terminology and
  reuse it (for example Dutch uses the formal `uw`, Portuguese uses European Portuguese `ficheiros`).
- A later Transifex sync (`tx pull`, see `translations/Makefile`) may supersede these local translations.

## Validation

- Check `git diff --stat`: only `translations/*.ts`, Loco export files under `src/gui4/`, and the relevant
  `.import_loco.yml` files when changing language coverage may change. In a scoped run, every *translation* hunk (a
  translated text, a newly finished entry) must trace back to a string the PR touches; the mechanical `lupdate`
  churn in other `client_*.ts` (`<location>` line numbers, dropped obsolete entries) is expected from the full pass
  and is kept. Revert anything else and say so.
- After a GUI import, run `import_loco --check` in each GUI directory: it catches straight apostrophes, `...`
  ellipses, trailing spaces and language-specific punctuation before they ship. It can produce false positives on
  strings that contain file paths with colons (e.g. `D:/`), which the French space-before-colon rule flags; verify
  the string before reporting a real error.
- Verify the `.ts` files are still well-formed XML:
  ```bash
  python3 -c "import glob, xml.dom.minidom; [xml.dom.minidom.parse(f) for f in glob.glob('translations/client_*.ts')]"
  ```
- Unfinished entries, scoped run (default): non-empty output of the check below is expected, because out-of-scope gaps
  stay unfinished by design. List their sources and check that none belongs to a string of the PR (the rest is
  reported, not fixed):
  ```bash
  grep -l 'type="unfinished"' translations/client_*.ts
  grep -B3 'type="unfinished"' translations/client_*.ts | grep -o 'filename="[^"]*"' | sort -u
  ```
- Unfinished entries, full run (translate everything): the check below must print nothing.
  ```bash
  grep -l 'type="unfinished"' translations/client_*.ts
  ```
- Loco, scoped run (default): check only the PR's assets — `get_translations` (needs mcp-loco) on each of them must
  return a text for every locale. Untranslated assets unrelated to the PR are reported, not fixed.
- Loco, full run: re-run `list_locales` (needs mcp-loco) and confirm every locale reports `untranslated: 0`. Every
  language of the `Language` enum must be covered.
- Never commit or push unless the user explicitly asks.

## Error handling

- `lupdate` not found: say so, offer to run the Conan dependency build; do not fall back to a system Qt that may not
  exist.
- `import_loco` not installed: install it with pipx (`pipx install pipx:infomaniak/importloco`); report if that
  fails.
- API key missing for `import_loco`: set `LOCO_API_KEY` or create the `.import_loco_api` file; never inline the key
  in a command output or commit it.
- mcp-loco unavailable: the GUI import still runs (`import_loco` needs no MCP server); only the Loco-side inspection
  and fixes are skipped. Report exactly what was skipped and why.
- Loco writes rejected with 403 `Read-only key disallows POST`: the mcp-loco API key is read-only, so nothing can be
  written to Loco (imports keep working, they only read). Report the missing translations and ask the user for a
  full-access key; never hand-edit the exported files instead.
- Loco 429 `Too many simultaneous requests`: write calls were sent in parallel; retry them sequentially.
- `lupdate` warnings on generated or third-party sources: keep the `.ts` update and list the warnings in the report.
- `import_loco` API errors (asset not found, bad tag, HTTP errors): report the failing asset ID or request, never
  silently drop entries.
