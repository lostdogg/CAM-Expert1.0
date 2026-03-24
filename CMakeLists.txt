cmake_minimum_required(VERSION 3.16)
project(CAMExpert VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/MainWindow.cpp
    src/ui/RibbonUI.cpp
    src/ui/Viewport3D.cpp
    src/ui/SelectionBar.cpp
    src/cad/Geometry.cpp
    src/cad/BRep.cpp
    src/cad/NurbsSurface.cpp
    src/cad/MeshData.cpp
    src/cad/FileImporter.cpp
    src/cad/ModelPrep.cpp
    src/cad/FeatureRecognition.cpp
    src/cam/Toolpath.cpp
    src/cam/DynamicMotion.cpp
    src/cam/Strategies2D.cpp
    src/cam/Strategies3D.cpp
    src/cam/MultiAxis.cpp
    src/cam/Turning.cpp
    src/cam/PostProcessor.cpp
    src/cam/NciFormat.cpp
    src/cam/MaterialLibrary.cpp
    src/cam/MillTurn.cpp
    src/cam/SwissMachining.cpp
    src/cam/CloudToolLibrary.cpp
    src/managers/ToolpathManager.cpp
    src/managers/SolidsManager.cpp
    src/managers/LevelsManager.cpp
    src/managers/PlanesManager.cpp
    src/simulation/Backplot.cpp
    src/simulation/Verify.cpp
    src/simulation/MachineSimulation.cpp
)

# Header files (for IDE visibility)
set(HEADERS
    src/Application.h
    src/MainWindow.h
    src/ui/RibbonUI.h
    src/ui/Viewport3D.h
    src/ui/SelectionBar.h
    src/cad/Geometry.h
    src/cad/BRep.h
    src/cad/NurbsSurface.h
    src/cad/MeshData.h
    src/cad/FileImporter.h
    src/cad/ModelPrep.h
    src/cad/FeatureRecognition.h
    src/cam/Toolpath.h
    src/cam/DynamicMotion.h
    src/cam/Strategies2D.h
    src/cam/Strategies3D.h
    src/cam/MultiAxis.h
    src/cam/Turning.h
    src/cam/PostProcessor.h
    src/cam/NciFormat.h
    src/cam/MaterialLibrary.h
    src/cam/MillTurn.h
    src/cam/SwissMachining.h
    src/cam/CloudToolLibrary.h
    src/managers/ToolpathManager.h
    src/managers/SolidsManager.h
    src/managers/LevelsManager.h
    src/managers/PlanesManager.h
    src/simulation/Backplot.h
    src/simulation/Verify.h
    src/simulation/MachineSimulation.h
    src/resources/resource.h
)

# Resource file (Windows only)
if(WIN32)
    list(APPEND SOURCES src/resources/CAMExpert.rc)
endif()

add_executable(CAMExpert WIN32 ${SOURCES} ${HEADERS})

target_include_directories(CAMExpert PRIVATE src)

# Windows libraries
if(WIN32)
    target_link_libraries(CAMExpert
        opengl32
        glu32
        gdi32
        user32
        comctl32
        comdlg32
        shell32
        ole32
        oleaut32
        uuid
        dwmapi
        uxtheme
    )
    # Enable visual styles (Common Controls v6)
    target_link_options(CAMExpert PRIVATE
        "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(CAMExpert PRIVATE /W4 /MP)
    target_compile_definitions(CAMExpert PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(CAMExpert PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS CAMExpert DESTINATION bin)
