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

Module choices (rationale):
  The flatpak base is the Flatpak TARGET system (org.gnome.Platform 51;
  GNOME 51 ships GTK 4.24.1 / GLib 2.90.1 / libX11 1.8.14), i.e. the app
  is built and run against exactly the toolkit versions users get. Do not
  substitute older/newer GTK here.
  xmake    - the build system; built here for /app and removed in the
             final cleanup (the app itself does not depend on xmake).
             The final cleanup also strips the from-source modules'
             development artifacts (static libs, pkg-config files, debug
             symbols): the runtime needs none of them.
  wordnet  - upstream Princeton WordNet 3.0; libwordnet + wn.h only
             (statically linked into the app). Built with -std=gnu11:
             pre-C23 codebase (the SDK's gcc defaults to gnu23).
             DEFAULTPATH is pinned to /app/dict.
  wordnet-dict - WordNet 3.1 dictionary data (utf8 dict files) overlaid
             into /app/dict (simple buildsystem, plain copy). WordNet
             3.1 exists only as a dict data set; there is no 3.1 source
             release, so the 3.0 source provides the library and the 3.1
             data files provide the dictionary. The 3.0 dict is NOT
             installed (superseded); the 3.1 set drops frames.vrb and
             lexnames (absent from 3.1).
  artha    - the application, built with the xmake.lua from this tree.

Enchant (libenchant-2) is intentionally NOT included: artha loads it at
runtime via dlopen() and silently disables spelling suggestions when it
is absent.
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
        ],
    }


def module_wordnet_dict(pin):
    return {
        "name": "wordnet-dict",
        "buildsystem": "simple",
        "sources": [
            {"type": "archive", "url": pin["url"], "sha256": pin["sha256"]},
        ],
        "build-commands": [
            "install -d /app/dict",
            # The archive extracts flat into the build root; dbfiles/ is
            # reindex metadata (~17MB), not referenced at runtime.
            "find . -mindepth 1 -maxdepth 1 ! -name dbfiles -exec cp -a {} /app/dict/ \\;",
        ],
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
        "wordnet-dict": module_wordnet_dict,
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
        "cleanup": [
            "/bin/xmake",
            # development artifacts of the from-source modules (libwordnet.a,
            # wn.h); useless (and large) at runtime. Artha is a C application:
            # no headers, static libs, pkg-config files or debug symbols needed.
            "/include",
            "/lib/*.a",
            "/lib/*.la",
            "/lib/pkgconfig",
            "/lib/debug",
        ],
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
