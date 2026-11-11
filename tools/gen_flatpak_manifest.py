#!/usr/bin/env python3
"""Generate flatpak/org.artha.Artha.json from deps.json.

deps.json (repo root) is the single source of truth for dependency
versions/pins. This script merges those pins with the build recipes below
(buildsystem, cflags, config-opts, build-commands), which are not
dependency metadata and therefore live here, not in deps.json.

Usage:
    python3 tools/gen_flatpak_manifest.py           # regenerate the manifest
    python3 tools/gen_flatpak_manifest.py --check   # exit 1 if out of sync

The generated manifest is committed to the repo. --check is the drift
guard (e.g. for CI): it fails if the committed manifest does not match a
fresh generation from deps.json.

Module choices (rationale, unchanged from the hand-written .yml):
  xmake    - the build system; built here for /app and removed in the
             final cleanup (the app itself does not depend on xmake).
  wordnet  - upstream Princeton WordNet 3.0; only lib + headers + dict
             data are installed. The upstream configure requires Tcl/Tk
             (for the wishwn GUI) which we deliberately do not build, so
             the library and data are installed directly with make-free
             commands (buildsystem: simple). DEFAULTPATH is pinned to
             /app/dict, the upstream $prefix/dict default.
  dbus-glib- POSIX session-bus helper for artha's single-instance logic.
             Built with -std=gnu11: this pre-C23 codebase uses `bool` as an
             identifier, which became a keyword in C23 (the SDK's gcc
             defaults to gnu23). Same treatment for GTK+ 2 and WordNet.
  gtk2     - GTK+ 2.24.33 (>= 2.24 as required); the SDK ships
             glib/pango/cairo/atk/gdk-pixbuf 2 but not GTK 2.
  artha    - the application, built with the xmake.lua from this tree.

Enchant (libenchant-2) is intentionally NOT included: artha loads it at
runtime via dlopen() and silently disables spelling suggestions when it
is absent. The freedesktop SDK does not ship enchant-2.
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEPS_PATH = ROOT / "deps.json"
OUT_PATH = ROOT / "flatpak" / "org.artha.Artha.json"

FINISH_ARGS = [
    # artha is a pure X11 application (it binds hotkeys via libX11)
    "--share=ipc",
    "--socket=x11",
    "--device=dri",
    # desktop notifications: artha dlopens libnotify at runtime
    "--talk-name=org.freedesktop.Notifications",
    # single-instance support (org.artha.unique bus name + broadcast signal).
    # NB: do not add a separate --talk-name for the same name: flatpak's
    # finish-stage applies both flags to one policy key and the last wins.
    "--own-name=org.artha.unique",
]


def module_xmake(tool):
    filename = tool["url"].rsplit("/", 1)[-1]
    return {
        "name": "xmake",
        "buildsystem": "simple",
        "sources": [
            {"type": "file", "url": tool["url"], "sha256": tool["sha256"]},
        ],
        "build-commands": [
            f"install -Dm755 {filename} /app/bin/xmake",
        ],
    }


def module_wordnet(pin):
    return {
        "name": "wordnet",
        "buildsystem": "simple",
        "sources": [
            {"type": "archive", "url": pin["url"], "sha256": pin["sha256"]},
        ],
        "build-commands": [
            # libwordnet: same sources Debian's wordnet-dev packages as libwordnet.a
            "cd lib && gcc -I../include -DDEFAULTPATH='\"/app/dict\"' -O2 -std=gnu11 -c binsrch.c morph.c search.c wnglobal.c wnhelp.c wnrtl.c wnutil.c",
            "cd lib && ar rcs libwordnet.a *.o",
            "install -Dm644 lib/libwordnet.a /app/lib/libwordnet.a",
            "install -Dm644 include/wn.h /app/include/wn.h",
            "install -d /app/dict",
            (
                "install -m 644 dict/adj.exc dict/adv.exc dict/cntlist dict/cntlist.rev "
                "dict/data.adj dict/data.adv dict/data.noun dict/data.verb "
                "dict/frames.vrb dict/index.adj dict/index.adv dict/index.noun "
                "dict/index.sense dict/index.verb dict/lexnames dict/noun.exc "
                "dict/sentidx.vrb dict/sents.vrb dict/verb.Framestext dict/verb.exc "
                "dict/log.grind.3.0 /app/dict/"
            ),
        ],
    }


def module_dbus_glib(pin):
    return {
        "name": "dbus-glib",
        "buildsystem": "autotools",
        "build-options": {
            # pre-C23 codebase: `bool` as an identifier (keyword in C23)
            "cflags": "-O2 -std=gnu11",
        },
        "sources": [
            {"type": "archive", "url": pin["url"], "sha256": pin["sha256"]},
        ],
        "config-opts": ["--prefix=/app"],
    }


def module_gtk2(pin):
    return {
        "name": "gtk2",
        "buildsystem": "autotools",
        "build-options": {
            # GTK+ 2 predates gcc 14, which turned IncompatiblePointerTypes
            # into an error; keep the historic diagnostic level (flag-only fix).
            "cflags": "-O2 -std=gnu11 -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-implicit-int",
        },
        "sources": [
            {"type": "archive", "url": pin["url"], "sha256": pin["sha256"]},
        ],
        "config-opts": ["--prefix=/app", "--disable-gtk-doc", "--disable-gtktest"],
    }


def module_artha():
    return {
        "name": "artha",
        "buildsystem": "simple",
        "sources": [
            {"type": "dir", "path": "../"},
        ],
        "build-commands": [
            "CPATH=/app/include LIBRARY_PATH=/app/lib xmake f --prefix=/app -m release",
            "CPATH=/app/include LIBRARY_PATH=/app/lib xmake",
            "CPATH=/app/include LIBRARY_PATH=/app/lib xmake install -o /app",
            # Flatpak exports desktop files/icons named after the app-id.
            # Icons must live in the hicolor theme to be exported (share/pixmaps
            # is not picked up by this flatpak-builder); artha.png is 128x128.
            "install -Dm644 /app/share/applications/artha.desktop /app/share/applications/org.artha.Artha.desktop",
            "sed -i 's/^Icon=artha$/Icon=org.artha.Artha/' /app/share/applications/org.artha.Artha.desktop",
            "install -Dm644 /app/share/pixmaps/artha.png /app/share/icons/hicolor/128x128/apps/org.artha.Artha.png",
        ],
    }


def build_manifest(deps):
    recipes = {
        "wordnet": module_wordnet,
        "dbus-glib": module_dbus_glib,
        "gtk2": module_gtk2,
    }

    modules = [module_xmake(deps["build_tool"]["xmake"])]
    for pin in deps["flatpak"]:
        name = pin["name"]
        if name not in recipes:
            sys.exit(
                f"deps.json: no build recipe in {Path(__file__).name} for flatpak module '{name}'"
            )
        modules.append(recipes[name](pin))
    modules.append(module_artha())

    runtime = deps["runtime"]
    return {
        "app-id": "org.artha.Artha",
        "runtime": runtime["platform"],
        "runtime-version": runtime["version"],
        "sdk": runtime["sdk"],
        "command": "artha",
        "separate-locales": False,
        "finish-args": FINISH_ARGS,
        "cleanup": ["/bin/xmake"],
        "modules": modules,
    }


def main():
    check = "--check" in sys.argv[1:]
    deps = json.loads(DEPS_PATH.read_text(encoding="utf-8"))
    manifest = build_manifest(deps)
    text = json.dumps(manifest, indent=4) + "\n"

    if check:
        current = OUT_PATH.read_text(encoding="utf-8") if OUT_PATH.exists() else None
        if current != text:
            sys.exit(
                f"out of sync: {OUT_PATH} differs from a fresh generation from deps.json (run: python3 tools/gen_flatpak_manifest.py)"
            )
        print(f"ok: {OUT_PATH.relative_to(ROOT)} is in sync with deps.json")
    else:
        OUT_PATH.write_text(text, encoding="utf-8")
        print(f"wrote {OUT_PATH.relative_to(ROOT)} (from deps.json)")


if __name__ == "__main__":
    main()
