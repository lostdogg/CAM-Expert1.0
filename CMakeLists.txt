cmake_minimum_required(VERSION 3.20)
project(CAMExpert VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

function(camexpert_configure_target target)
    target_include_directories(${target} PUBLIC src)

    if(WIN32)
        target_compile_definitions(${target} PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(CAMExpertCad STATIC)
camexpert_configure_target(CAMExpertCad)
target_sources(CAMExpertCad PRIVATE
    src/cad/Geometry.cpp
    src/cad/ConstraintSolver.cpp
    src/cad/BRep.cpp
    src/cad/NurbsSurface.cpp
    src/cad/MeshData.cpp
    src/cad/FileImporter.cpp
    src/cad/ModelPrep.cpp
    src/cad/FeatureRecognition.cpp
    src/cad/WireframeScene.cpp
    src/cad/Geometry.h
    src/cad/ConstraintSolver.h
    src/cad/BRep.h
    src/cad/NurbsSurface.h
    src/cad/MeshData.h
    src/cad/FileImporter.h
    src/cad/ModelPrep.h
    src/cad/FeatureRecognition.h
    src/cad/WireframeScene.h
    src/cad/ZMap.h
)

add_library(CAMExpertCam STATIC)
camexpert_configure_target(CAMExpertCam)
target_sources(CAMExpertCam PRIVATE
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
    src/cam/SqlToolDatabase.cpp
    src/cam/DynamicPlane.cpp
    src/cam/TransformToolpath.cpp
    src/cam/ProbingCycles.cpp
    src/cam/WireEDM.cpp
    src/cam/Router.cpp
    src/cam/Art.cpp
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
    src/cam/SqlToolDatabase.h
    src/cam/DynamicPlane.h
    src/cam/TransformToolpath.h
    src/cam/ProbingCycles.h
    src/cam/WireEDM.h
    src/cam/Router.h
    src/cam/Art.h
)
target_link_libraries(CAMExpertCam PUBLIC CAMExpertCad)

add_library(CAMExpertSimulation STATIC)
camexpert_configure_target(CAMExpertSimulation)
target_sources(CAMExpertSimulation PRIVATE
    src/simulation/Backplot.cpp
    src/simulation/Verify.cpp
    src/simulation/MachineSimulation.cpp
    src/simulation/Backplot.h
    src/simulation/Verify.h
    src/simulation/MachineSimulation.h
)
target_link_libraries(CAMExpertSimulation PUBLIC CAMExpertCam CAMExpertCad)

add_library(CAMExpertCopilot STATIC)
camexpert_configure_target(CAMExpertCopilot)
target_sources(CAMExpertCopilot PRIVATE
    src/copilot/IntentParser.cpp
    src/copilot/ContextBuffer.cpp
    src/copilot/ParameterNegotiator.cpp
    src/copilot/AuditLog.cpp
    src/copilot/CopilotEngine.cpp
    src/copilot/LocalInferenceEngine.cpp
    src/copilot/VectorDatabase.cpp
    src/copilot/GeometricTokenizer.cpp
    src/copilot/ConstrainedOutputValidator.cpp
    src/copilot/SelfCorrectionLoop.cpp
    src/copilot/IntentParser.h
    src/copilot/ContextBuffer.h
    src/copilot/ParameterNegotiator.h
    src/copilot/AuditLog.h
    src/copilot/CopilotEngine.h
    src/copilot/LocalInferenceEngine.h
    src/copilot/VectorDatabase.h
    src/copilot/GeometricTokenizer.h
    src/copilot/ConstrainedOutputValidator.h
    src/copilot/SelfCorrectionLoop.h
)
target_link_libraries(CAMExpertCopilot PUBLIC CAMExpertCam CAMExpertCad)

add_executable(CAMExpert WIN32)
camexpert_configure_target(CAMExpert)
target_sources(CAMExpert PRIVATE
    src/main.cpp
    src/Application.cpp
    src/MainWindow.cpp
    src/MainWindowFileCommands.cpp
    src/MainWindowWireframeCommands.cpp
    src/MainWindowSurfaceCommands.cpp
    src/MainWindowSolidCommands.cpp
    src/MainWindowCamCommands.cpp
    src/MainWindowSetupWorkflowCommands.cpp
    src/ui/RibbonUI.cpp
    src/ui/Viewport3D.cpp
    src/ui/SelectionBar.cpp
    src/ui/CopilotPanel.cpp
    src/managers/ToolpathManager.cpp
    src/managers/SolidsManager.cpp
    src/managers/SurfacesManager.cpp
    src/managers/LevelsManager.cpp
    src/managers/PlanesManager.cpp
    src/Application.h
    src/MainWindow.h
    src/ui/CommandIds.h
    src/ui/MenuCommands.h
    src/ui/RibbonUI.h
    src/ui/Viewport3D.h
    src/ui/SelectionBar.h
    src/ui/CopilotPanel.h
    src/managers/ToolpathManager.h
    src/managers/SolidsManager.h
    src/managers/SurfacesManager.h
    src/managers/LevelsManager.h
    src/managers/PlanesManager.h
    src/resources/resource.h
)

if(WIN32)
    target_sources(CAMExpert PRIVATE src/resources/CAMExpert.rc)
endif()

target_link_libraries(CAMExpert PRIVATE
    CAMExpertCopilot
    CAMExpertSimulation
    CAMExpertCam
    CAMExpertCad
)

if(WIN32)
    set_target_properties(CAMExpert PROPERTIES
        LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS"
    )
    target_link_libraries(CAMExpert PRIVATE
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
    target_link_options(CAMExpert PRIVATE
        "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
    )
endif()

find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(CAMExpert PRIVATE OpenMP::OpenMP_CXX)
    target_link_libraries(CAMExpertCam PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(CAMExpert PRIVATE CAMEXPERT_USE_OPENMP=1)
    target_compile_definitions(CAMExpertCam PRIVATE CAMEXPERT_USE_OPENMP=1)
endif()

install(TARGETS CAMExpert DESTINATION bin)
