---
name: pr-description-writer
description: "Generate a pull request title and a bilingual (French and English) description in a PR.md file, based on the git diff between the current branch and its base branch (develop by default). Use when the user asks to create, write, generate or update a PR description, a PR title or a PR.md file."
---

# PR description writer

Analyze the git diff of the current branch and write a `PR.md` file at the repository root, containing the PR title and
a description with a French section and an English section.

## Language rules

- The PR title is always in **English** and follows the Conventional Commits format.
- `PR.md` contains two language sections: French inside `<details>`, English after the `---` separator. Both sections
  are complete and say the same thing.
- **French orthography is mandatory**: use every diacritical mark (é, è, ê, ë, à, â, î, ï, ô, û, ù, ü, ç, œ, æ). Never
  write ASCII substitutes: "généré" not "genere", "désormais" not "desormais", "nécessaire" not "necessaire".

## Workflow

1. **Determine the base branch.**
    - Default: `develop`, unless the user names another branch.
    - If a PR already exists, read its real base first: `gh pr view --json baseRefName,title,body,headRefName`.

2. **Get the current branch**: `git rev-parse --abbrev-ref HEAD`. Use its name to understand the intent (`fix/...`,
   `feat/...`).

3. **Get the diff.**
    - Committed work: `git diff origin/<base>...HEAD` (add `--stat` when the diff is large).
    - Run `git status` too. Work is sometimes staged or uncommitted, or a commit is being reverted in the index. If the
      user says the changes are not committed yet, or if the index changes what the PR will contain, use
      `git diff origin/<base>` (working tree) instead, and say so in your answer.
    - If the branch is not pushed yet (`origin/<branch>` missing), fall back to `HEAD` and warn the user.

4. **Read the real content.** Never infer the content of the PR from commit titles: several commits can share the same
   message while containing unrelated changes. Read the full diff, and the per-file diff of the substantial files.

5. **Use the context given by the user.** Motivation, related issue, logs, analysis documents: integrate them. When the
   user points to a local analysis or crash report, read it in full and use it for the "why" (root cause, call sequence,
   why each part of the fix is needed, rejected alternatives). If that file is untracked, never cite or link it as a
   repository file: fold its content into Technical Details, Testing or Notes.

6. **Analyze and synthesize**:
    - what changed (features, fixes, refactoring);
    - why it changed (problem solved, motivation);
    - how it changed (key technical decisions);
    - breaking changes, new dependencies, migration steps.

7. **Write `PR.md`** with the format below, then confirm to the user that the file was written, with the title and a
   short summary of the content.

## Accuracy rules

- Only state verified facts. Clearly separate what was **observed** (in logs, tests, reproduction) from what is only
  **possible** (a risk, a potential consequence).
- Do not describe an issue as systematic if it is intermittent, and do not claim a fix was validated if no build or test
  was run. Say explicitly what was not tested.
- Do not invent an explanation for a mechanism that was not identified: say it was not pinpointed.

## Output format

Create `PR.md` with exactly this structure:

````markdown
```
<PR title: concise, imperative mood, max ~72 characters>
```

```
<details>
<summary>:information_source: Description en français</summary>

## Résumé
<1 à 3 phrases : ce que fait la PR et pourquoi>

## Changements
- <changement clé>
- <autre changement clé>

## Détails techniques
<Optionnel : décisions d'implémentation non évidentes, notes d'architecture>

## Notes
<Optionnel : changements cassants, migration, limites connues, suites prévues>

</details>

---

## Summary
<1 to 3 sentences: what this PR does and why>

## Changes
- <key change>
- <another key change>

## Technical Details
<Optional: non-obvious implementation decisions, architecture notes>

## Notes
<Optional: breaking changes, migration steps, known limitations, follow-ups>
```
````

### Formatting rules

- Exactly **two fenced code blocks**, with plain triple backticks and no language specifier.
- The **first block** contains only the title, on a single line, with no markdown inside.
- The **second block** contains the whole description: the French `<details>` section, then `---`, then the English
  description. Both languages go in this same block.
- A log excerpt inside the description cannot use nested triple backticks: indent it with 4 spaces instead.
- Never use em dashes or en dashes. For ranges, write "to" (e.g. "s2 to s64"); otherwise use a simple hyphen or
  rephrase.
- Omit the optional sections (Détails techniques / Technical Details, Tests / Testing, Notes) consistently in both
  languages when there is nothing meaningful to say.
- The title **must** follow Conventional Commits and match:
  ```
  ^(Merge .+|((feat|fix|chore|docs|style|refactor|revert|perf|ci|test)(\(.+\))?!?: [A-Za-z0-9].+[^.\s])$)
  ```
  Examples: `feat(auth): add OAuth login support for Linux`, `fix(sync): prevent crash when drive is unavailable`,
  `chore: update Qt to 6.8.3`.

### Stacked PRs

When the PR depends on another open PR, add a `Depends on #<number>` line in Notes, with the **real** PR number. Find it
with:

```
gh pr list --head <parent-branch> --state all --json number,title,baseRefName,state
```

## Quality standards

- Be specific: name files, classes, functions and components when relevant.
- Avoid vague wording such as "various fixes" or "some improvements".
- Focus on the **what** and the **why**, do not paraphrase the diff line by line.
- For deep technical fixes (concurrency, object lifetime, crashes), a substantial Technical Details section that
  reconstructs the failure sequence and justifies each part of the fix is welcome.
- If the diff is too large to analyze fully, prioritize the most impactful changes.
- Do not leak internal or private information (personal paths, tokens, customer data) that should not appear in a public
  PR.

## Error handling

- Empty `git diff`: tell the user there is no difference between the current branch and the base branch.
- Base branch missing locally: try `origin/<base>` and tell the user.
- Not a git repository, or git unavailable: report the error clearly.
- `PR.md` is a working file: do not commit it.
