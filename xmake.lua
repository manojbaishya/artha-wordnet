-- xmake.lua - Artha Thesaurus (xmake build)
--
-- xmake is the only build system for this tree.
--
-- Design notes
-- ------------
-- * Dependencies are NOT upgraded and NOT replaced beyond the GTK2→GTK4
--   migration: the system libraries are used here, via read-only
--   pkg-config metadata queries (or -l flags for WordNet).
--   No xmake package downloads, no installs, no system changes.
--   Enforced minimum versions live in deps.json (repo root, `pc_minimum`):
--   gtk4 >= 4.6, glib-2.0 >= 2.66, gmodule-2.0 >= 2.66.
--   deps.json is the single source of truth for dependency pins:
--   tools/gen_flatpak_manifest.py reads the same file to emit
--   flatpak/org.artha.Artha.json. The flatpak base (org.gnome.Platform
--   51) is the reference target system: GNOME 51 ships gtk4 4.24.1.
--   GDBus (dbus) is part of glib; there is no dbus-1/dbus-glib-1 pin.
-- * Libraries whose sources live inside this project (libwni) are built
--   as a static library target and linked statically into the binary.
--   No shared libwni is ever produced.
-- * System libraries (gtk4, glib, X11, wordnet, ...) stay dynamic;
--   they are not checked beyond pkg-config metadata reads and are not
--   modified or replaced.
-- * Build artifacts are written to ./sandbox (gitignored); xmake is
--   the only build system.
--
-- Usage
-- -----
--   xmake                       # configure + build into ./sandbox
--   xmake f -m debug && xmake   # debug build (mode is a configure option)
--   xmake f --prefix=/opt/artha # override install prefix (default /usr/local,
--                               # APP_DIR/ICON_DIR layout)
--   xmake install -o /opt/artha # install into /opt/artha (bin, share/...)
--
-- APP_DIR/ICON_DIR (where gui.glade / artha.png are looked up at runtime)
-- are baked in at configure time from `prefix`, and `xmake install` uses
-- the same layout, so `xmake f --prefix=/x && xmake install -o /x` is
-- self-consistent.
--
-- Windows (untested here): builds the vendored Win32 libnotify
-- (src/libnotify.c) as a shared library. The POSIX path is the one
-- that is built and tested.

set_project("artha")
set_version("1.0.5")
set_description("A cross-platform open-source thesaurus based on WordNet")
set_license("GPL-2.0-or-later")
set_xmakever("3.0.0")

add_rules("mode.debug", "mode.release")
set_languages("c99")

-- Keep every build artifact inside the gitignored sandbox/ directory.
-- Command-line `xmake f -o <dir>` overrides this.
set_config("builddir", "sandbox")

option("prefix")
    set_default("/usr/local")
    set_showmenu(true)
    set_description("Install prefix (APP_DIR=<prefix>/share/artha, ICON_DIR=<prefix>/share/pixmaps)")

-- Replaces the old --enable-debug configure switch. The value maps to
-- DEBUG_LEVEL, consumed by src/wni.h (G_DEBUG/G_MESSAGE/G_PRINTF) and
-- src/wni.c (#if DEBUG_LEVEL > 2). Default keeps debug output off.
option("debug")
    set_default("no")
    set_showmenu(true)
    set_values("no", "yes", "messages", "verbose")
    set_description("Debug output level (no|yes|messages|verbose -> DEBUG_LEVEL 0|1|2|3)")

-- NB: during the very first `xmake f`, get_config() may not see the
-- cached option yet; fall back to the option's default.
local prefix = get_config("prefix") or "/usr/local"
local app_dir = is_plat("windows") and "." or (prefix .. "/share/artha")
local icon_dir = is_plat("windows") and "." or (prefix .. "/share/pixmaps")

-- Common flags
local common_cxflags = {
    "-Wall",
    "-Wextra",
    "-std=c99",
    "-pedantic",
    "-Wno-unused-parameter",
    "-fstrict-aliasing",
    "-Wno-missing-field-initializers",
}

local common_defines = {
    -- (GTK_DISABLE_DEPRECATED/GDK_DISABLE_DEPRECATED are no-ops in GTK4)
    "G_DISABLE_DEPRECATED=1",
    -- package metadata defines fed to the sources
    'ARTHA_NAME="Artha"',
    'ARTHA_TARNAME="artha"',
    'ARTHA_VERSION="1.0.5"',
    'ARTHA_URL="http://artha.sourceforge.net/"',
    'APP_DIR="' .. app_dir .. '"',
    'ICON_DIR="' .. icon_dir .. '"',
}

-- NB: same first-configure caveat as `prefix` above; "no" is the default.
local debug_level = ({ yes = 1, messages = 2, verbose = 3 })[get_config("debug") or "no"]
if debug_level then
    table.insert(common_defines, "DEBUG_LEVEL=" .. debug_level)
end

-- Dependency minimums are single-sourced in deps.json (repo root); the
-- flatpak manifest generator reads the same file. `import` is not
-- available at the root scope of xmake.lua, so the file is read lazily
-- (memoized) from inside on_load, where import works.
local pc_min_cache = nil
local function pc_minimum(import_fn)
    if pc_min_cache then return pc_min_cache end
    local json = import_fn("core.base.json")
    local path = os.projectdir() .. "/deps.json"
    local data, errors = json.loadfile(path)
    if not data then
        raise("deps.json not readable at %s: %s", path, tostring(errors))
    end
    pc_min_cache = data.pc_minimum or {}
    return pc_min_cache
end

-- Version comparison: x.y.z >= a.b.c
local function version_ge(version, minimum)
    local function split(v)
        local t = {}
        for n in string.gmatch(tostring(v), "%d+") do table.insert(t, tonumber(n)) end
        return t
    end
    local a, b = split(version), split(minimum)
    for i = 1, math.max(#a, #b) do
        local x, y = a[i] or 0, b[i] or 0
        if x ~= y then return x > y end
    end
    return true
end

-- Shared pkg-config resolution for the on_load handlers. Read-only
-- queries (pkg-config --cflags/--libs/--modversion); nothing is
-- downloaded, installed or modified. Floors come from deps.json
-- (`pc_minimum`), the same file the flatpak manifest generator reads —
-- no version list is duplicated here.
-- NB: this helper is defined at root scope, where xmake exposes only a
-- restricted set of globals (no `raise`/`assert`/`import`); signal
-- failure with (nil, err) and raise from inside the on_load closure.
local function resolve_pkgconfig(import_fn, packages)
    local pkgconfig = import_fn("lib.detect.pkgconfig")
    local collected = { defines = {}, cxflags = {}, ldflags = {},
                        includedirs = {}, linkdirs = {}, links = {} }
    for _, name in ipairs(packages) do
        local info = pkgconfig.libinfo(name)
        if not info then
            return nil, table.concat({
                "pkg-config: package '" .. name .. "' not found.",
                "Install its development package (e.g. apt install <name>-dev / wordnet-dev),",
                "then re-run `xmake f`. Nothing is downloaded automatically.",
            }, "\n")
        end
        local min = pc_minimum(import_fn)[name]
        if min and not version_ge(info.version, min) then
            return nil, string.format("package '%s' >= %s required (found %s)",
                name, min, tostring(info.version))
        end
        for _, v in ipairs(info.includedirs or {}) do table.insert(collected.includedirs, v) end
        for _, v in ipairs(info.defines or {})     do table.insert(collected.defines, v) end
        for _, v in ipairs(info.cxflags or {})     do table.insert(collected.cxflags, v) end
        for _, v in ipairs(info.linkdirs or {})    do table.insert(collected.linkdirs, v) end
        for _, v in ipairs(info.links or {})       do table.insert(collected.links, v) end
        for _, v in ipairs(info.ldflags or {})     do table.insert(collected.ldflags, v) end
    end
    return collected
end

local function apply_pkgconfig(target, resolved)
    if #resolved.includedirs > 0 then target:add("includedirs", resolved.includedirs) end
    for _, v in ipairs(resolved.defines)     do target:add("defines", v) end
    for _, v in ipairs(resolved.cxflags)     do target:add("cxflags", v) end
    for _, v in ipairs(resolved.linkdirs)    do target:add("linkdirs", v) end
    for _, v in ipairs(resolved.links)       do target:add("syslinks", v) end
    for _, v in ipairs(resolved.ldflags)     do target:add("ldflags", v) end
end

-- Build an on_load handler that resolves pkg-config packages.
-- Root scope cannot import modules or run commands, so the metadata
-- lookup runs lazily inside the target's on_load callback.
-- spec.packages: list of pkg-config package names (all must exist).
-- spec.defines:  optional extra defines to add
local function pkgconfig_on_load(spec)
    return function (target)
        local import_fn = import
        local resolved, err = resolve_pkgconfig(import_fn, spec.packages)
        if not resolved then raise(err) end
        apply_pkgconfig(target, resolved)
        if spec.defines then target:add("defines", spec.defines) end
    end
end

-- POSIX on_load: pkg-config resolution plus the configure-time probes the
-- old build script performed (wordnet header, X11 headers, wordnet
-- library name). Failures stop the configure with a clear message
-- instead of a raw compiler/linker error deep into the build.
local function posix_on_load(spec)
    return function (target)
        local import_fn = import
        local resolved, err = resolve_pkgconfig(import_fn, spec.packages)
        if not resolved then raise(err) end
        apply_pkgconfig(target, resolved)
        if spec.defines then target:add("defines", spec.defines) end

        local has_cincludes = import_fn("lib.detect.has_cincludes")
        local check_csnippets = import_fn("lib.detect.check_csnippets")

        -- wordnet development header (wn.h) — hard error if missing
        if not has_cincludes("wn.h") then
            raise([[wordnet development headers (wn.h) not found.
Install wordnet-dev (Debian/Ubuntu), then re-run `xmake f`.
Nothing is downloaded automatically.]])
        end

        -- X11 headers — global hotkeys are a core feature; hard error
        -- (as the old configure did) instead of silently building without.
        if not has_cincludes({"X11/Xlib.h", "X11/Xatom.h"}) then
            raise([[X11 development headers (X11/Xlib.h, X11/Xatom.h) not found.
Install libx11-dev (Debian/Ubuntu), then re-run `xmake f`.]])
        end

        -- wordnet library name. Distros ship libwordnet, but a from-source
        -- build of WordNet 3.x installs libWN/libwn; probe in the order
        -- the old configure did and use the one providing `wninit`.
        local candidates = {"wordnet", "WN", "wn", "WordNet", "WORDNET"}
        local found = nil
        for _, lib in ipairs(candidates) do
            if check_csnippets({wordnet_lib_probe = ""}, {
                includes = "wn.h",
                funcs = {"wninit"},
                configs = {links = {lib}},
            }) then
                found = lib
                break
            end
        end
        if not found then
            raise([[wordnet library not found (tried: wordnet, WN, wn, WordNet, WORDNET).
No candidate provides the symbol 'wninit'. Install wordnet-dev
(Debian/Ubuntu) or WordNet 3.x from source, then re-run `xmake f`.]])
        end
        target:add("syslinks", found)
    end
end

-- ---------------------------------------------------------------------
-- libwni: static library from sources inside this project.
-- Linked statically into artha; no shared version is produced.
-- ---------------------------------------------------------------------
target("wni")
    set_kind("static")
    set_default(false) -- private; the archive is only a build intermediate
    add_files("src/wni.c")
    add_cxflags(common_cxflags)
    add_defines(common_defines)
    on_load(pkgconfig_on_load({
        packages = {"glib-2.0"},
    }))

-- ---------------------------------------------------------------------
-- artha: the application binary
-- ---------------------------------------------------------------------
target("artha")
    set_kind("binary")
    set_default(true)

    add_files("src/gui.c", "src/mod_notify.c",
              "src/suggestions.c", "src/hotkey_editor.c",
              "src/status_notifier.c")

    if is_plat("windows") then
        add_files("src/app_icon.rc")
        add_defines("WINVER=0x0501", "_WIN32_WINDOWS=0x0501", "_WIN32_WINNT=0x0501")
        add_cxflags("-mwin32", "-DUNICODE", "-D_UNICODE", "-DNDEBUG")
    else
        -- POSIX-only parts (X11 hotkey grabs, tomboy util, single-instance)
        add_files("src/tomboyutil.c",
                  "src/instance_handler.c")
    end

    add_deps("wni") -- static link of the in-project library

    add_cxflags(common_cxflags)
    add_defines(common_defines)

    if is_plat("windows") then
        add_deps("libnotify")
        add_cxflags("-mwin32")
        add_ldflags("-mwindows")
        on_load(pkgconfig_on_load({
            packages = {"gtk4"},
        }))
    else
        add_defines("DBUS_AVAILABLE", "X11_AVAILABLE")
        -- system libraries stay dynamic; never downloaded, never replaced
        add_syslinks("X11")
        -- WordNet has no pkg-config file on common distros; the library
        -- name is probed at configure time (libwordnet on distros,
        -- libWN/libwn from a source build) in posix_on_load below.
        on_load(posix_on_load({
            packages = {"glib-2.0", "gmodule-2.0", "gtk4", "x11"},
        }))
    end

    -- data files
    add_installfiles("data/gui.glade", {prefixdir = "share/artha"})
    -- the binary reads the icon via ICON_DIR (share/pixmaps/artha.png);
    -- the hicolor entry lets launchers resolve the menu icon by name.
    -- NB: the hicolor rule carries a distinct raw pattern ("|data") so
    -- xmake's extraconf lookup does not merge the two rules for the
    -- same source file.
    add_installfiles("data/artha.png", {prefixdir = "share/pixmaps"})
    add_installfiles("data/artha.png|data", {prefixdir = "share/icons/hicolor/128x128/apps", filename = "org.artha.Artha.png"})
    add_installfiles("data/artha.1", {prefixdir = "share/man/man1"})
    if not is_plat("windows") then
        add_installfiles("data/org.artha.Artha.desktop", {prefixdir = "share/applications"})
        add_installfiles("data/org.artha.Artha.metainfo.xml", {prefixdir = "share/metainfo"})
    end

-- ---------------------------------------------------------------------
-- Windows-only: vendored win32 libnotify (shared DLL)
-- ---------------------------------------------------------------------
if is_plat("windows") then
    target("libnotify")
        set_kind("shared")
        set_default(false)
        set_basename("libnotify")
        add_files("src/libnotify.c")
        add_cxflags("-Wall", "-Wextra", "-pedantic", "-std=c99",
                    "-Wno-unused-parameter", "-fstrict-aliasing", "-mwin32",
                    "-DUNICODE", "-D_UNICODE", "-DNDEBUG")
        add_defines(common_defines)
        add_cxflags("-mwin32")
        add_ldflags("-mwindows")
        on_load(pkgconfig_on_load({
            packages = {"glib-2.0"},
        }))
end
