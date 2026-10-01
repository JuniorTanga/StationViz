# StationViz: Review Findings and Build Plan

**Purpose of this file.** This is the durable record of the codebase review and the agreed plan.
It exists so that work can continue across context compaction without re-deriving findings.
Keep it updated as phases complete.

**Written:** 2026-10-01
**Repo:** `/mnt/d/Documents/Memoire/PFE2025/StationViz`
**Context:** Final-year project (PFE) for Factory Acceptance Tests (FAT) of IEC 61850 substations.

---

## 1. Decisions Agreed

| Question | Decision | Why |
|---|---|---|
| Development platform | **Linux first, Windows later** | Raw-socket GOOSE/SV and libpcap are far easier here. Every change can be verified by building. Cross-compile to MinGW at the end. |
| Strategy | **Harden `sclLib`, rewrite `sldLib`** | `sclLib` design is sound, its bugs are localized. SLD heuristics are the weak part: transformers invisible, endpoint detection dead, 3/2-breaker unrepresentable. |
| Definition of done | **Full FAT loop** | Load SCD, draw SLD, connect over MMS/GOOSE/SV, supervise live values, run test sequences, emit a report. |
| Test target | **No hardware, simulator only** | Verify against libiec61850's own server plus our own SCD fixtures. This is also the strongest thesis demo. |

---

## 2. Critical Blockers Found

These are the highest-value findings. All were verified by reading code, and several were reproduced.

### 2.1 `SldView` never repaints on load (flagship feature does not render)

`ui/cpp/Graph/SldView.cpp` contains **zero `connect()` calls**. `update()` is only reached from
the zoom/pan/selection setters, and `setNodes` only fires it when the model *pointer* changes.

Consequence: after `App.openSclFile(...)` the SLD canvas stays **blank** until the user zooms or
pans. `NodeModel::endReset()` emits `countChanged` and `SldPage.qml:98` binds to
`App.nodes.count` (so the *label* updates), but nothing triggers a repaint.

**Fix:** connect `NodeModel::countChanged` and `dataChanged` to `SldView::update()`.

### 2.2 Qt version mismatch blocks the Linux build

- Installed: **Qt 6.4.2** (`/usr/lib/x86_64-linux-gnu/cmake`)
- Declared: `CMakeLists.txt:11` has `qt_standard_project_setup(REQUIRES 6.8)`
- `ui/CMakeLists.txt` requests `Quick Concurrent Svg` but **never `Qt6::QuickDialogs2`**, which
  `QtQuick.Dialogs`' `FileDialog` needs. It linked on MinGW by accident.

### 2.3 No baseline commit

`git status` shows **143 modified files** and 5 untracked paths, with no commit to roll back to.
Nothing else in this plan is safe until this is committed.

Untracked: `core/sld/patch_sld_safe_minimal.diff`, `tests/CMakeLists.txt`,
`tests/scl_sld_demo/mnt/`, `tests/scl_sld_demo/nlohmann/`, `tests/scl_sld_demo/test_manifest.json`.

### 2.4 `core/network` has never compiled

17 hard errors. See Phase 4. Highlights:
- `CommandEngine.cpp`: four consecutive wrong libiec61850 signatures in 49 lines, written
  against a 1.4-era API.
- `NetworkManager.cpp:90`: `mmsByIed_[ep.ied]` default-constructs a **null** `unique_ptr`, then
  line 91 dereferences it. **Guaranteed crash on the first MMS endpoint.**
- GOOSE completely dead: `goCbRef` hardcoded `""`, and libiec61850 matches subscribers via
  `goCBRefLen == elementLength`, so zero never matches a real frame.
- SV fabricates every field as `UINT32` at 4-byte offsets; IEC 61850-9-2LE uses 16-byte CMV
  triplets (FLOAT32 mag + 4 B quality + 8 B timestamp).
- `parseMac` (`NetUtils.cpp`) splits on `':'`, but every SCD in the repo writes
  `01-0C-CD-01-00-01` with **dashes**. Returns `nullopt` for 100% of real data, and
  `NetworkManager::start()` ignores the return value, so GOOSE and SV fail silently.

---

## 3. State of `core/scl` (`sclLib`): keep, harden

**Verdict: the design is good. The claim that it is "robust enough" is not accurate.**

### What is genuinely good (keep all of this)
- pugixml tree walk, iterative, no recursion risk.
- **Zero Qt coupling** (verified: no `QObject`/`QString`/`Q_INVOKABLE` in any of the 10 first-party
  files). The README claim holds.
- Immutable heap-owned model via `unique_ptr<SclModel>`.
- **No pointer-into-`std::vector` hazards.** Every stored pointer was checked
  (`iedByName_`, `cnByPath_`, `datasets_`, `Result<ResolvedLNode>`). Pointers are only taken
  *after* the model is fully built.
- No XXE: `parse_default` omits `parse_doctype`.
- Every pugixml attribute access supplies a default, so no null `const char*` can reach a
  `std::string`.
- All numerics are `double`, so no integer overflow.

### P0: crashes and UB (roughly one hour)
| Fix | Location |
|---|---|
| `iedByName_.at()` throws out of `loadScl` (no try/catch anywhere). Reproduced with a `ConnectedAP` naming an IED absent from the file. Escape reaches `AppContext::openSclFile` → `std::terminate`. Worse, the throw happens *after* `model_` is assigned, so the manager is fully loaded while the caller believes it failed. | `SclManager.cpp:162`, `:174` |
| `std::stod` throws on garbage/overflow `<Voltage>`, and silently accepts `nan`/`inf` (which then reach JSON output). `as_string("0")` only defaults when the node is absent. | `SclParser.cpp:92` |
| `Result<T>::value()/error()/operator->` dereference a disengaged `std::optional`. That is UB, not an exception. | `Result.h:38-43` |
| `loadScl` replaces the model, dangling every pointer previously returned. `core/sld` caches `const ConductingEquipment*`/`const ConnectivityNode*`. Safe only by accident of load ordering. | `SclManager.cpp:195` |
| `loadScl` runs on a QtConcurrent worker while the GUI thread reads the same object. `busy_` guards only `loadSclAsync`, so `openSclFile` can run concurrently with it. | `SclManager.cpp:188`, `ui/cpp/AppContext.cpp:92` |
| Delete `core/scl/JsonWriter.{h,cpp}`: the `.cpp` does not compile and is a *different implementation* of the header (`os_`/`Ctx`/`sep()` vs `ss_`/`Context`/`startValue()`). | whole files |

### P1: correctness and conformance (biggest functional impact)
| Fix | Location |
|---|---|
| **Invert the layering.** Endpoints are computed *only* from `Communication/.../GSE\|SMV`, but SCL declares control blocks in `LN0` and maps addresses later. Reproduced: `LN0` with `GSEControl` + `SampledValueControl` and **no** `<Communication>` yields `gseEndpoints=0 svEndpoints=0 mmsEndpoints=0` **and zero diagnostics**. Enumerate `LN0` control blocks as the source of truth, then *join* the address. | `SclManager.cpp:308-363`, `:102`, `:126` |
| Read **all** `<Server>` elements, not `ap.child("Server")`. A two-server AP silently drops LD2. | `SclParser.cpp:185` |
| Handle the `<Equipment>` container and `<Container>`. A valid ED2 file yields `powerTransformers=0`. | `SclParser.cpp:254` |
| Read every `TapChanger` **and** `PhaseTapChanger`; change `tapChanger` to `vector<TapChangerInfo>`. | `SclParser.cpp:265`, `SclTypes.h:39` |
| Strip XML namespace prefixes. pugixml returns *qualified* names, so a `<scl:SCL>` file fails as "Missing `<SCL>` root". | `SclParser.cpp:240` |
| Feed `AccessPoint/Address` and `Server/Address` into `mmsEndpoints_`; default `apName` when absent (the key becomes `"IED1\|"`, which no `"IED1\|AP1"` lookup finds). | `SclManager.cpp:284-296` |
| **Add `ReportControl` / `RptEnabled` / `Inputs` / `ExtRef`.** Zero occurrences in the module. There is currently no way to do a B-report, which blocks the FAT loop. | new, after `SclParser.cpp:136` |
| Add `Header` to `SclModel` (there is nowhere to put it today). | `SclTypes.h:216` |
| Detect duplicates: `ConnectivityNode@pathName`, `DataSet@name`, `ldInst`, CE names. All four fail silently today (`datasets_.emplace` keeps the first, `cnByPath_[full]` keeps the last, no diagnostic). | `SclManager.cpp:241`, `:369` |
| **Fix `parseMac`** to accept `-` and `.` separators, and parse `APPID`/`VLAN-ID` as **base 16** (`4001` decimal is 16385; hex letters like `3FFF` would parse to 0). | `core/network/NetUtils.cpp`, `NetworkMapBuilder.cpp:30` |
| `CNAddress::ss` is never assigned by `parseConnectivityPath`, so `ResolvedEnd.ss` is always the containing substation. Breaks cross-substation transformer terminals. | `SclTypes.h:97`, `SclParser.cpp:20-24` |
| `Voltage` multiplier is stored as a string and never interpreted; no SI table. `std::stod` is locale-dependent (under `fr_FR`, `"380.5"` parses as `380`). | `SclParser.cpp:93-94` |

### P2: diagnostics quality
| Fix | Location |
|---|---|
| Remove duplicate reports: 3 real problems produce 8 diagnostics, same fact under two `ErrorCode`s. The `// déjà signalé` comments acknowledge it and emit anyway. | `SclManager.cpp:324-331, 351-358` vs `:162-183` |
| Add `line`/`col` to `Diag` via `pugi::xml_node::offset_debug()`. Today the only positional data is a byte offset for a whole-file parse failure. | `SclManager.h:89` |
| Stop overloading `InvalidPath`. Delete dead codes `FileNotFound`, `SchemaNotSupported`, `MissingMandatoryField` (documented in the README, never used) or wire them. | `SclManager.cpp:324`, `Result.h:10-13` |
| Add `to_string(ErrorCode)`, `operator<<`, `hasErrors()`, `hasWarnings()`. | `Result.h`, `SclManager.h:90` |
| Unify `Error` (load failure, no severity/location) and `Diag` (has both). The duplication is the root of the double-reporting. | `Result.h:25-28` |

### P3: cleanup
- **Wire the test suite.** It has **never run**: `core/scl/CMakeLists.txt` has no
  `add_subdirectory(tests)`, and `tests/CMakeLists.txt:55` references `test_scl.cpp` while the
  file is `tests_scl.cpp`. It also recompiles the module as a separate `scl_core` target instead
  of linking `sclLib`. **Current coverage is zero.**
- `SclParser` is a stateless namespace with no members. Delete the class, keep the file split,
  use free functions.
- Split `SclManager` (49 `std::cout` statements, 4 hand-rolled JSON serializers, validator, 9
  indexes): `SclDocument`, `SclValidator`, `SclJson`. Make printers take `std::ostream&` or delete.
- Delete `Internet.h` / `StringInterner` (global namespace, never cleared across reloads, pointless here).
- Extract `"<SS>"`, `"<VL>"`, `"<BAY>"`, `"CE:"` into named constants. `SclTypes.h:247` documents a
  *different* anchor format than what is actually produced (`S1:VL1:<VL>:<VL>`, VL key repeated).
- Fix ambiguous composite keys: `ied|ld|prefix+lnClass+lnInst` has no separator, so `A/XCBR/1`
  collides with `AXCBR1`. Same for the FCDA key.
- `DatasetKey::operator==` declared twice (member + free). `DatasetKeyHash` has no avalanche step.
- `getLn0Dataset`/`resolveDatasetMembers` do linear scans, ignoring the `datasets_` index built for
  exactly this. `toJsonNetworkMap` calls the latter per endpoint: O(n^2).
- Add `#include <map>` (`SclManager.cpp:998` compiles only via nlohmann's transitive include).
- `SclManager::fcdaToMmsRef` is a functional duplicate of `ObjectRefMapper::toMmsRef`. One
  implementation, not two.
- `EquipmentClassifier.h` is a hand-picked 13-class list that is not SCL semantics: it omits
  standard primary LNs (PIP/PIS, CAP, PDIF, PDIR, PFLT, PMTR, PTHR, RBRF, RECT, RPSB, RSPS, PTUF,
  ZLTP) while including pure protection functions. `is_excluded_ln` is dead code.
- `onReloaded` iterates `reloadCbs_` while invoking (re-entrancy UB), and runs on whatever thread
  called `loadScl` (a worker in our app) in a deliberately Qt-free core.
- Add `[[nodiscard]]` to `loadScl`, `parseFile`, `Result::value`.
- Fix README drift: `demo_main.cpp` and `SCL_BUILD_DEMO` do not exist; §4.1 omits 9 real API
  functions; §10 says "Idées de tests unitaires" when one suite exists. **Stop describing the
  library as IEC 61850-6 conformant**: it is a well-built SLD/network extractor, roughly 40% of the
  schema, with two crash paths.

### Open scope question: `DataTypeTemplates`
`LNodeType`/`DOType`/`DAType`/`EnumType` are absent, so `IED@type` template resolution is
impossible and FCDA cannot be validated. `DOI`/`DAI`/`Val` are absent entirely. This is the one
item that is a project rather than a fix, and it is **prerequisite for SV decoding from SCL types**
(Phase 4). Decide whether DO-level navigation is in scope; if not, leave it out but say so
honestly in the thesis.

---

## 4. State of `core/sld` (`sldLib`): rewrite

### Confirmed regressions
| Defect | Evidence |
|---|---|
| `endpointKinds` / `seriesPassKinds` never populated | `SldConfig.h:24,27` have no initialiser; `isPass`/`isEnd` (`SldBuilder.cpp:381,385`) iterate empty vectors, so both are constant `false`. Every caller passes `HeuristicsConfig{}` so nothing can fix it at runtime. **`Feeder::endpointType == "Unknown"` for 12/12 feeders on `substation.scd`.** A refactor regression: the dead predecessor `sld_/SldTypes.h:165,167` *had* the initialisers. The rewrite changed the element type from `EquipmentKind` to raw `int` and dropped them. |
| `"BB"` busbar hint lost | `SldConfig.h:18` = `{"BUS","BUSBAR","BBS","BARRE","BAR"}`. The predecessor `sld_/SldTypes.h:161` had `"BB"`. `BB2` in `scl.scd` is missed, and with it the explicit `COUP1` coupler. Add **both**. |
| `hardMin = 4` overrides `busDegreeThreshold{3}` | `SldBuilder.cpp:42`. The config field is only reachable at >= 5, the opposite of what "threshold" means. `patch_sld_safe_minimal.diff:12` even records `hardMin = 3`, so the committed patch would re-apply a value contradicting the source. |
| **`@cNodeName` resolved bay-scoped** | `SldBuilder.cpp:159`. IEC 61850 scopes it **voltage-level**. Verified on `scl.scd`: phantom `BB1`/`BB2` at degree 1 instead of the real `S12/E1/W1/BB1`; transformer `T1` lands on `S12/E1/TR1_E1/BB2`, so it never touches the busbar. `SclManager::matchCN` was built to fix exactly this and `SldBuilder` never calls it. |

### Measured failure across all fixtures
| file | Sub/VL/CE | rawV | buses | feeders | couplers | transformers |
|---|---|---|---|---|---|---|
| `scl.scd` | 1/2/9 | 20 | 2 (both singleton) | 2 | **0** | **0** |
| `test.scd` | 1/2/13 | 29 | **0** | **0** | 0 | 0 |
| `ied.scd` | 1/2/5 | 7 | **0** | **0** | 0 | 0 |
| `substation.scd` | 2/5/45 | 96 | 3 (all singleton) | 12, all `endpoint=Unknown` | 0 | **0** |
| `station1.scd` (170k lines, real Siemens ICD) | 0/0/0 | 0 | 0 | 0 | 0 | 0 |

**Every fixture with a `PowerTransformer` reports `transformers=0`.** `substation.scd` has T4/T3/T2
with 3/3/2 windings across 380/110/30 kV, all invisible.

**There is no assertion anywhere**: `tests/scl_sld_demo/main.cpp:246-268` writes the counts to
`sld.csv` and never checks them. Zero regression protection.

### Topology failure modes (all unhandled)
- **Double bus with section** (the most common real 400 kV arrangement): half the busbar is
  missing, the coupler is missed, attached transformers are missed, everything downstream is wrong.
- **3/2 breaker, ring bus, breaker-and-a-half**: `BusCoupler` is a single `busA`/`busB` pair
  (`SldTypes.h:109-114`), so a 3/2 string has **no representation for its middle breaker**.
  `TopologyHint::BreakerAndHalf` is declared and never read.
- **Double bus wrongly merged**: unioning on `DS` (`SldBuilder.cpp:267`) merges two physically
  distinct buses separated by a disconnector. For double-bus-with-section that is exactly the
  topology, and the wrong outcome is silent.
- **Meshed/ring**: the walk is a greedy non-backtracking line walk. A ring returns to a bus CN
  (skipped) and silently stops halfway.
- **Breakers in series**: any CB touching >= 2 buses is claimed as a coupler and excluded from
  feeder detection, so a mid-feeder breaker in a multi-circuit bay is drawn as a bus-to-bus edge.

### Other defects
- **Non-deterministic IDs**: `clusterIndex` is incremented while iterating an `unordered_map`
  (`SldBuilder.cpp:253`). Those IDs persist into `NodeModel`, `UiStore::selectionId` and
  `QSettings`. Sort the keys.
- **Parallel edges**: `add_edge` return values discarded (`:167`, `:305`) with `undirectedS` +
  default `listS` out-edges. `scl.scd` produces 2 parallel `Equip_to_Bus` edges.
- **`SldPlan` doubles peak memory**: `SldTypes.h:79-81` makes only the *vertex* container `vecS`;
  the edge list is `listS`, so `plan.condensed = condensed` (`:518`) is a deep copy of every
  out-edge list. The comment at `:518` claiming contiguity is factually wrong.
- **O(n^2)**: `clusterAndCondense:262` iterates all CEs once per `SS:VL` group; `:558` is
  `std::find` over clusters inside a loop over winding ends; `detectTransformers:499` is a linear
  cluster scan per transformer.
- **Pointer hazard**: `VertexProp` caches `const ConductingEquipment*`/`const ConnectivityNode*`
  into nested `std::vector`s (`SclTypes.h:62-63`). Safe only because the model is fully built
  before `build()`. `onReloaded`/`onUpdated` exist as invalidation hooks with **zero subscribers**,
  so nothing structurally prevents a future handler reading a stale `SldPlan`. Use-after-free
  waiting for one refactor. The pointers are used for exactly two things, both immediately
  copied into strings.
- **Errors silently swallowed**: no diagnostic is ever emitted from the SLD engine. A FAT tool must
  tell the user it could not classify a bay. `SclManager` has a whole `Severity::Error/Warning/Info`
  pipeline the SLD engine never uses.
- `Status` is returned but never carries information: all three stage functions return
  `Status::Ok()` unconditionally.
- `<map>` used at `SldBuilder.cpp:679` without the include (compiles via nlohmann).
- `SldManager::notify_()` does `const_cast<SldManager*>(this)` on itself.

### The key design problem
**All layout lives in `ui/cpp/AppContext.cpp`** (360 of its 852 lines are pure geometry).
`sldLib` emits only ranks and lane indices. Consequences:

1. `canonicalizeFeeder` (`AppContext.cpp:429-451`) exists because `Feeder::chain` is a flat
   unordered id list, so each element's *role* must be sniffed from equipment kinds, taking the
   first and last DS. It emits `DS, CB, DS, CT, Line`, placing the CT **after** the line-side
   disconnector. The real order is `DS, CB, CT, DS, Line`. **This misrepresents the primary
   topology.** It is a missing domain model papered over in the view.
2. The classification table is duplicated three times with three spellings: `mapEquipmentKind`
   (`SldBuilder.cpp:70-82`), `friendlyKindFromLnClass` (`AppContext.cpp:661-670`), `SldView::iconMap_`
   (`SldView.cpp:20-27`). `canonicalizeFeeder:435-436` compares against both `"CB"`/`"DS"` **and**
   `"CircuitBreaker"`/`"Disconnector"`, but `toString(EquipmentKind)` only ever emits the first pair.
   Two of four comparisons are dead.
3. `planJson()` is called twice per load (`:236` and `:161`), each re-serialising the whole
   condensed graph plus the IED tree: **two full serialise+parse round trips per file load.**
4. The output is not addressable, so no layout test can exist.
5. `AppContext.cpp:542-551` decides a VT hangs off the CT at `+vtDx`. That is a statement about
   station wiring, not about where to put a glyph.

### Rewrite design
Move the **structure**, keep the **pixels**.

- **In `sldLib`:** a `Feeder` gains a resolved, ordered role struct
  (`busSideDs, breaker, ct, lineSideDs, lineEndpoint, vtBranch`) computed once, so the
  `DS, CB, CT, DS, Line` ordering is a fact rather than a guess. `canonicalizeFeeder` disappears and
  the ordering bug goes away as a side effect.
- **In the UI:** the numeric layout, behind a settable `SldLayoutParams` struct instead of 8
  hardcoded consts at `AppContext.cpp:254-265`. This is genuinely view policy and should stay.
- **Eliminate the JSON round trip:** expose a typed API (`plan().buses`, `.feeders`, `.couplers`).
  Keep `planJson()` for `scl_sld_demo` and debug export only. This also removes schema-drift risk.
- **Represent topology explicitly** rather than hoping a greedy walk finds it: single bus, double
  bus with section, 3/2 breaker, ring/mesh.
- **Make the walk branch-aware.** The current "first unvisited candidate" over an `unordered_map`
  is topologically blind.
- **Drop Boost BGL** in favour of index-based adjacency over `vector`. This removes the
  pointer-stability hazard, the `SldPlan` deep copy, and the hash-map copy of the whole graph that
  currently happens three times (once per detector).
- **Deterministic IDs**: sort `SS:VL` keys before assigning cluster indices.
- **Wire up the existing `SclManager` helper** for VL-scoped CN resolution (`matchCN`,
  `logicalCNKey`, `mapCNByLogical_`), which `SldBuilder` already holds a pointer to and never uses.

---

## 5. State of `core/network` (`networkLib`): has never compiled

Not in the build: `core/CMakeLists.txt:3` has `add_subdirectory(network)` commented out.

### Compile errors (all verified)
| # | Location | Error |
|---|---|---|
| 1 | `NetworkManager.cpp:119` | Stray `);` after a `return`. Syntax error. |
| 2 | `NetworkManager.cpp:130` | Bare out-of-class *declaration* of `selectOperate` at namespace scope. |
| 3 | `NetworkManager.cpp:104` | `std::min(milliseconds, seconds)` type mismatch. `backoffMax` is `seconds(5)`. Also `#include <algorithm>` missing. |
| 4 | `NetworkManager.h:9`, `NetworkManager.cpp:2` | `#include "SvEngine.h"` but the file is `SVEngine.h`. Bites on any case-sensitive filesystem. |
| 5 | `TagRegistry.cpp:71` | `std::unique_lock` not found. Missing `<mutex>`. |
| 6 | `StateStore.cpp:15` | Same. |
| 7 | `MmsSession.cpp:145`, `GooseEngine.cpp:128` | `MmsValue_getOctetString(v, buf, size)` was removed in 1.5.1. Use `MmsValue_getOctetStringBuffer` + `memcpy`. |
| 8 | `DatasetResolver.cpp:12` | `sm.getIed()` does not exist. Use `SclManager::getLn0Dataset(ied, ldInst, dsName)`. |
| 9 | `DatasetResolver.cpp:16` | `getLogicalDevice()` does not exist (and `findLD_` is private). |
| 10-11 | `NetworkMapBuilder.cpp:36-37` | `GseEndpoint` has no member `name` (it is `cbName`), plus a cascading `push_back` failure. |
| 12 | `CommandEngine.cpp:18` | Args swapped: `ControlObjectClient_create(objectReference, connection)`. |
| 13 | `CommandEngine.cpp:23` | `setOrigin(self, const char* orIdent, int orCat)`. Passed `1, 1`. |
| 14 | `CommandEngine.cpp:29` | Sync `selectWithValue` returns `bool`, has no error out-param. A timeout requires the `...Async` variant. |
| 15 | `CommandEngine.cpp:37` | `operate(self, ctlVal, uint64_t operTime)`. Third arg is operate time, not timeout. |
| 16 | `CommandEngine.cpp:43` | `cancel(self)` takes one argument. |
| 17 | `PcapReplayer.cpp:9`, `InterfaceManager.cpp:6` | `pcap/pcap.h` not found. Environment issue (see below). |

Also: `CommandEngine.h:9` includes `<control.h>`, which is **server-side private**
(`inc_private/control.h`, pulls in `mms_server_libinternal.h`). It is not part of the client API
and is not installed. Delete it; `iec61850_client.h` already declares everything needed.

**Name collision landmine:** `network::Quality` (`NetworkTypes.h:40`) and `network::Timestamp`
(`:36`) collide with libiec61850's global `typedef uint16_t Quality;` and
`typedef union { uint8_t val[8]; } Timestamp;`. With `using namespace network;` any consumer
including a libiec61850 header gets ambiguity. Rename to `QualityBits` / `TimeStamp64`.

### Link errors
libiec61850 is **1.5.1**. It exports exactly two targets: `iec61850` (static, links `hal`, pulls
`-lpthread -lm -lrt`) and `iec61850-shared`. No config package, nothing installed.

- `core/network/CMakeLists.txt:24` has `find_library` **commented out**, so `${IEC61850_LIB}` at
  `:27` expands to nothing.
- `IEC61850_INCLUDE_DIR` names **one of eleven** needed directories. The real failure is
  `iec61850_client.h:31` cannot find `libiec61850_common_api.h`. It also misses the *generated*
  `stack_config.h`, which only exists in `${CMAKE_CURRENT_BINARY_DIR}/config/`.
- Required set: `src/common/inc`, `src/mms/inc`, `src/mms/inc_private`, `src/mms/iso_mms/asn1c`,
  `src/goose`, `src/sampled_values`, `hal/inc`, `src/logging`, `src/iec61850/inc`, plus the
  generated config dir.
- Use `PUBLIC`, not `PRIVATE`: our public headers expose `IedConnection`, `GooseSubscriber`,
  `SVSubscriber` in their signatures.
- **`BUILD_EXAMPLES` defaults to ON** (`libiec61850/CMakeLists.txt:31`) and drags in ~25 binaries
  plus `Findsqlite`. Must be forced OFF.
- **Windows has no Ethernet HAL in this checkout** without wpcap. `third_party/winpcap` is
  README-only, so `WITH_WPCAP` is false and `-DEXCLUDE_ETHERNET_WINDOWS` compiles GOOSE and SV out
  entirely. `GooseEngine::start()` then returns false.
- `find_package(PCAP)` **can never succeed here**: CMake 3.28 on this box has no `FindPCAP.cmake`,
  so it falls through to CONFIG mode and only warns. `HAVE_PCAP=1` is defined but **nothing checks
  it**; the pcap includes are unconditional. Use `pkg_check_modules(PCAP REQUIRED libpcap)`.
  Note `/usr/include/pcap.h` does not exist and `pkg-config` is not installed.
- **TLS is not available and is not a one-flag fix.** `libiec61850/CMakeLists.txt:129-132` enables
  mbedTLS only `if(EXISTS third_party/mbedtls/mbedtls-2.16)`. That directory contains **only a
  README**, so the generated `stack_config.h` has zero occurrences of `CONFIG_MMS_SUPPORT_TLS`.
  Enabling means vendoring mbedtls 2.16, then writing `TLSConfiguration_create`, then switching to
  `IedConnection_createEx`. **The implementation doc's TLS claim is currently unbacked by code.**
- **Raw sockets need `CAP_NET_RAW`** (`socket(AF_PACKET, SOCK_RAW, ...)`). `InterfaceManager` tests
  this correctly. `sudo` requires a password in this environment, so raw-socket tests will need
  `setcap` or a password prompt. The Linux HAL does **not** use libpcap; libpcap is only needed for
  `PcapReplayer`.

### Threading model: `spawnMmsWorker` is broken four ways
```cpp
spawnMmsWorker(m, ctx->rcbs);          // line 65
mmsByIed_[m.ied] = std::move(ctx);     // line 66  TOO LATE
// inside:
auto& ctx = mmsByIed_[ep.ied];        // line 90  default-constructs nullptr
ctx->th = std::thread([..., ctxPtr=ctx.get()](){ ... });  // line 91  NULL DEREF
```
1. Line 90 default-constructs a null `unique_ptr`; line 91 dereferences it. **Guaranteed crash on
   the first MMS endpoint.**
2. Line 66 then overwrites that entry, destroying what the thread captured. Use-after-free, not
   merely a stale read.
3. `ctx->th = std::thread(...)` calls `std::terminate` if `th` is already joinable, so the design
   cannot support restart.
4. Shutdown is broken: the worker can be in `sleep_for(1s)` or `sleep_for(backoff)` up to 5 s, so
   `stop()` blocks **up to 5 s per IED, sequentially**. 30 IEDs is a 150 s shutdown.

**Fix:** insert first, then spawn; hold the context as `std::shared_ptr`; make `MmsCtx`
non-copyable; use an interruptible wait.

**Better model.** `IedConnection_create()` is thread mode, so libiec61850 already spawns its own
receive thread. You currently pay **two threads per IED**, and the worker's only jobs are poll,
connect, `enableReports`, sleep. Use **one `MmsSession` per IED** (RCB state is per-connection) but
**one shared supervisor thread** walking the IED map. For a 30-IED substation that is 30 threads
to 1.

There is also a genuine race: `selectOperate` (`NetworkManager.cpp:118`) calls
`cmd_.operateSBOw(...getConnection()...)` from the **caller's** thread while the worker may be in
`connect()` on the same connection. `MmsSession::mu_` (`MmsSession.h:63`) is declared and **never
locked**. libiec61850 guards individual structures (`outstandingCallsLock`, `transmitBufferMutex`)
but not the request/response pair, so association corruption is possible. Serialize all
`IedConnection` calls behind `MmsSession::mu_`; report callbacks must not take it.

### Protocol correctness
**Reports (`MmsSession`)** — the sequence `getRCBValues → setDataSet → setRptEna →
installReportHandler → GI` is in the **wrong order**. `RptEna` is set at `:60` but the handler is
installed at `:63`, so reports in that window are **silently dropped**; for a buffered RCB you
never captured `EntryID`. libiec61850's own example installs the handler *first*.

- `rptId` passed as `nullptr` (`:63`). libiec61850 then matches `rptId` against the rcbReference,
  but real IEDs set `RptID` from SCL. **The callback will never fire on real equipment.**
- Hardcoded `DATSET | RPT_ENA` (`:59`) is **invalid for buffered RCBs**, which need `RESV_TMS`.
  `RcbConfig::buffered` is stored and never read.
- No `TrgOps`. Many IEDs ship with triggers off, so you get nothing until an integrity period
  fires (and `IntgPd` defaults to 0 = never). Use
  `TRG_OPT_DATA_CHANGED | TRG_OPT_QUALITY_CHANGED | TRG_OPT_GI`.
- `triggerGI` is deprecated; use `ClientReportControlBlock_setGI`.
- **No `EntryID`-based resync.** Buffered events between disconnects are lost, so buffered-report
  FAT results are wrong after any link drop.
- `MMS_UTC_TIME` returns `Timestamp{0}` (`:136-140`). `MmsValue_getUtcTimeInMs`,
  `getUtcTimeInMsWithUs` and **`getUtcTimeQuality`** all exist and are unused.
- `ClientReport_getDataSetValues` returns the full array with unchanged members holding their
  **previous** values. The loop at `:111-120` republishes **every** member on every report. Use
  `getReasonForInclusion` to skip `NOT_INCLUDED`. This is why the EventBus is so hot.
- Index→objectRef mapping relies on SCL FCDA order matching the server's DataSet order. If they
  differ, **every value is silently misattributed.** Validate with
  `IedConnection_getDataSetDirectory` and emit a diagnostic on mismatch.
- `enableReports` returns `true` unconditionally, so reporting can be entirely dead with no signal.
- `DatasetResolver.cpp:24` builds `ldInst + "/LLN0." + ds.name`, but `setDataSetReference` wants
  the **`$` form**. `:27` ignores `fcda.ldInst` (SclManager does this correctly), so cross-LD dataset
  members get the wrong domain prefix.

**`NetworkMapBuilder` fabricates the RCB reference.** `ldInst + "/LLN0.RP" + ge.name` synthesizes a
report control *from a GOOSE control block*. `ReportControl` blocks are declared in `LN0` and are
unrelated to `GSEControl`; `SclTypes.h` has **no `ReportControl` struct at all**. Every GOOSE
endpoint pushes an RCB, so an IED with 5 GOOSE blocks and 3 real report controls produces 5 RCB
entries (3 wrong) and misses 2 real ones. `getRCBValues` on a bogus ref returns an error, hits
`continue` at `MmsSession.cpp:49`, and is silently skipped: a plausible-looking setup with reports
mostly dead.

**GOOSE (`GooseEngine`)** — `goCbRef` hardcoded `""` (`NetworkManager.cpp:40`) means
`goCBRefLen = 0`, which never equals a real `elementLength`, so `matchingSubscriber` stays NULL and
`parseGoosePayload` returns immediately. **GOOSE reception is completely non-functional.** The
correct ref is `ldInst + "/LLN0$GO$" + cbName` (`$` separators, per `mms_goose.c:776`), or use
`GooseSubscriber_setObserver` for promiscuous sniffing. Pick one, not the empty middle.

- No `unsubscribe`. `subs_` grows monotonically; `GooseReceiver_removeSubscriber` is never called.
- The stop/add/restart hack per subscription (`GooseEngine.cpp:61-67`) **joins the thread and
  recreates the Ethernet socket** every call. With 20 control blocks you churn 20 raw sockets and
  lose frames in each window. Add all subscribers, then a single `start`.
- **`stNum` / `sqNum` / `isValid` / TAL are never read** (all available in `goose_subscriber.h`).
  **This is the most dangerous omission in the layer.** `isValid` goes false when the stream goes
  stale: your HMI would show a frozen breaker position as if it were live. Surface `isValid` +
  `getTimestamp` and drive them into a quality flag.
- `needsCommission` (`ndsCom`) is not honoured, though the spec requires it to be ignored.
- No duplicate suppression: unchanged state is republished on every 500 ms retransmission.
  Comparing `stNum` to the last published value would cut EventBus traffic by ~99% in steady state.
- VLAN filtering is not settable on a subscriber in 1.5.1.
- `mmsToTagValue` is copy-pasted from `MmsSession.cpp:126-172` (both missing `MMS_STRUCTURE`).

**SV (`SVEngine`)** — the field specs are fabricated and this is **not fixable by tweaking a type**.
SV is not self-describing; `sv_subscriber.h:47-53` says so explicitly. Offsets must come from the
FCDA bType and order of the dataset named by `SampledValueControl@datSet`. For 9-2LE each
measurement is a **16-byte CMV triplet**, not 4 bytes. So `offset += 4` is wrong by 4x, and every
`mag` is followed by a 4-byte quality field that UINT32 decoding reads as garbage.
`SclTypes.h` has **no bType/length at all** (`FcdaRef` is only ldInst/lnClass/lnInst/doName/daName/fc),
so `SclParser` must capture the FCDA's referenced DA type from `DataTypeTemplates`. **This is a
prerequisite for SV being useful and depends on the `DataTypeTemplates` decision (P1).**

- `readField` has **no bounds check**. `SVSubscriber_ASDU_getINT32U` does a raw `memcpy` with no
  validation against `SVSubscriber_ASDU_getDataSize`. With fabricated offsets that can run past the
  ASDU: an **out-of-bounds read**.
- `SVReceiver_disableDestAddrCheck` / `enableDestAddrCheck` never called. `SVReceiver_create` sets
  `checkDestAddr = false`, so **dst MAC filtering is silently disabled**; the MAC is stored and
  never compared.
- No `svID` filtering (the real discriminator when APPIDs are shared), no `confRev` validation, and
  `smpCnt` / `SmpSynch` / `smpRate` / `RefrTm` all unread. `handleSv` publishes with **no
  timestamp**.
- 9 of 20 `SvType` cases return `monostate`/`0` (`SVEngine.cpp:104-113`).
- `start()` ignoring `subscribe` return values means **all** GOOSE/SV failure is silent.
  `if (!sv_->start(iface_)) return false;` also means a missing SV privilege kills the whole manager
  including MMS. Degrade gracefully: MMS should start regardless.

**Control (`CommandEngine`)** — the SBOw sequence is structurally wrong for a FAT tool.

- `ctlVal` type is wrong. A hardcoded `MmsValue_newIntegerFromInt64` is rejected by any real IED for
  an SPCSO (needs boolean). An APC needs an `AnalogueValue` **structure**; SPCSI/DPCSI need two
  control values plus `ctlNum`. Use **`ControlObjectClient_getCtlValType`** and build per type.
- Control model is never detected. Always SBOw fails on `DIRECT`. Use `ControlObjectClient_createEx`
  / `getControlModel`. **This is the #1 interop issue for control.**
- `timeoutMs` is silently ignored (sync API has none). Use the `...Async` variants.
- `cancel` is issued **after a successful `operate`**, which consumes the select. Cancel only on
  the failure path.
- **No `CommandTermination` handling.** Without `setCommandTerminationHandler` you cannot tell
  whether a breaker actually operated. Reporting "operate succeeded" when only the select was
  accepted is a **false pass**. Non-negotiable for FAT.
- The IEC 61850 `test` bit is never set (`setTestMode` exists).
- No status read-back after operate (operate success != state change), no `LastApplError`.

**The "Test mode" FAT safety gate is the most dangerous thing in the repo.** `CommandEngine.cpp:11`
(`if (mode_.load() != Mode::Test) return false;`) is a *client-side software flag* with **zero
effect on the IED**: it does not set the IEC 61850 `test` bit, so a "Test mode" operate is a **real
operation** and the IED will trip a real CB. It is also toggled by a **command-line flag**
(`network_demo/main.cpp:52,99`). A mode that gates breaker operations must not be settable from a
CLI flag on a laptop. A defensible gate: set the `test` bit, verify the `CommandTermination` is
`AddCause`-consistent, require an explicit arming action with a visible countdown, log every
command to an immutable audit file, and require the station to be in a declared test state.

### Event pipeline
The shape is right: `wire → MmsValue → TagValue → StateStore::set() → EventBus::publish() → UI`.
FNV-1a 64 for `TagId` is a good choice (fast, no allocation, stable). Two problems:

- `TagRegistry.cpp:58` falls back to `std::hash<std::string>`, which is **not stable across runs or
  platforms**, breaking the stated contract in `NetworkTypes.h:12`. It also drops `ied` from the
  hash, so identical unparseable refs on two IEDs collide. Use FNV on the raw ref.
- The hash has **no canonicalization**: `LD0/XCBR1.Pos.stVal[ST]` and `ld0/xcbr1.pos.stval[st]` are
  different tags for the same object. A lowercase-emitting IED silently creates a shadow tag set.
- `EventBus::publish` copies the subscriber vector and every `std::function` on **every publish**.
  At 4000 Hz SV x 30 tags that is 120k allocations/sec. Also, publishing on the GOOSE/SV receive
  thread means a slow subscriber **stalls packet reception**. Use a copy-on-write subscriber list
  or a lock-free MPSC queue drained by a publisher thread.
- `StateStore` deep-copies the `TagValue` variant (which holds a `string` and two `vector`s) on
  every set. Combined with the EventBus copy that is 2-3x per update.

### Other network defects
- `PcapReplayer.cpp:66-69` computes replay timing as `dt_src` from `first_ts` (cumulative, not a
  delta), so replay is progressively slower and asymptotically wrong. Use `hdr->ts - prevTs`.
- `NetUtils.cpp:16-19` does not check that the whole token was consumed, so `"zz"` yields
  `00:00:00:00:00:00` with `bs.fail()` false.
- `TagRegistry.cpp:3` includes `<regex>`, never used.
- `tests/network_demo/main.cpp:80` calls `sm.loadFromFile()`; the real API is `loadScl()`.
  Its `CMakeLists.txt:15` links target `network`; the library is `networkLib`.

---

## 6. State of `ui/`

### `AppContext` is a god object
94 (`.h`) + 852 (`.cpp`) = **946 lines**, 8 responsibilities: owns 7 objects, synchronous **and**
threaded file loading, diagnostics mapping, plan-JSON → model translation, **the entire SLD layout
algorithm** (360 lines of geometry), feeder canonicalisation, IED grouping, two inventory builders
that re-parse the same `graph` sub-object, and icon resolution.

`fillDiagnosticsFromScl` (`:119`) **throws away `Diag::severity` and `Diag::hint`**, collapsing
everything to `"info"`/`"warn"` at `:123-125`. The strict acceptance logic in the test harness keys
off exactly the severity the UI discards.

Target: `SldModelBuilder` (~80), `InventoryBuilder` (~150), layout engine (~120), leaving
`AppContext` as ~200 lines of wiring.

### `SldView` (`ui/cpp/Graph/SldView.cpp`, 423 lines)
Correct: the zoom-about-cursor math (`:380-390`), `setZoom` clamping to `[0.05, 20]` applied before
the pan recomputation, and `orthPolyline` (`:149-180`), which produces readable L-routes.

Bugs:
- **Never repaints on model change** (see 2.1). The flagship feature.
- `iconWorldHeight`/`iconWorldWidth` properties are **declared with setters and never read**;
  `SldView.cpp:345` hardcodes `46.f`. Setting it from QML re-renders identical pixels.
  The `46.f` also causes real overlap: layout pitch is `chainStepY = 44.0` (icons 46 tall overlap
  vertically), and a 46-tall `vt.svg` (aspect 1.71) is **78.5 wide**, offset 56 right of the CT, so
  its right edge lands at `ct.x + 95` while the next lane is at `ct.x + 72`. **The VT icon swallows
  the adjacent lane on any multi-lane bay.**
- `dragging_` is **permanently sticky**: no `mouseReleaseEvent` exists to reset it. After one
  middle-drag every subsequent move pans the view, and clicking a node drags the canvas.
- Clicking empty space never deselects, so `PropertyPanel` shows "Élément sélectionné" forever.
- Bus and Junction nodes get drawn as 12x12 wireframe boxes (`:252-260` emits line vertices for
  every node with no kind filtering), so every bus has a black square on its thick span.
- Missing endpoints fall back to `pos.value(id, {})` = `(0,0)`, so a dangling edge is drawn to the
  origin. Reachable: `AppContext.cpp:573-598` uses `ensureBus(..., 180.0, busY)` fallbacks that can
  collide.
- `NodeModel - Copie.h` is a backup copy **with the same `Q_OBJECT`** on the AUTOMOC include path.

Performance: `updatePaintNode` does a **full rebuild on every pan and zoom frame**. It rebuilds two
`QHash`es from the whole model (`:183-192`), allocates four fresh `QVector`s (`:198,220,248,271`),
and re-uploads all vertices (`:210,236,248,271`). The `UpdatePaintNodeData*` parameter (`:90`) is
accepted and ignored, so `cleanNode`/`isDirty` are not exploited. For 3000 nodes that is ~50k vertex
writes plus 6000 hash insertions **per mouse-move event**. Fix: cache built geometry, rebuild only
on model change, and put pan/zoom in an outer `QSGTransformNode`.

- `DrawLines` is 1 physical pixel regardless of zoom, with no minimum weight. At `zoom_ = 20` every
  edge is a hairline. `QSGGeometry::DrawLines` ignores width, so only the bus span (a thick quad via
  `pushThickRect`) looks professional.
- Hit testing is a linear O(N) scan with a **world-space** 16-unit radius, independent of `zoom_`:
  3 screen px at `zoom_ = 0.2`, 160 screen px at `zoom_ = 10`.
- `SldIconAtlas::build` runs **on the render thread** (`:294` calls it from `updatePaintNode`):
  `QPainter` on a `QImage`, a `QSvgRenderer` per icon. `QSvgRenderer` is a `QObject`-backed XML
  document parser and is unsupported there. It also mutates `atlasBuilt_` and the `mutable
  QHash texPerWin_` from the render thread with no synchronisation.
- `texPerWin_` is keyed on a raw `QQuickWindow*` and **never pruned**: destroying and recreating a
  window leaks the `QSGTexture` and keeps a dangling key.
- The atlas is **NPOT with mipmap filtering** (6 icons, 3x2 grid of 100 px cells → 304x204). NPOT
  plus mipmaps is implementation-defined in GL ES, and with a 2 px gutter mip levels >= 1 bleed
  neighbouring icons together.
- `unknown.svg` has no `viewBox`, so its aspect silently falls back to 1.0.
- `mouseMoveEvent` (`:413`) accepts unconditionally, stealing moves from any child item. `Legend`
  and `MiniMap` are children of `SldView` (`SldPage.qml:54,61`).

### QML
- **No `qt_add_qml_module`.** Everything via `qml.qrc` with **relative directory imports**
  (`import "qml/pages"`). Consequences: no `qmlcachegen` (every file interpreted at startup, e.g.
  `InventoryPage.qml` at 368 lines with four nested `Repeater`s), **no compile-time QML checking**,
  and **no `qmllint`**, which *is* installed (`/usr/lib/qt6/bin/qmllint`) and would catch the
  current errors at build time.
- **Theme singleton is broken three ways.** `Theme.qml:3` has no `pragma Singleton` and is
  instantiated **9 times** (one per importing file), so `darkMode` is per-instance. `ThemeSettings.qml`
  declares `pragma Singleton` but is **never referenced anywhere** and nothing binds to it; without
  a `qmldir` the pragma is ignored. `App.qml:39-40` has the dark-mode handler **commented out**, and
  `Theme.qml:7` has no `Settings` binding. Dark mode is not broken, it is unimplemented behind a
  façade.
- **Four disjoint hardcoded palettes.** `Theme.qml` has a coherent light+dark palette, but
  `SldPage.qml:47-49` binds the SLD canvas to the theme's **private light-mode** properties
  (`_L_edge`, `_L_node`, `_L_accent`) so the diagram would stay light even with a working theme.
  `InventoryPage.qml:22-45` defines a wholly separate palette. `SegmentedTabs.qml` a third.
  Plus ~30 scattered literals in the other components.
- `PropertyPanel.qml:18-39` is the worst performance offender, exactly as suspected: a `ListView`
  over the **entire** node model where each delegate is an `Item` + `Column` + 3 `Text`s, all bound
  and alive, 99.97% `visible: false`. ~12,000 QObjects for a 3000-node station to display one
  node's three fields. Fix: `Q_INVOKABLE QVariantMap getById(id)` plus a plain `Column`.
- `SldLabelsOverlay.qml:31` binds `property var n: nodeModel.get(index)`. `get` is a plain
  function, not a property, so **the binding never re-evaluates when data changes**. Load a second
  SCD with the same node count and the labels show the **first** station's text. Also N map
  allocations per rebuild, and every non-Junction node gets a 12 px label with a translucent
  background, which on a real substation is a wall of overlapping text.
- `InventoryPage.qml:98-229` and `:241-365`: four levels of nested `Repeater`, no virtualisation
  (~16,000 objects for a 2000-CE model), and `App.qml:75-106` `StackLayout` **instantiates all five
  pages eagerly** and only toggles `visible`. Use five `Loader`s.
- `IedPage.qml` correctly uses a `GridView`, but `IedCard.qml` nests a `Flickable` + `Flow` +
  `Repeater`s per LD, there is no `reuseItems`, and cell/delegate sizes mismatch (360/340 and
  220/200).
- `Legend.qml:9` references `theme` before its `id` on `:10-12` (legal via hoisting, fragile).
- `IedCard.qml:54` hardcodes `#FFFFFF` regardless of theme.

### Five tabs
| Tab | State |
|---|---|
| IED | Real (`GridView` + `IedCard`, `model: App.ieds`) |
| SUBSTATION | Real (`SldView` + Legend + MiniMap + labels + PropertyPanel), but blank on load (2.1) |
| INVENTORY | Real, two modes (physical SS/VL/Bay and IED/LD/LN) |
| COMMUNICATION | **Placeholder**, `App.qml:102` |
| TESTS | **Placeholder**, `App.qml:105` |

The COMMUNICATION page is the cheapest high-value addition: `SclManager::toJsonNetworkMap()` already
produces the right schema (`mms[]`, `gse[]`, `sv[]` with `dataset` + resolved `members[]`).

`iconForKind` (`AppContext.cpp:655`) returns `":/icons/equipment/busbar.svg"`, **which does not
exist** and is not in `qml.qrc`. (`SldView.cpp:27` has the same path commented out, so the canvas is
unaffected, but `InventoryPage` would show a broken image.)

`AppContext.h:70` declares `static bool parseAnchor(...)` that is never defined or called.

---

## 7. Tests

| Target | Kind | Wired? |
|---|---|---|
| `scl_sld_demo` | Batch runner reading a JSON manifest, 6 CSVs, non-zero exit on failure | Yes |
| `scl_tests` | Real GoogleTest, 193 lines, `TEST(Scl, LoadAndIndexes)` | **No.** Never run |
| `network_demo` | Interactive `main()`, does not compile | **No** |

**`scl_tests` has never executed** (no `add_subdirectory(tests)`; `test_scl.cpp` vs
`tests_scl.cpp`; recompiles the module as `scl_core` instead of linking `sclLib`). Treat current
coverage as zero.

**The manifest is out of sync with the data directory.** `test_manifest.json` references
`SCD_2VL_TR_v2.scd`, `SCD_IED_ONLY_MIN.scd`, and `SCD_IED_WITH_ANCHORS.scd`, **none of which exist**
in `mnt/data/` (15 files present). `output_dir` is the absolute Linux path `/mnt/data/test_out`.

`scl_sld_demo` writes `coverage/scl/sld/network/perf/results` CSVs. Its acceptance logic
(`main.cpp:300-347`) requires zero `Severity::Error` for `ref`/`variant`/`network`/`heavy`, and the
exact `expected_errors` set for `invalid`. It **never asserts the SLD counts**, which is why the
`transformers=0` regression survived.

### Fixtures available (reuse these, do not author new ones yet)
`tests/scl_sld_demo/mnt/data/` (15 files): reference topologies `SCD_SB_2L`, `SCD_DB_COUPLER`,
`SCD_2VL_TR` plus variants and `SCD_PRIVATE_HEAVY`; network `SCD_NET_GOOD`,
`SCD_NET_MISSING_DATASET`, `SCD_NET_BAD_LD`; invalid `SCD_BROKEN_CN`, `SCD_DUP_IED_NAME`; stress
`SCD_HEAVY_SMALL` (2.3 MB), `SCD_HEAVY_LARGE` (11.2 MB); plus `tmp/`.

`tests/tests_files/`: `station1.scd` (6.0 MB, 170k lines, real **Siemens SIEDIG** ICD with
`Header`, `Private`, `xsi:schemaLocation`, vendor namespaces, `DataTypeTemplates`), `substation.scd`
(31 KB), `ied.scd`, `scl.scd` (7.5 KB hand-written with BBS1/COUP1/Q01_CB/Q01_DS), `test.scd`, plus
`temp/` (`station_TOG.scd` is a byte-identical 6 MB duplicate of `station1.scd`).

Note `station1.scd` currently yields `Sub/VL/CE = 0/0/0`, so it is a **free acceptance test** for
Phase 1's namespace and multi-server work.

Real SCD values observed: `<P type="MAC-Address">01-0C-CD-01-00-01</P>` (dashes, confirming 2.4),
`<P type="APPID">0001</P>` and `4001` (hex, confirming the base-16 requirement),
`<P type="VLAN-ID">000</P>`.

---

## 8. Phase Plan

### Phase 0: green build on Linux, with a rollback point
**Gate: the app launches and shows a diagram.**
1. Commit current state as a baseline (143 modified + 5 untracked).
2. Lower `REQUIRES 6.8` to 6.4 (or install Qt 6.8+). Add `Qt6::QuickDialogs2` to `find_package`.
3. Enable warnings (`-Wall -Wextra -Wpedantic`), currently commented out at `core/scl/CMakeLists.txt:27`.
4. Delete dead trees: `core/_scl/`, `core/sld/sld_/`, `Main.qml` (references a nonexistent
   `sldFacade`; not in `qml.qrc`), `core/sld/JsonWriter.{h,cpp}` (`.cpp` does not compile),
   `SldJson.{h,cpp}` (0 bytes), `core/sld/IdFactory.h` (unused, logic duplicated in
   `SldBuilder.cpp:25-35`), `core/sld/nlohmannJson/json.hpp` (25k-line duplicate of
   `core/scl/nlohmann/json.hpp`), `patch_sld_safe_minimal.diff` (stale), `ui/cpp/header.h`,
   `ui/cpp/source.cpp` (0 bytes), `NodeModel - Copie.h`, `app/`, `tests/scl_demo/`,
   `tests/tests_files/temp/station_TOG.scd` (duplicate), `tests/scl_sld_demo/mnt/test_manifest.json`
   (superseded).
5. Fix the trivial UI bugs so a diagram is trustworthy: connect `NodeModel::countChanged` to
   `SldView::update()`, reset `dragging_`, deselect on empty click, add `busbar.svg`, drop the
   duplicated bus-span loop at `AppContext.cpp:555`.
6. Wire `core/scl/tests` and fix `test_scl.cpp` → `tests_scl.cpp`.

### Phase 1: make `sclLib` trustworthy
**Gate: the test suite runs and every defect in §3 has a regression test.**
P0 crashes and UB, then the layering inversion, then `ReportControl`, then namespaces/`Server`/
`Equipment`/duplicates, then diagnostics quality, then the `parseMac` and base-16 fixes.

### Phase 2: rewrite `sldLib` topology-first
**Gate: an acceptance table over all 15 fixtures, asserted in CI, with transformers and couplers
non-zero where they should be.** Design in §4: structure into core, pixels into the UI, drop Boost,
explicit topology classes, deterministic IDs, typed API.

### Phase 3: the simulator
Since there is no hardware, the simulator is the test rig and must be **SCD-driven** so it
exercises the real import path. Build N simulated IEDs on loopback MMS with reports, plus GOOSE
publishing and an SV stream, generated from our own fixtures. This is what makes the full FAT loop
verifiable and is the strongest thesis demo. Keep it a separate target that never ships in the app.
Uses libiec61850 `examples/server_example_goose`, `server_example_control`,
`iec61850_9_2_LE_example`, `sv_publisher` as references.

### Phase 4: make `networkLib` real
**Gate: 17 compile errors fixed, GOOSE receiving, SV decoding from SCL types, control with
`CommandTermination`.** Linkage, threading, then reports, GOOSE, SV, control, per §5.
**Hard dependency on the `DataTypeTemplates` decision from Phase 1** for SV.

### Phase 5: UI to the full FAT loop
**Gate: the two new tabs work and the SLD shows live state.** Split `AppContext`, move structure into
`sldLib`, convert to `qt_add_qml_module` and delete `qml.qrc`, fix the Theme singleton, collapse the
palettes, replace the three non-virtualised views, fix `SldView` performance and the render-thread
atlas, then build the COMMUNICATION and TESTS tabs plus the report generator.

### Phase 6: report generation and polish
HTML report with pass/fail, evidence and diagnostics. Then cross-compile to Windows/MinGW, noting
that libiec61850 there needs Npcap for GOOSE or GOOSE/SV compile out entirely.

---

## 9. Honest Assessment of Scale

Phases 0 to 2 are roughly **two to three weeks** of focused work and yield a correct, tested SCL and
SLD layer. The full FAT loop (SV from SCL, control with `CommandTermination`, test/report UI) is
substantially more.

**Recommendation: treat Phase 0 + 1 + 2 as the next milestone**, verifying each, rather than
attempting all six phases at once.

---

## 10. Open Questions Still To Decide

1. **`DataTypeTemplates` scope.** Absent today. It is a project rather than a fix, and it gates
   both FCDA validation and SV decoding. In or out? If out, the thesis must not claim DO-level
   navigation or SV-from-SCL.
2. **TLS (IEC 62351).** The implementation doc claims it via `IedConnection_createEx`. Unbacked:
   mbedtls is a README only, so the build has zero `CONFIG_MMS_SUPPORT_TLS`. Vendor mbedtls 2.16
   and write `TLSConfiguration`, or drop the claim?
3. **`CAP_NET_RAW` for tests.** `sudo` needs a password here. Raw-socket GOOSE/SV tests need
   `setcap cap_net_raw+ep` on the test binary, or a password prompt per run. Decide the workflow
   before Phase 4.
4. **libpcap.** `/usr/include/pcap.h` is absent and `pkg-config` is not installed. Is `PcapReplayer`
   (pcap replay/injection) actually needed for the PFE, or is it a nice-to-have? If not needed,
   drop it from `networkLib` and remove the whole `find_package(PCAP)` problem.

---

## 11. Progress Log

Append entries here as phases complete. Newest at the bottom.

- **2026-10-01** Review completed, plan written. No code changed yet.
- **2026-10-01** Baseline commit `675e9c3` (143 modified + 5 untracked paths captured, tree clean).
  `DataTypeTemplates` decided **out of scope** for the first prototype.

### Phase 0: complete

Build is green on Linux (Qt 6.4.2, gcc 13.3.0). `ctest`: **35/35 pass**.

- Baseline commit first, so every later step is revertible.
- Deleted the dead trees and stubs listed in §8 Phase 0 item 4 (47 files).
- Qt floor lowered 6.8 to 6.2 (matches the installed 6.4.2) and `QuickDialogs2` now requested,
  which `QtQuick.Dialogs`' `FileDialog` needs. `Qt6::Svg` is **optional**: it is absent here
  (`libqt6svg6-dev` not installed, `sudo` needs a password), so the SLD compiles and runs without
  equipment symbols via a `STATIONVIZ_HAVE_SVG` guard. Install the dev package to re-enable them.
- Warnings on for `sclLib`/`sldLib`/`stationviz_ui`.
- `scl_tests` now **runs and links the real `sclLib`** (was recompiling the sources as `scl_core`).
  Added `tests_scl_robustness.cpp`, 14 new tests, 15 total.
- New `tests/ui_smoke`: headless (`QT_QPA_PLATFORM=offscreen`) test of the real `AppContext`
  pipeline, registered per fixture in CTest. It is what proves the SLD models actually populate,
  ids stay unique, and edges are not duplicated. Added `EdgeModel::get()` to make this possible.
- Manifest paths made relative to the manifest; the three fixtures it referenced but that did not
  exist were dropped, and `REAL_STATION1` (the real Siemens ICD) added. Moved `mnt/data` to `data`.
- Removed a second hand-rolled nlohmann copy and the checked-in MinGW `.dll`/`.exe`.

Bugs found and fixed, each with a regression test:

| Bug | Fix |
|---|---|
| `matchCN` could never resolve a full path against a logical key | `lastSegment` split on `'/'` only; logical keys use `':'`. Now splits on both. This was the sole pre-existing test failure. |
| `loadScl` threw `std::out_of_range` on a `ConnectedAP` naming an absent IED | `find()` + new `InvalidIedRef` diagnostic. Reproduced before the fix. |
| `std::stod` threw on garbage/overflow `Voltage`, accepted `nan`/`inf`, and was locale-dependent | `parseDoubleStrict` (locale-independent, finite-only) + `ScalarWithUnit::valid` |
| `Result<T>` accessors dereferenced a disengaged `optional` (UB) | throw `BadResultAccess`; added `has_value()`, `[[nodiscard]]` |
| `<scl:SCL>` prefixed documents failed as "Missing `<SCL>` root" | normalise element names once at parse time, so all 24 lookup sites keep working |
| `AccessPoint` with two `Server`s silently dropped every LD after the first | iterate all `Server` elements; added `AccessPoint::serverAddresses` |
| `<Equipment>`/`<Container>`-wrapped `PowerTransformer` was invisible | search those containers too |
| A winding kept only its first `TapChanger`, and `PhaseTapChanger` was dropped | `std::optional` → `vector<TapChangerInfo>` |
| 3 real problems produced 8 diagnostics, same fact under two codes | `buildIndexes_` no longer duplicates; `ControlBlockNotFound` suppressed when the LD itself is missing (a cascade) |
| Duplicate `ConnectivityNode@pathName` / `DataSet@name` failed silently | new `DuplicateConnectivityNode` / `DuplicateDataSetName` diagnostics (the latter scoped per IED) |
| `code_to_string` in the test harness fell through to `"Other"` for every new code, weakening the acceptance policy | delegates to the new `scl::to_string` |
| **`SldView` never repainted on load** | `setNodes`/`setEdges` connect to `modelReset`/`dataChanged`/`rows*`. This is 2.1, the flagship bug |
| `dragging_` permanently sticky after one pan | added `mouseReleaseEvent` |
| Clicking empty space never deselected | emits `nodeClicked("")` |
| Every bus span emitted **two** identical `BusSpan` edges | removed the duplicated loop in `AppContext.cpp` |
| `iconForKind` returned a nonexistent `busbar.svg` | removed the branch |
| `iconWorldHeight` was declared, settable, and ignored (hardcoded `46.f`, which overlapped the adjacent lane) | now actually used |

Two fixtures were **invalid**, not a parser defect: `SCD_BROKEN_CN.scd` declared the CN it was meant
to be missing, and `SCD_2VL_TR*.scd` nested `PowerTransformer` inside `VoltageLevel` (not schema
valid, so the parser correctly ignored it). Both rewritten; `SCD_2VL_TR` now yields
`pt=1` with 2 resolved windings.

### Phase 1: complete (P0, P1, P2)

All of §3 P0, P1 (except the `Communication`/`LN0` layering inversion and `ReportControl`) and P2
are done, with 15 tests. `scl_sld_demo` acceptance is green (16 fixtures, exit 0).

Still open in Phase 1:
- **`ReportControl` / `RptEnabled` / `Inputs` / `ExtRef`** are still absent. This is now the
  **critical path**: B-reports are impossible without it, so no FAT supervision loop.
- The **layering inversion** (§3 P1 first row) is still present: endpoints still come only from
  `Communication/.../GSE|SMV`, so an `LN0` control block with no `<Communication>` still yields
  zero endpoints. Reproduced case is now covered by a test only for the crash, not the zero-result.
- `mmsEndpoints_` still does not take `AccessPoint/Address` or `Server/Address`; the `"IED1|"` key
  defect stands.
- `CNAddress::ss` still unassigned; `Voltage` multiplier still a raw string.

### Phase 2: not started
The confirmed root cause of `transformers=0` is now **confirmed and localised**: `SldBuilder`
(`buildRaw`) only walks `VoltageLevel/Bay/ConductingEquipment` and never seeds
`PowerTransformer` winding terminals into the graph. `substation.scd` has 2 PowerTransformers with
winding terminals that the parser resolves correctly; the SLD simply never sees them. Alongside the
regressions already listed in §4.
