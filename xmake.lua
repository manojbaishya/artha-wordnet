-- xmake.lua - Artha Thesaurus (xmake build)
--
-- Replaces the autotools build (configure.ac + */Makefile.am), which
-- has been removed from this tree.
--
-- Design notes
-- ------------
-- * Dependencies are NOT upgraded and NOT replaced: the exact same
--   system libraries used by the autotools build are used here, via
--   read-only pkg-config metadata queries (or -l flags for WordNet).
--   No xmake package downloads, no installs, no system changes.
--   Enforced minimum versions live in deps.json (repo root, `pc_minimum`)
--   and are the ones configure.ac enforced: gtk+-2.0 >= 2.24,
--   glib-2.0 >= 2.22, dbus-1 >= 0.60, dbus-glib-1 >= 0.60.
--   deps.json is the single source of truth for dependency pins:
--   tools/gen_flatpak_manifest.py reads the same file to emit
--   flatpak/org.artha.Artha.json.
-- * Libraries whose sources live inside this project (libwni) are built
--   as a static library target and linked statically into the binary.
--   No shared libwni is ever produced.
-- * System libraries (gtk, glib, dbus, X11, wordnet, ...) stay dynamic;
--   they are not checked beyond pkg-config metadata reads and are not
--   modified or replaced.
-- * Build artifacts are written to ./sandbox (gitignored). The legacy
--   autotools build was removed; xmake is the only build system.
--
-- Usage
-- -----
--   xmake                       # configure + build into ./sandbox
--   xmake f -m debug && xmake   # debug build (mode is a configure option)
--   xmake f --prefix=/opt/artha # override install prefix (default /usr/local,
--                               # autotools-compatible APP_DIR/ICON_DIR)
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
    set_description("Install prefix (autotools-compatible: APP_DIR=<prefix>/share/artha, ICON_DIR=<prefix>/share/pixmaps)")

-- NB: during the very first `xmake f`, get_config() may not see the
-- cached option yet; fall back to the option's default.
local prefix = get_config("prefix") or "/usr/local"
local app_dir = is_plat("windows") and "." or (prefix .. "/share/artha")
local icon_dir = is_plat("windows") and "." or (prefix .. "/share/pixmaps")

-- Common flags (were AM_CFLAGS in the autotools build)
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
    "GTK_DISABLE_DEPRECATED=1",
    "GDK_DISABLE_DEPRECATED=1",
    "GDK_PIXBUF_DISABLE_DEPRECATED=1",
    "G_DISABLE_DEPRECATED=1",
    -- package metadata macros formerly provided by config.h
    'PACKAGE="artha"',
    'PACKAGE_NAME="Artha"',
    'PACKAGE_TARNAME="artha"',
    'PACKAGE_VERSION="1.0.5"',
    'PACKAGE_URL="http://artha.sourceforge.net/"',
    'VERSION="1.0.5"',
    'APP_DIR="' .. app_dir .. '"',
    'ICON_DIR="' .. icon_dir .. '"',
}

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

-- Build an on_load handler that resolves pkg-config packages.
-- Root scope cannot import modules or run commands, so the metadata
-- lookup runs lazily inside the target's on_load callback.
-- Queries are read-only (pkg-config --cflags/--libs/--modversion);
-- nothing is downloaded, installed or modified.
--
-- spec.packages: list of pkg-config package names (all must exist).
-- Floors come from deps.json (`pc_minimum`), the same file the flatpak
-- manifest generator reads — no version list is duplicated here.
-- spec.defines:  optional extra defines to add
local function pkgconfig_on_load(spec)
    return function (target)
        import("lib.detect.pkgconfig")

        for _, name in ipairs(spec.packages) do
            local info = pkgconfig.libinfo(name)
            if not info then
                raise([[pkg-config: package '%s' not found.
Install its development package (e.g. apt install <name>-dev / wordnet-dev),
then re-run `xmake f`. Nothing is downloaded automatically.]], name)
            end
            local min = pc_minimum(import)[name]
            if min then
                assert(version_ge(info.version, min),
                    string.format("package '%s' >= %s required (found %s)", name, min, tostring(info.version)))
            end
            target:add("includedirs", info.includedirs or {})
            if info.defines then target:add("defines", info.defines) end
            if info.cxflags then target:add("cxflags", info.cxflags) end
            if info.linkdirs then target:add("linkdirs", info.linkdirs) end
            if info.links then target:add("syslinks", info.links) end
            if info.ldflags then target:add("ldflags", info.ldflags) end
        end
        if spec.defines then target:add("defines", spec.defines) end
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
              "src/suggestions.c", "src/hotkey_editor.c")

    if is_plat("windows") then
        add_files("src/app_icon.rc")
        add_defines("WINVER=0x0501", "_WIN32_WINDOWS=0x0501", "_WIN32_WINNT=0x0501")
        add_cxflags("-mwin32", "-DUNICODE", "-D_UNICODE", "-DNDEBUG")
    else
        -- POSIX-only parts (X11 hotkey, tomboy util, dbus single-instance)
        add_files("src/eggaccelerators.c", "src/tomboyutil.c",
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
            packages = {"gtk+-2.0"},
        }))
    else
        add_defines("DBUS_AVAILABLE", "X11_AVAILABLE")
        -- system libraries stay dynamic; never downloaded, never replaced
        add_syslinks("X11", "dbus-1", "dbus-glib-1")
        -- WordNet has no pkg-config file on common distros; -lwordnet is
        -- what configure.ac used. Static or shared flavour is the
        -- system's choice; the system is untouched.
        add_syslinks("wordnet")
        on_load(pkgconfig_on_load({
            packages = {"glib-2.0", "gmodule-2.0", "gtk+-2.0", "dbus-1", "dbus-glib-1", "x11"},
        }))
    end

    -- data files
    add_installfiles("data/gui.glade", {prefixdir = "share/artha"})
    add_installfiles("data/artha.png", {prefixdir = "share/pixmaps"})
    add_installfiles("data/artha.1", {prefixdir = "share/man/man1"})
    if not is_plat("windows") then
        add_installfiles("data/artha.desktop", {prefixdir = "share/applications"})
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
