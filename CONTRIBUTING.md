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
TASK-<ID>: - <SCOPE> - <TITLE>
```

Example: `TASK-4: - build - GTK4 / GDBus / WordNet 3.1 migration + Flatpak GNOME 51 + CI gate`

| Part | Meaning |
|------|---------|
| `TASK-<ID>` | The Project board task issue number (e.g. `TASK-4` for issue #4) |
| `<SCOPE>` | A noun giving context to the affected codebase part (e.g. `build`, `gtk4`, `packaging`, `ci`) |
| `<TITLE>` | Concise summary of what changes were made |

When a PR resolves an issue, include `closes #<issue-number>` or `fixes #<issue-number>` in the PR body so GitHub auto-closes the issue on merge.

The `TASK-ID:` prefix links each PR to its Project board issue and takes precedence.

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
