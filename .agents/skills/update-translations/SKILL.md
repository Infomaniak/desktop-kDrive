---
name: update-translations
description: "Update kDrive app translations: GUI strings (macOS and Windows redesigns) are imported from the Loco platform with Infomaniak's import_loco CLI (one config per GUI is already committed in the repo); the mcp-loco MCP server stays available for Loco-side inspection and fixes. Server / legacy Qt GUI strings are refreshed with Qt lupdate from the Conan-managed Qt, and every unfinished entry is translated directly by the agent. The language list comes from the Language enum in src/libcommon/utility/cstypes.h. Use when asked to update or refresh translations, extract new translatable strings, complete unfinished or missing translations, ensure all supported languages are translated, sync Localizable.strings, Resources.resw or client_*.ts files, or run lupdate."
---

# Update translations

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

## Language list

The authoritative list of supported languages is the `Language` enum in `src/libcommon/utility/cstypes.h` (ignore
`Default` and `EnumEnd`). English is the source language. Read the enum at the start of every run and derive each
language's code with `CommonUtility::languageCode()` in `src/libcommon/utility/utility.cpp`; do not rely on a
hardcoded list. The current mapping is French `fr`, German `de`, Spanish `es`, Italian `it`, Dutch `nl`, Swedish `sv`,
Portuguese `pt`, Polish `pl`, Norwegian `nb`, Finnish `fi`, Danish `da`, Greek `el`.

**A language added to the enum is not automatically supported end-to-end.** Create its `client_<code>.ts` target and
Loco locale, then verify that the pinned `import_loco` release supports its Windows language-to-country mapping before
adding it to the GUI import configs (see Part 1 for the exact changes).

When the user asks to update all translations or to make sure translations are up to date without naming a specific
part or language, run both parts and cover **every language** of the enum:
- Qt: every `client_<code>.ts` must exist and contain no `unfinished` entry; create a missing file by running lupdate
  on it (it generates the skeleton).
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

1. Import (quote the Windows path, it contains spaces):
   ```bash
   import_loco                 # import every resource type of the platform
   import_loco -r strings      # macOS only: just Localizable.strings
   import_loco --check         # validate the local files, import nothing
   ```
   `import_loco` rewrites the exported files wholesale, preserving the Loco export header and the `loco:<id>` comment
   above each entry. Never hand-edit these files: every entry is anchored to a Loco asset ID that only Loco provides,
   and the next import would overwrite local edits. Never rename, reorder or delete existing loco IDs.
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

1. Locate `lupdate` from the Conan Qt package (it is usually not in `PATH`):
   ```bash
   find ~/.conan2 -path "*/p/bin/lupdate" -type f 2>/dev/null | head -n 1
   ```
   If the Conan cache is empty, install the dependencies first with
   `infomaniak-build-tools/conan/build_dependencies.sh Debug`, then search again.
2. Update every checked-in Qt translation file (run from the repository root):
   ```bash
   LUPDATE="$(find ~/.conan2 -path "*/p/bin/lupdate" -type f 2>/dev/null | head -n 1)"
   for f in translations/client_*.ts; do "$LUPDATE" src -no-obsolete -ts "$f"; done
   ```
    - `src` is scanned recursively and covers the server, the legacy Qt GUI and the common libraries.
    - `-no-obsolete` drops entries for removed strings, matching `translations/updateTool/update-translation-files.sh`.
    - New strings appear as `<translation type="unfinished">`.
    - A run with no new string may still refresh the `<location>` line numbers: that diff is expected and harmless.
3. Translate every `unfinished` entry, in every `client_*.ts`, directly in the file (no DeepL, no external service):
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
  `.import_loco.yml` files when changing language coverage may change.
- After a GUI import, run `import_loco --check` in each GUI directory: it catches straight apostrophes, `...`
  ellipses, trailing spaces and language-specific punctuation before they ship.
- Verify the `.ts` files are still well-formed XML:
  ```bash
  python3 -c "import glob, xml.dom.minidom; [xml.dom.minidom.parse(f) for f in glob.glob('translations/client_*.ts')]"
  ```
- Verify no unfinished entry remains (this must print nothing):
  ```bash
  grep -l 'type="unfinished"' translations/client_*.ts
  ```
- Loco: re-run `list_locales` (needs mcp-loco) and confirm every locale reports `untranslated: 0` when the Loco part
  ran. Every language of the `Language` enum must be covered.
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
