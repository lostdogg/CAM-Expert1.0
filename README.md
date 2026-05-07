# CAM-Expert 1.2

**CAM-Expert** is a comprehensive Computer-Aided Manufacturing (CAM) and Computer-Aided Design (CAD) application for Windows, written in C++20. It bridges the gap between a digital 3D model and a physical CNC machine by generating the toolpaths needed to cut parts from raw material.

Built entirely on the Win32 API with an OpenGL 3-D viewport, CAM-Expert requires no third-party UI framework and ships as a single native executable. An embedded **AI Copilot** subsystem (local SLM inference, intent parsing, vector-database RAG) lets operators describe machining intent in plain English and receive fully parameterised toolpath proposals.

---

## What's New in v1.2

- **3D Waterline (Z-level) toolpath** – Z-level roughing/finishing on any loaded NURBS surface; accessible from Machine → 3D or keyboard shortcut **F6**
- **3D Scallop toolpath** – constant step-over finishing with live scallop-height readout (`h = R − √(R²−(Sₒ/2)²)`); shortcut **F7**
- **3D Raster toolpath** – parallel passes at a configurable angle projected onto the stock mesh
- **5-Axis Swarf toolpath** – simultaneous 5-axis swarf milling with configurable lead angle and automatic IK/gouge protection
- **AI Copilot subsystem** – natural-language machining assistant (`src/copilot/`); comprises `IntentParser`, `ParameterNegotiator`, `LocalInferenceEngine`, `VectorDatabase` (RAG history), `ConstrainedOutputValidator`, `SelfCorrectionLoop`, and `AuditLog`; opened via **F1** or the Copilot panel
- **CopilotEngine 3D dispatch** – Copilot "Apply" for WaterlineRough/Scallop3D/Raster3D now calls real `Strategies3D` functions when a surface is loaded, with correct geometric Z-extent sampling; multi-axis applies `MultiAxis::swarfMill`
- **Dynamic Plane & Transform Toolpath** – `DynamicPlane` re-orientates the active WCS at runtime; `TransformToolpath` mirrors/rotates/translates any toolpath in 3-D space
- **Probing Cycles** – Z-surface probe, bore/boss centre-finder (4-touch), and corner-finder exposed through `ProbingCycles`
- **Dual-light OpenGL rendering** – secondary blue-tinted fill light (`GL_LIGHT1`) for professional 3-point lighting quality
- **NURBS offset surface fix** – `makeOffset()` previously wrote zero-filled placeholder knot vectors; now uses proper clamped uniform knot vectors
- **Header deduplication** – removed 200-line duplicate class/constant block that existed outside the `#endif` in `MainWindow.h`
- **Constraint solver foundation** – new `src/cad/ConstraintSolver.*` module adds geometric constraint types, edit/solve lifecycle, and diagnostics for parametric sketch workflows
- **SQL single-source-of-truth foundation** – new `src/cam/SqlToolDatabase.*` provides canonical SQL schema/migration/export and integration hooks for tool/material/cutting data
- **Scriptable post-processor profiles** – `PostProcessor` now supports loading external key/value script profiles for controller/dialect customization without recompilation
- **OpenMP-ready 3D projection loop** – `Strategies3D::projectOntoMesh` now supports optional OpenMP acceleration with deterministic minimum-hit reduction

### Keyboard Shortcuts (v1.2 additions)

| Key    | Action                           |
|--------|----------------------------------|
| F1     | Open AI Copilot panel            |
| F5     | Regenerate all toolpaths         |
| **F6** | 3D Waterline                     |
| **F7** | 3D Scallop                       |

---

## Features

### Interface
- **Ribbon-style UI** with workflow tabs: Home, Wireframe, Surfaces, Solids, Model Prep, Machine, View
- **3-D OpenGL Viewport** – wireframe, shaded, and translucent rendering with professional dual-light shading; orbit/pan/zoom camera
- **Managers Panel** (left side):
  - **Toolpaths Manager** – ordered list of all machining operations with regeneration support
  - **Solids Manager** – B-Rep solid history tree with visibility control
  - **Surfaces Manager** – NURBS surface list with visibility and trim state
  - **Levels Manager** – layer-based geometry organisation
  - **Planes Manager** – coordinate systems (WCS, Tool Plane, Construction Plane)
- **Selection Bar & Quick Masks** – filter selections by geometry type

### Wireframe quick reference

The **Wireframe** tab is the primary toolkit for sketching the lines, arcs, and points that later drive toolpaths.

#### 1. Points and lines
- **Point Position** – place a coordinate node in 3D space by clicking in the graphics area or entering exact X/Y/Z values.
- **Line Endpoints** – create a straight segment by picking the start point and end point; length or angle can be locked as needed.
- **Line Parallel** – create a line parallel to an existing one by choosing the source line, the side, and the offset distance.
- **Line Perpendicular** – create a line at 90° to a selected entity by choosing the base line and where the new line starts or ends.

#### 2. Arcs and circles
- **Circle Center Point** – create a round feature by picking the centre and then defining the radius or diameter.
- **Arc 3 Points** – create an arc from a start point, an end point, and a third point that sets the curvature.
- **Arc Tangent** – create an arc that transitions smoothly from an existing line or arc by maintaining tangency.

#### 3. Shapes and polygons
- **Rectangle** – create a rectangle from opposite corners, with the option to anchor construction from the centre.
- **Rectangular Shapes** – create obrounds, D-shapes, or rectangles with pre-filleted corners from entered dimensions.
- **Polygon** – create regular polygons by defining the number of sides and the size across flats or across corners.

#### 4. Modification and cleanup
- **Trim / Break / Extend** – use dynamic trim tools to keep, trim, extend, or split wireframe at the nearest intersection.
- **Fillet Entities** – round off sharp corners by entering a radius and selecting the two intersecting entities.
- **Chamfer Entities** – create an angled flat by entering the chamfer size and selecting the intersecting entities.
- **Offset** – duplicate wireframe geometry at a specified distance by choosing the entity, direction, and offset value.

#### 5. Advanced curves
- **Curve on Edge** – convert a solid-model edge directly into matching wireframe geometry.
- **Manual Spline** – create a smooth curve by defining a series of spline points or control points.
- **Letters** – generate text as wireframe geometry for engraving by entering the text, font, and height, then placing it on the part.

### Solids quick reference

The **Solids** tab is where 2D wireframe sketches become 3D volume representing the material to machine.

#### 1. Primary creation tools
- **Extrude** – push a closed wireframe chain into the third dimension to create a solid body or cut material.
- **Revolve** – spin a wireframe profile around a centre axis to build cylindrical or conical bodies.
- **Sweep** – move a profile cross-section along a selected path curve.
- **Loft** – blend two or more wireframe chains to skin a smooth transition between them.

#### 2. Refining the solid
- **Constant Fillet** – round sharp edges by entering a radius and selecting target edges or faces.
- **Chamfer** – create a flat angled edge break from entered distances or angle settings.
- **Shell** – hollow a body by selecting an opening face and setting wall thickness.

#### 3. Boolean operations
- **Boolean Add** – merge target and tool solids into one body.
- **Boolean Remove (Subtract)** – carve material from a target using one or more tool solids.
- **Boolean Common (Intersect)** – keep only the overlapping volume between selected solids.

#### 4. Advanced modification
- **Thicken** – convert a thin surface into a solid by applying thickness.
- **Trim to Plane** – cut a solid using a plane or surface and keep the required side.
- **Remove Faces** – remove selected feature faces (for example holes or fillets) and heal the surrounding area during model-prep cleanup.

Use the **Solids Manager** side panel as a feature/history tree. You can revisit operations such as extrusion height, fillet radius, shell thickness, and boolean intent without redrawing the model.

### CAD / File Handling
- **B-Rep (Boundary Representation)** solid modelling with topology awareness
- **NURBS Surfaces** – full tensor-product NURBS with Cox-de Boor evaluation, normals, and tessellation
- **Mesh Data** – STL/OBJ triangle meshes with silhouette extraction and gouge detection
- **Modular File Importer Architecture** – pluggable importer interface (`IGeometryImporter`) so new formats can be added without rewriting the core graphics engine
- **File Import**:
  - Precise geometry first: STEP (`.stp`/`.step`), IGES (`.igs`/`.iges`), Parasolid (`.x_t`/`.x_b`)
  - Mesh formats: STL (ASCII & binary), OBJ
  - Additional connectors/stubs: 3MF/AMF, SolidWorks (`.sldprt`/`.sldasm`), AutoCAD (`.dwg`/`.dxf`), Rhino (`.3dm`), Inventor (`.ipt`/`.iam`), CATIA (`.catpart`)
- **Model Prep Engine** – fillet removal, surface healing, boundary extraction, feature classification
- **Feature Recognition** – automatic identification of holes, pockets, bosses, and slots

### CAM Toolpath Strategies
| Category | Strategies |
|---|---|
| **2D / 2.5D Milling** | Contour (lead-in + lead-out arcs), Pocket (concentric offsets), Face Mill, Drilling (G81/G83 peck), Thread Mill, Chamfer |
| **Dynamic Motion** | Trochoidal milling, constant chip-load, micro-lifts, helical arc entry |
| **3D Milling** | Waterline (Z-level, **F6**), Raster (parallel passes on mesh), Scallop (h = R − √(R²−(Sₒ/2)²), **F7**), Spiral – all with geometric surface projection |
| **Multi-Axis** | 5-axis swarf (lead/lag configurable), 5-axis normal-to-surface, 4-axis rotary wrap, tool-holder collision avoidance |
| **Probing** | Z-surface probe, Bore/Boss centre-finder (4-touch), Corner-finder |
| **Turning & Mill-Turn** | Rough turn, finish turn (profile following), groove (peck), thread (G76), sub-spindle transfer |
| **Swiss-Style Machining** | Sliding headstock (Z-inverted coords), Pinch/Sync two-tool whip prevention |
| **Cloud Tool Library** | Digital twin import, manufacturer feeds/speeds, dynamic arc limit calculation |

### Verification & Simulation
- **Backplot** – wireframe animation of tool motion (rapid/feed/plunge/retract colour-coded)
- **Verify** – Z-map (dexel) solid stock simulation with gouge and under-cut detection
- **Machine Simulation** – kinematic machine model with over-travel checking and collision detection (3-axis VMC, 4-axis HMC, 5-axis head/table)

### Post-Processor Engine
- **NCI Intermediate Format** – serialise/parse internal toolpath data
- **G-Code Generation** supporting:
  - **Fanuc**, **Haas**, **Heidenhain**, Siemens Sinumerik, Mazak, Okuma, Generic
  - Modal / non-modal code suppression
  - Metric / imperial, absolute / incremental
  - Tool change, spindle, coolant blocks
  - 5-axis A/B axis output
- **Machine output extensions**: `.nc`, `.ncc`, `.tap`, `.gcode`, `.mpf`, `.spf`, `.din`, `.sbp`

---

## Building

### Requirements
- **Windows 10/11** (64-bit)
- **MSVC 2019 or later** (Visual Studio 2019, 2022, or 2026) *or* **MinGW-w64 / Clang** with Win32 headers
- **CMake 3.20+**
- **OpenGL** (provided by Windows via `opengl32.lib` and `glu32.lib`)
- **C++20-capable compiler**

### Build steps

Choose the CMake generator that matches your installed Visual Studio version.

#### Visual Studio 2026 (recommended — toolset v180)

```powershell
# Clone and enter the repository
git clone https://github.com/lostdogg/CAM-Expert1.0.git
cd CAM-Expert1.0

# Configure (Visual Studio 2026, x64)
cmake -B build -G "Visual Studio 18 2026" -A x64

# Build Release
cmake --build build --config Release

# The executable is in:
#   build/Release/CAMExpert.exe
```

#### Visual Studio 2022 (toolset v143)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

#### Visual Studio 2019 (toolset v142)

```powershell
cmake -B build -G "Visual Studio 16 2019" -A x64
cmake --build build --config Release
```

#### Debug build

Append `--config Debug` to produce a debug binary with full symbols:

```powershell
cmake --build build --config Debug
# Output: build/Debug/CAMExpert.exe
```

#### Using CMake Presets (any Visual Studio version)

A `CMakePresets.json` is provided for convenience. List available presets and pick the one
that matches your installed Visual Studio:

```powershell
cmake --list-presets                  # list configure presets
cmake --preset windows-vs2026         # configure with VS 2026
cmake --build --preset release-vs2026

# Other preset names:
#   windows-vs2022     / release-vs2022
#   windows-vs2019     / release-vs2019
#   windows-mingw-release / release-mingw
```

#### MinGW-w64 / Clang

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Reproducible containerized configure/build

```bash
docker build -t camexpert-ci .
docker run --rm camexpert-ci
```

> **Toolset mismatch?**  If you see error `MSB8020: The build tools for Visual Studio 20XX
> (Platform Toolset = 'vYYY') cannot be found`, you are using a generator for a different
> Visual Studio version than the one installed.  Use the generator that matches your
> installed version (see the options above), or install the matching build tools via the
> Visual Studio Installer.

---

## Testing

CAM-Expert does not yet have an automated unit-test suite.  The validation workflow below
covers the essential build-smoke and feature-exercise steps.

### 1. Build smoke test

After a successful CMake build, verify the executable starts without errors:

```powershell
.\build\Release\CAMExpert.exe
```

The application window must appear with the ribbon UI and the 3-D viewport.
A clean launch produces no error dialogs.

### 2. Geometry & CAD smoke test

| Step | Action | Expected result |
|------|--------|----------------|
| 2a | **Wireframe → Line** – draw a closed contour | Contour appears in viewport |
| 2b | **Surfaces → NURBS** – create a test surface | Surface listed in Surfaces Manager |
| 2c | **File → Import** – load a small `.stl` file | Mesh visible; no crash or error |

### 3. CAM toolpath smoke test

| Step | Action | Expected result |
|------|--------|----------------|
| 3a | **Machine → 2D → Contour** | Toolpath entry appears in Toolpaths Manager |
| 3b | **Machine → 2D → Pocket** | Concentric-offset pocket toolpath generated |
| 3c | **F6** (3D Waterline) | Z-level passes drawn in viewport |
| 3d | **F7** (3D Scallop) | Scallop passes drawn with correct step-over |
| 3e | **F5** (Regenerate all) | All toolpaths refresh without errors |

### 4. AI Copilot smoke test

| Step | Action | Expected result |
|------|--------|----------------|
| 4a | **F1** – open Copilot panel | Panel opens; input field is active |
| 4b | Type `"rough waterline on surface, 3mm stepdown"` and click **Apply** | Copilot proposes parameters; confirms intent |
| 4c | Click **Accept** | Waterline toolpath generated using proposed parameters |

### 5. Post-processor & G-code export

| Step | Action | Expected result |
|------|--------|----------------|
| 5a | **Machine → Post Process** – choose Fanuc | Save dialog appears |
| 5b | Save as `test.nc` and open in a text editor | File contains valid G-code starting with `%` and ending with `M30` |

### 6. Simulation

| Step | Action | Expected result |
|------|--------|----------------|
| 6a | **Machine → Backplot** | Wireframe tool animation plays without crash |
| 6b | **Machine → Verify** | Z-map stock model updates; no gouge alerts on valid toolpath |

### 7. Setup usability workflows (manual QA checklist)

| Step | Action | Expected result |
|------|--------|----------------|
| 7a | **Machine → Setup Constraints** → add two constraints, then solve | Diagnostics dialog appears with solve status and applied-count details |
| 7b | **Machine → Setup Post Profile** → load/validate profile | Success message shown; profile remains active for later posting |
| 7c | **Machine → Setup Tool/Material DB** → upsert rows and export SQL | SQL snapshot file is written and status bar confirms export |
| 7d | **Machine → Setup Tool/Material DB** → apply DB to libraries | Status message confirms runtime libraries were synced from SQL cache |
| 7e | **Machine → Setup Performance Mode** | Mode change is reported with OpenMP availability/fallback text |
| 7f | **Machine → Context Guidance** and **Recent Audit Trail** | Guidance and recent operation list dialogs show actionable session info |

> **Tip:** Use `build/Debug/CAMExpert.exe` when investigating failures – the debug build
> includes assertions and richer diagnostic output.

---

## Project Structure

```
CAM-Expert1.0/
├── CMakeLists.txt
├── CMakePresets.json
├── src/
│   ├── main.cpp                  # WinMain entry point
│   ├── Application.h/.cpp        # Application singleton
│   ├── MainWindow.h/.cpp         # Top-level frame window
│   ├── ui/
│   │   ├── RibbonUI.h/.cpp       # Ribbon tab control
│   │   ├── Viewport3D.h/.cpp     # OpenGL 3-D viewport
│   │   ├── SelectionBar.h/.cpp   # Quick-mask filter bar
│   │   └── CopilotPanel.h/.cpp   # AI Copilot side panel (F1)
│   ├── cad/
│   │   ├── Geometry.h/.cpp       # Vec2/Vec3/Mat4/AABB/Ray/Triangle
│   │   ├── BRep.h/.cpp           # B-Rep solid (faces/edges/vertices)
│   │   ├── NurbsSurface.h/.cpp   # NURBS surface (Cox-de Boor)
│   │   ├── MeshData.h/.cpp       # Triangle mesh (STL/OBJ)
│   │   ├── FileImporter.h/.cpp   # Multi-format CAD import
│   │   ├── ModelPrep.h/.cpp      # Model preparation engine
│   │   └── FeatureRecognition.h/.cpp  # Hole/pocket/boss recognition
│   ├── cam/
│   │   ├── Toolpath.h/.cpp       # Tool, CuttingParams, ToolpathPoint
│   │   ├── DynamicMotion.h/.cpp  # Trochoidal / high-efficiency strategies
│   │   ├── Strategies2D.h/.cpp   # 2D/2.5D milling (contour w/ lead-in/out, pocket, drill)
│   │   ├── Strategies3D.h/.cpp   # 3D surface milling (waterline, scallop w/ h formula)
│   │   ├── MultiAxis.h/.cpp      # 4/5-axis strategies + IK + holder collision avoidance
│   │   ├── Turning.h/.cpp        # Turning & mill-turn operations
│   │   ├── SwissMachining.h/.cpp # Swiss sliding-headstock + pinch/sync toolpaths
│   │   ├── CloudToolLibrary.h/.cpp # Digital twin + manufacturer feed/speed data
│   │   ├── MaterialLibrary.h/.cpp # Material properties + feed/speed calculator
│   │   ├── MillTurn.h/.cpp       # Multi-turret synchronisation (pinch, balanced, cutoff)
│   │   ├── DynamicPlane.h/.cpp   # Runtime WCS re-orientation
│   │   ├── TransformToolpath.h/.cpp # Mirror/rotate/translate toolpaths in 3-D
│   │   ├── ProbingCycles.h/.cpp  # Z-probe, bore/boss centre-finder, corner-finder
│   │   ├── NciFormat.h/.cpp      # NCI intermediate format
│   │   └── PostProcessor.h/.cpp  # G-code post-processor
│   ├── copilot/
│   │   ├── CopilotEngine.h/.cpp          # Top-level Copilot orchestration
│   │   ├── IntentParser.h/.cpp           # Natural-language → structured intent
│   │   ├── ParameterNegotiator.h/.cpp    # Multi-turn parameter clarification
│   │   ├── LocalInferenceEngine.h/.cpp   # Embedded SLM inference backend
│   │   ├── VectorDatabase.h/.cpp         # RAG history / embedding store
│   │   ├── GeometricTokenizer.h/.cpp     # Geometry-aware tokenisation
│   │   ├── ConstrainedOutputValidator.h/.cpp  # Output schema validation
│   │   ├── SelfCorrectionLoop.h/.cpp     # Iterative self-correction pass
│   │   ├── ContextBuffer.h/.cpp          # Conversation context window
│   │   └── AuditLog.h/.cpp               # Immutable Copilot decision log
│   ├── managers/
│   │   ├── ToolpathManager.h/.cpp
│   │   ├── SolidsManager.h/.cpp
│   │   ├── SurfacesManager.h/.cpp    # NURBS surface list with trim/visibility state
│   │   ├── LevelsManager.h/.cpp
│   │   └── PlanesManager.h/.cpp
│   ├── simulation/
│   │   ├── Backplot.h/.cpp       # Wireframe animation
│   │   ├── Verify.h/.cpp         # Z-map stock simulation
│   │   └── MachineSimulation.h/.cpp  # Kinematic machine model
│   └── resources/
│       ├── resource.h
│       ├── CAMExpert.rc
│       └── CAMExpert.exe.manifest
└── README.md
```

---

## Architecture Overview

```
CAD Input (STEP / IGES / STL / OBJ / Native)
        │
        ▼
  FileImporter ──► ModelPrep ──► FeatureRecognition
        │
        ▼
  BRep::Solid / NurbsSurface / MeshData
        │
        ▼
  CAM Strategies (2D / 3D / MultiAxis / Turning)
        │
        ├──── CopilotEngine (AI Copilot panel, F1)
        │       ├─ IntentParser → ParameterNegotiator
        │       ├─ LocalInferenceEngine (SLM backend)
        │       ├─ VectorDatabase (RAG history)
        │       └─ apply → Strategies3D / MultiAxis / DynamicMotion
        │
        ▼
  Toolpath (list of ToolpathPoints + NCI records)
        │
        ├──► Backplot (wireframe verification)
        ├──► Verify (Z-map stock simulation)
        ├──► MachineSimulation (kinematic collision check)
        │
        ▼
  PostProcessor ──► G-code (.nc / .tap) ──► CNC Controller
```

---

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.
