# CAM-Expert 1.0

**CAM-Expert 1.0** is a comprehensive Computer-Aided Manufacturing (CAM) and Computer-Aided Design (CAD) application for Windows, written in C++17. It bridges the gap between a digital 3D model and a physical CNC machine by generating the toolpaths needed to cut parts from raw material.

---

## Features

### Interface
- **Ribbon-style UI** with workflow tabs: Home, Wireframe, Surfaces, Solids, Model Prep, Machine, View
- **3-D OpenGL Viewport** – wireframe, shaded, and translucent rendering modes with orbit/pan/zoom camera
- **Managers Panel** (left side):
  - **Toolpaths Manager** – ordered list of all machining operations with regeneration support
  - **Solids Manager** – B-Rep solid history tree with visibility control
  - **Levels Manager** – layer-based geometry organisation
  - **Planes Manager** – coordinate systems (WCS, Tool Plane, Construction Plane)
- **Selection Bar & Quick Masks** – filter selections by geometry type

### CAD / File Handling
- **B-Rep (Boundary Representation)** solid modelling with topology awareness
- **NURBS Surfaces** – full tensor-product NURBS with Cox-de Boor evaluation, normals, and tessellation
- **Mesh Data** – STL/OBJ triangle meshes with silhouette extraction and gouge detection
- **File Import**:
  - Neutral formats: STEP, IGES, STL (ASCII & binary), OBJ
  - Native formats: SolidWorks (`.sldprt`/`.sldasm`), AutoCAD (`.dwg`/`.dxf`), Inventor, Siemens NX
- **Model Prep Engine** – fillet removal, surface healing, boundary extraction, feature classification
- **Feature Recognition** – automatic identification of holes, pockets, bosses, and slots

### CAM Toolpath Strategies
| Category | Strategies |
|---|---|
| **2D / 2.5D Milling** | Contour, Pocket (concentric offsets), Face Mill, Drilling (G81/G83 peck), Thread Mill |
| **Dynamic Motion** | Trochoidal milling, constant chip-load, micro-lifts, helical arc entry |
| **3D Milling** | Waterline (Z-level), Raster, Scallop, Spiral – all with surface projection |
| **Multi-Axis** | 5-axis swarf, 5-axis normal-to-surface, 4-axis rotary wrap, lead/lag tilt |
| **Turning & Mill-Turn** | Rough turn, finish turn (profile following), groove (peck), thread (G76), sub-spindle transfer |

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

---

## Building

### Requirements
- **Windows 10/11** (64-bit)
- **MSVC 2019 or later** (Visual Studio) *or* **MinGW-w64 / Clang** with Win32 headers
- **CMake 3.16+**
- **OpenGL** (provided by Windows via `opengl32.lib` and `glu32.lib`)

### Build steps

```powershell
# Clone and enter the repository
git clone https://github.com/lostdogg/CAM-Expert1.0.git
cd CAM-Expert1.0

# Configure (Visual Studio 2022, x64)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build Release
cmake --build build --config Release

# The executable is in:
#   build/Release/CAMExpert.exe
```

For MinGW-w64:

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Project Structure

```
CAM-Expert1.0/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                  # WinMain entry point
│   ├── Application.h/.cpp        # Application singleton
│   ├── MainWindow.h/.cpp         # Top-level frame window
│   ├── ui/
│   │   ├── RibbonUI.h/.cpp       # Ribbon tab control
│   │   ├── Viewport3D.h/.cpp     # OpenGL 3-D viewport
│   │   └── SelectionBar.h/.cpp   # Quick-mask filter bar
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
│   │   ├── Strategies2D.h/.cpp   # 2D/2.5D milling operations
│   │   ├── Strategies3D.h/.cpp   # 3D surface milling operations
│   │   ├── MultiAxis.h/.cpp      # 4/5-axis strategies + IK
│   │   ├── Turning.h/.cpp        # Turning & mill-turn operations
│   │   ├── NciFormat.h/.cpp      # NCI intermediate format
│   │   └── PostProcessor.h/.cpp  # G-code post-processor
│   ├── managers/
│   │   ├── ToolpathManager.h/.cpp
│   │   ├── SolidsManager.h/.cpp
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
  CAM Strategies (2D/3D/MultiAxis/Turning)
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