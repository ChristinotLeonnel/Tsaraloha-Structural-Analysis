cmake_minimum_required(VERSION 3.20)
project(TSA VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Activer l'infrastructure Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -----------------------------------------------------------------------------
# Configuration Qt 6
# -----------------------------------------------------------------------------
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg)

# -----------------------------------------------------------------------------
# Configuration OpenCASCADE (OCCT)
# -----------------------------------------------------------------------------
# On peut définir OpenCASCADE_DIR en ligne de commande ou via variable d'environnement :
# ex: -DOpenCASCADE_DIR="C:/OpenCASCADE-7.8.0/cmake"
find_package(OpenCASCADE REQUIRED)

message(STATUS "OpenCASCADE Version: ${OpenCASCADE_VERSION}")
message(STATUS "OpenCASCADE Include: ${OpenCASCADE_INCLUDE_DIR}")

# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/App/Application.h
    src/App/Application.cpp
    src/UI/MainWindow.h
    src/UI/MainWindow.cpp
    src/UI/ModelTree/ModelTreeWidget.h
    src/UI/ModelTree/ModelTreeWidget.cpp
    src/UI/Properties/PropertyPanel.h
    src/UI/Properties/PropertyPanel.cpp
    src/UI/Dialogs/TransformDialog.h
    src/UI/Dialogs/TransformDialog.cpp
    src/UI/Dialogs/GridDialog.h
    src/UI/Dialogs/GridDialog.cpp
    src/UI/Dialogs/GridSettingsDialog.h
    src/UI/Dialogs/GridSettingsDialog.cpp
    src/UI/Dialogs/LevelDialog.h
    src/UI/Dialogs/LevelDialog.cpp
    src/UI/Dialogs/SectionCutDialog.h
    src/UI/Dialogs/SectionCutDialog.cpp
    src/UI/Dialogs/HelpDialog.h
    src/UI/Dialogs/HelpDialog.cpp
    src/UI/Dialogs/StructurePresetDialog.h
    src/UI/Dialogs/StructurePresetDialog.cpp
    src/UI/Dialogs/BarCreationDialog.h
    src/UI/Dialogs/BarCreationDialog.cpp
    src/UI/Dialogs/SurfaceCreationDialog.h
    src/UI/Dialogs/SurfaceCreationDialog.cpp
    src/Model/CreationPresets.h
    src/UI/Theme/ThemeManager.h
    src/UI/Theme/ThemeManager.cpp
    src/UI/Ruler/ViewportRuler.h
    src/UI/Ruler/ViewportRuler.cpp
    src/UI/Ruler/ViewportContainer.h
    src/UI/Ruler/ViewportContainer.cpp
    src/UI/Ribbon/RibbonTypes.h
    src/UI/Ribbon/RibbonButton.h
    src/UI/Ribbon/RibbonButton.cpp
    src/UI/Ribbon/RibbonPanel.h
    src/UI/Ribbon/RibbonPanel.cpp
    src/UI/Ribbon/RibbonTab.h
    src/UI/Ribbon/RibbonTab.cpp
    src/UI/Ribbon/RibbonBar.h
    src/UI/Ribbon/RibbonBar.cpp
    src/UI/Ribbon/RibbonBuilder.h
    src/UI/Ribbon/RibbonBuilder.cpp
    src/UI/Dock/VisibilityDock.h
    src/UI/Dock/VisibilityDock.cpp
    src/UI/Dock/LogConsoleDock.h
    src/UI/Dock/LogConsoleDock.cpp
    src/Coordinate/Point3D.h
    src/Coordinate/Level.h
    src/Coordinate/LevelManager.h
    src/Coordinate/LevelManager.cpp
    src/Coordinate/CoordinateSystem.h
    src/Coordinate/CoordinateSystem.cpp
    src/Coordinate/CylindricalCoordinates.h
    src/Coordinate/CylindricalCoordinates.cpp
    src/Grid/GridType.h
    src/Grid/GridDefinition.h
    src/Grid/GridDefinition.cpp
    src/Grid/CartesianGrid.h
    src/Grid/CartesianGrid.cpp
    src/Grid/CylindricalGrid.h
    src/Grid/CylindricalGrid.cpp
    src/Grid/GridLabelRenderer.h
    src/Grid/GridLabelRenderer.cpp
    src/Grid/GridRenderer.h
    src/Grid/GridRenderer.cpp
    src/Grid/GridSnapManager.h
    src/Grid/GridSnapManager.cpp
    src/Grid/GridSystem.h
    src/Grid/GridSystem.cpp
    src/Grid/GridManager.h
    src/Grid/GridManager.cpp
    src/Viewer/OccView.h
    src/Viewer/OccView.cpp
    src/Viewer/SelectionManager.h
    src/Viewer/SelectionManager.cpp
    src/Geometry/BeamGeometry.h
    src/Geometry/BeamGeometry.cpp
    src/Geometry/SlabGeometry.h
    src/Geometry/SlabGeometry.cpp
    src/Geometry/WallGeometry.h
    src/Geometry/WallGeometry.cpp
    src/Geometry/FoundationGeometry.h
    src/Geometry/FoundationGeometry.cpp
    src/Model/Material.h
    src/Model/Material.cpp
    src/Model/Section.h
    src/Model/Section.cpp
    src/Model/Node.h
    src/Model/Node.cpp
    src/Model/Beam.h
    src/Model/Beam.cpp
    src/Model/Column.h
    src/Model/Column.cpp
    src/Model/Slab.h
    src/Model/Slab.cpp
    src/Model/Wall.h
    src/Model/Wall.cpp
    src/Model/Foundation.h
    src/Model/Foundation.cpp
    src/Model/TrussMember.h
    src/Model/TrussMember.cpp
    src/Model/Model.h
    src/Model/Model.cpp
    resources/resources.qrc
    resources/TSA.rc
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${OpenCASCADE_INCLUDE_DIR}
)

# Modules OCCT nécessaires pour l'affichage 3D et primitives de base
set(OCCT_LIBS
    TKernel
    TKMath
    TKG2d
    TKG3d
    TKGeomBase
    TKGeomAlgo
    TKBRep
    TKTopAlgo
    TKPrim
    TKService
    TKV3d
    TKOpenGl
    TKMesh
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    ${OCCT_LIBS}
)

# Propriétés spécifiques MSVC / Windows
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
    # Définition pour éviter min/max de windows.h
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

    # Sous-système Windows :
    # - Mode Debug : conserve la console cmd pour afficher les logs (qDebug, std::cout, diagnostics)
    # - Mode Release / RelWithDebInfo / MinSizeRel : mode fenêtré pur sans console cmd
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
        $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>
        $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
    )
endif()

# -----------------------------------------------------------------------------
# Tests unitaires
# -----------------------------------------------------------------------------
enable_testing()

set(TEST_SOURCES
    tests/test_coordinates.cpp
    src/Coordinate/Point3D.h
    src/Coordinate/Level.h
    src/Coordinate/LevelManager.h
    src/Coordinate/LevelManager.cpp
    src/Coordinate/CoordinateSystem.h
    src/Coordinate/CoordinateSystem.cpp
    src/Coordinate/CylindricalCoordinates.h
    src/Coordinate/CylindricalCoordinates.cpp
    src/Model/Material.h
    src/Model/Material.cpp
    src/Model/Section.h
    src/Model/Section.cpp
    src/Model/Node.h
    src/Model/Node.cpp
    src/Model/Beam.h
    src/Model/Beam.cpp
    src/Model/Column.h
    src/Model/Column.cpp
    src/Model/Slab.h
    src/Model/Slab.cpp
    src/Model/Wall.h
    src/Model/Wall.cpp
    src/Model/Foundation.h
    src/Model/Foundation.cpp
    src/Model/TrussMember.h
    src/Model/TrussMember.cpp
    src/Model/Model.h
    src/Model/Model.cpp
    src/Grid/GridType.h
    src/Grid/GridDefinition.h
    src/Grid/GridDefinition.cpp
    src/Grid/CartesianGrid.h
    src/Grid/CartesianGrid.cpp
    src/Grid/CylindricalGrid.h
    src/Grid/CylindricalGrid.cpp
    src/Grid/GridSystem.h
    src/Grid/GridSystem.cpp
    src/Grid/GridSnapManager.h
    src/Grid/GridSnapManager.cpp
    src/Geometry/BeamGeometry.h
    src/Geometry/BeamGeometry.cpp
)

add_executable(TSA_Tests ${TEST_SOURCES})
target_include_directories(TSA_Tests PRIVATE src ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(TSA_Tests PRIVATE Qt6::Core ${OCCT_LIBS})
if(MSVC)
    target_compile_options(TSA_Tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(TSA_Tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
add_test(NAME CoordinatesAndLevelsTest COMMAND TSA_Tests)

# -----------------------------------------------------------------------------
# Déploiement automatique des DLLs et dépendances dans le dossier de l'exécutable
# -----------------------------------------------------------------------------
if(WIN32)
    # Localisation de windeployqt pour Qt 6
    get_target_property(_qmake_loc Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_loc}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_DIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DTARGET_FILE=$<TARGET_FILE:${PROJECT_NAME}>
            -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployDependencies.cmake"
        COMMENT "Copie automatique de toutes les DLLs (Qt, OCCT, 3rdparty) vers le dossier du .exe..."
    )
endif()

