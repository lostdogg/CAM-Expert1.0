# CAM-Expert 1.2

CAM-Expert is a native **Windows CAD/CAM application** written in **C++20** with a **Win32 UI** and an **OpenGL viewport**. It takes geometry from wireframe/surface/solid or imported CAD data, generates CNC toolpaths, simulates/validates machining, and outputs controller-specific G-code.

It also includes a built-in **AI Copilot** pipeline for natural-language CAM assistance (intent parsing, parameter negotiation, constrained validation, local inference, RAG retrieval, and audit logging).

---

## Table of Contents

- [1) Project at a glance](#1-project-at-a-glance)
- [2) Feature status](#2-feature-status)
- [3) UI and workflow model](#3-ui-and-workflow-model)
- [4) Keyboard shortcuts](#4-keyboard-shortcuts)
- [5) Architecture](#5-architecture)
- [6) Repository structure](#6-repository-structure)
- [7) Build and run](#7-build-and-run)
- [8) Validation and testing](#8-validation-and-testing)
- [9) Troubleshooting](#9-troubleshooting)
- [10) Known constraints](#10-known-constraints)
- [11) Contribution notes](#11-contribution-notes)
- [12) Security and safety notes](#12-security-and-safety-notes)
- [13) Version and metadata notes](#13-version-and-metadata-notes)

---

## 1) Project at a glance

### Stack
- **Language:** C++20
- **UI:** Win32 API + Common Controls
- **Rendering:** OpenGL (`opengl32`, `glu32`)
- **Build system:** CMake (3.20+)
- **Optional acceleration:** OpenMP (`CAMEXPERT_USE_OPENMP=1` when available)

### Primary domains
- CAD wireframe + surface + solid operations
- CAM strategy generation (2D/3D/multi-axis/turning/swiss/probing)
- Toolpath management and regeneration
- Verification/simulation (backplot, stock, kinematics)
- Post-processing to multiple CNC dialects
- AI Copilot-assisted command flow

---

## 2) Feature status

The table below classifies every major feature into one of four tiers, based on what is actually present in the source tree.

| Tier | Meaning |
|---|---|
| ✅ **Implemented** | Real, working code with meaningful logic — not just a skeleton. |
| 🧪 **Experimental** | Core algorithm is present but some sub-paths are incomplete, delegate to simplified behaviour, or have known edge-case gaps. |
| 🔧 **Stub / in-progress** | Architectural placeholder: interface and registration exist; bodies return representative/dummy results or emit a "not implemented" message. |
| 📋 **Planned** | No code yet — feature listed here so contributors know it is on the roadmap. |

---

### CAD / geometry

| Feature | Status | Notes |
|---|---|---|
| Wireframe scene (points, lines, arcs, circles, polygons, splines, ellipses, helix/spiral) | ✅ Implemented | Full entity management, construction-plane helpers, coordinate transforms. |
| NURBS surface evaluation and tessellation | ✅ Implemented | Cox–de Boor basis, tangent computation, full surface eval loop. |
| B-Rep solid topology | ✅ Implemented | Vertices, edges, faces, hole detection. |
| Mesh support — STL (ASCII + binary) and OBJ import | ✅ Implemented | Bounding box, centroid, surface-area, degenerate-triangle removal. |
| Feature recognition (holes, pockets, bosses, slots) | ✅ Implemented | Returns feature type and strategy recommendations. |
| Constraint solver | ✅ Implemented | Add/remove constraints, sketch entity reference resolution. |
| STEP (.step / .stp) importer | ✅ Implemented | Parses entities, derives representative B-Rep geometry. |
| IGES (.igs / .iges) importer | ✅ Implemented | Parses parameter records, derives representative geometry. |
| STL / OBJ importer | ✅ Implemented | Native ASCII and binary STL; Wavefront OBJ. |
| Parasolid (.x_t / .x_b) import bridge | 🔧 Stub | Interface wired; body prints a "kernel-accurate reader can be plugged in" message. |
| SolidWorks (.sldprt), CATIA, Siemens NX, Solid Edge, Rhino (.3dm), ACIS (.sat) importers | 🔧 Stub | Registered via `StubPreciseImporter`; return placeholder geometry + a descriptive message. |
| 3MF and AMF importers | 🔧 Stub | Files registered; bodies return "not implemented yet". |
| Model prep — fillet/chamfer removal, surface healing | 🧪 Experimental | Removal and healing logic present; mesh simplification body contains a placeholder comment (no Garland–Heckbert yet). |
| Wireframe `Intersection` entity | 📋 Planned | Type enumerated in `WireframeScene.h`; no construction or query logic yet. |

---

### CAM strategies

| Feature | Status | Notes |
|---|---|---|
| 2D / 2.5D: contour, pocket, face mill, drilling, chamfer, thread mill | ✅ Implemented | Contour passes with parametric lead-in/out arcs; full depth/step-over loop. |
| 3D: waterline (Z-level), raster, scallop | ✅ Implemented | Ray–triangle intersection (Möller–Trumbore), Z-map and surface-deviation passes. |
| Multi-axis: swarf, normal-to-surface, rotary wrap, lead/lag, IK helpers | ✅ Implemented | Inverse kinematics, 5-axis orientation resolution, head-table and table-table kinematics. |
| Turning (rough + finish) | ✅ Implemented | Multiple radial passes with depth-of-cut control. |
| Mill-turn (dual-turret) | ✅ Implemented | Per-channel G-code generation with synchronisation wait codes. |
| Toolpath data model | ✅ Implemented | Total length, time estimation, bounding box, point storage. |
| Dynamic plane (construction plane snap + translate) | ✅ Implemented | Face-normal derivation, local-axis translations. |
| Dynamic / HSM / trochoidal motion patterns | 🧪 Experimental | Core motion geometry present; `buildFromMaterial` delegates to passed-in parameters rather than performing its own material-lookup logic. |
| Swiss sliding-headstock machining | 🧪 Experimental | Headstock feed mechanics and pinch-mode XSign implemented; complex sub-spindle synchronisation still maturing. |
| Probing cycles (Z-surface, bore/boss centre, corner) | 🧪 Experimental | Probe approach/retract and WCS register updates work; macro-variable expansion has edge-case gaps. |
| Toolpath transforms (mirror, rotate, work-offset sequences, subprograms) | 🧪 Experimental | Matrix transforms and offset sequences functional; subprogram body generation is in progress. |
| 3D spiral strategy | 📋 Planned | Listed in documentation and ribbon UI; no generation code present yet. |

---

### Simulation and verification

| Feature | Status | Notes |
|---|---|---|
| Backplot (path animation / visual verification) | ✅ Implemented | Move sequencing from `ToolpathManager`; full forward-plot build loop. |
| Verify (stock Z-map gouge / undercut analysis) | ✅ Implemented | Z-map collision detection, deviation reporting. |
| Machine simulation (kinematic machine checks) | ✅ Implemented | Pre-built 3-, 4-, and 5-axis machine configurations with component definitions. |

---

### Post-processing and NC output

| Feature | Status | Notes |
|---|---|---|
| NCI format serialisation | ✅ Implemented | 5-axis tool-axis vectors, full motion-type mapping. |
| Post processor (G-code generation) | ✅ Implemented | Script-profile loading, motion-to-code conversion, Fanuc/Haas/Heidenhain style targets. |
| Material library | ✅ Implemented | Material properties, feed/speed recommendations. |
| SQL tool database | ✅ Implemented | Schema DDL, tool and material table definitions. |
| Cloud tool library (manufacturer digital twins) | ✅ Implemented | Sandvik, Kennametal, and others with manufacturer specifications. |

---

### AI Copilot subsystem (`src/copilot`)

| Feature | Status | Notes |
|---|---|---|
| `CopilotEngine` orchestration (parse → negotiate → validate → apply) | ✅ Implemented | Full inference loop with context refresh and audit logging. |
| `IntentParser` (natural-language intent extraction) | ✅ Implemented | Action / target / parameter extraction; material and strategy hint classification. |
| `ContextBuffer` (live CAM context snapshots) | ✅ Implemented | Snapshot construction from `ToolpathManager`. |
| `ParameterNegotiator` (strategy / material / tool shaping) | ✅ Implemented | Material resolution, strategy selection, feed/speed negotiation. |
| `ConstrainedOutputValidator` (safety / feasibility checks) | ✅ Implemented | Radial engagement validation, feed/speed constraint enforcement. |
| `VectorDatabase` (RAG-style knowledge recall) | ✅ Implemented | Knowledge base with tagging, CSV import/export, similarity search. |
| `GeometricTokenizer` (geometry → text encoding) | ✅ Implemented | Feature tokenisation, geometry-to-text encoding. |
| `SelfCorrectionLoop` (iterative gouge/undercut correction) | ✅ Implemented | Step-reduction logic, correction proposal generation. |
| `AuditLog` (traceability) | ✅ Implemented | Entry formatting, session tracking, timestamp management. |
| `LocalInferenceEngine` hardware detection (AVX2 / AVX-512 / AMX) | ✅ Implemented | CPUID detection on MSVC and GCC/Clang. |
| `LocalInferenceEngine` ML model inference | 🧪 Experimental | Hardware path detected and branched; actual model loading is not present — falls back to rule-based logic. |

---

## 3) UI and workflow model

### Main layout
- Ribbon-style top control with workflow tabs.
- Left manager region (toolpaths/solids/surfaces/levels/planes).
- Central 3D viewport.
- Selection bar and status bar.
- Optional Copilot side panel.

### Ribbon/workflow intent
High-level workflow aligns to:
- Home
- Wireframe
- Surfaces
- Solids
- Model Prep
- Machine
- View

### Typical part-to-NC flow
1. Create/import geometry.
2. Clean and prepare model data.
3. Build/select machining strategy.
4. Generate toolpaths.
5. Backplot / verify / machine-sim.
6. Post-process and export NC.

### Setup and support workflows in Machine area
- Setup Constraints
- Setup Post Profile
- Setup Tool/Material DB
- Setup Performance Mode
- Context Guidance
- Recent Audit Trail

---

## 4) Keyboard shortcuts

### File / edit
- `Ctrl+N` New
- `Ctrl+O` Open
- `Ctrl+S` Save
- `Ctrl+I` Import
- `Ctrl+P` Post process
- `Ctrl+Z` Undo
- `Ctrl+Y` Redo
- `Ctrl+C` Copy
- `Ctrl+V` Paste
- `Delete` Delete selected
- `End` Analyze

### View / machine / construction
- `F1` Help topics
- `F2` Zoom selected
- `F3` Zoom fit
- `F4` Toggle grid
- `F5` Toggle gnomon
- `F6` Generate 3D waterline
- `F7` Generate 3D scallop
- `F8` Cycle construction plane
- `F9` Set Z-depth

> Function keys are routed via command accelerators and `WM_KEYDOWN` handling in `MainWindow`; `F1` is currently bound to Help Topics.

### Toolpath manager and utility
- `T` Toolpath manager toggle
- `Ctrl+Shift+T` Toggle toolpath display
- `Ctrl+Shift+C` Copy toolpath parameters

### Geometry creation/manipulation (single-key mode)
- `L` Line
- `A` Arc
- `C` Circle
- `P` Point
- `M` Move
- `R` Rotate
- `S` Scale
- `Space` Toggle selection mode

> Note: Single-letter CAD shortcuts are intentionally separated from accelerator-table behavior to avoid interfering with text-entry controls.

---

## 5) Architecture

```text
Geometry Input (Create / Import)
        │
        ▼
 CAD Core (Wireframe / NURBS / B-Rep / Mesh)
        │
        ├──► Model Prep + Feature Recognition + Constraint Solver
        │
        ▼
 CAM Strategies (2D, 3D, MultiAxis, Turning, Probing)
        │
        ├──► CopilotEngine (intent/context/validation/inference/RAG/audit)
        │
        ▼
 Toolpath Manager
        │
        ├──► Backplot
        ├──► Verify
        ├──► MachineSimulation
        │
        ▼
 PostProcessor / NCI
        │
        ▼
 CNC Program Output
```

### Core runtime composition
- `Application` manages singleton lifecycle and startup.
- `MainWindow` hosts UI, command routing, menu/accelerators, manager orchestration.
- Managers own major runtime collections (toolpaths, solids, surfaces, levels, planes).
- CAM engines generate `Toolpath` objects, then simulation/post consume them.

---

## 6) Repository structure

```text
CAM-Expert1.0/
├── CMakeLists.txt
├── CMakePresets.json
├── Dockerfile
├── README.md
└── src/
    ├── main.cpp
    ├── Application.h/.cpp
    ├── MainWindow.h/.cpp
    ├── resources/
    │   ├── CAMExpert.rc
    │   ├── CAMExpert.exe.manifest
    │   └── resource.h
    ├── ui/
    │   ├── RibbonUI.h/.cpp
    │   ├── Viewport3D.h/.cpp
    │   ├── SelectionBar.h/.cpp
    │   └── CopilotPanel.h/.cpp
    ├── cad/
    │   ├── Geometry.h/.cpp
    │   ├── ConstraintSolver.h/.cpp
    │   ├── WireframeScene.h/.cpp
    │   ├── BRep.h/.cpp
    │   ├── NurbsSurface.h/.cpp
    │   ├── MeshData.h/.cpp
    │   ├── FileImporter.h/.cpp
    │   ├── ModelPrep.h/.cpp
    │   └── FeatureRecognition.h/.cpp
    ├── cam/
    │   ├── Toolpath.h/.cpp
    │   ├── DynamicMotion.h/.cpp
    │   ├── Strategies2D.h/.cpp
    │   ├── Strategies3D.h/.cpp
    │   ├── MultiAxis.h/.cpp
    │   ├── Turning.h/.cpp
    │   ├── MillTurn.h/.cpp
    │   ├── SwissMachining.h/.cpp
    │   ├── ProbingCycles.h/.cpp
    │   ├── DynamicPlane.h/.cpp
    │   ├── TransformToolpath.h/.cpp
    │   ├── MaterialLibrary.h/.cpp
    │   ├── CloudToolLibrary.h/.cpp
    │   ├── SqlToolDatabase.h/.cpp
    │   ├── NciFormat.h/.cpp
    │   └── PostProcessor.h/.cpp
    ├── managers/
    │   ├── ToolpathManager.h/.cpp
    │   ├── SolidsManager.h/.cpp
    │   ├── SurfacesManager.h/.cpp
    │   ├── LevelsManager.h/.cpp
    │   └── PlanesManager.h/.cpp
    ├── simulation/
    │   ├── Backplot.h/.cpp
    │   ├── Verify.h/.cpp
    │   └── MachineSimulation.h/.cpp
    └── copilot/
        ├── CopilotEngine.h/.cpp
        ├── IntentParser.h/.cpp
        ├── ContextBuffer.h/.cpp
        ├── ParameterNegotiator.h/.cpp
        ├── LocalInferenceEngine.h/.cpp
        ├── VectorDatabase.h/.cpp
        ├── GeometricTokenizer.h/.cpp
        ├── ConstrainedOutputValidator.h/.cpp
        ├── SelfCorrectionLoop.h/.cpp
        └── AuditLog.h/.cpp
```

---

## 7) Build and run

### Requirements
- Windows 10/11 (primary runtime target)
- CMake 3.20+
- C++20-capable compiler/toolset
- Visual Studio generator (2019/2022/2026 presets) or MinGW-w64 on Windows
- OpenGL system libs (Windows platform libs)

### Configure/build (Windows examples)

### Visual Studio 2026
```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

### Visual Studio 2022
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Visual Studio 2019
```powershell
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
```

### Presets
```powershell
cmake --list-presets
cmake --preset windows-vs2026
cmake --build --preset release-vs2026
```

### MinGW on Windows
```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux/container notes
A containerized configure step is supported by the `Dockerfile` for reproducibility of toolchain/configure behavior:

```bash
docker build -t camexpert-ci .
docker run --rm camexpert-ci
```

Important: this project is Win32-based and Linux native compile is expected to fail at `windows.h` unless using a Windows-targeting toolchain environment.

---

## 8) Validation and testing

There is currently **no automated unit-test suite** in the repository.

Current practical validation workflow:
1. CMake configure smoke test.
2. Build smoke test in Windows toolchain.
3. Manual UI + toolpath smoke run (wireframe/surface/solid actions).
4. CAM generation checks (2D/3D/multi-axis as applicable).
5. Post output sanity check (generated NC file contents).
6. Verify/backplot/machine simulation smoke checks.

In this environment (Linux), build reaches expected failure at Win32 headers during compile.

---

## 9) Troubleshooting

### `windows.h: No such file or directory`
You are compiling in a non-Windows-native environment/toolchain; this is expected for this project unless cross-compiling with Win32 headers/toolchain support.

### Visual Studio toolset mismatch (`MSB8020`)
Use the generator matching your installed VS version, or install the required toolset in Visual Studio Installer.

### CMake preset not available
Presets are host-conditioned for Windows (`${hostSystemName} == Windows`). On non-Windows hosts, use standard configure commands for smoke/configure checks.

### OpenMP behavior
OpenMP is optional. If unavailable, project builds without OpenMP acceleration paths.

---

## 10) Known constraints

- Runtime target is Windows desktop (Win32 + OpenGL).
- No full automated test harness is present yet.
- Some advanced workflow entries are scaffold/foundation-level and continue to evolve.
- Repository currently does not include a top-level `LICENSE` file.
- Copilot panel toggle exists in UI commands/menu (`IDM_COPILOT_TOGGLE`); it is not currently bound to a default function-key shortcut.

---

## 11) Contribution notes

When making changes:
- Keep edits scoped and module-local.
- Validate build/configure behavior after changes.
- Prefer updating docs when changing command IDs, workflows, presets, or module layout.
- Keep Win32 command wiring (`MainWindow`) and UI labels synchronized.

Suggested review checklist for contributors:
- [ ] CMake configure passes
- [ ] Build behavior is understood for target host
- [ ] Manual smoke flow covers changed area
- [ ] README/docs updated if user-visible behavior changed

---

## 12) Security and safety notes

- CAM parameter suggestions should always be verified before production use.
- Copilot output is constrained/validated in code, but operators must still confirm tooling, fixtures, limits, and stock setup.
- Machine simulation and verify flows should be run before posting production NC.

---

## 13) Version and metadata notes

The repository currently contains version markers across multiple components (project, app strings, and resources). During release hardening, keep these synchronized across:
- `CMakeLists.txt` (`project(... VERSION ...)`)
- `src/Application.h` app version strings
- `src/resources/CAMExpert.rc` version resource fields

---

