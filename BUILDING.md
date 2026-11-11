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
  `org.freedesktop.Platform`/`Sdk` runtime version, the prebuilt xmake
  tool, and the from-source module pins (wordnet, dbus-glib, gtk2).

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
  - `gtk+-2.0 >= 2.24`, `glib-2.0 >= 2.22`, `gmodule-2.0`
  - `dbus-1 >= 0.60`, `dbus-glib-1 >= 0.60`, `x11`
  - WordNet development files (`wn.h` + `libwordnet`; on Debian: `wordnet-dev`)
- `libenchant-2` and `libnotify` are **not** build-time dependencies
  (loaded at runtime via `dlopen()`; Artha runs fine without them)
- On this machine, linuxbrew's `ld`/`as`/`ar` shadow the system ones and
  misresolve glibc symbols. Invoke xmake as
  `PATH=/usr/bin:$PATH xmake ...` or pass `xmake f --ld=/usr/bin/ld --as=/usr/bin/as --ar=/usr/bin/ar`.
  (A host quirk, deliberately not baked into `xmake.lua`.)

## Verified on this machine (2026-11-11)

Read-only `pkg-config --modversion` queries on the machine this tree was
verified on. Informational only — `deps.json` remains the sole source of
pinned floors/versions; never script dependency logic against this block.

    glib-2.0 2.72.4   gmodule-2.0 2.72.4   gtk+-2.0 2.24.33
    dbus-1 1.12.20    dbus-glib-1 0.112    x11 1.7.5
    wordnet-dev 1:3.0-37 (libwordnet-3.0.so, /usr/include/wn.h, no .pc)
    xmake 3.1.2       GNU ld 2.47.20260726
    libenchant-2: not installed (runtime dlopen; Artha runs fine)
    libnotify: not installed (runtime dlopen; Artha runs fine)

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

    bin/artha  share/applications/artha.desktop  share/artha/gui.glade
    share/man/man1/artha.1  share/pixmaps/artha.png

## What the build does to (not) your system

- Dependencies are **not** upgraded, downloaded, replaced or installed.
  Only read-only pkg-config metadata queries are used (`--cflags`,
  `--libs`, `--modversion`); minimum versions (`gtk+-2.0 >= 2.24`,
  `glib-2.0 >= 2.22`, `dbus >= 0.60`) are read from `deps.json`
  (`pc_minimum`).
- WordNet has no `.pc` file; it is linked with `-lwordnet`. Shared vs.
  static flavour is the system's choice.
- `libwni` (built from `src/wni.c`, sources inside this project) is an
  xmake **static** target and is archived into the binary. No shared
  `libwni` is produced or installed. System libraries stay dynamic.
- Package metadata is passed as `-D` defines (`ARTHA_*`, `APP_DIR`,
  `ICON_DIR`, `DBUS_AVAILABLE`, `X11_AVAILABLE`, deprecated-API guards).
  Debug output stays off by default.

## What is not migrated / leftovers

- xmake is the only build system. `README` points here for build
  instructions.
- Windows (Win32) build: shared `libnotify.dll` from `src/libnotify.c`,
  `app_icon.rc` / `libnotify_ver.rc` (self-contained), **untested**.
- `xmake install` does not install `libwni`.

## Flatpak (local exploration)

`flatpak/org.artha.Artha.json` (generated from `deps.json`; see
"Dependencies" above) builds Artha as a flatpak for
`org.freedesktop.Platform//25.08`. Every dependency is built from source
inside the sandbox (WordNet 3.0 from Princeton, GTK+ 2.24.33,
dbus-glib 0.112 — the exact versions the system has); nothing from the
host is used. The app itself is built with the xmake.lua in this tree,
via a prebuilt xmake 3.1.2 module which is removed in the final cleanup.

```sh
cd flatpak
# build (org.freedesktop.Sdk//25.08 and Platform//25.08 are already
# installed on this machine; first build downloads module sources)
flatpak run --command=flatpak-builder --user org.flatpak.Builder \
    --user .flatpak-build org.artha.Artha.json --state-dir=.flatpak-builder
# install into the user installation from a local repo and run
rm -rf repo && flatpak build-export repo .flatpak-build org.artha.Artha
flatpak install --user -y ./repo org.artha.Artha
flatpak run org.artha.Artha
```

`flatpak-builder` is used from the user-installed `org.flatpak.Builder`
app (no sudo needed); a system `flatpak-builder` package would also work.

Permissions (`finish-args`): `--share=ipc`, `--socket=x11`,
`--device=dri` (Artha is a pure X11 app), `--talk-name=org.freedesktop.Notifications`
(it dlopens libnotify, which the 25.08 runtime ships), and
`--own-name=org.artha.unique` (single-instance bus name). No network
permission. Notes:

- Single instance: do not add a second `--talk-name=org.artha.unique`;
  flatpak applies same-name policies last-flag-wins and the name ends up
  `talk`-only (this exact bug was hit and fixed).
- WordNet dict lives at `/app/dict` (upstream `$prefix/dict` layout via
  `DEFAULTPATH`).
- Enchant is absent from the SDK; spelling suggestions are silently
  disabled, exactly as on a host without libenchant-2.
- The gtk2 build uses `-std=gnu11` and historic diagnostic levels
  (`-Wno-incompatible-pointer-types`, `-Wno-implicit-int`) because the
  SDK's gcc defaults to gnu23 / gcc-14 diagnostics and this pre-C23
  codebase does not compile under them. No source was patched.
- Export naming: the desktop file is installed as `org.artha.Artha.desktop`
  and the icon lives at `share/icons/hicolor/128x128/apps/org.artha.Artha.png`
  (`share/pixmaps` is not exported by this flatpak-builder); `Exec=artha`
  still works.
- The flatpak is NOT published anywhere and is not ready for flathub:
  no metainfo file, no repo hosting, sources are a local `type: dir`.
  Slimming (`cleanup: /lib/*.a`, gtk dev tools, etc.) is possible but
  was not done, to keep the exploratory build byte-identical to the
  tested one.
