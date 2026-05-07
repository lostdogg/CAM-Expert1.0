# CAM-Expert 1.2

CAM-Expert is a native **Windows CAD/CAM application** written in **C++20** with a **Win32 UI** and an **OpenGL viewport**. It takes geometry from wireframe/surface/solid or imported CAD data, generates CNC toolpaths, simulates/validates machining, and outputs controller-specific G-code.

It also includes a built-in **AI Copilot** pipeline for natural-language CAM assistance (intent parsing, parameter negotiation, constrained validation, local inference, RAG retrieval, and audit logging).

---

## Table of Contents

- [1) Project at a glance](#1-project-at-a-glance)
- [2) What’s implemented](#2-whats-implemented)
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

## 2) What’s implemented

### CAD / geometry
- Wireframe scene with points, lines, arcs, circles, polygons, splines, ellipses, helix/spiral, and extraction/modification tools.
- NURBS surface support with tessellation/evaluation.
- B-Rep solids and solid operations (constructive + boolean style workflows).
- Mesh support (STL/OBJ) and mesh-driven operations.
- File importer architecture for multiple format families.
- Model-prep and feature-recognition foundations.
- Constraint solver module for sketch/edit workflows.

### CAM strategies
- **2D / 2.5D:** contour, pocket, face mill, drilling, chamfer, thread mill.
- **Dynamic motion:** adaptive/trochoidal style motion patterns.
- **3D:** waterline (Z-level), raster, scallop, spiral.
- **Multi-axis:** swarf, normal-to-surface, rotary wrap, lead/lag, IK helpers, singularity smoothing, inverse-time feed helpers.
- **Turning / mill-turn / swiss:** dedicated strategy modules.
- **Probing cycles:** Z-surface, bore/boss center, corner.
- **Toolpath transforms:** dynamic plane + geometric transform utilities.

### Simulation and verification
- **Backplot:** path animation/visual verification.
- **Verify:** stock/deviation/gouge-oriented checks.
- **Machine simulation:** kinematic machine behavior checks.

### Post-processing and NC output
- Internal NCI format support.
- Post processor module with profile/script-oriented customization path.
- Controller families represented in code/docs (Fanuc/Haas/Heidenhain/etc. style targets).

### AI Copilot subsystem (`src/copilot`)
- `IntentParser` for NL intent extraction.
- `ContextBuffer` for live CAM context snapshots.
- `ParameterNegotiator` for strategy/material/tool parameter shaping.
- `ConstrainedOutputValidator` for safety/feasibility checks and corrections.
- `LocalInferenceEngine` for local model-backed enhancement (rule fallback available).
- `VectorDatabase` for RAG-like recall.
- `GeometricTokenizer` for geometry/context packing.
- `SelfCorrectionLoop` for iterative correction.
- `AuditLog` for traceability.
- `CopilotEngine` orchestrates parse → negotiate → validate → suggest → apply.

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

## Requirements
- Windows 10/11 (primary runtime target)
- CMake 3.20+
- C++20-capable compiler/toolset
- Visual Studio generator (2019/2022/2026 presets) or MinGW-w64 on Windows
- OpenGL system libs (Windows platform libs)

## Configure/build (Windows examples)

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

If you want, I can also generate a second, role-based README variant (operator-focused quickstart + developer-focused architecture guide split) in this same repository.
