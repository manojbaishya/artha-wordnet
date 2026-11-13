# Contributing to Artha

Thank you for contributing! This document explains how we track work, name issues and PRs, and move work through the team board.

## Issue types

We use GitHub Issues as the unit of work, typed by label:

| Label | Meaning | Example |
|-------|---------|---------|
| `epic` | Large body of work spanning multiple sprints; has a User Story as parent | `App Modernization` |
| `user-story` | End-user-facing capability, sized for one sprint | "As a user, I want to search for synonyms" |
| `bug` | Something broken; regression or defect | "History writes `0x01` instead of the term" |
| `task` | Engineering work not directly user-facing | "Migrate TreeView to GtkListView" |
| `sub-task` | Step under a parent task/user-story | "Remove `can-focus` from gui.glade containers" |

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

PR titles carry the task ID so the board and code history stay linked:

```
<TASK-ID>: <issue title>
```

Example: `TASK-42: Replace history GtkComboBox with GtkDropDown`

When a PR resolves an issue, include `closes #<issue-number>` or `fixes #<issue-number>` in the PR body so GitHub auto-closes the issue on merge.

### Adobe PR Naming Guide (reference)

We also follow the spirit of [Adobe's Pull Request Naming Guide](https://github.com/adobe/react-spectrum/wiki/Pull-Request-Naming-Guide), which uses the format:

```
Type (Scope): Summary of changes
```

| Type | Meaning | Example |
|------|---------|---------|
| `fix` | Fixing a bug | `fix: allow useHref on synthetic links` |
| `feat` | Adding a new feature | `feat(virtualization): add support for custom collection renderers` |
| `build` | Updates that affect the build system/process | `build: update xmake to latest` |
| `chore` | Miscellaneous non-meaningful commits | `chore: fix typo in comment` |
| `docs` | Documentation-only change | `docs: fix typo in README` |
| `test` | Adding or fixing tests | `test: add unit tests for strutils` |
| `refactor` | Code change, no bug fix or feature | `refactor: simplify strip_invalid_edges` |
| `ci` | CI config changes | `ci: add GitHub Actions PR gate` |
| `localize` | Translation/localization | `localize: add French strings` |
| `bump` | Dependency version bump | `bump: update GTK to 4.12` |
| `revert` | Undoing a previous commit | `revert: revert bad gui.c change` |

The `Scope` is optional — a noun giving context to the affected codebase part (do not use issue identifiers as scope).

Our `TASK-ID:` prefix takes precedence for Artha since it links each PR to a Project board issue. Combined form:

```
<TASK-ID>: Type (Scope): Summary of changes
```

Example: `TASK-4: feat(build): add check harness, smoke test and GitHub Actions PR gate`

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
