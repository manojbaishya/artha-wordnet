# Agent instructions for Artha

This file is the project-wide operating manual for any agent (or agent swarm) working in the Artha repository. It consolidates the build, test, branching, PR, wiki, and release policies so every agent follows the same conventions without re-deriving them.

## 1. Project identity

* **Artha** — an open cross-platform thesaurus based on WordNet, written in pure C using GTK4 and GLib.
* Homepage: https://artha.sourceforge.net/ | Source: https://github.com/manojbaishya/artha-wordnet | Wiki: https://github.com/manojbaishya/artha-wordnet/wiki
* License: GNU GPL v2 or later (`LICENSE`).

## 2. Build system — read-only, self-contained, no system changes

Artha is built exclusively with **xmake** (pinned to **3.1.2** in `deps.json`). xmake is *not* installed system-wide; it is reused from a shared versioned user cache at `~/.artha-cache/xmake-3.1.2/` via `Taskfile.yml` recipes `bootstrap-xmake` / `xmake-ensure`. Nothing is installed to `/usr/local` and nothing is written to `~/.config/artha.conf` on the host.

All build artifacts stay inside the gitignored `sandbox/` directory.

**Quick start** (ensure `/usr/bin` precedes the host toolchain on `PATH`):

```sh
task build        # configure + build into ./sandbox
task install      # stage into ./sandbox/stage (no system changes)
task run          # run staged binary on scratch HOME / Xephyr :94
task test         # unit + integration tests
task test-behavioral  # Gherkin + Python behavioural tests (needs Xephyr :94)
```

`task install` stages into `sandbox/stage` and lays out `bin/artha`, `share/applications/org.artha.Artha.desktop`, `share/artha/gui.glade`, `share/man/man1/artha.1`, `share/pixmaps/artha.png`, `share/icons/hicolor/128x128/apps/org.artha.Artha.png`, `share/metainfo/org.artha.Artha.metainfo.xml`.

## 3. Dependencies (single source of truth: `deps.json`)

Minimum versions enforced at configure time via read-only `pkg-config --modversion` queries (`pc_minimum`). No xmake package downloads, no installs, no system changes.

| Dependency | Minimum | Notes |
|------------|---------|-------|
| `gtk4` | 4.6 | system library, dynamic |
| `glib-2.0` | 2.66 | system library, dynamic |
| `gmodule-2.0` | 2.66 | system library, dynamic |
| `x11` | — | Xlib development headers |
| `wordnet` | 3.0 | `wn.h` + `libwordnet`; linked with `-lwordnet` |

`libenchant-2` and `libnotify` are **not** build-time dependencies — loaded at runtime via `dlopen()`; Artha runs fine without them (spelling suggestions / notifications silently disabled). `dbus` is needed only transitively (GDBus lives in GLib); there is no `dbus-1`/`dbus-glib-1` build dependency.

## 4. Test pyramid — three layers

| Layer | Tool | xmake target | What it covers |
|-------|------|--------------|----------------|
| Unit | GLib GTest | `artha-unit-test` | Pure logic: `wni_strcmp0`, `strip_adj_marker`. No WordNet DB, no GUI, no D-Bus. |
| Integration | GLib GTest + real WordNet DB | `artha-integration-test` | `wni_request_nyms()` / `wni_free()` vs `/usr/share/wordnet`. |
| Behavioural (app + UI) | Gherkin + Python runner | `tests/behavioral/run_tests.py` | Real `artha` binary end-to-end on scratch HOME / Xephyr `:94`. |

**Rules:**
* GUI interaction/tests run on nested Xephyr `:94` only; `:0` is read-only.
* Scratch `HOME`/`XDG_*` directories are used; `~/.config/artha.conf` is never touched.
* `task test` builds `artha-unit-test` and `artha-integration-test` (xmake targets added in `xmake.lua`) and is gated by `xmake-ensure`.
* All scenarios must pass before a PR is merged to `main`.

## 5. Flatpak (local exploration / reference target)

`flatpak/org.artha.Artha.json` is **generated** from `deps.json` by `python3 tools/gen_flatpak_manifest.py` (stdlib-only, no pip installs). Never edit the generated manifest by hand. After changing `deps.json`:

```sh
python3 tools/gen_flatpak_manifest.py          # regenerate
python3 tools/gen_flatpak_manifest.py --check  # verify in sync (exit 1 if not)
```

The generated manifest is committed; `--check` is the drift guard. Flatpak targets `org.gnome.Platform`/`Sdk` 51 (reference environment: gtk4 4.24.1, glib-2.0 2.90.1, libx11 1.8.14).

## 6. Issue hierarchy & types

Type order: `epic` > `feature` > `user-story` > `task` > `sub-task` > `bug`.

| Label | Meaning | Example |
|-------|---------|---------|
| `epic` | Top-level body of work spanning multiple sprints | `Artha Go-Live` (#7) |
| `feature` | Large capability, spans sprints; child of an epic | `App Modernization` (#3) |
| `user-story` | End-user-facing capability, sized for one sprint | "search for synonyms" (#6) |
| `bug` | Something broken; regression or defect | "history writes `0x01` instead of the term" |
| `task` | Engineering work not directly user-facing | "Migrate TreeView to GtkListView" (#4, #5) |
| `sub-task` | Step under a parent task/user-story | "Remove `can-focus` from gui.glade containers" |

Hierarchy: `epic → feature → user-story/task → sub-task`. Bugs stand alone or link to the feature/task they fix.

Parent-child relationships are recorded in issue bodies (`Parent: #N (feature: ...)`) since the formal sub-issues API is unavailable for this repo.

## 7. PR naming, linking, and merge policy

**PR title:** `<TYPE>: <SCOPE> - <TITLE>`, where `TYPE ∈ {epic, feature, user-story, bug, task, sub-task}` and `scope` is a noun naming the affected codebase part (e.g. `build`, `gtk4`, `wni`, `gui`, `packaging`, `ci`, `test`, `docs`).

**Example:** `task: docs - README.md migration + wiki sync action`

**Linking / auto-close:** include `closes #<issue>` or `fixes #<issue>` in the PR body so GitHub auto-closes the linked issue on merge. `.github/workflows/pr-issue-link.yml` scans PR title/body for JOIN keys (`closes`, `fixes`, `join`, case-insensitive) and comments on each linked issue.

**Merge policy:**
* Merge to `main` only via PR; no direct pushes; no force-push after review starts.
* Squash-merge feature branches; merge commits for PRs into `main`.
* Branch protection on `main` requires PR + passing CI status checks.
* Only one `In Progress` issue/PR per person at a time.
* Branch name: `<type>/<short-slug>` (e.g. `task/gtk4-history-dropdown`).

## 8. Release strategy (see wiki: Release-Strategy.md)

Release-train branching model over a stable `main` trunk:

1. **`main`** — always stable; carries an X-count set of features. No direct push; PR + CI required.
2. **`release/v-Y26.M11.W{Week-Index-1-Based}`** — unstable staging branch cut from `main` every 1–2 weeks after the previous cycle closes. Stabilised and merged back to `main` at cycle end.
3. **Dev/test branches** — fan out from the active `release/v-*` staging branch and develop in silos.
4. **`pre-release/v-Y26.M11.W{Week-Index-1-Based}-FeatureSet{i}`** — integration branch created when X out of Total_count feature branches share interaction points (data structures, algorithms, functionality, code, tests, non-functional behaviour). Mixed, tested, stabilised, then merged to the release branch.

This is **not** strict trunk-based development (which requires daily integration into the trunk with short-lived feature branches). Artha's model is closer to a release-train / scheduled release-branching model (GitFlow-lite).

## 9. Wiki sync

The GitHub Wiki root page (`Home`) mirrors `README.md` and is kept in sync automatically by `.github/workflows/sync-wiki.yml`, which pushes `README.md` → wiki `Home.md` on every push to `main`. Wiki default branch is `master`.

Hierarchical wiki pages: `Home`, `Building`, `Testing`, `Contributing`, `Project-Plan`, `Deployment`, `Release-Strategy`. Cross-linked via `[[Page]]` wiki links.

## 10. Agent operating principles

1. **Conclusion first, proof after** — state the outcome, then minimal supporting evidence (numbers/diffs over prose).
2. **One decision per turn** — `DO X unless you say no` (plus at most one alternative).
3. **Never re-explain a closed/frozen decision** — if it's documented here or in `docs/planning/PROJECT_PLAN.md`, treat it as settled.
4. **Never re-ask what's pre-answered** — check this file, `docs/planning/PROJECT_PLAN.md`, `README.md`, and the wiki first.
5. **Verification before reporting complete** — run the project's lint/typecheck/build checks; fix failures and rerun. Do not silently skip checks.
6. **No host mutations** — never `~/.config/artha.conf`, never `/usr/local`, never `:0` for GUI tests; use scratch `HOME`/`XDG_*`, Xephyr `:94`; ensure `/usr/bin` on `PATH`.
7. **Compress context** — put details in files (`SYSTEM_MEMORY.md`, `docs/planning/PROJECT_PLAN.md`); chat carries 10-line distillates.

## 11. Verification commands (run before declaring done)

```sh
task test
task test-behavioral   # needs Xephyr :94
python3 tools/gen_flatpak_manifest.py --check
```

CI gates: `Host build + smoke (ubuntu-24.04, GTK 4.14)` and `Flatpak build (GNOME 51, GTK 4.24)`.
