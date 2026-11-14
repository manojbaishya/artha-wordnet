# Artha - An open cross-platform thesaurus based on WordNet

**Copyright (c) 2009 Sundaram Ramaswamy, <legends2k@yahoo.com>**

Artha is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later
version. See `COPYING` for details.

---

## Overview

Artha is a cross-platform open-source thesaurus with WordNet as its database.
The main focus is high usability, with distinct features:

* **Hotkey lookup** — select text in any window and press a user-set hotkey to
  pop up Artha with the word looked up.
* **Regex-based search** — find words using wildcards (`*`, `?`, `[a|c-k]`).
* **Passive desktop notifications** — word definitions arrive as tray
  notifications without opening the window.
* **Spelling suggestions** — when the exact spelling is vague/not known
  (via libenchant, loaded at runtime).

Artha is written in pure C using GTK4 and GLib, making it cross-desktop and
cross-platform. It uses `libnotify` for desktop notifications (dlopen'd at
runtime) and `libenchant` for spelling suggestions (dlopen'd at runtime).

## Build system

Artha is built exclusively with **xmake** (pinned to version 3.1.2 in
`deps.json`). xmake is *not* installed system-wide; it is reused from a
shared versioned user cache at `~/.artha-cache/xmake-3.1.2/` via the
`Taskfile.yml` recipes `bootstrap-xmake` / `xmake-ensure`. Nothing is
installed to `/usr/local` and nothing is written to `~/.config/artha.conf`.

All build artifacts stay inside the gitignored `sandbox/` directory. On hosts
where a shadowing toolchain (e.g. linuxbrew) precedes the system one in
`PATH`, ensure `/usr/bin` precedes it on `PATH` before running builds.

See `BUILDING.md` for full build, install, and flatpak instructions.

### Quick start

```sh
# configure + build into ./sandbox (ensure /usr/bin on PATH if needed)
task build

# stage into ./sandbox/stage (no system changes, never /usr/local)
task install

# run the staged binary on a scratch HOME / Xephyr :94
task run
```

## Dependencies

`deps.json` (repo root) is the single source of truth. Minimum versions are
enforced at configure time via read-only `pkg-config --modversion` queries
(`pc_minimum`). No xmake package downloads, no installs, no system changes.

| Dependency | Minimum | Notes |
|------------|---------|-------|
| `gtk4` | 4.6 | system library, dynamic |
| `glib-2.0` | 2.66 | system library, dynamic |
| `gmodule-2.0` | 2.66 | system library, dynamic |
| `x11` | — | Xlib development headers |
| `wordnet` | 3.0 | `wn.h` + `libwordnet`; linked with `-lwordnet` |

`libenchant-2` and `libnotify` are **not** build-time dependencies — they are
loaded at runtime via `dlopen()`; Artha runs fine without them (spelling
suggestions / notifications are silently disabled).

`dbus` is needed only transitively (GDBus lives in GLib); there is no
`dbus-1`/`dbus-glib-1` build dependency.

## Testing

Artha uses a three-layer test pyramid (GLib GTest + a dependency-free Gherkin
runner):

| Layer | Tool | Target | What it covers |
|-------|------|--------|----------------|
| Unit | GLib GTest | `artha-unit-test` | `wni_strcmp0`, `strip_adj_marker` (pure logic) |
| Integration | GLib GTest | `artha-integration-test` | `wni_request_nyms`/`wni_free` vs real WordNet DB |
| Behavioural | Gherkin + Python | `tests/behavioral/run_tests.py` | app + UI on scratch HOME / Xephyr `:94` |

```sh
# configure, build, then run unit + integration tests
task test

# run behavioural (app + UI) Gherkin scenarios (needs Xephyr :94; :0 is read-only)
task test-behavioral
```

GUI interaction/tests run on nested Xephyr `:94` only; `:0` is read-only;
scratch `HOME`/`XDG_*` directories are used so `~/.config/artha.conf` is never
touched.

## Wiki

Hierarchical project documentation lives on the
[GitHub Wiki](https://github.com/manojbaishya/artha-wordnet/wiki):

* **Home** — this README (kept in sync automatically via GitHub Action)
* **Building** — build, install, flatpak, host quirks (`BUILDING.md`)
* **Testing** — test pyramid, unit/integration/behavioural layers
* **Contributing** — issue types, board status, PR naming, merge policy
* **Project Plan** — requirement → issue hierarchy mapping
* **Deployment** — deployment targets (Linux/Windows/macOS), versioned GitHub Release assets, no-system-changes install model
* **Release Strategy** — release-train branching model: stable `main`, `release/v-*` staging, feature branches, `pre-release/...-FeatureSet{i}` integration

The root wiki page (`Home`) mirrors `README.md` and is updated automatically
whenever `README.md` is merged to `main` (see
`.github/workflows/sync-wiki.yml`).

## Bugs & Queries

Suggestions, bugs, and queries are tracked as GitHub Issues at
https://github.com/manojbaishya/artha-wordnet/issues. Link a PR to an issue
with `closes #<issue>` / `fixes #<issue>` in the PR body so the issue closes
automatically on merge.

## Licence

GNU General Public License v2 or later. See `COPYING`.

## More Information

* Project home: https://artha.sourceforge.net/
* Source: https://github.com/manojbaishya/artha-wordnet
* Wiki: https://github.com/manojbaishya/artha-wordnet/wiki
