---
name: update-translations
description: "Update kDrive app translations: GUI strings (macOS and Windows redesigns) via the Loco platform using the mcp-loco MCP server when available, and server / legacy Qt GUI strings via Qt lupdate from the Conan-managed Qt. The language list comes from the Language enum in src/libcommon/utility/cstypes.h. Every missing translation is filled: unfinished entries in client_*.ts and untranslated Loco assets are translated directly by the agent. Use when asked to update or refresh translations, extract new translatable strings, complete unfinished or missing translations, ensure all supported languages are translated, sync Localizable.strings, Resources.resw or client_*.ts files, or run lupdate."
---

# Update translations

The app has two independent translation pipelines. Determine the scope first: the user may ask for one part only, or
both.

| Part | Where | Files | Tool |
| --- | --- | --- | --- |
| GUI (macOS v4 redesign) | `src/gui4/macOS/` | `src/gui4/macOS/kDriveResources/Localizable/<lang>.lproj/Localizable.strings` (+ `.stringsdict`) | Loco, via the **mcp-loco** MCP server (only if available) |
| GUI (Windows WinUI3 redesign) | `src/gui4/windows/` | `src/gui4/windows/kDrive client/kDrive client/Strings/<locale>/Resources.resw` | Loco, via the **mcp-loco** MCP server (only if available) |
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

**A language added to the enum is automatically supported by this skill.** With no other change to this skill, it gets
the same targets as every other language: a `client_<code>.ts` file, a Loco locale and a GUI export folder (`.lproj` /
`Strings/<locale>/`).

When the user asks to update all translations or to make sure translations are up to date without naming a specific
part or language, run both parts and cover **every language** of the enum:
- Qt: every `client_<code>.ts` must exist and contain no `unfinished` entry; create a missing file by running lupdate
  on it (it generates the skeleton).
- Loco: every locale of the Loco project must have zero untranslated assets. If a language of the enum is missing from
  `list_locales`, report it: the locale itself must be added in the Loco project, which the MCP server cannot do.
- GUI exports: the macOS `.lproj` folders and the Windows `Strings/` folders must exist for every language of the
  enum, like the Qt and Loco targets.

## Part 1 — GUI redesigns: Loco

**If no mcp-loco MCP tool is available, skip this part entirely** and tell the user. Never hand-edit the exported
files: every entry is anchored to a Loco asset ID that only Loco can provide, and the next Loco export would
overwrite local edits.

1. Discover the loco MCP tools available in the session and use them for every Loco operation (list assets, create
   assets, read/write translations, export). Adapt to the actual tool names and parameters offered by the server.
2. Locales — one export per language of the `Language` enum, for both GUIs:
    - macOS: `<code>.lproj/Localizable.strings` (+ `.stringsdict` for plural assets) in
      `src/gui4/macOS/kDriveResources/Localizable/`, for every language of the enum, `en` being the source. Create the
      `.lproj` folder of a language that has none yet, seeded from Loco like the others.
    - Windows: one `Resources.resw` per `Strings/<locale>/` folder in `src/gui4/windows/kDrive client/kDrive
      client/Strings/`, for every language of the enum, using the language's common regional variant (e.g. `fr-FR`,
      `de-DE`, `nb-NO`, `pt-PT`). Create the folder of a language that has none yet.
3. Workflow:
    - Collect the new strings: keys referenced in the GUI code but missing from the source-locale files.
    - Create one Loco asset per new key (English source text), tagged like the existing assets of that platform
      (`macOS` or `windows`).
    - Fill missing translations: `list_locales` shows per-locale untranslated counts, `list_assets` finds the assets
      with `progress.untranslated > 0`, and `get_translations` on each of those assets reveals exactly which locales
      are missing. For every missing locale, write the translation directly into Loco with `update_translation`,
      following the translation rules of Part 2. Cover every locale of the Loco project: all languages of the enum
      plus English.
    - Send Loco write calls one at a time: Loco rate-limits concurrent requests (HTTP 429), so never batch several
      write calls in parallel.
    - Retrieve the per-locale translations from Loco and refresh the exported files for every locale, preserving the
      Loco export header and the `loco:<id>` comment above each entry.
    - Never rename, reorder or delete existing loco IDs.
4. Report which keys were added, which translations were filled, and which locale files were refreshed.

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

- Check `git diff --stat`: only `translations/*.ts` and/or Loco export files under `src/gui4/` must change.
- Verify the `.ts` files are still well-formed XML:
  ```bash
  python3 -c "import glob, xml.dom.minidom; [xml.dom.minidom.parse(f) for f in glob.glob('translations/client_*.ts')]"
  ```
- Verify no unfinished entry remains (this must print nothing):
  ```bash
  grep -l 'type="unfinished"' translations/client_*.ts
  ```
- Loco: re-run `list_locales` and confirm every locale reports `untranslated: 0` when the Loco part ran. Every
  language of the `Language` enum must be covered.
- Never commit or push unless the user explicitly asks.

## Error handling

- `lupdate` not found: say so, offer to run the Conan dependency build; do not fall back to a system Qt that may not
  exist.
- mcp-loco unavailable: do the Qt part, skip the GUI parts, and report exactly what was skipped and why.
- Loco writes rejected with 403 `Read-only key disallows POST`: the configured API key is read-only, so no Loco write
  is possible. Report the missing translations and ask the user for a full-access key; never hand-edit the exported
  files instead.
- Loco 429 `Too many simultaneous requests`: write calls were sent in parallel; retry them sequentially.
- `lupdate` warnings on generated or third-party sources: keep the `.ts` update and list the warnings in the report.
- `.strings` or `.resw` exports rejected by Loco (asset not found, bad tag): report the failing asset ID, never
  silently drop entries.
