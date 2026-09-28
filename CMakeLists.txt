cmake_minimum_required(VERSION 3.20)
project(TSA VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Activer l'infrastructure Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# =============================================================================
# Configuration Qt 6
# =============================================================================
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg)

# =============================================================================
# Configuration OpenCASCADE (OCCT) et dépendances tierces
# =============================================================================
# Gestion automatique : si OCCT ou 3rdparty sont absents, CMake les télécharge
# automatiquement depuis les dépôts officiels GitHub (V8.0.1).
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupDependencies.cmake")
setup_external_dependencies()

find_package(OpenCASCADE REQUIRED)

message(STATUS "OpenCASCADE Version: ${OpenCASCADE_VERSION}")
message(STATUS "OpenCASCADE Include: ${OpenCASCADE_INCLUDE_DIR}")

# =============================================================================
# Fichiers sources — regroupés par module
# -----------------------------------------------------------------------------
# Les modules "métier" (sans dépendance Qt Widgets / UI) sont isolés dans des
# variables réutilisées à la fois par l'exécutable principal et par les tests
# (voir CORE_SOURCES plus bas), afin d'éviter de dupliquer la liste des
# fichiers à deux endroits.
# =============================================================================

# --- Point d'entrée et application ------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/App/Application.h
    src/App/Application.cpp
)

# --- Diagnostics (logs, crash handler, rapports) — [CORE] -------------------
set(DIAGNOSTICS_SOURCES
    src/Diagnostics/LogLevel.h
    src/Diagnostics/LogEntry.h
    src/Diagnostics/RingBuffer.h
    src/Diagnostics/Logger.h
    src/Diagnostics/Logger.cpp
    src/Diagnostics/CrashHandler.h
    src/Diagnostics/CrashHandler.cpp
    src/Diagnostics/DiagnosticReport.h
    src/Diagnostics/DiagnosticReport.cpp
)

# --- Fenêtre principale ------------------------------------------------------
set(UI_MAINWINDOW_SOURCES
    src/UI/MainWindow.h
    src/UI/MainWindow.cpp
    src/UI/MainWindow_Actions.cpp
    src/UI/MainWindow_Tools.cpp
    src/UI/MainWindow_Transform.cpp
)

# --- Arbre du modèle ----------------------------------------------------------
set(UI_MODELTREE_SOURCES
    src/UI/ModelTree/ModelTreeWidget.h
    src/UI/ModelTree/ModelTreeWidget.cpp
)

# --- Panneaux de propriétés par type d'élément -------------------------------
set(UI_PROPERTIES_SOURCES
    src/UI/Properties/IElementPropertyView.h
    src/UI/Properties/PropertyPanel.h
    src/UI/Properties/PropertyPanel.cpp
    src/UI/Properties/NodePropertiesView.h
    src/UI/Properties/NodePropertiesView.cpp
    src/UI/Properties/BeamPropertiesView.h
    src/UI/Properties/BeamPropertiesView.cpp
    src/UI/Properties/ColumnPropertiesView.h
    src/UI/Properties/ColumnPropertiesView.cpp
    src/UI/Properties/CablePropertiesView.h
    src/UI/Properties/CablePropertiesView.cpp
    src/UI/Properties/SlabPropertiesView.h
    src/UI/Properties/SlabPropertiesView.cpp
    src/UI/Properties/WallPropertiesView.h
    src/UI/Properties/WallPropertiesView.cpp
    src/UI/Properties/FoundationPropertiesView.h
    src/UI/Properties/FoundationPropertiesView.cpp
    src/UI/Properties/TrussMemberPropertiesView.h
    src/UI/Properties/TrussMemberPropertiesView.cpp
    src/UI/Properties/WorkPlanePropertiesView.h
    src/UI/Properties/WorkPlanePropertiesView.cpp
)

# --- Boîtes de dialogue -------------------------------------------------------
set(UI_DIALOGS_SOURCES
    src/UI/Dialogs/TransformDialog.h
    src/UI/Dialogs/TransformDialog.cpp
    src/UI/Dialogs/GridDialog.h
    src/UI/Dialogs/GridDialog.cpp
    src/UI/Dialogs/GridSettingsDialog.h
    src/UI/Dialogs/GridSettingsDialog.cpp
    src/UI/Dialogs/GridAdvancedSettingsDialog.h
    src/UI/Dialogs/GridAdvancedSettingsDialog.cpp
    src/UI/Dialogs/LevelDialog.h
    src/UI/Dialogs/LevelDialog.cpp
    src/UI/Dialogs/WorkPlaneDialog.h
    src/UI/Dialogs/WorkPlaneDialog.cpp
    src/UI/Dialogs/SectionCutDialog.h
    src/UI/Dialogs/SectionCutDialog.cpp
    src/UI/Dialogs/HelpDialog.h
    src/UI/Dialogs/HelpDialog.cpp
    src/UI/Dialogs/StructurePresetDialog.h
    src/UI/Dialogs/StructurePresetDialog.cpp
    src/UI/Dialogs/BarCreationDialog.h
    src/UI/Dialogs/BarCreationDialog.cpp
    src/UI/Dialogs/CableCreationDialog.h
    src/UI/Dialogs/CableCreationDialog.cpp
    src/UI/Dialogs/SectionCustomizationDialog.h
    src/UI/Dialogs/SectionCustomizationDialog.cpp
    src/UI/Dialogs/SurfaceCreationDialog.h
    src/UI/Dialogs/SurfaceCreationDialog.cpp
    src/UI/Dialogs/LibraryDialog.h
    src/UI/Dialogs/LibraryDialog.cpp
)

# Cette boîte de dialogue dépend d'ExtensionManager, testé indépendamment de
# l'UI : on l'isole pour pouvoir la réutiliser dans CORE_SOURCES. — [CORE]
set(UI_EXTENSIONMANAGERDIALOG_SOURCES
    src/UI/Dialogs/ExtensionManagerDialog.h
    src/UI/Dialogs/ExtensionManagerDialog.cpp
)

# --- Système d'extensions / bibliothèques de composants — [CORE] ------------
set(EXTENSIONSYSTEM_SOURCES
    src/ExtensionSystem/ExtensionTypes.h
    src/ExtensionSystem/ExtensionTypes.cpp
    src/ExtensionSystem/DefinitionModels.h
    src/ExtensionSystem/DefinitionModels.cpp
    src/ExtensionSystem/LibraryRegistry.h
    src/ExtensionSystem/LibraryRegistry.cpp
    src/ExtensionSystem/LibraryValidator.h
    src/ExtensionSystem/LibraryValidator.cpp
    src/ExtensionSystem/LibraryLoader.h
    src/ExtensionSystem/LibraryLoader.cpp
    src/ExtensionSystem/LibraryCache.h
    src/ExtensionSystem/LibraryCache.cpp
    src/ExtensionSystem/LibraryVersionManager.h
    src/ExtensionSystem/LibraryVersionManager.cpp
    src/ExtensionSystem/LibraryDependencyManager.h
    src/ExtensionSystem/LibraryDependencyManager.cpp
    src/ExtensionSystem/LibraryManager.h
    src/ExtensionSystem/LibraryManager.cpp
    src/ExtensionSystem/ExtensionManager.h
    src/ExtensionSystem/ExtensionManager.cpp
    src/ExtensionSystem/ExtensionPackager.h
    src/ExtensionSystem/ExtensionPackager.cpp
)

# --- Bibliothèque de composants (gestionnaire générique) — [CORE] -----------
set(LIBRARY_SOURCES
    src/Library/LibraryManager.h
    src/Library/LibraryManager.cpp
)

# --- Préréglages de création (UI uniquement) ---------------------------------
set(MODEL_PRESETS_SOURCES
    src/Model/CreationPresets.h
)

# --- Thème de l'application ---------------------------------------------------
set(UI_THEME_SOURCES
    src/UI/Theme/ThemeManager.h
    src/UI/Theme/ThemeManager.cpp
)

# --- Règles graduées du viewport ---------------------------------------------
set(UI_RULER_SOURCES
    src/UI/Ruler/ViewportRuler.h
    src/UI/Ruler/ViewportRuler.cpp
    src/UI/Ruler/ViewportContainer.h
    src/UI/Ruler/ViewportContainer.cpp
)

# --- Widgets divers ------------------------------------------------------------
set(UI_WIDGETS_SOURCES
    src/UI/Widgets/SectionPreviewWidget.h
    src/UI/Widgets/SectionPreviewWidget.cpp
)

# --- Ruban (Ribbon UI) ---------------------------------------------------------
set(UI_RIBBON_SOURCES
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
)

# --- Panneaux ancrés (docks) ----------------------------------------------------
set(UI_DOCK_SOURCES
    src/UI/Dock/VisibilityDock.h
    src/UI/Dock/VisibilityDock.cpp
    src/UI/Dock/StructuralElementsDock.h
    src/UI/Dock/StructuralElementsDock.cpp
    src/UI/Dock/LogConsoleDock.h
    src/UI/Dock/LogConsoleDock.cpp
)

# --- Systèmes de coordonnées et plans de travail — [CORE] --------------------
set(COORDINATE_SOURCES
    src/Coordinate/Point3D.h
    src/Coordinate/Level.h
    src/Coordinate/LevelManager.h
    src/Coordinate/LevelManager.cpp
    src/Coordinate/CoordinateSystem.h
    src/Coordinate/CoordinateSystem.cpp
    src/Coordinate/CylindricalCoordinates.h
    src/Coordinate/CylindricalCoordinates.cpp
    src/Coordinate/WorkPlane.h
    src/Coordinate/WorkPlane.cpp
    src/Coordinate/WorkPlaneManager.h
    src/Coordinate/WorkPlaneManager.cpp
    src/Coordinate/CoordinateTransformationService.h
    src/Coordinate/CoordinateTransformationService.cpp
)

# --- Grilles : définitions et logique (indépendantes du rendu) — [CORE] ------
set(GRID_CORE_SOURCES
    src/Grid/GridType.h
    src/Grid/GridDefinition.h
    src/Grid/GridDefinition.cpp
    src/Grid/CartesianGrid.h
    src/Grid/CartesianGrid.cpp
    src/Grid/CylindricalGrid.h
    src/Grid/CylindricalGrid.cpp
    src/Grid/ArbitraryGrid.h
    src/Grid/ArbitraryGrid.cpp
    src/Grid/GridSnapManager.h
    src/Grid/GridSnapManager.cpp
    src/Grid/GridSystem.h
    src/Grid/GridSystem.cpp
    src/Grid/GridManager.h
    src/Grid/GridManager.cpp
)

# --- Grilles : rendu visuel (nécessite le viewer OCCT, UI uniquement) --------
set(GRID_RENDERING_SOURCES
    src/Grid/GridLabelRenderer.h
    src/Grid/GridLabelRenderer.cpp
    src/Grid/GridRenderer.h
    src/Grid/GridRenderer.cpp
)

# --- Viewer 3D OCCT (UI uniquement) --------------------------------------------
set(VIEWER_MAIN_SOURCES
    src/Viewer/OccView.h
    src/Viewer/OccView.cpp
    src/Viewer/OccView_Shapes.cpp
    src/Viewer/OccView_Navigation.cpp
    src/Viewer/OccView_Events.cpp
    src/Viewer/SelectionManager.h
    src/Viewer/SelectionManager.cpp
)

# --- Matériaux visuels (texturing) — [CORE] -----------------------------------
set(VIEWER_CORE_SOURCES
    src/Viewer/MaterialVisual.h
    src/Viewer/MaterialVisual.cpp
    src/Viewer/TextureManager.h
    src/Viewer/TextureManager.cpp
)

# --- Géométrie des poutres (utilisée par les tests) — [CORE] -----------------
set(GEOMETRY_CORE_SOURCES
    src/Geometry/BeamGeometry.h
    src/Geometry/BeamGeometry.cpp
)

# --- Géométrie des autres éléments (UI uniquement) ----------------------------
set(GEOMETRY_MAIN_SOURCES
    src/Geometry/SlabGeometry.h
    src/Geometry/SlabGeometry.cpp
    src/Geometry/WallGeometry.h
    src/Geometry/WallGeometry.cpp
    src/Geometry/FoundationGeometry.h
    src/Geometry/FoundationGeometry.cpp
)

# --- Modèle de données structurel — [CORE] ------------------------------------
set(MODEL_CORE_SOURCES
    src/Model/Element.h
    src/Model/Material.h
    src/Model/Material.cpp
    src/Model/MaterialLibrary.h
    src/Model/MaterialLibrary.cpp
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
    src/Model/ModelDiff.h
    src/Model/ModelDiff.cpp
    src/Model/StructuralClipboard.h
    src/Model/StructuralClipboard.cpp
)

# --- Câbles : modèle et normes de calcul — [CORE] -----------------------------
set(CABLE_SOURCES
    src/Model/Cable/CableTypes.h
    src/Model/Cable/CableStandards.h
    src/Model/Cable/CableStandards.cpp
    src/Model/Cable/CableAnchor.h
    src/Model/Cable/CableAnchor.cpp
    src/Model/Cable/CablePrestress.h
    src/Model/Cable/CableAnalysisProperties.h
    src/Model/Cable/CableDefinition.h
    src/Model/Cable/CableDefinition.cpp
    src/Model/Cable/CableGeometry.h
    src/Model/Cable/CableGeometry.cpp
    src/Model/Cable/Cable.h
    src/Model/Cable/Cable.cpp
    src/Model/Cable/StayCable.h
    src/Model/Cable/StayCable.cpp
    src/Model/Cable/SuspensionSystem.h
    src/Model/Cable/SuspensionSystem.cpp
)

# --- Câbles : géométrie 3D, grille et bibliothèque — [CORE] ------------------
set(CABLE_GEOMETRY_SOURCES
    src/Geometry/CableGeometry3D.h
    src/Geometry/CableGeometry3D.cpp
    src/Grid/CableGrid.h
    src/Grid/CableGrid.cpp
    src/Library/CableLibrary.h
    src/Library/CableLibrary.cpp
)

# --- Gestion de projet — [CORE] -----------------------------------------------
set(PROJECT_SOURCES
    src/Project/ProjectManager.h
    src/Project/ProjectManager.cpp
)

# --- Commandes (pattern Command pour undo/redo) — [CORE] ---------------------
set(COMMANDS_SOURCES
    src/Commands/ICommand.h
    src/Commands/CommandCategory.h
    src/Commands/CommandCatalog.h
    src/Commands/CommandCatalog.cpp
    src/Commands/CreateElementCommands.h
    src/Commands/CreateElementCommands.cpp
    src/Commands/ModifyCommands.h
    src/Commands/ModifyCommands.cpp
    src/Commands/CreateBeamCommand.h
    src/Commands/CreateBeamCommand.cpp
    src/Commands/GridCommands.h
    src/Commands/GridCommands.cpp
)

# --- Undo / Redo — [CORE] -------------------------------------------------------
set(UNDOREDO_SOURCES
    src/UndoRedo/UndoManager.h
    src/UndoRedo/UndoManager.cpp
    src/UndoRedo/CommandManager.h
    src/UndoRedo/CommandManager.cpp
)

# --- Gestion des interactions utilisateur — [CORE] ---------------------------
set(INTERACTION_SOURCES
    src/Interaction/InteractionManager.h
    src/Interaction/InteractionManager.cpp
)

# --- Format de fichier TSA (sauvegarde / chargement) — [CORE] ----------------
set(IO_SOURCES
    src/IO/TSAFileFormat.h
    src/IO/TSAFile.h
    src/IO/TSAFile.cpp
    src/IO/TSAPreviewGenerator.h
    src/IO/TSAPreviewGenerator.cpp
)

# --- Intégration spécifique Windows (association de fichiers) ----------------
set(PLATFORM_SOURCES
    src/Platform/WindowsAssociation.h
    src/Platform/WindowsAssociation.cpp
)

# --- Ressources (icônes, .rc Windows) -----------------------------------------
set(RESOURCES_SOURCES
    resources/resources.qrc
    resources/TSA.rc
)

# --- Regroupement des modules "métier", partagés avec les tests unitaires ----
set(CORE_SOURCES
    ${DIAGNOSTICS_SOURCES}
    ${UI_EXTENSIONMANAGERDIALOG_SOURCES}
    ${EXTENSIONSYSTEM_SOURCES}
    ${LIBRARY_SOURCES}
    ${COORDINATE_SOURCES}
    ${GRID_CORE_SOURCES}
    ${VIEWER_CORE_SOURCES}
    ${GEOMETRY_CORE_SOURCES}
    ${MODEL_CORE_SOURCES}
    ${CABLE_SOURCES}
    ${CABLE_GEOMETRY_SOURCES}
    ${PROJECT_SOURCES}
    ${COMMANDS_SOURCES}
    ${UNDOREDO_SOURCES}
    ${INTERACTION_SOURCES}
    ${IO_SOURCES}
)

# --- Liste complète des sources de l'exécutable principal --------------------
set(SOURCES
    ${APP_SOURCES}
    ${UI_MAINWINDOW_SOURCES}
    ${UI_MODELTREE_SOURCES}
    ${UI_PROPERTIES_SOURCES}
    ${UI_DIALOGS_SOURCES}
    ${UI_THEME_SOURCES}
    ${UI_RULER_SOURCES}
    ${UI_WIDGETS_SOURCES}
    ${UI_RIBBON_SOURCES}
    ${UI_DOCK_SOURCES}
    ${MODEL_PRESETS_SOURCES}
    ${GRID_RENDERING_SOURCES}
    ${VIEWER_MAIN_SOURCES}
    ${GEOMETRY_MAIN_SOURCES}
    ${PLATFORM_SOURCES}
    ${RESOURCES_SOURCES}
    ${CORE_SOURCES}
)

# =============================================================================
# Exécutable principal
# =============================================================================
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
    TKOffset
    TKBO
    TKBool
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

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE DbgHelp)
endif()

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

# =============================================================================
# Tests unitaires
# -----------------------------------------------------------------------------
# Réutilise CORE_SOURCES : les tests ne compilent que la logique "métier",
# sans les fichiers spécifiques à l'UI (fenêtres, dialogues, rendu du viewer...).
# =============================================================================
enable_testing()

set(TEST_SOURCES
    tests/test_common.h
    tests/test_main.cpp
    tests/test_coordinates.cpp
    tests/test_model.cpp
    tests/test_file_io.cpp
    tests/test_commands.cpp
    tests/test_grids.cpp
    tests/test_viewer.cpp
    tests/test_cables.cpp
    tests/test_extensions.cpp
    tests/test_workplane.cpp
    resources/resources.qrc
    ${CORE_SOURCES}
)

add_executable(TSA_Tests ${TEST_SOURCES})
target_include_directories(TSA_Tests PRIVATE src tests ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(TSA_Tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets ${OCCT_LIBS})
if(WIN32)
    target_link_libraries(TSA_Tests PRIVATE DbgHelp)
endif()
if(MSVC)
    target_compile_options(TSA_Tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(TSA_Tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
set_target_properties(TSA_Tests PROPERTIES OUTPUT_NAME "TSA_TestSuite")

# Tests complets et suites thématiques enregistrés sous CTest
add_test(NAME TSA_AllTests COMMAND TSA_Tests)
add_test(NAME TSA_CoordinatesTests COMMAND TSA_Tests --suite=coordinates)
add_test(NAME TSA_ModelElementsTests COMMAND TSA_Tests --suite=model)
add_test(NAME TSA_FileIOTests COMMAND TSA_Tests --suite=io)
add_test(NAME TSA_CommandsUndoTests COMMAND TSA_Tests --suite=commands)
add_test(NAME TSA_GridsTests COMMAND TSA_Tests --suite=grids)
add_test(NAME TSA_ViewerTests COMMAND TSA_Tests --suite=viewer)
add_test(NAME TSA_CablesTests COMMAND TSA_Tests --suite=cables)
add_test(NAME TSA_ExtensionsTests COMMAND TSA_Tests --suite=extensions)
add_test(NAME TSA_WorkPlaneTests COMMAND TSA_Tests --suite=workplane)

# =============================================================================
# Déploiement automatique des DLLs et dépendances dans le dossier de l'exécutable
# =============================================================================
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