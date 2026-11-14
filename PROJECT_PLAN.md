# Artha Project Plan — Requirement → Issue Hierarchy

Project memory: the single source of truth for how every requirement maps to
the GitHub issue hierarchy (epic → feature/user-story → task → sub-task) and
the PR naming convention `<TYPE>: <SCOPE> - <TITLE>`.

Type order: `epic` > `feature` > `user-story` > `task` > `sub-task` > `bug`.
Scope is a noun naming the affected codebase part (e.g. `build`, `gtk4`,
`wni`, `gui`, `packaging`, `ci`, `test`, `i18n`).

---

## Hierarchy model

```
Epic (issue, label `epic`)
└── Feature (issue, label `feature`)
    ├── User Story (issue, label `user-story`)   — end-user capability, 1 sprint
    └── Task (issue, label `task`)
        └── Sub-task (issue, label `sub-task`)
Bug (issue, label `bug`) — stands alone or links to the feature/task it fixes
```

PR → issue linkage: `closes #<issue>` / `fixes #<issue>` in the PR body.
Branch name: `<type>-<short-slug>` (e.g. `task/gtk4-history-dropdown`).
Only one `In Progress` issue/PR per person at a time.

---

## Epic: Artha Go-Live — GTK4-modernized release (#7)

Umbrella body of work spanning multiple sprints: GTK4 migration, test
infrastructure, build/release tooling, and polish toward a 1.1 release.
Parent of all features below.

### Feature: App Modernization (#3) — DONE

GTK2→GTK4 migration + self-contained pinned build + CI.

| Child | Type | Scope | Status | Notes |
|-------|------|-------|--------|-------|
| Self-contained pinned xmake via versioned cache | `task` | `build` | DONE (#9) | `deps.json` pin 3.1.2, `~/.artha-cache/xmake-3.1.2/`, `bootstrap-xmake`/`xmake-ensure` |
| CI host-build + flatpak | `task` | `ci` | DONE | `.github/workflows/ci.yml` (ubuntu-24.04, GTK 4.14, xvfb) |
| Auto-assign new issues | `task` | `ci` | DONE (#8) | `.github/workflows/auto-assign.yml` |
| PR → issue linking | `task` | `ci` | DONE (#9) | `.github/workflows/pr-issue-link.yml`, JOIN key `closes`/`fixes`/`join` |
| CONTRIBUTING.md process doc | `task` | `docs` | DONE (#9) | issue types, board status, PR naming, merge policy |

### Feature: Test Infrastructure — DONE (#6 → PR #12)

Test pyramid for the WNI layer and app behaviour.

| Child | Type | Scope | Status | Notes |
|-------|------|-------|--------|-------|
| Unit + integration + behavioural test harness | `user-story` | `test` | DONE (#6, closed by #12) | GLib GTest unit (12), integration vs real WordNet DB (4), Gherkin+Python behavioural (2) |
| xmake targets `artha-unit-test` / `artha-integration-test` | `sub-task` | `build` | DONE (in #12) | linked into `task test` |
| Taskfile `task test` / `task test-behavioral` | `sub-task` | `build` | DONE (in #12) | run with `/usr/bin` on `PATH` |
| `__pycache__/` gitignore | `sub-task` | `build` | DONE (in #12) | behavioural runner artifacts |

### Feature: Preferences & Help Windows

End-user settings UI (currently settings exist only as TODO list items:
"Watch clipboard/selection", "Fonts and Colours"; no Preferences/Help
windows exist).

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| Preferences dialog | `task` | `gui` | `task: gui - Preferences window` | Glade dialog + `artha.conf` write-back for fonts/colours/clipboard-watch |
| Fonts & Colours settings | `sub-task` | `gui` | `sub-task: gui - font/colour prefs` | preview applies live to definition textview |
| Watch clipboard/selection | `sub-task` | `gui` | `sub-task: gui - clipboard-watch pref` | toggle persists across restarts |
| Help docs window | `task` | `gui` | `task: gui - Help window` | about/help content reachable from menu |

### Feature: Hotkey UX improvements

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| Make hotkey a toggle/switch | `task` | `gui` | `task: gui - hotkey toggle` | hotkey can be enabled/disabled without re-registering |
| Single click jump to words | `task` | `gui` | `task: gui - single-click jump` | clicking a relative in list/tree jumps to its lookup |
| Replace platform-specific hotkey code with libgtkhotkey | `task` | `wni` | `task: wni - libgtkhotkey` | removes Win32/X11 hotkey ifdefs, single code path |

### Bug: Index '_' spacing issue

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| Spaces not all filled with '_' in index files (e.g. `24-karet_gold` in `index.noun`) | `bug` | `wni` | `bug: wni - index underscore spacing` | lookup normalizes `-`/` ` to `_` before index search; regression test added |

### Task: WNI memory hygiene

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| Use `g_slist_free_full` in WNI free code | `task` | `wni` | `task: wni - g_slist_free_full` | cleaner free paths, no new leaks (valgrind clean on smoke run) |

### Feature: Internationalization (i18n)

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| i18n string extraction | `task` | `i18n` | `task: i18n - gettext strings` | all user-facing strings wrapped; `.pot` generated |
| Locale data scaffolding | `task` | `i18n` | `task: i18n - locale scaffolding` | `po/` dir, `LINGUAS`, `make`-free refresh via xmake custom rule |

### Feature: Pronunciations (speech)

Uses flite speech engine + Moby phonetics (from TODO).

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| Integrate flite speech engine | `task` | `wni` | `task: wni - flite integration` | runtime dlopen (like libenchant), graceful degrade when absent |
| Show phonetics | `task` | `gui` | `task: gui - phonetics display` | phonetic rendering next to headword from Moby data |

### Feature: Command-Line Interface (CLI)

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| n/PDCurses CLI probe | `task` | `cli` | `task: cli - PDCurses TUI probe` | read-only PoC: lookup a word, print synonyms; decision gate for full CLI |

### Task: Windows build validation

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| Windows (Win32) build validation | `task` | `packaging` | `task: packaging - Windows build` | shared `libnotify.dll` builds; note global hotkeys unavailable (GTK4 removed window filters) — document as known limitation |

### Task: Flatpak publication readiness

| Child | Type | Scope | Suggested PR | Acceptance criteria |
|-------|------|-------|--------------|---------------------|
| Flatpak flathub readiness | `task` | `packaging` | `task: packaging - flatpak publish` | hosted source + repo, not just local `type: dir`; appstream + icon validated |

---

## Definition of done (per issue type)

| Type | DoD |
|------|-----|
| `epic` | All child features merged to `main`; release notes written |
| `feature` | All child user-stories/tasks merged; demoed on Xephyr `:94` |
| `user-story` | PR merged to `main`; CI green; test layer (unit/integration/behavioural) added where applicable |
| `task` | PR merged to `main`; CI green; no `~/.config/artha.conf` or `/usr/local` touched on host |
| `sub-task` | PR merged to parent task's branch; code reviewed |
| `bug` | PR merged; regression test added; verified on host + Xephyr `:94` |

---

## Merge policy (recap)

- Merge to `main` only via PR; no direct push; no force-push after review.
- Squash-merge feature branches; merge commits for PRs into `main`.
- PR title: `<TYPE>: <SCOPE> - <TITLE>`, `TYPE ∈ {epic, feature, user-story, bug, task, sub-task}`.
- PR body: `closes #<issue>` / `fixes #<issue>` to auto-close linked issue.
- `:0` is read-only; GUI tests run on nested Xephyr `:94` only; scratch `HOME`/`XDG_*`; never `~/.config/artha.conf`; ensure `/usr/bin` on `PATH` for host builds; never `/usr/local` or artha-origin flatpak.
