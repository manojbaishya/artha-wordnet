# Decision record: WordNet data source (OEWN 2025 evaluation)

**Status:** proposal — human decision required.  
**Parent issue:** #78 (epic: evaluate moving WordNet data source to Open English WordNet 2025)  
**Related PR:** W2 PR (this document) — no code change, adds this document only.

---

## 1. Context / problem

Artha ships WordNet dictionary data today from Princeton WordNet:
- Host + CI lane: distro `wordnet`/`wordnet-dev` → libwordnet **3.0** + data at `/usr/share/wordnet` (**WordNet 3.0**).
- Flatpak lane: WordNet **3.0** library built from source, statically linked; **WordNet 3.1** dictionary overlaid at `/app/dict` via `DEFAULTPATH`.

`deps.json` pins both (URL + sha256); the Flatpak manifest is generated from `deps.json` and must not be hand-edited.

Open English WordNet (OEWN) 2025 and 2025 plus were released 2025-12-31, CC-BY 4.0, and ship a WNDB ("Princeton WordNet DB Format for legacy applications") archive. The central unknown was whether OEWN's WNDB works with the Princeton 3.0 `libwordnet`. W1 answers that: **yes**.

---

## 2. W0 — inventory of dictionary version per build lane

| Lane | Library | Dictionary | Dictionary path | How selected |
|------|---------|------------|-----------------|--------------|
| Host (Mint 21.3) | libwordnet-3.0.so (`wordnet-dev` 1:3.0-37) | WordNet **3.0** | `/usr/share/wordnet` | Compiled default; overridable via `WNHOME`/`WNSEARCHDIR` |
| CI (ubuntu-24.04, `.github/workflows/ci.yml`) | `wordnet`+`wordnet-dev` apt → libwordnet **3.0** | WordNet **3.0** | `/usr/share/wordnet` | Same compiled default / env vars |
| Flatpak (`deps.json` → generated manifest) | WordNet **3.0** built from source, static `libwordnet.a`, `-DDEFAULTPATH="/app/dict"` | WordNet **3.1** | `/app/dict` | `DEFAULTPATH` compile-time define |

`wn.h` defines `DICTDIR="/dict"` and `DEFAULTPATH="/usr/local/WordNet-3.0/dict"`; the `wn` man page documents `WNHOME` and `WNSEARCHDIR` (default `WNHOME/disk`). Verified via `strace` that libwordnet opens `data.*`, `index.*`, `cntlist.rev`, `sents.vrb`, `sentidx.vrb`, `*.exc`; `index.sense`/`index.key` absent are tolerated.

Existing integration tests (`tests/integration/test_wni_integration.c`) are version-tolerant: they assert structure only (`happy` → definitions+synonyms exist; nonexistent → false), never specific sense counts.

---

## 3. W1 — compatibility spike results

Tested four dictionaries against Artha's actual `libwordnet` calls (`wninit`, `is_defined`, `getindex`, `read_synset`, `morphstr`) via `WNSEARCHDIR` in the ubuntu:24.04 CI-matching container. Princeton 3.1 download was blocked from this environment (wordnetcode.princeton.edu unreachable) — covered by Flatpak precedent instead.

| Check | Princeton 3.0 (baseline) | OEWN 2025 | OEWN 2025 plus |
|-------|-------------------------|-----------|----------------|
| `wninit()` loads | ✓ | ✓ (2 non-fatal warnings: `sents.vrb`, `sentidx.vrb` missing) | ✓ (same warnings) |
| File set vs 3.0 | — | Missing `cntlist.rev`, `sents.vrb`, `sentidx.vrb`, `bin/`, `pixmaps/`, `wnres/` | Same as 2025 |
| Integration tests `artha-integration-test` (4/4) | ✓ 4/4 | ✓ 4/4 | ✓ 4/4 |
| Valgrind (`--leak-check=full --error-exitcode=1`) | 0 errors, 0 leaks | 0 errors, 0 leaks | 0 errors, 0 leaks |
| Dictionary size on disk | 28M | 31M | 36M |
| `wninit` time | instant | instant | instant |
| Peak RSS (lookup) | ~1960 KB | ~1928 KB | ~2064 KB |

Golden-word sense counts (noun\|verb\|adj\|adv), selected words:

| Word | P3.0 | OEWN 2025 | OEWN 2025+ | Notes |
|------|------|-----------|------------|-------|
| set (n) | 13 | 12 | 13 | OEWN trimmed one noun sense |
| know (v) | 11 | 10 | 10 | |
| give (v) | 44 | 43 | 43 | |
| seem (v) | 4 | 2 | 2 | |
| find (v) | 16 | 15 | 15 | |
| day (n) | 10 | 9 | 9 | |
| man (n) | 11 | 10 | 10 | |
| was/were (v) | 13 | 12 | 12 | |
| Washington (n) | 5 | **0** | 5 | proper noun dropped in 2025, restored in plus |
| Einstein (n) | 2 | **0** | 2 | proper noun dropped in 2025, restored in plus |
| Microsoft | 0 | 0 | 0 | |
| Google | 0 | 0 | 0 | |
| Apple (n) | 2 | 2 | 2 | |

Full per-word outputs are in `/tmp/wn-data/out-p30.txt`, `out-o25.txt`, `out-o25plus.txt` (not committed; W1 writes nothing to the repo).

### Hypotheses (from research) — confirmed / ruled out

| Hypothesis | Result |
|------------|--------|
| OEWN WNDB loads with Princeton 3.0 `libwordnet` | **CONFIRMED** — `wninit` succeeds, all integration tests pass |
| Missing `cntlist.rev`/`sents.vrb`/`sentidx.vrb` breaks the library | **RULED OUT** — all three missing from OEWN; library tolerates them as non-fatal warnings; Valgrind clean |
| `index.sense` required | **RULED OUT** — present in OEWN but absent on host P3.0 too; tolerated |
| OEWN content differs from P3.0 | **CONFIRMED** — small sense-count differences above |
| OEWN breaks Artha's `libwordnet` calls | **RULED OUT** — `wni_request_nyms`/`is_defined`/`getindex`/`read_synset`/`morphstr` all work |

**W1 conclusion:** OEWN 2025 and 2025 plus both load and work correctly with Artha's `libwordnet` usage. The data is usable. Remaining questions are editorial (sense counts differ slightly), policy (CC-BY 4.0 attribution), and product (which edition / optional vs default).

---

## 4. Options

### (a) Status quo
Keep host/CI on Princeton 3.0 distro data; Flatpak on 3.0 library + 3.1 dictionary.

- **Cost:** none (no change).
- **Risk:** none.
- **Reproducible:** yes (distro-managed on host/CI; URL+sha256 pins in `deps.json` for Flatpak).
- **Update path:** distro releases WordNet slowly; Princeton 3.1 is "database files only, no software".
- **Changes:** none to `deps.json`, manifest, tests, docs.

### (b) Move the Flatpak to OEWN 2025
Replace the Flatpak dictionary pin with OEWN 2025 (decide 2025 vs 2025 plus). Host/CI stay on Princeton 3.0.

- **Cost:** low — pin OEWN archive in `deps.json` (URL+sha256), regenerate manifest, update `DEFAULTPATH`/overlay path if layout differs, docs/BUILDING.md + README + attribution.
- **Risk:** low-medium — W1 confirms compatibility, but OEWN editorial choices differ (sense counts, proper nouns in plus vs not). OEWN quality "may differ from Princeton WordNet" (per project README).
- **Reproducible:** yes — OEWN 2025 WNDB: `https://en-word.net/static/english-wordnet-2025.zip`; plus: `https://en-word.net/static/english-wordnet-2025-plus.zip` (sha256 to be pinned at download time).
- **Update path:** OEWN releases ~yearly; re-pin new archive + sha256 in `deps.json`, regenerate manifest.
- **Changes:** `deps.json` (swap wordnet-dict pin), regenerated Flatpak manifest, docs/BUILDING.md, README dependency table, attribution notice (CC-BY 4.0), metainfo if needed. Integration tests stay version-tolerant (no hard-coded counts).
- **Edition choice:** 2025 (common words only) vs 2025 plus (adds curated proper nouns — Washington, Einstein restored). Human must pick.

### (c) Support OEWN as an optional dictionary
Default stays Princeton data; OEWN 2025 selectable via environment (`WNSEARCHDIR`) or a setting, documented as an option.

- **Cost:** medium — document `WNSEARCHDIR` usage; optionally add a settings toggle; pin both Princeton and OEWN archives.
- **Risk:** low — default unchanged, so no regression; opt-in OEWN tested in W1.
- **Reproducible:** yes — both archives pinned by URL+sha256.
- **Update path:** yearly OEWN re-pin; distro Princeton unchanged.
- **Changes:** docs (BUILDING.md, README) documenting `WNSEARCHDIR` override; optional `deps.json` entries; no code change required (libwordnet already honors `WNSEARCHDIR`).

---

## 5. Licence facts (report only; human decides)

- Artha: GNU GPL v2-or-later (`LICENSE`).
- OEWN data: CC-BY 4.0 (attribution required).
- Princeton WordNet: its own permissive licence text shipped in the tarball.

Bundling CC-BY 4.0 data in a Flatpak raises attribution and compatibility questions. This is not legal advice; the human decides whether attribution wording is needed in the metainfo/README/AUTHORS.

---

## 6. Recommendation (proposal)

**Provisional recommendation: (b) — move the Flatpak to OEWN 2025 plus**, with host/CI unchanged on Princeton 3.0 (option (a) for those lanes).

Rationale: W1 confirms OEWN works with `libwordnet`; the Flatpak already runs a 3.0 library against non-3.0 data (3.1 precedent); the "plus" edition restores proper nouns (Washington, Einstein) that plain 2025 drops, giving better coverage. But this is a **proposal** — the human chooses among (a), (b), (c) and between 2025 vs 2025 plus.

No implementation (W3) starts without explicit approval.
