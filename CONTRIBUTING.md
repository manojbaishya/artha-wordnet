# Contributing to Artha

Thank you for contributing! This document explains how we track work, name issues and PRs, and move work through the team board.

## Issue types

We use GitHub Issues as the unit of work, typed by label. Labels sit in this order:

| Label | Meaning | Example |
|-------|---------|---------|
| `epic` | Top-level body of work spanning multiple sprints; parents features/user-stories | `Artha Go-Live` (#7) |
| `feature` | Large capability, spans sprints; child of an epic | `App Modernization` (#3) |
| `user-story` | End-user-facing capability, sized for one sprint | "As a user, I want to search for synonyms" (#6) |
| `bug` | Something broken; regression or defect | "History writes `0x01` instead of the term" |
| `task` | Engineering work not directly user-facing | "Migrate TreeView to GtkListView" (#4, #5) |
| `sub-task` | Step under a parent task/user-story | "Remove `can-focus` from gui.glade containers" |

Issue hierarchy (example): #7 `epic: Artha Go-Live` → #3 `feature: App Modernization` → #4/#5 `task` (closed) + #6 `user-story: test - Test Strategy` (open).

## Work status (Project board)

Status is tracked as a single-select custom field on the GitHub Project board:

| Status | Meaning |
|--------|---------|
| `Not Planned` | Idea captured, not yet committed to a sprint |
| `Selected` | Picked for the current sprint, not started |
| `In Progress` | Actively being worked on (a PR is open) |
| `In Review` | PR opened, awaiting review / CI |
| `In QA` | Reviewed, being verified on the host / nested Xephyr |
| `Done` | Merged to `main` and released |

Only one issue/PR should be `In Progress` per person at a time.

## PR naming convention

PR titles follow `<TYPE>: <SCOPE> - <TITLE>`:

```
<type>: <scope> - <title>
```

`type ∈ {epic, feature, user-story, bug, task, sub-task}`. `scope` is a noun giving context to the affected codebase part (e.g. `build`, `gtk4`, `packaging`, `ci`, `test`). `title` is a concise summary of what changed.

Example: `task: build - self-contained pinned xmake via versioned cache`

When a PR resolves an issue, include `closes #<issue-number>` or `fixes #<issue-number>` in the PR body so GitHub auto-closes the issue on merge.

## Linking PRs to issues (JOIN key + auto-close)

`.github/workflows/pr-issue-link.yml` runs on every PR open/reopen/sync/close:

- It scans the PR title/body for a JOIN key (`closes`, `fixes`, or `join`, case-insensitive, e.g. `Closes #12`, `Fixes #12`, `Join: #12`) and comments on each linked issue with a pointer back to the PR.
- When the PR is merged, every linked issue is closed automatically.

So a PR body like `closes #10` both links the PR to issue #10 *and* closes #10 once merged — no manual linkage needed.

## Build (Taskfile)

`Taskfile.yml` is the single entry point for build/install/run. It is self-contained in the maven/uv/pnpm sense:

- xmake is pinned to the versioned reference in `deps.json` (3.1.2) and reused from a shared versioned user cache at `~/.artha-cache/xmake-3.1.2/`. Populate it once with `task bootstrap-xmake`; every build task reuses it via `xmake-ensure`. Nothing is installed to `/usr/local` or `~/.config/artha.conf`.
- The C compiler/linker and system libraries (gtk4, glib, wordnet, X11) remain platform dependencies located by pkg-config's built-in search path — no distro-specific `PKG_CONFIG_PATH` is hardcoded.
- On hosts where linuxbrew shadows the system toolchain, ensure `/usr/bin` precedes it on `PATH`, then run `task <recipe>` (the host toolchain must win).

Recipes: `build`, `install`, `run`, `debug`, `rebuild`, `clean`, `default`. Artifacts stay under the gitignored `sandbox/`; `task install` stages into `sandbox/stage` (no system changes).

## Submitting a PR

1. Create an issue (User Story / Bug / Task) and note its number.
2. Create a branch named `<type>-<short-slug>` (e.g. `task/gtk4-history-dropdown`).
3. Open a draft PR early; mark it `In Review` when ready.
4. Link the PR to the issue in the Project board (set the PR's Status field).
5. Keep one logical change per PR; no force-push after review starts.
6. CI (`ci/check.sh`) must pass before review.

## Merge policy

- Merge to `main` only through a PR; no direct pushes.
- Squash-merge feature branches; keep merge commits for PRs into `main`.
- Do not force-push to shared branches.
