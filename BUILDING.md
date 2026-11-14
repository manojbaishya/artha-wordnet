# Building Artha

Artha is built with **xmake** (see `xmake.lua`). This document covers
building, installing and packaging locally. Nothing here is published
anywhere; the flatpak is for local testing only.

## Dependencies (deps.json — single source of truth)

`deps.json` (repo root) is the one authoritative dependency file:

- `pc_minimum` — minimum versions enforced at configure time (read-only
  pkg-config `--modversion`). `xmake.lua` reads this; no version list is
  duplicated there.
- `runtime` / `build_tool` / `flatpak` — the flatpak pins:
  `org.gnome.Platform`/`Sdk` 51 (the reference target system), the
  prebuilt xmake tool, and the from-source module pins (wordnet 3.0
  library, wordnet-dict 3.1 dictionary data).

`flatpak/org.artha.Artha.json` is **generated** from deps.json by
`python3 tools/gen_flatpak_manifest.py` (stdlib-only, no pip installs).
Never edit the generated manifest by hand. After changing deps.json:

```sh
python3 tools/gen_flatpak_manifest.py          # regenerate
python3 tools/gen_flatpak_manifest.py --check  # verify in sync (exit 1 if not)
```

The generated manifest is committed; `--check` is the drift guard.

## Prerequisites

- `xmake` >= 3.0 (this tree was verified with 3.1.2) and `pkg-config`
- A C compiler and the system development packages for:
  - `gtk4 >= 4.6`, `glib-2.0 >= 2.66`, `gmodule-2.0 >= 2.66`
  - `x11` (Xlib development headers; on Debian: `libx11-dev`)
  - WordNet development files (`wn.h` + `libwordnet`; on Debian: `wordnet-dev`)
- `dbus` is needed only transitively (GDBus lives in GLib); there is no
  dbus-1/dbus-glib-1 build dependency.
- `libenchant-2` and `libnotify` are **not** build-time dependencies
  (loaded at runtime via `dlopen()`; Artha runs fine without them)
- On this machine, linuxbrew's `ld`/`as`/`ar` shadow the system ones in
  `PATH` and gcc's `collect2` then resolves `ld` from linuxbrew, which
  cannot see `/usr/lib/x86_64-linux-gnu` and mis-links system libraries
  (undefined `pcre_*`, `Xcursor*`, `__isoc23_*@GLIBC_2.38`, ...).
  Invoke xmake as `PATH=/usr/bin:$PATH xmake ...`.
  Note: passing `--ld=/usr/bin/ld --as=/usr/bin/as --ar=/usr/bin/ar` to
  `xmake config` does NOT help — the configure-time probes then fail and
  gcc still prefers linuxbrew's `ld`. `PATH` order is the only fix.
  For CLion, the xmake toolkit (`options/xmakeToolkits.xml`) is pointed
  at `~/.local/bin/clion-xmake`, a wrapper that forces `/usr/bin` first
  and execs `/usr/bin/xmake`; this keeps the quirk out of `xmake.lua`
  and out of this repository.

## Verified on this machine (2026-11-12)

Read-only `pkg-config --modversion` queries on the machine this tree was
verified on, plus the GNOME 51 SDK reference queries. Informational only —
`deps.json` remains the sole source of pinned floors/versions; never
script dependency logic against this block.

    (host, fast iteration floor)
    glib-2.0 2.72.4   gmodule-2.0 2.72.4   gtk4 4.6.9
    x11 1.7.5
    wordnet-dev 1:3.0-37 (libwordnet-3.0.so, /usr/include/wn.h, no .pc)
    xmake 3.1.2       GNU ld 2.47.20260726
    libenchant-2: not installed (runtime dlopen; Artha runs fine)
    libnotify: not installed (runtime dlopen; Artha runs fine)

    (reference target system — the flatpak build)
    org.gnome.Sdk 51: gtk4 4.24.1, glib-2.0/gmodule-2.0 2.90.1, libx11 1.8.14
    (identical to the current linuxbrew gtk4/glib/x11)

## Build

```sh
xmake                 # configures and builds into ./sandbox
xmake f -m debug && xmake   # debug build
xmake f --debug=verbose && xmake   # wni debug output (DEBUG_LEVEL=3)
```

`--debug` takes `no|yes|messages|verbose` (default `no`) and maps to
`DEBUG_LEVEL` 0|1|2|3, enabling `G_DEBUG`, `G_MESSAGE` and `G_PRINTF` in
`src/wni.h` / `src/wni.c`. It affects the `wni` diagnostics only; it does
not change optimization.

Artifacts (objects and the binary) stay inside the gitignored `sandbox/`
directory of the project root:

    sandbox/linux/x86_64/release/artha

A plain `xmake` binary uses the default prefix `/usr/local`, i.e. it looks
for `gui.glade`/`artha.png` in `/usr/local/share/artha` and
`/usr/local/share/pixmaps`. To run the binary without installing into
the system, stage it locally:

```sh
xmake f --prefix="$PWD/sandbox/stage"
xmake
xmake install -o "$PWD/sandbox/stage"
./sandbox/stage/bin/artha
```

`xmake install` lays out:

    bin/artha  share/applications/org.artha.Artha.desktop
    share/artha/gui.glade  share/man/man1/artha.1
    share/pixmaps/artha.png  share/icons/hicolor/128x128/apps/org.artha.Artha.png
    share/metainfo/org.artha.Artha.metainfo.xml

## What the build does to (not) your system

- Dependencies are **not** upgraded, downloaded, replaced or installed.
  Only read-only pkg-config metadata queries are used (`--cflags`,
  `--libs`, `--modversion`); minimum versions (`gtk4 >= 4.6`,
  `glib-2.0 >= 2.66`, `gmodule-2.0 >= 2.66`) are read from `deps.json`
  (`pc_minimum`).
- WordNet has no `.pc` file; it is linked with `-lwordnet`. Shared vs.
  static flavour is the system's choice.
- `libwni` (built from `src/wni.c`, sources inside this project) is an
  xmake **static** target and is archived into the binary. No shared
  `libwni` is produced or installed. System libraries stay dynamic.
- Package metadata is passed as `-D` defines (`ARTHA_*`, `APP_DIR`,
  `ICON_DIR`, `DBUS_AVAILABLE`, `X11_AVAILABLE`). `G_DISABLE_DEPRECATED=1`
  stays enabled (zero deprecated GLib calls remain). Debug output stays
  off by default.

## What is not migrated / leftovers

- xmake is the only build system. `README` points here for build
  instructions.
- Windows (Win32) build: shared `libnotify.dll` from `src/libnotify.c`,
  `app_icon.rc` / `libnotify_ver.rc` (self-contained), **untested**.
- `xmake install` does not install `libwni`.

## Flatpak (local exploration)

`flatpak/org.artha.Artha.json` (generated from `deps.json`; see
"Dependencies" above) builds Artha as a flatpak for
`org.gnome.Platform//51`. The flatpak target system is the **reference
environment**: GNOME 51 ships gtk4 4.24.1, glib-2.0 2.90.1 and libx11
1.8.14, identical to the current linuxbrew toolchain. Nothing from the
host is used. The app itself is built with the xmake.lua in this tree,
via a prebuilt xmake 3.1.2 module which is removed in the final cleanup,
and against the WordNet 3.0 source library (statically linked) with the
WordNet 3.1 dictionary overlaid at `/app/dict`. The final cleanup strips
the modules' development artifacts (`/include`, `/lib/*.a`, `/lib/*.la`,
`/lib/pkgconfig`, `/lib/debug`, and `/bin/xmake`); the runtime image is
~38 MB. The SDK's gcc (14, C23 defaults) treats
-Wincompatible-pointer-types as an error; one call site was fixed to
satisfy it.

```sh
# dep layer only (fast check, no source changes needed):
flatpak run --user \
    --filesystem="$PWD":ro \
    --filesystem="$HOME/.cache/fb-state" \
    --filesystem="$HOME/.cache/fb-out" \
    org.flatpak.Builder --disable-rofiles-fuse \
    --state-dir="$HOME/.cache/fb-state" --stop-at=artha \
    "$HOME/.cache/fb-out" flatpak/org.artha.Artha.json

# full build (what CI would do; keep fb-out empty first):
rm -rf "$HOME/.cache/fb-out"
flatpak run --user \
    --filesystem="$PWD":ro \
    --filesystem="$HOME/.cache/fb-state" \
    --filesystem="$HOME/.cache/fb-out" \
    org.flatpak.Builder --disable-rofiles-fuse \
    --state-dir="$HOME/.cache/fb-state" --force-clean \
    "$HOME/.cache/fb-out" flatpak/org.artha.Artha.json

# run it directly from the build output (no install, no user-scope changes):
flatpak build --runtime --readonly --die-with-parent \
    --share=ipc --socket=x11 --device=dri \
    --talk-name=org.freedesktop.Notifications \
    --own-name=org.artha.unique \
    --env=HOME=/tmp/artha-flatpak-home \
    "$HOME/.cache/fb-out" artha
```

`flatpak-builder` is used from the user-installed flathub app
`org.flatpak.Builder` (no sudo needed); there is no Homebrew formula for
it. Hard requirements on this machine/host:

- `--disable-rofiles-fuse` (FUSE unavailable).
- state/out dirs must not be on `/tmp` (the builder sandbox has a
  private `/tmp`; the module children then cannot see them).
- `--filesystem=host` is never used; grant only the worktree (read-only)
  and the two cache dirs.
- `flatpak build` runs the sandboxed app directly from the build output;
  the result was smoke-verified on Xephyr (`:90`) with metacity: window
  renders, app alive, WordNet index loads.
- Running the host-built binary manually (e.g. on Xephyr): artha reads
  `$XDG_CONFIG_HOME/artha.conf` (or `$HOME/.config/artha.conf`), and
  startup unconditionally rewrites it (the "after all setting based
  loads" save). To avoid touching the real user config, point
  `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_CACHE_HOME` and `HOME` at a
  scratch dir, e.g. `HOME=/tmp/artha-home XDG_CONFIG_HOME=/tmp/artha-xdg-config ... artha`.
  (A bare `HOME=/tmp/...` is not enough when the caller exports
  `XDG_CONFIG_HOME`; g_get_user_config_dir() prefers it over `HOME`.)
- This checkout may live inside another configured xmake project (e.g. a
  git worktree under `.opencode/worktrees/`). If the outer tree is also
  configured, a bare `xmake` builds the outer project and shadows this
  one; build this tree with `xmake f -P .` / `xmake -P .`.

Permissions (`finish-args`): `--share=ipc`, `--socket=x11`,
`--device=dri` (Artha is a pure X11 app), `--talk-name=org.freedesktop.Notifications`
(it dlopens libnotify, which the GNOME 51 runtime ships), and
`--own-name=org.artha.unique` (single-instance bus name). No network
permission. Notes:

- Single instance: do not add a second `--talk-name=org.artha.unique`;
  flatpak applies same-name policies last-flag-wins and the name ends up
  `talk`-only (this exact bug was hit and fixed).
- WordNet dict lives at `/app/dict` (upstream `$prefix/dict` layout via
  `DEFAULTPATH`); the 3.1 tarball extracts flat, so the overlay command
  copies everything except `dbfiles/` (~17 MB of reindex metadata the
  runtime never reads). `frames.vrb`, `lexnames` and `log.grind.3.0` do
  not exist in 3.1 and are not shipped.
- Enchant is absent from the SDK; spelling suggestions are silently
  disabled, exactly as on a host without libenchant-2.
- Export naming: the desktop file is installed as `org.artha.Artha.desktop`
  and the icon lives at `share/icons/hicolor/128x128/apps/org.artha.Artha.png`
  (`share/pixmaps` is not exported by this flatpak-builder); `Exec=artha`
  still works.
- Metainfo: `data/org.artha.Artha.metainfo.xml` (AppStream, id
  `org.artha.Artha`) is installed to `share/metainfo` and validates with
  `appstreamcli validate`. The desktop file and the hicolor icon are
  installed app-id-named out of the box (`data/org.artha.Artha.desktop`,
  `share/icons/hicolor/128x128/apps/org.artha.Artha.png`), so
  `appstreamcli compose` during the flatpak build accepts the component
  (launchable and stock icon match the id). Preparatory work for later
  distribution only: the flatpak is NOT published anywhere and is not
  flathub-ready (no hosted source, no repo, local `type: dir`).
- The SDK's `gcc` defaults to gnu23 with gcc-14 diagnostics; artha's own
  sources compile clean under it (WordNet 3.0 is built with
  `-std=gnu11` as before).
- Windows build: untested; the Win32 hotkey receiver (RegisterHotKey
  message-window) is not yet ported (GTK4 removed the window filters
  this used). Global hotkeys are unavailable in the Windows build.
