# Base technique commune de TSA et de TSALab : listes des sources et construction d'un produit.
#
# TSA (ce dépôt) et TSALab (dépôt voisin) compilent les MÊMES sources (src/, tests/) : un bug corrigé
# ici l'est pour les deux. Ce qui distingue un produit (nom, extension, signature de fichier, icônes,
# panneaux propres…) est fourni par son dossier product/ (ProductIdentity.h, ProductShellIds.h,
# ) et, pour TSALab, par ses sources propres (OVERLAY_DIRS).
#
# Utilisation (après find_package(Qt6 …) et find_package(OpenCASCADE …)) :
#   include(<racine TSA>/cmake/TSAProduct.cmake)
#   tsa_setup_compiler_launchers()
#   tsa_add_product(NAME TSA PRODUCT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/product RESOURCES …)

get_filename_component(TSA_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# Dépendances (cmake/SetupDependencies.cmake, inclus par le produit avant find_package) : contrôles, puis rapport en fin
# de configuration, quand Gmsh, IfcOpenShell et les outils ont été enregistrés.
tsa_validate_dependencies()
include("${CMAKE_CURRENT_LIST_DIR}/Gmsh.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/IfcOpenShell.cmake")
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL tsa_write_dependency_report)
# Diagnostic à la demande (lecture seule) : rapport de configuration + présence actuelle des fichiers retenus.
add_custom_target(tsa-dependency-report
    COMMAND ${CMAKE_COMMAND} "-DREPORT=${CMAKE_BINARY_DIR}/tsa-dependencies.txt" "-DSOURCE_DIR=${TSA_ROOT}"
            -P "${CMAKE_CURRENT_LIST_DIR}/DependencyReport.cmake"
    COMMENT "Rapport des dépendances" VERBATIM)
# Cœur scientifique : dépôt TSALab voisin (TSALab/science).
set(TSALAB_ROOT_DIR "${TSA_ROOT}/../TSALab" CACHE PATH "Racine du dépôt TSALab (cœur scientifique tsalab_science)")
get_filename_component(TSALAB_ROOT_DIR "${TSALAB_ROOT_DIR}" ABSOLUTE)
# Outils et tests du cœur scientifique : compilés par TSALab, pas par défaut dans le build de TSA.
if(NOT DEFINED TSALAB_SCIENCE_TOOLS)
    set(TSALAB_SCIENCE_TOOLS OFF CACHE BOOL "Compiler tsalab-bench et les tests du cœur scientifique")
endif()
# Moteur EF 3D « fem3d » (dépôt voisin 3DDesplacement, TSA-3D-FEM-Engine) : FACULTATIF. Présent, il est compilé et
# enregistré comme moteur secondaire (docs/ANALYSIS_ENGINES.md) ; absent, TSA se construit sans lui (OpenSees reste
# le moteur 3D) et le signale.
set(TSA_FEM3D_DIR "${TSA_ROOT}/../3DDesplacement" CACHE PATH "Racine du dépôt du moteur EF 3D Fem3D (facultatif)")
get_filename_component(TSA_FEM3D_DIR "${TSA_FEM3D_DIR}" ABSOLUTE)
if(EXISTS "${TSA_FEM3D_DIR}/CMakeLists.txt" AND EXISTS "${TSA_FEM3D_DIR}/include/Fem3D/Analysis/LinearStaticAnalysis.h")
    set(TSA_HAS_FEM3D ON)
else()
    set(TSA_HAS_FEM3D OFF)
    message(STATUS "Moteur Fem3D absent (${TSA_FEM3D_DIR}) : TSA est construit sans le moteur « fem3d ».")
endif()

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
    ${TSA_ROOT}/src/main.cpp
    ${TSA_ROOT}/src/App/Application.h
    ${TSA_ROOT}/src/App/Application.cpp
    ${TSA_ROOT}/src/App/ProductInfo.h
)

# --- Diagnostics (logs, crash handler, rapports) — [CORE] -------------------
set(DIAGNOSTICS_SOURCES
    ${TSA_ROOT}/src/Diagnostics/LogLevel.h
    ${TSA_ROOT}/src/Diagnostics/LogEntry.h
    ${TSA_ROOT}/src/Diagnostics/RingBuffer.h
    ${TSA_ROOT}/src/Diagnostics/Logger.h
    ${TSA_ROOT}/src/Diagnostics/Logger.cpp
    ${TSA_ROOT}/src/Diagnostics/CrashHandler.h
    ${TSA_ROOT}/src/Diagnostics/CrashHandler.cpp
    ${TSA_ROOT}/src/Diagnostics/DiagnosticReport.h
    ${TSA_ROOT}/src/Diagnostics/DiagnosticReport.cpp
)

# --- Fenêtre principale ------------------------------------------------------
set(UI_MAINWINDOW_SOURCES
    ${TSA_ROOT}/src/UI/MainWindow.h
    ${TSA_ROOT}/src/UI/MainWindow.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Actions.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Tools.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Transform.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Bim.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Topology.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Dimensions.cpp
    ${TSA_ROOT}/src/UI/MainWindow_DisplayMode.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Shortcuts.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Tsa3d.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Reports.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Mesh.cpp
    ${TSA_ROOT}/src/UI/MainWindow_MeshView.cpp
    ${TSA_ROOT}/src/UI/MainWindow_ModelingTools.cpp
)

# --- Arbre du modèle — [CORE] (testé par <Produit>_Tests, suite cleanup) ------------
set(UI_MODELTREE_SOURCES
    ${TSA_ROOT}/src/UI/ModelTree/ModelTreeWidget.h
    ${TSA_ROOT}/src/UI/ModelTree/ModelTreeWidget.cpp
)

# --- Panneaux de propriétés par type d'élément -------------------------------
set(UI_PROPERTIES_SOURCES
    ${TSA_ROOT}/src/UI/Properties/IElementPropertyView.h
    ${TSA_ROOT}/src/UI/Properties/EditedFields.h
    ${TSA_ROOT}/src/UI/Properties/PropertyPanel.h
    ${TSA_ROOT}/src/UI/Properties/PropertyPanel.cpp
    ${TSA_ROOT}/src/UI/Properties/ElementBimPanel.h
    ${TSA_ROOT}/src/UI/Properties/ElementBimPanel.cpp
    ${TSA_ROOT}/src/UI/Properties/ElementResultsPanel.h
    ${TSA_ROOT}/src/UI/Properties/ElementResultsPanel.cpp
    ${TSA_ROOT}/src/UI/Properties/MemberEndNodesWidget.h
    ${TSA_ROOT}/src/UI/Properties/MemberEndNodesWidget.cpp
    ${TSA_ROOT}/src/UI/Properties/NodePropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/NodePropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/BeamPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/BeamPropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/ColumnPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/ColumnPropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/CablePropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/CablePropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/SlabPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/SlabPropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/WallPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/WallPropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/FoundationPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/FoundationPropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/TrussMemberPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/TrussMemberPropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/WorkPlanePropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/WorkPlanePropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/LoadPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/LoadPropertiesView.cpp
    ${TSA_ROOT}/src/UI/Properties/BimProductPropertiesView.h
    ${TSA_ROOT}/src/UI/Properties/BimProductPropertiesView.cpp
)

# --- Boîtes de dialogue -------------------------------------------------------
set(UI_DIALOGS_SOURCES
    ${TSA_ROOT}/src/UI/Shortcuts/ShortcutConfig.h
    ${TSA_ROOT}/src/UI/Shortcuts/ShortcutConfig.cpp
    ${TSA_ROOT}/src/UI/Shortcuts/ShortcutManager.h
    ${TSA_ROOT}/src/UI/Shortcuts/ShortcutManager.cpp
    ${TSA_ROOT}/src/UI/Dialogs/ShortcutEditorDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/ShortcutEditorDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/LanguageDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/LanguageDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/DimensionDialogs.h
    ${TSA_ROOT}/src/UI/Dialogs/DimensionDialogs.cpp
    ${TSA_ROOT}/src/UI/Dialogs/IfcDialogs.h
    ${TSA_ROOT}/src/UI/Dialogs/IfcDialogs.cpp
    ${TSA_ROOT}/src/UI/Dialogs/TopologyDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/TopologyDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/SurfaceLoadsDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/SurfaceLoadsDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/SurfaceSupportsDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/SurfaceSupportsDialog.cpp
    ${TSA_ROOT}/src/UI/HelpLauncher.h
    ${TSA_ROOT}/src/UI/HelpLauncher.cpp
    ${TSA_ROOT}/src/UI/Dialogs/GridDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/GridDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/GridSettingsDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/GridSettingsDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/NamedViewsDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/NamedViewsDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/GridAdvancedSettingsDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/GridAdvancedSettingsDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/LevelDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/LevelDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/WorkPlaneDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/WorkPlaneDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/SectionCutDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/SectionCutDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/HelpDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/HelpDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/StructurePresetDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/StructurePresetDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/BarCreationDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/BarCreationDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/CableCreationDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/CableCreationDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/SectionCustomizationDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/SectionCustomizationDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/SurfaceCreationDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/SurfaceCreationDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/LibraryDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/LibraryDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/NodeSelectionDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/NodeSelectionDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/NewNodeDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/NewNodeDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/NodalLoadDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/NodalLoadDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/MemberLoadDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/MemberLoadDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/LoadCaseDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/LoadCaseDialog.cpp
    ${TSA_ROOT}/src/UI/Widgets/StandardDialogHeader.h
    ${TSA_ROOT}/src/UI/Widgets/StandardDialogHeader.cpp
    ${TSA_ROOT}/src/UI/Dialogs/DialogStandardHelper.h
    ${TSA_ROOT}/src/UI/Dialogs/DialogStandardHelper.cpp
)

# Cette boîte de dialogue dépend d'ExtensionManager, testé indépendamment de
# l'UI : on l'isole pour pouvoir la réutiliser dans CORE_SOURCES. — [CORE]
# Fenêtre Analysis multi-moteurs et panneaux d'options par moteur — [CORE] (testée par TSA_Tests)
set(UI_TOOLS_SOURCES
    ${TSA_ROOT}/src/UI/Tools/ModelingToolDialog.h
    ${TSA_ROOT}/src/UI/Tools/ModelingToolDialog.cpp
    ${TSA_ROOT}/src/UI/Tools/ModelCleanupDialog.h
    ${TSA_ROOT}/src/UI/Tools/ModelCleanupDialog.cpp
)

set(UI_ANALYSIS_SOURCES
    ${TSA_ROOT}/src/UI/Analysis/AnalysisEngineOptions.h
    ${TSA_ROOT}/src/UI/Analysis/AnalysisEngineOptions.cpp
    ${TSA_ROOT}/src/UI/Analysis/OpenSeesOptionsWidget.h
    ${TSA_ROOT}/src/UI/Analysis/OpenSeesOptionsWidget.cpp
    ${TSA_ROOT}/src/UI/Analysis/Custom2DOptionsWidget.h
    ${TSA_ROOT}/src/UI/Analysis/Custom2DOptionsWidget.cpp
    ${TSA_ROOT}/src/UI/Analysis/Fem3DOptionsWidget.h
    ${TSA_ROOT}/src/UI/Analysis/Fem3DOptionsWidget.cpp
    ${TSA_ROOT}/src/UI/Analysis/AnalysisDialog.h
    ${TSA_ROOT}/src/UI/Analysis/AnalysisDialog.cpp
    ${TSA_ROOT}/src/UI/Analysis/AnalysisManagerPanel.h
    ${TSA_ROOT}/src/UI/Analysis/AnalysisManagerPanel.cpp
)

set(UI_EXTENSIONMANAGERDIALOG_SOURCES
    ${TSA_ROOT}/src/UI/Dialogs/ExtensionManagerDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/ExtensionManagerDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/ModulesDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/ModulesDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/ReportTemplatesDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/ReportTemplatesDialog.cpp
    ${TSA_ROOT}/src/UI/Mesh/MeshController.h
    ${TSA_ROOT}/src/UI/Mesh/MeshController.cpp
    ${TSA_ROOT}/src/UI/Mesh/MeshAnalysisBridge.h
    ${TSA_ROOT}/src/UI/Mesh/MeshAnalysisBridge.cpp
    ${TSA_ROOT}/src/UI/Mesh/MeshSettingsPanel.h
    ${TSA_ROOT}/src/UI/Mesh/MeshSettingsPanel.cpp
    ${TSA_ROOT}/src/UI/Mesh/MeshDialogs.h
    ${TSA_ROOT}/src/UI/Mesh/MeshDialogs.cpp
)

# --- Gestionnaire centralisé des fenêtres et disposition (Window Manager) — [CORE] ---
set(UI_WINDOWMANAGER_SOURCES
    ${TSA_ROOT}/src/UI/WindowManager/WindowItem.h
    ${TSA_ROOT}/src/UI/WindowManager/WindowItem.cpp
    ${TSA_ROOT}/src/UI/WindowManager/WindowRegistry.h
    ${TSA_ROOT}/src/UI/WindowManager/WindowRegistry.cpp
    ${TSA_ROOT}/src/UI/WindowManager/LayoutManager.h
    ${TSA_ROOT}/src/UI/WindowManager/LayoutManager.cpp
    ${TSA_ROOT}/src/UI/WindowManager/WindowManager.h
    ${TSA_ROOT}/src/UI/WindowManager/WindowManager.cpp
)

# --- Système d'extensions / bibliothèques de composants — [CORE] ------------
set(EXTENSIONSYSTEM_SOURCES
    ${TSA_ROOT}/src/ExtensionSystem/ExtensionTypes.h
    ${TSA_ROOT}/src/ExtensionSystem/ExtensionTypes.cpp
    ${TSA_ROOT}/src/ExtensionSystem/DefinitionModels.h
    ${TSA_ROOT}/src/ExtensionSystem/DefinitionModels.cpp
    ${TSA_ROOT}/src/ExtensionSystem/LibraryRegistry.h
    ${TSA_ROOT}/src/ExtensionSystem/LibraryRegistry.cpp
    ${TSA_ROOT}/src/ExtensionSystem/LibraryValidator.h
    ${TSA_ROOT}/src/ExtensionSystem/LibraryValidator.cpp
    ${TSA_ROOT}/src/ExtensionSystem/LibraryLoader.h
    ${TSA_ROOT}/src/ExtensionSystem/LibraryLoader.cpp
    ${TSA_ROOT}/src/ExtensionSystem/LibraryCache.h
    ${TSA_ROOT}/src/ExtensionSystem/LibraryCache.cpp
    ${TSA_ROOT}/src/ExtensionSystem/LibraryVersionManager.h
    ${TSA_ROOT}/src/ExtensionSystem/LibraryVersionManager.cpp
    ${TSA_ROOT}/src/ExtensionSystem/LibraryDependencyManager.h
    ${TSA_ROOT}/src/ExtensionSystem/LibraryDependencyManager.cpp
    ${TSA_ROOT}/src/ExtensionSystem/LibraryManager.h
    ${TSA_ROOT}/src/ExtensionSystem/LibraryManager.cpp
    ${TSA_ROOT}/src/ExtensionSystem/ExtensionManager.h
    ${TSA_ROOT}/src/ExtensionSystem/ExtensionManager.cpp
    ${TSA_ROOT}/src/ExtensionSystem/ExtensionPackager.h
    ${TSA_ROOT}/src/ExtensionSystem/ExtensionPackager.cpp
)

# --- Bibliothèque de composants (gestionnaire générique) — [CORE] -----------
set(LIBRARY_SOURCES
    ${TSA_ROOT}/src/Library/LibraryManager.h
    ${TSA_ROOT}/src/Library/LibraryManager.cpp
)

# --- Préréglages de création (UI uniquement) ---------------------------------
set(MODEL_PRESETS_SOURCES
    ${TSA_ROOT}/src/Model/CreationPresets.h
)

# --- Thème de l'application ---------------------------------------------------
set(UI_THEME_SOURCES
    ${TSA_ROOT}/src/UI/Theme/ThemeManager.h
    ${TSA_ROOT}/src/UI/Theme/ThemeManager.cpp
    ${TSA_ROOT}/src/UI/Theme/ThemedIcon.h
    ${TSA_ROOT}/src/UI/Theme/ThemedIcon.cpp
)

# --- Règles graduées du viewport ---------------------------------------------
set(UI_RULER_SOURCES
    ${TSA_ROOT}/src/UI/Ruler/ViewportRuler.h
    ${TSA_ROOT}/src/UI/Ruler/ViewportRuler.cpp
    ${TSA_ROOT}/src/UI/Ruler/ViewportContainer.h
    ${TSA_ROOT}/src/UI/Ruler/ViewportContainer.cpp
)

# --- Widgets divers ------------------------------------------------------------
set(UI_WIDGETS_SOURCES
    ${TSA_ROOT}/src/UI/Display/DisplayMenus.h
    ${TSA_ROOT}/src/UI/Display/DisplayMenus.cpp
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintScene.h
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintScene.cpp
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintEditor.h
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintEditor.cpp
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintObjectViewer.h
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintObjectViewer.cpp
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintSubgraphDialog.h
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintSubgraphDialog.cpp
    ${TSA_ROOT}/src/UI/Common/EcosystemApplication.h
    ${TSA_ROOT}/src/UI/Common/EcosystemApplication.cpp
    ${TSA_ROOT}/src/UI/Common/SelectionSynchronizer.h
    ${TSA_ROOT}/src/UI/Common/SelectionSynchronizer.cpp
    ${TSA_ROOT}/src/UI/Widgets/SectionPreviewWidget.h
    ${TSA_ROOT}/src/UI/Widgets/SectionPreviewWidget.cpp
    ${TSA_ROOT}/src/UI/Widgets/PointSelector.h
    ${TSA_ROOT}/src/UI/Widgets/PointSelector.cpp
    ${TSA_ROOT}/src/UI/Diagrams/Diagram2DWidget.h
    ${TSA_ROOT}/src/UI/Diagrams/Diagram2DWidget.cpp
    ${TSA_ROOT}/src/NDC/NDCViewerWidget.h
    ${TSA_ROOT}/src/NDC/NDCViewerWidget.cpp
    ${TSA_ROOT}/src/NDC/ReportConfigDialog.h
    ${TSA_ROOT}/src/NDC/ReportConfigDialog.cpp
    ${TSA_ROOT}/src/UI/Widgets/ProjectStatusOverlay.h
    ${TSA_ROOT}/src/UI/Widgets/ProjectStatusOverlay.cpp
)

# --- Ruban (Ribbon UI) ---------------------------------------------------------
set(UI_RIBBON_SOURCES
    ${TSA_ROOT}/src/UI/Ribbon/RibbonTypes.h
    ${TSA_ROOT}/src/UI/Ribbon/RibbonButton.h
    ${TSA_ROOT}/src/UI/Ribbon/RibbonButton.cpp
    ${TSA_ROOT}/src/UI/Ribbon/RibbonPanel.h
    ${TSA_ROOT}/src/UI/Ribbon/RibbonPanel.cpp
    ${TSA_ROOT}/src/UI/Ribbon/RibbonTab.h
    ${TSA_ROOT}/src/UI/Ribbon/RibbonTab.cpp
    ${TSA_ROOT}/src/UI/Ribbon/RibbonBar.h
    ${TSA_ROOT}/src/UI/Ribbon/RibbonBar.cpp
    ${TSA_ROOT}/src/UI/Ribbon/RibbonBuilder.h
    ${TSA_ROOT}/src/UI/Ribbon/RibbonBuilder.cpp
)

# --- Panneaux ancrés (docks) ----------------------------------------------------
set(UI_DOCK_SOURCES
    ${TSA_ROOT}/src/UI/Dock/VisibilityDock.h
    ${TSA_ROOT}/src/UI/Dock/VisibilityDock.cpp
    ${TSA_ROOT}/src/UI/Dock/StructuralElementsDock.h
    ${TSA_ROOT}/src/UI/Dock/StructuralElementsDock.cpp
    ${TSA_ROOT}/src/UI/Dock/LogConsoleDock.h
    ${TSA_ROOT}/src/UI/Dock/LogConsoleDock.cpp
    ${TSA_ROOT}/src/UI/Dock/ProjectionViewDock.h
    ${TSA_ROOT}/src/UI/Dock/ProjectionViewDock.cpp
    ${TSA_ROOT}/src/UI/Dock/ResultsDockWidget.h
    ${TSA_ROOT}/src/UI/Dock/ResultsDockWidget.cpp
    ${TSA_ROOT}/src/UI/Dock/AnalysisDataDock.h
    ${TSA_ROOT}/src/UI/Dock/AnalysisDataDock.cpp
)

# --- Systèmes de coordonnées et plans de travail — [CORE] --------------------
set(COORDINATE_SOURCES
    ${TSA_ROOT}/src/Coordinate/Point3D.h
    ${TSA_ROOT}/src/Coordinate/Level.h
    ${TSA_ROOT}/src/Coordinate/LevelManager.h
    ${TSA_ROOT}/src/Coordinate/LevelManager.cpp
    ${TSA_ROOT}/src/Coordinate/CoordinateSystem.h
    ${TSA_ROOT}/src/Coordinate/CoordinateSystem.cpp
    ${TSA_ROOT}/src/Coordinate/CylindricalCoordinates.h
    ${TSA_ROOT}/src/Coordinate/CylindricalCoordinates.cpp
    ${TSA_ROOT}/src/Coordinate/WorkPlane.h
    ${TSA_ROOT}/src/Coordinate/GeometryTolerance.h
    ${TSA_ROOT}/src/Coordinate/WorkPlane.cpp
    ${TSA_ROOT}/src/Coordinate/WorkPlaneCoordinateSystem.h
    ${TSA_ROOT}/src/Coordinate/WorkPlaneCoordinateSystem.cpp
    ${TSA_ROOT}/src/Coordinate/CoordinateTransform.h
    ${TSA_ROOT}/src/Coordinate/CoordinateTransform.cpp
    ${TSA_ROOT}/src/Coordinate/AxisColorConfig.h
    ${TSA_ROOT}/src/Coordinate/AxisColorConfig.cpp
    ${TSA_ROOT}/src/Coordinate/WorkPlaneManager.h
    ${TSA_ROOT}/src/Coordinate/WorkPlaneManager.cpp
    ${TSA_ROOT}/src/Coordinate/CoordinateTransformationService.h
    ${TSA_ROOT}/src/Coordinate/CoordinateTransformationService.cpp
)

# --- Grilles : définitions et logique (indépendantes du rendu) — [CORE] ------
set(GRID_CORE_SOURCES
    ${TSA_ROOT}/src/Grid/GridType.h
    ${TSA_ROOT}/src/Grid/GridDefinition.h
    ${TSA_ROOT}/src/Grid/GridDefinition.cpp
    ${TSA_ROOT}/src/Grid/GridCoordinateSync.h
    ${TSA_ROOT}/src/Grid/GridCoordinateSync.cpp
    ${TSA_ROOT}/src/Grid/CartesianGrid.h
    ${TSA_ROOT}/src/Grid/CartesianGrid.cpp
    ${TSA_ROOT}/src/Grid/CylindricalGrid.h
    ${TSA_ROOT}/src/Grid/CylindricalGrid.cpp
    ${TSA_ROOT}/src/Grid/ArbitraryGrid.h
    ${TSA_ROOT}/src/Grid/ArbitraryGrid.cpp
    ${TSA_ROOT}/src/Grid/GridSnapManager.h
    ${TSA_ROOT}/src/Grid/GridSnapManager.cpp
    ${TSA_ROOT}/src/Grid/SnapEngine.h
    ${TSA_ROOT}/src/Grid/SnapEngine.cpp
    ${TSA_ROOT}/src/Grid/SnapManager.h
    ${TSA_ROOT}/src/Grid/SnapManager.cpp
    ${TSA_ROOT}/src/Grid/GridSystem.h
    ${TSA_ROOT}/src/Grid/WorkPlaneReferences.h
    ${TSA_ROOT}/src/Grid/WorkPlaneReferences.cpp
    ${TSA_ROOT}/src/Grid/GridSystem.cpp
    ${TSA_ROOT}/src/Grid/GridManager.h
    ${TSA_ROOT}/src/Grid/GridManager.cpp
    ${TSA_ROOT}/src/Grid/GridLabelLayout.h
    ${TSA_ROOT}/src/Grid/GridLabelLayout.cpp
)

# --- Grilles : rendu visuel (nécessite le viewer OCCT, UI uniquement) --------
set(GRID_RENDERING_SOURCES
    ${TSA_ROOT}/src/Grid/GridLabelRenderer.h
    ${TSA_ROOT}/src/Grid/GridLabelRenderer.cpp
    ${TSA_ROOT}/src/Grid/GridRenderer.h
    ${TSA_ROOT}/src/Grid/GridRenderer.cpp
    ${TSA_ROOT}/src/Grid/SnapMarker.h
    ${TSA_ROOT}/src/Grid/SnapMarker.cpp
)

# --- Viewer 3D OCCT (UI uniquement) --------------------------------------------
set(VIEWER_MAIN_SOURCES
    ${TSA_ROOT}/src/Viewer/OccView.h
    ${TSA_ROOT}/src/Viewer/OccView.cpp
    ${TSA_ROOT}/src/Viewer/OccView_Shapes.cpp
    ${TSA_ROOT}/src/Viewer/MeshVisual.h
    ${TSA_ROOT}/src/Viewer/MeshVisual.cpp
    ${TSA_ROOT}/src/Viewer/MeshVSDataSource.h
    ${TSA_ROOT}/src/Viewer/MeshVSDataSource.cpp
    ${TSA_ROOT}/src/Viewer/OccView_DisplayMode.cpp
    ${TSA_ROOT}/src/Viewer/OccView_Navigation.cpp
    ${TSA_ROOT}/src/Viewer/OccView_Events.cpp
    ${TSA_ROOT}/src/Viewer/OccView_Tools.cpp
    ${TSA_ROOT}/src/Viewer/SelectionManager.h
    ${TSA_ROOT}/src/Viewer/SelectionManager.cpp
    ${TSA_ROOT}/src/Viewer/ResultsVisualManager.h
    ${TSA_ROOT}/src/Viewer/ResultsVisualManager.cpp
    ${TSA_ROOT}/src/Viewer/AnnotationStyle.h
    ${TSA_ROOT}/src/Viewer/AnnotationStyle.cpp
    ${TSA_ROOT}/src/Viewer/DisplayProfile.h
    ${TSA_ROOT}/src/Viewer/DisplayProfile.cpp
)

# --- Matériaux visuels, projection et navigation de vue — [CORE] --------------
set(VIEWER_CORE_SOURCES
    ${TSA_ROOT}/src/Viewer/DimensionRenderer.h
    ${TSA_ROOT}/src/Viewer/TextDisplayFallback.h
    ${TSA_ROOT}/src/Viewer/DimensionRenderer.cpp
    ${TSA_ROOT}/src/Viewer/ModelDisplayMode.h
    ${TSA_ROOT}/src/Viewer/ModelDisplayMode.cpp
    ${TSA_ROOT}/src/Viewer/DisplayModeRenderers.h
    ${TSA_ROOT}/src/Viewer/DisplayModeRenderers.cpp
    ${TSA_ROOT}/src/Viewer/BimGeometryRenderer.h
    ${TSA_ROOT}/src/Viewer/BimGeometryRenderer.cpp
    ${TSA_ROOT}/src/Viewer/MaterialVisual.h
    ${TSA_ROOT}/src/Viewer/MaterialVisual.cpp
    ${TSA_ROOT}/src/Viewer/TextureManager.h
    ${TSA_ROOT}/src/Viewer/TextureManager.cpp
    ${TSA_ROOT}/src/Viewer/ProjectionManager.h
    ${TSA_ROOT}/src/Viewer/ProjectionManager.cpp
    ${TSA_ROOT}/src/Viewer/ViewManager.h
    ${TSA_ROOT}/src/Viewer/ViewManager.cpp
    ${TSA_ROOT}/src/Viewer/WorkPlaneGrid.h
    ${TSA_ROOT}/src/Viewer/WorkPlaneGrid.cpp
)

# --- Géométrie des poutres et résultats (utilisée par les tests) — [CORE] ----
set(GEOMETRY_CORE_SOURCES
    ${TSA_ROOT}/src/Geometry/BeamGeometry.h
    ${TSA_ROOT}/src/Geometry/BeamGeometry.cpp
    ${TSA_ROOT}/src/Geometry/DeformedGeometry.h
    ${TSA_ROOT}/src/Geometry/DeformedGeometry.cpp
    ${TSA_ROOT}/src/Geometry/DiagramGeometry.h
    ${TSA_ROOT}/src/Geometry/DiagramGeometry.cpp
    ${TSA_ROOT}/src/Geometry/MemberLoadGlyph.h
    ${TSA_ROOT}/src/Geometry/MemberLoadGlyph.cpp
    ${TSA_ROOT}/src/Geometry/LoadDisplayScale.h
    ${TSA_ROOT}/src/Geometry/LoadDisplayScale.cpp
    ${TSA_ROOT}/src/Geometry/SupportGeometry.h
    ${TSA_ROOT}/src/Geometry/SupportGeometry.cpp
)

# --- Géométrie des autres éléments (UI uniquement) ----------------------------
set(GEOMETRY_MAIN_SOURCES
    ${TSA_ROOT}/src/Geometry/SlabGeometry.h
    ${TSA_ROOT}/src/Geometry/SlabGeometry.cpp
    ${TSA_ROOT}/src/Geometry/WallGeometry.h
    ${TSA_ROOT}/src/Geometry/WallGeometry.cpp
    ${TSA_ROOT}/src/Geometry/FoundationGeometry.h
    ${TSA_ROOT}/src/Geometry/FoundationGeometry.cpp
)

# --- Modèle de données structurel — [CORE] ------------------------------------
set(MODEL_CORE_SOURCES
    ${TSA_ROOT}/src/Model/Element.h
    ${TSA_ROOT}/src/Model/Material.h
    ${TSA_ROOT}/src/Model/Material.cpp
    ${TSA_ROOT}/src/Model/MaterialLibrary.h
    ${TSA_ROOT}/src/Model/MaterialLibrary.cpp
    ${TSA_ROOT}/src/Model/Section.h
    ${TSA_ROOT}/src/Model/Section.cpp
    ${TSA_ROOT}/src/Model/SupportDefinition.h
    ${TSA_ROOT}/src/Model/SupportDefinition.cpp
    ${TSA_ROOT}/src/Model/Node.h
    ${TSA_ROOT}/src/Model/Node.cpp
    ${TSA_ROOT}/src/Model/NamedViews.h
    ${TSA_ROOT}/src/Model/NamedViews.cpp
    ${TSA_ROOT}/src/Model/Beam.h
    ${TSA_ROOT}/src/Model/Beam.cpp
    ${TSA_ROOT}/src/Model/Column.h
    ${TSA_ROOT}/src/Model/Column.cpp
    ${TSA_ROOT}/src/Model/Slab.h
    ${TSA_ROOT}/src/Model/Slab.cpp
    ${TSA_ROOT}/src/Model/Wall.h
    ${TSA_ROOT}/src/Model/Wall.cpp
    ${TSA_ROOT}/src/Model/Foundation.h
    ${TSA_ROOT}/src/Model/Foundation.cpp
    ${TSA_ROOT}/src/Model/TrussMember.h
    ${TSA_ROOT}/src/Model/TrussMember.cpp
    ${TSA_ROOT}/src/Model/Model.h
    ${TSA_ROOT}/src/Model/Model.cpp
    ${TSA_ROOT}/src/Model/Model_Transformations.cpp
    ${TSA_ROOT}/src/Model/Model_Topology.cpp
    ${TSA_ROOT}/src/Model/ModelCleanup.h
    ${TSA_ROOT}/src/Model/ModelCleanup.cpp
    ${TSA_ROOT}/src/Model/MultiEditSession.h
    ${TSA_ROOT}/src/Model/MultiEditSession.cpp
    # Couche BIM (docs/BIM_ARCHITECTURE.md)
    ${TSA_ROOT}/src/Core/Units.h
    ${TSA_ROOT}/src/Core/AppPaths.h
    ${TSA_ROOT}/src/Core/AppPaths.cpp
    ${TSA_ROOT}/src/Core/NumericLocale.cpp
    ${TSA_ROOT}/src/Core/I18n/LanguageManager.h
    ${TSA_ROOT}/src/Core/I18n/LanguageManager.cpp
    ${TSA_ROOT}/src/Modules/ModuleRegistry.h
    ${TSA_ROOT}/src/Modules/ModuleRegistry.cpp
    # Maillage (docs/MESH.md) : moteur Gmsh encapsulé dans Mesh/Gmsh/GmshMeshEngine
    ${TSA_ROOT}/src/Mesh/MeshTypes.h
    ${TSA_ROOT}/src/Mesh/MeshGeometry.h
    ${TSA_ROOT}/src/Mesh/MeshGeometry.cpp
    ${TSA_ROOT}/src/Mesh/MeshSizing.h
    ${TSA_ROOT}/src/Mesh/MeshGenerator.h
    ${TSA_ROOT}/src/Mesh/MeshGenerator.cpp
    ${TSA_ROOT}/src/Mesh/MeshProject.h
    ${TSA_ROOT}/src/Mesh/MeshProject.cpp
    ${TSA_ROOT}/src/Mesh/MeshQuality.h
    ${TSA_ROOT}/src/Mesh/MeshQuality.cpp
    ${TSA_ROOT}/src/Mesh/MeshDisplayModel.h
    ${TSA_ROOT}/src/Mesh/MeshDisplayModel.cpp
    ${TSA_ROOT}/src/Mesh/MeshResultField.h
    ${TSA_ROOT}/src/Mesh/MeshResultField.cpp
    ${TSA_ROOT}/src/Mesh/SurfaceLoads.h
    ${TSA_ROOT}/src/Mesh/SurfaceLoads.cpp
    ${TSA_ROOT}/src/Mesh/Gmsh/GmshMeshEngine.h
    ${TSA_ROOT}/src/Mesh/Gmsh/GmshMeshEngine.cpp
    ${TSA_ROOT}/src/Mesh/Solvers/OpenSeesMeshAdapter.h
    ${TSA_ROOT}/src/Mesh/Solvers/OpenSeesMeshAdapter.cpp
    # Templates de documents et rapports (docs/TEMPLATES.md)
    ${TSA_ROOT}/src/Templates/TemplateEngine.h
    ${TSA_ROOT}/src/Templates/TemplateEngine.cpp
    ${TSA_ROOT}/src/Templates/TemplatePackage.h
    ${TSA_ROOT}/src/Templates/TemplatePackage.cpp
    ${TSA_ROOT}/src/Templates/TemplateRepository.h
    ${TSA_ROOT}/src/Templates/TemplateRepository.cpp
    ${TSA_ROOT}/src/Reports/ReportDataBuilder.h
    ${TSA_ROOT}/src/Reports/ReportDataBuilder.cpp
    ${TSA_ROOT}/src/Reports/DocumentRenderer.h
    ${TSA_ROOT}/src/Reports/DocumentRenderer.cpp
    ${TSA_ROOT}/src/BIM/Core/BimTypes.h
    ${TSA_ROOT}/src/BIM/Core/BimTypes.cpp
    ${TSA_ROOT}/src/BIM/Core/IfcGuid.h
    ${TSA_ROOT}/src/BIM/Core/IfcGuid.cpp
    ${TSA_ROOT}/src/BIM/Core/BimModel.h
    ${TSA_ROOT}/src/BIM/Core/BimModel.cpp
    ${TSA_ROOT}/src/BIM/Core/BimGeometry.h
    ${TSA_ROOT}/src/BIM/Core/BimGeometry.cpp
    ${TSA_ROOT}/src/BIM/Core/QuantityTakeoff.h
    ${TSA_ROOT}/src/BIM/Core/QuantityTakeoff.cpp
    ${TSA_ROOT}/src/BIM/Core/ProductTypes.h
    ${TSA_ROOT}/src/BIM/Core/ProductTypes.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcStepWriter.h
    ${TSA_ROOT}/src/BIM/IFC/IfcStepWriter.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcExportContext.h
    ${TSA_ROOT}/src/BIM/IFC/IfcMapper.h
    ${TSA_ROOT}/src/BIM/IFC/IfcMapper.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcGeometryMapper.h
    ${TSA_ROOT}/src/BIM/IFC/IfcGeometryMapper.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcPropertyMapper.h
    ${TSA_ROOT}/src/BIM/IFC/IfcPropertyMapper.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcRelationshipMapper.h
    ${TSA_ROOT}/src/BIM/IFC/IfcRelationshipMapper.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcExporter.h
    ${TSA_ROOT}/src/BIM/IFC/IfcExporter.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcStepReader.h
    ${TSA_ROOT}/src/BIM/IFC/IfcStepReader.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcImporter.h
    ${TSA_ROOT}/src/BIM/IFC/IfcImporter.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcOpenShellImport.h
    ${TSA_ROOT}/src/BIM/IFC/IfcOpenShellImport.cpp
    ${TSA_ROOT}/src/BIM/IFC/IfcComparison.h
    ${TSA_ROOT}/src/BIM/IFC/IfcComparison.cpp
    ${TSA_ROOT}/src/Model/Model_Snapshots.cpp
    ${TSA_ROOT}/src/Model/ModelDiff.h
    ${TSA_ROOT}/src/Model/ModelDiff.cpp
    ${TSA_ROOT}/src/Model/StructuralClipboard.h
    ${TSA_ROOT}/src/Model/StructuralClipboard.cpp
    ${TSA_ROOT}/src/Model/SelectionQuery.h
    ${TSA_ROOT}/src/Model/SelectionQuery.cpp
)

# --- Système de chargement et solveur OpenSees — [CORE] ------------------------
set(LOAD_CORE_SOURCES
    ${TSA_ROOT}/src/Model/Load/LoadEnums.h
    ${TSA_ROOT}/src/Model/Load/NodalLoad.h
    ${TSA_ROOT}/src/Model/Load/NodalLoad.cpp
    ${TSA_ROOT}/src/Model/Load/MemberLoad.h
    ${TSA_ROOT}/src/Model/Load/MemberLoad.cpp
    ${TSA_ROOT}/src/Model/Load/MemberLoadCommands.h
    ${TSA_ROOT}/src/Model/Load/MemberLoadCommands.cpp
    ${TSA_ROOT}/src/Model/Load/LoadCase.h
    ${TSA_ROOT}/src/Model/Load/LoadCase.cpp
    ${TSA_ROOT}/src/Model/Load/LoadCombination.h
    ${TSA_ROOT}/src/Model/Load/LoadCombination.cpp
    ${TSA_ROOT}/src/Model/Load/LoadManager.h
    ${TSA_ROOT}/src/Model/Load/LoadManager.cpp
    ${TSA_ROOT}/src/Analysis/LoadResolver.h
    ${TSA_ROOT}/src/Analysis/LoadResolver.cpp
    ${TSA_ROOT}/src/Analysis/LoadValidation.h
    ${TSA_ROOT}/src/Analysis/LoadValidation.cpp
    ${TSA_ROOT}/src/Analysis/OpenSeesAdapter.h
    ${TSA_ROOT}/src/Analysis/OpenSeesAdapter.cpp
    ${TSA_ROOT}/src/Analysis/OpenSeesManager.h
    ${TSA_ROOT}/src/Analysis/OpenSeesManager.cpp
    ${TSA_ROOT}/src/Analysis/CalculationSnapshot.h
    ${TSA_ROOT}/src/Analysis/CalculationSnapshot.cpp
    ${TSA_ROOT}/src/Analysis/ResultsModel.h
    ${TSA_ROOT}/src/Analysis/ResultsModel.cpp
    ${TSA_ROOT}/src/Analysis/SolverMesh.h
    ${TSA_ROOT}/src/Analysis/SolverMesh.cpp
    ${TSA_ROOT}/src/Analysis/AnalysisTypes.h
    ${TSA_ROOT}/src/Analysis/ElementTransformation.h
    ${TSA_ROOT}/src/Analysis/ElementTransformation.cpp
    ${TSA_ROOT}/src/Analysis/OpenSeesModelMap.h
    ${TSA_ROOT}/src/Analysis/OpenSeesModelMap.cpp
    ${TSA_ROOT}/src/Analysis/ResultsContext.h
    ${TSA_ROOT}/src/Analysis/ResultsContext.cpp
    ${TSA_ROOT}/src/Analysis/ResultsExport.h
    ${TSA_ROOT}/src/Analysis/ResultsExport.cpp
    ${TSA_ROOT}/src/Analysis/ResultsValidityGuard.h
    ${TSA_ROOT}/src/Analysis/ResultsValidityGuard.cpp
    ${TSA_ROOT}/src/Analysis/AnalysisController.h
    ${TSA_ROOT}/src/Analysis/AnalysisController.cpp
    ${TSA_ROOT}/src/Analysis/OpenSeesAnalysisBuilder.h
    ${TSA_ROOT}/src/Analysis/OpenSeesAnalysisBuilder.cpp
    ${TSA_ROOT}/src/Analysis/OpenSeesResultsReader.h
    ${TSA_ROOT}/src/Analysis/OpenSeesResultsReader.cpp
    ${TSA_ROOT}/src/Analysis/OpenSeesSolver.h
    ${TSA_ROOT}/src/Analysis/OpenSeesSolver.cpp
)

# --- Analyse multi-moteurs (contexte, portée, modèle d'analyse, registre, adaptateurs) — [CORE] ---
set(ANALYSIS_ENGINE_SOURCES
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisValidation.h
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisContext.h
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisContext.cpp
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisModel.h
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisModel.cpp
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisEngine.h
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisEngineRegistry.h
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisEngineRegistry.cpp
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisManager.h
    ${TSA_ROOT}/src/Analysis/Engine/AnalysisManager.cpp
    ${TSA_ROOT}/src/Api/SurfaceApi.h
    ${TSA_ROOT}/src/Api/SurfaceApi.cpp
    ${TSA_ROOT}/src/Analysis/FiberStress.h
    ${TSA_ROOT}/src/Validation/BenchmarkSuite.h
    ${TSA_ROOT}/src/Validation/BenchmarkSuite.cpp
    ${TSA_ROOT}/src/Analysis/Engines/BuiltInEngines.cpp
    ${TSA_ROOT}/src/Analysis/Engines/OpenSees/OpenSeesEngine.h
    ${TSA_ROOT}/src/Analysis/Engines/OpenSees/OpenSeesEngine.cpp
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DSolver.h
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DAdapter.h
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DAdapter.cpp
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DEngine.h
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DEngine.cpp
)
if(TSA_HAS_FEM3D)
    list(APPEND ANALYSIS_ENGINE_SOURCES
        ${TSA_ROOT}/src/Analysis/Engines/Fem3D/Fem3DAdapter.h
        ${TSA_ROOT}/src/Analysis/Engines/Fem3D/Fem3DAdapter.cpp
        ${TSA_ROOT}/src/Analysis/Engines/Fem3D/Fem3DEngine.h
        ${TSA_ROOT}/src/Analysis/Engines/Fem3D/Fem3DEngine.cpp
    )
endif()

# --- IA Co-Engineering (matériel, modèles, fournisseurs, contexte, outils, RAG) — [CORE] ---
set(AI_CORE_SOURCES
    ${TSA_ROOT}/src/AI/Hardware/HardwareProfiler.h
    ${TSA_ROOT}/src/AI/Hardware/HardwareProfiler.cpp
    ${TSA_ROOT}/src/AI/Models/ModelRegistry.h
    ${TSA_ROOT}/src/AI/Models/ModelRegistry.cpp
    ${TSA_ROOT}/src/AI/Models/ModelManager.h
    ${TSA_ROOT}/src/AI/Models/ModelManager.cpp
    ${TSA_ROOT}/src/AI/Providers/AIProvider.h
    ${TSA_ROOT}/src/AI/Providers/OpenAICompatibleProvider.h
    ${TSA_ROOT}/src/AI/Providers/OpenAICompatibleProvider.cpp
    ${TSA_ROOT}/src/AI/Providers/MockAIProvider.h
    ${TSA_ROOT}/src/AI/Providers/MockAIProvider.cpp
    ${TSA_ROOT}/src/AI/Providers/AIProviderRouter.h
    ${TSA_ROOT}/src/AI/Providers/AIProviderRouter.cpp
    ${TSA_ROOT}/src/AI/Providers/LocalLlamaServer.h
    ${TSA_ROOT}/src/AI/Providers/LocalLlamaServer.cpp
    ${TSA_ROOT}/src/AI/Providers/ProcessLifetime.h
    ${TSA_ROOT}/src/AI/Providers/ProcessLifetime.cpp
    ${TSA_ROOT}/src/AI/Checker/StructuralChecker.h
    ${TSA_ROOT}/src/AI/Checker/StructuralChecker.cpp
    ${TSA_ROOT}/src/AI/Context/EngineeringContext.h
    ${TSA_ROOT}/src/AI/Context/EngineeringContext.cpp
    ${TSA_ROOT}/src/AI/Tools/AIToolRegistry.h
    ${TSA_ROOT}/src/AI/Tools/AIToolRegistry.cpp
    ${TSA_ROOT}/src/AI/Operator/AIOperator.h
    ${TSA_ROOT}/src/AI/Operator/AIOperator.cpp
    ${TSA_ROOT}/src/AI/Python/PythonAIBridge.h
    ${TSA_ROOT}/src/AI/Python/PythonAIBridge.cpp
    ${TSA_ROOT}/src/AI/RAG/EngineeringKnowledgeBase.h
    ${TSA_ROOT}/src/AI/RAG/EngineeringKnowledgeBase.cpp
    ${TSA_ROOT}/src/AI/Core/AISettings.h
    ${TSA_ROOT}/src/AI/Core/AISettings.cpp
    ${TSA_ROOT}/src/AI/Core/SecretStore.cpp
    ${TSA_ROOT}/src/AI/Core/AILog.h
    ${TSA_ROOT}/src/AI/Core/AILog.cpp
    ${TSA_ROOT}/src/AI/Core/AIOrchestrator.h
    ${TSA_ROOT}/src/AI/Core/AIOrchestrator.cpp
)

# --- Note de Calcul (NDC) — [CORE] ---------------------------------------------
set(NDC_CORE_SOURCES
    ${TSA_ROOT}/src/NDC/NDCDocumentModel.h
    ${TSA_ROOT}/src/NDC/NDCDocumentModel.cpp
    ${TSA_ROOT}/src/NDC/ReportConfiguration.h
    ${TSA_ROOT}/src/NDC/ReportConfiguration.cpp
    ${TSA_ROOT}/src/NDC/ReportTemplate.h
    ${TSA_ROOT}/src/NDC/ReportTemplate.cpp
    ${TSA_ROOT}/src/NDC/NormativeReferenceDetector.h
    ${TSA_ROOT}/src/NDC/NormativeReferenceDetector.cpp
    ${TSA_ROOT}/src/NDC/ResultAnalyzer.h
    ${TSA_ROOT}/src/NDC/ResultAnalyzer.cpp
    ${TSA_ROOT}/src/NDC/NDCGenerator.h
    ${TSA_ROOT}/src/NDC/NDCGenerator.cpp
    ${TSA_ROOT}/src/NDC/NDCExporter.h
    ${TSA_ROOT}/src/NDC/NDCExporter.cpp
    ${TSA_ROOT}/src/NDC/NDCTemplateData.h
    ${TSA_ROOT}/src/NDC/NDCTemplateData.cpp
    ${TSA_ROOT}/src/NDC/NDCProjectNote.h
    ${TSA_ROOT}/src/NDC/NDCProjectNote.cpp
    ${TSA_ROOT}/src/NDC/ReportManager.h
    ${TSA_ROOT}/src/NDC/ReportManager.cpp
    ${TSA_ROOT}/src/NDC/NDCPlanarCurves.h
    ${TSA_ROOT}/src/NDC/NDCPlanarCurves.cpp
)

# --- Câbles : modèle et normes de calcul — [CORE] -----------------------------
set(CABLE_SOURCES
    ${TSA_ROOT}/src/Model/Cable/CableTypes.h
    ${TSA_ROOT}/src/Model/Cable/CableStandards.h
    ${TSA_ROOT}/src/Model/Cable/CableStandards.cpp
    ${TSA_ROOT}/src/Model/Cable/CableAnchor.h
    ${TSA_ROOT}/src/Model/Cable/CableAnchor.cpp
    ${TSA_ROOT}/src/Model/Cable/CablePrestress.h
    ${TSA_ROOT}/src/Model/Cable/CableAnalysisProperties.h
    ${TSA_ROOT}/src/Model/Cable/CableDefinition.h
    ${TSA_ROOT}/src/Model/Cable/CableDefinition.cpp
    ${TSA_ROOT}/src/Model/Cable/CableGeometry.h
    ${TSA_ROOT}/src/Model/Cable/CableGeometry.cpp
    ${TSA_ROOT}/src/Model/Cable/Cable.h
    ${TSA_ROOT}/src/Model/Cable/Cable.cpp
    ${TSA_ROOT}/src/Model/Cable/StayCable.h
    ${TSA_ROOT}/src/Model/Cable/StayCable.cpp
    ${TSA_ROOT}/src/Model/Cable/SuspensionSystem.h
    ${TSA_ROOT}/src/Model/Cable/SuspensionSystem.cpp
)

# --- Câbles : géométrie 3D, grille et bibliothèque — [CORE] ------------------
set(CABLE_GEOMETRY_SOURCES
    ${TSA_ROOT}/src/Geometry/CableGeometry3D.h
    ${TSA_ROOT}/src/Geometry/CableGeometry3D.cpp
    ${TSA_ROOT}/src/Grid/CableGrid.h
    ${TSA_ROOT}/src/Grid/CableGrid.cpp
    ${TSA_ROOT}/src/Library/CableLibrary.h
    ${TSA_ROOT}/src/Library/CableLibrary.cpp
)

# --- Gestion de projet — [CORE] -----------------------------------------------
set(PROJECT_SOURCES
    ${TSA_ROOT}/src/Project/ProjectSession.h
    ${TSA_ROOT}/src/Project/ProjectSession.cpp
    ${TSA_ROOT}/src/Project/ProjectManager.h
    ${TSA_ROOT}/src/Project/ProjectManager.cpp
    ${TSA_ROOT}/src/Project/ProjectTemplates.h
    ${TSA_ROOT}/src/Project/ProjectTemplates.cpp
    ${TSA_ROOT}/src/Project/RecentProjects.h
    ${TSA_ROOT}/src/Project/RecentProjects.cpp
    ${TSA_ROOT}/src/Project/ModelPreviewCache.h
    ${TSA_ROOT}/src/Project/ModelPreviewCache.cpp
)

# --- Commandes (pattern Command pour undo/redo) — [CORE] ---------------------
set(COMMANDS_SOURCES
    ${TSA_ROOT}/src/Commands/ICommand.h
    ${TSA_ROOT}/src/Commands/CommandCategory.h
    ${TSA_ROOT}/src/Commands/CommandCatalog.h
    ${TSA_ROOT}/src/Commands/CommandCatalog.cpp
    ${TSA_ROOT}/src/Commands/CommandSearchIndex.h
    ${TSA_ROOT}/src/Commands/CommandSearchIndex.cpp
    ${TSA_ROOT}/src/Commands/CommandSynonyms.h
    ${TSA_ROOT}/src/Commands/CommandSynonyms.cpp
    ${TSA_ROOT}/src/Commands/CreateElementCommands.h
    ${TSA_ROOT}/src/Commands/CreateElementCommands.cpp
    ${TSA_ROOT}/src/Commands/ModifyCommands.h
    ${TSA_ROOT}/src/Commands/ModifyCommands.cpp
    ${TSA_ROOT}/src/Commands/CreateBeamCommand.h
    ${TSA_ROOT}/src/Commands/CreateBeamCommand.cpp
    ${TSA_ROOT}/src/Commands/GridCommands.h
    ${TSA_ROOT}/src/Commands/GridCommands.cpp
)

# --- Undo / Redo — [CORE] -------------------------------------------------------
set(UNDOREDO_SOURCES
    ${TSA_ROOT}/src/UndoRedo/UndoManager.h
    ${TSA_ROOT}/src/UndoRedo/UndoManager.cpp
    ${TSA_ROOT}/src/UndoRedo/EditRecord.h
    ${TSA_ROOT}/src/UndoRedo/EditTransaction.h
    ${TSA_ROOT}/src/UndoRedo/CommandManager.h
    ${TSA_ROOT}/src/UndoRedo/CommandManager.cpp
)

# --- Gestion des interactions utilisateur — [CORE] ---------------------------
set(INTERACTION_SOURCES
    ${TSA_ROOT}/src/Interaction/InteractionManager.h
    ${TSA_ROOT}/src/Interaction/InteractionManager.cpp
    ${TSA_ROOT}/src/Interaction/ElementCreation.h
    ${TSA_ROOT}/src/Interaction/ElementCreation.cpp
    ${TSA_ROOT}/src/Interaction/PropertyAssignment.h
    ${TSA_ROOT}/src/Interaction/PropertyAssignment.cpp
    ${TSA_ROOT}/src/Interaction/Tools/ModelingTool.h
    ${TSA_ROOT}/src/Interaction/Tools/ModelingTool.cpp
    ${TSA_ROOT}/src/Interaction/Tools/ModifyTools.h
    ${TSA_ROOT}/src/Interaction/Tools/ModifyTools.cpp
    ${TSA_ROOT}/src/Interaction/Tools/DrawTools.cpp
    ${TSA_ROOT}/src/Interaction/Tools/DimensionTools.h
    ${TSA_ROOT}/src/Interaction/Tools/DimensionTools.cpp
    ${TSA_ROOT}/src/Model/ModelElementCopy.h
)

# --- Format de fichier TSA (sauvegarde / chargement) — [CORE] ----------------
set(IO_SOURCES
    ${TSA_ROOT}/src/IO/TSAFileFormat.h
    ${TSA_ROOT}/src/IO/TSAFile_BinaryUtils.h
    ${TSA_ROOT}/src/IO/TSAFile.h
    ${TSA_ROOT}/src/IO/TSAFile.cpp
    ${TSA_ROOT}/src/IO/Tsa3d/Tsa3d.h
    ${TSA_ROOT}/src/IO/Tsa3d/Tsa3dInternal.h
    ${TSA_ROOT}/src/IO/Tsa3d/Tsa3dCommon.cpp
    ${TSA_ROOT}/src/IO/Tsa3d/Tsa3dValidate.cpp
    ${TSA_ROOT}/src/IO/Tsa3d/Tsa3dExport.cpp
    ${TSA_ROOT}/src/IO/Tsa3d/Tsa3dImport.cpp
    ${TSA_ROOT}/src/IO/TSAFileWriter_Chunks.cpp
    ${TSA_ROOT}/src/IO/TSAFileReader_Chunks.cpp
    ${TSA_ROOT}/src/IO/TSAPreviewGenerator.h
    ${TSA_ROOT}/src/IO/TSAPreviewGenerator.cpp
)

# --- Système de traçabilité des exigences et normes internationales — [CORE] ---
set(STANDARDS_SOURCES
    ${TSA_ROOT}/src/Standards/NormativeTypes.h
    ${TSA_ROOT}/src/Standards/NormativeTypes.cpp
    ${TSA_ROOT}/src/Standards/RequirementsCatalog.h
    ${TSA_ROOT}/src/Standards/RequirementsCatalog.cpp
    ${TSA_ROOT}/src/Standards/NationalAnnexConfig.h
    ${TSA_ROOT}/src/Standards/NationalAnnexConfig.cpp
    ${TSA_ROOT}/src/Standards/ExternalLibraryCatalog.h
    ${TSA_ROOT}/src/Standards/ExternalLibraryCatalog.cpp
    ${TSA_ROOT}/src/Standards/DataDefinition.h
    ${TSA_ROOT}/src/Standards/ModelValidator.h
    ${TSA_ROOT}/src/Standards/ModelValidator.cpp
    ${TSA_ROOT}/src/Standards/AnalyticalBenchmark.h
    ${TSA_ROOT}/src/Standards/AnalyticalBenchmark.cpp
    ${TSA_ROOT}/src/Standards/Design/ConcreteDesignEC2.h
    ${TSA_ROOT}/src/Standards/Design/ConcreteDesignEC2.cpp
    ${TSA_ROOT}/src/Standards/Design/SteelDesignEC3.h
    ${TSA_ROOT}/src/Standards/Design/SteelDesignEC3.cpp
)

# --- Intégration spécifique Windows (association de fichiers) ----------------
set(PLATFORM_SOURCES
    ${TSA_ROOT}/src/Platform/WindowsAssociation.h
    ${TSA_ROOT}/src/Platform/WindowsAssociation.cpp
)

# --- Ressources (icônes, .rc Windows) -----------------------------------------
set(RESOURCES_SOURCES
    ${TSA_ROOT}/resources/resources.qrc
    ${TSA_ROOT}/resources/templates.qrc
)

# =============================================================================
# Regroupement par couche (docs/TSARALOHA_ARCHITECTURE.md, ADR-024 ; contrôlé par tools/check_layers.py)
#   TSARALOHA_MODEL_SOURCES    -> <P>_Model    : modèle de projet, sans widget (partagé TSA / TSALab)
#   TSARALOHA_GRAPHICS_SOURCES -> <P>_Graphics : viewport, géométrie, outils, grilles (rendu), thème
#   TSARALOHA_WIDGETS_SOURCES  -> <P>_Widgets  : composants d'interface partagés (arbre, propriétés, docks,
#                                                dialogues, fenêtre Analysis, dispositions, IA, NDC)
#   TSA_APP_SOURCES            -> TSA          : fenêtre de TSA (MainWindow, ruban, AppShell, Start Center)
# =============================================================================
set(TSARALOHA_MODEL_SOURCES
    ${TSA_ROOT}/src/Annotation/Dimension.h
    ${TSA_ROOT}/src/Annotation/Dimension.cpp
    ${TSA_ROOT}/src/Annotation/DimensionGeometry.h
    ${TSA_ROOT}/src/Annotation/DimensionGeometry.cpp
    ${TSA_ROOT}/src/Annotation/DimensionService.h
    ${TSA_ROOT}/src/Annotation/DimensionService.cpp
    ${TSA_ROOT}/src/Topology/TopologySettings.h
    ${TSA_ROOT}/src/Topology/TopologySettings.cpp
    ${TSA_ROOT}/src/Topology/NumberingStrategies.h
    ${TSA_ROOT}/src/Topology/NumberingStrategies.cpp
    ${TSA_ROOT}/src/Topology/TopologyNumberingService.h
    ${TSA_ROOT}/src/Topology/TopologyNumberingService.cpp
    ${TSA_ROOT}/src/Help/HelpTopics.h
    ${TSA_ROOT}/src/Help/HelpTopics.cpp
    ${TSA_ROOT}/src/Automation/CommandRegistry.h
    ${TSA_ROOT}/src/Automation/CommandRegistry.cpp
    ${TSA_ROOT}/src/Automation/AutomationServer.h
    ${TSA_ROOT}/src/Automation/AutomationServer.cpp
    ${TSA_ROOT}/src/Blueprint/BlueprintTypes.h
    ${TSA_ROOT}/src/Blueprint/BlueprintGraph.h
    ${TSA_ROOT}/src/Blueprint/BlueprintGraph.cpp
    ${TSA_ROOT}/src/Blueprint/BlueprintRuntime.h
    ${TSA_ROOT}/src/Blueprint/BlueprintRuntime.cpp
    ${TSA_ROOT}/src/Blueprint/BlueprintNodes.cpp
    ${TSA_ROOT}/src/Blueprint/BlueprintSubgraph.h
    ${TSA_ROOT}/src/Blueprint/BlueprintSubgraph.cpp
    ${TSA_ROOT}/src/Blueprint/BlueprintFile.h
    ${TSA_ROOT}/src/Blueprint/BlueprintFile.cpp
    ${TSA_ROOT}/src/Blueprint/BlueprintScript.h
    ${TSA_ROOT}/src/Blueprint/BlueprintScript.cpp
    ${TSA_ROOT}/src/Plugins/PluginApi.h
    ${TSA_ROOT}/src/Plugins/PluginManager.h
    ${TSA_ROOT}/src/Plugins/PluginManager.cpp
    ${DIAGNOSTICS_SOURCES}
    ${EXTENSIONSYSTEM_SOURCES}
    ${LIBRARY_SOURCES}
    ${COORDINATE_SOURCES}
    ${GRID_CORE_SOURCES}
    ${MODEL_CORE_SOURCES}
    ${LOAD_CORE_SOURCES}
    ${ANALYSIS_ENGINE_SOURCES}
    ${NDC_CORE_SOURCES}
    ${AI_CORE_SOURCES}
    ${CABLE_SOURCES}
    ${TSA_ROOT}/src/Grid/CableGrid.h
    ${TSA_ROOT}/src/Grid/CableGrid.cpp
    ${TSA_ROOT}/src/Library/CableLibrary.h
    ${TSA_ROOT}/src/Library/CableLibrary.cpp
    ${TSA_ROOT}/src/Model/ModelElementCopy.h
    ${TSA_ROOT}/src/App/ProductInfo.h
    ${PROJECT_SOURCES}
    ${COMMANDS_SOURCES}
    ${UNDOREDO_SOURCES}
    ${IO_SOURCES}
    ${STANDARDS_SOURCES}
    ${PLATFORM_SOURCES}
)

# --- TSA Core (<P>_Core, étape 1 du programme TSA Core) ------------------------------------------------------------
# Noyau du modèle structural : modèle, éléments, charges, câbles, repères et niveaux, cotations, BIM (données), grilles
# (données), historique par instantanés, journal. Sans widget, sans rendu, sans moteur de calcul : périmètre établi par
# fermeture à l'édition de liens (QtCore, TKernel, TKMath seulement). Les catalogues installés lui sont fournis par
# la couche des bibliothèques (Model/InstalledCatalogs, enregistré par ExtensionSystem/LibraryRegistry.cpp).
# Retirés de TSARALOHA_MODEL_SOURCES : compilés une seule fois, dans <P>_Core.
set(TSARALOHA_CORE_SOURCES
    ${TSA_ROOT}/src/Annotation/Dimension.h
    ${TSA_ROOT}/src/Annotation/Dimension.cpp
    ${TSA_ROOT}/src/BIM/Core/BimGeometry.h
    ${TSA_ROOT}/src/BIM/Core/BimGeometry.cpp
    ${TSA_ROOT}/src/BIM/Core/BimModel.h
    ${TSA_ROOT}/src/BIM/Core/BimModel.cpp
    ${TSA_ROOT}/src/BIM/Core/BimTypes.h
    ${TSA_ROOT}/src/BIM/Core/BimTypes.cpp
    ${TSA_ROOT}/src/BIM/Core/IfcGuid.h
    ${TSA_ROOT}/src/BIM/Core/IfcGuid.cpp
    ${TSA_ROOT}/src/BIM/Core/ProductTypes.h
    ${TSA_ROOT}/src/BIM/Core/ProductTypes.cpp
    ${TSA_ROOT}/src/BIM/Core/QuantityTakeoff.h
    ${TSA_ROOT}/src/BIM/Core/QuantityTakeoff.cpp
    ${TSA_ROOT}/src/Coordinate/CoordinateSystem.h
    ${TSA_ROOT}/src/Coordinate/CoordinateSystem.cpp
    ${TSA_ROOT}/src/Coordinate/GeometryTolerance.h
    ${TSA_ROOT}/src/Coordinate/Level.h
    ${TSA_ROOT}/src/Coordinate/LevelManager.h
    ${TSA_ROOT}/src/Coordinate/LevelManager.cpp
    ${TSA_ROOT}/src/Coordinate/Point3D.h
    ${TSA_ROOT}/src/Coordinate/WorkPlane.h
    ${TSA_ROOT}/src/Coordinate/WorkPlane.cpp
    ${TSA_ROOT}/src/Coordinate/WorkPlaneCoordinateSystem.h
    ${TSA_ROOT}/src/Coordinate/WorkPlaneCoordinateSystem.cpp
    ${TSA_ROOT}/src/Coordinate/WorkPlaneManager.h
    ${TSA_ROOT}/src/Coordinate/WorkPlaneManager.cpp
    ${TSA_ROOT}/src/Core/AppPaths.h
    ${TSA_ROOT}/src/Core/AppPaths.cpp
    ${TSA_ROOT}/src/Diagnostics/LogEntry.h
    ${TSA_ROOT}/src/Diagnostics/LogLevel.h
    ${TSA_ROOT}/src/Diagnostics/Logger.h
    ${TSA_ROOT}/src/Diagnostics/Logger.cpp
    ${TSA_ROOT}/src/Diagnostics/RingBuffer.h
    ${TSA_ROOT}/src/Grid/ArbitraryGrid.h
    ${TSA_ROOT}/src/Grid/ArbitraryGrid.cpp
    ${TSA_ROOT}/src/Grid/CartesianGrid.h
    ${TSA_ROOT}/src/Grid/CartesianGrid.cpp
    ${TSA_ROOT}/src/Grid/CylindricalGrid.h
    ${TSA_ROOT}/src/Grid/CylindricalGrid.cpp
    ${TSA_ROOT}/src/Grid/GridDefinition.h
    ${TSA_ROOT}/src/Grid/GridDefinition.cpp
    ${TSA_ROOT}/src/Grid/GridManager.h
    ${TSA_ROOT}/src/Grid/GridManager.cpp
    ${TSA_ROOT}/src/Grid/GridSystem.h
    ${TSA_ROOT}/src/Grid/GridSystem.cpp
    ${TSA_ROOT}/src/Grid/GridType.h
    ${TSA_ROOT}/src/Model/Beam.h
    ${TSA_ROOT}/src/Model/Beam.cpp
    ${TSA_ROOT}/src/Model/Column.h
    ${TSA_ROOT}/src/Model/Column.cpp
    ${TSA_ROOT}/src/Model/Element.h
    ${TSA_ROOT}/src/Model/Foundation.h
    ${TSA_ROOT}/src/Model/Foundation.cpp
    ${TSA_ROOT}/src/Model/DefinitionReference.h
    ${TSA_ROOT}/src/Model/DefinitionReference.cpp
    ${TSA_ROOT}/src/Model/InstalledCatalogs.h
    ${TSA_ROOT}/src/Model/InstalledCatalogs.cpp
    ${TSA_ROOT}/src/Model/Material.h
    ${TSA_ROOT}/src/Model/Material.cpp
    ${TSA_ROOT}/src/Model/Model.h
    ${TSA_ROOT}/src/Model/Model.cpp
    ${TSA_ROOT}/src/Model/ModelDiff.h
    ${TSA_ROOT}/src/Model/ModelDiff.cpp
    ${TSA_ROOT}/src/Model/ModelElementCopy.h
    ${TSA_ROOT}/src/Model/NamedViews.h
    ${TSA_ROOT}/src/Model/NamedViews.cpp
    ${TSA_ROOT}/src/Model/Model_Snapshots.cpp
    ${TSA_ROOT}/src/Model/Model_Topology.cpp
    ${TSA_ROOT}/src/Model/Model_Transformations.cpp
    ${TSA_ROOT}/src/Model/Node.h
    ${TSA_ROOT}/src/Model/Node.cpp
    ${TSA_ROOT}/src/Model/Section.h
    ${TSA_ROOT}/src/Model/Section.cpp
    ${TSA_ROOT}/src/Model/SelectionQuery.h
    ${TSA_ROOT}/src/Model/SelectionQuery.cpp
    ${TSA_ROOT}/src/Model/Slab.h
    ${TSA_ROOT}/src/Model/Slab.cpp
    ${TSA_ROOT}/src/Model/SupportDefinition.h
    ${TSA_ROOT}/src/Model/SupportDefinition.cpp
    ${TSA_ROOT}/src/Model/SurfaceFormulation.h
    ${TSA_ROOT}/src/Model/SurfaceSupport.h
    ${TSA_ROOT}/src/Model/TrussMember.h
    ${TSA_ROOT}/src/Model/TrussMember.cpp
    ${TSA_ROOT}/src/Model/Wall.h
    ${TSA_ROOT}/src/Model/Wall.cpp
    ${TSA_ROOT}/src/Model/Cable/Cable.h
    ${TSA_ROOT}/src/Model/Cable/Cable.cpp
    ${TSA_ROOT}/src/Model/Cable/CableAnalysisProperties.h
    ${TSA_ROOT}/src/Model/Cable/CableAnchor.h
    ${TSA_ROOT}/src/Model/Cable/CableAnchor.cpp
    ${TSA_ROOT}/src/Model/Cable/CableDefinition.h
    ${TSA_ROOT}/src/Model/Cable/CableDefinition.cpp
    ${TSA_ROOT}/src/Model/Cable/CableGeometry.h
    ${TSA_ROOT}/src/Model/Cable/CableGeometry.cpp
    ${TSA_ROOT}/src/Model/Cable/CablePrestress.h
    ${TSA_ROOT}/src/Model/Cable/CableStandards.h
    ${TSA_ROOT}/src/Model/Cable/CableStandards.cpp
    ${TSA_ROOT}/src/Model/Cable/CableTypes.h
    ${TSA_ROOT}/src/Model/Cable/StayCable.h
    ${TSA_ROOT}/src/Model/Cable/StayCable.cpp
    ${TSA_ROOT}/src/Model/Cable/SuspensionSystem.h
    ${TSA_ROOT}/src/Model/Cable/SuspensionSystem.cpp
    ${TSA_ROOT}/src/Model/Load/LoadCase.h
    ${TSA_ROOT}/src/Model/Load/LoadCase.cpp
    ${TSA_ROOT}/src/Model/Load/LoadCombination.h
    ${TSA_ROOT}/src/Model/Load/LoadCombination.cpp
    ${TSA_ROOT}/src/Model/Load/LoadEnums.h
    ${TSA_ROOT}/src/Model/Load/LoadManager.h
    ${TSA_ROOT}/src/Model/Load/LoadManager.cpp
    ${TSA_ROOT}/src/Model/Load/MemberLoad.h
    ${TSA_ROOT}/src/Model/Load/MemberLoad.cpp
    ${TSA_ROOT}/src/Model/Load/NodalLoad.h
    ${TSA_ROOT}/src/Model/Load/NodalLoad.cpp
    ${TSA_ROOT}/src/Model/Load/SurfaceLoad.h
    ${TSA_ROOT}/src/UndoRedo/EditRecord.h
    ${TSA_ROOT}/src/UndoRedo/EditTransaction.h
    ${TSA_ROOT}/src/UndoRedo/UndoManager.h
    ${TSA_ROOT}/src/UndoRedo/UndoManager.cpp
)
list(REMOVE_ITEM TSARALOHA_MODEL_SOURCES ${TSARALOHA_CORE_SOURCES})

set(TSARALOHA_GRAPHICS_SOURCES
    ${VIEWER_CORE_SOURCES}
    ${VIEWER_MAIN_SOURCES}
    ${GEOMETRY_CORE_SOURCES}
    ${GEOMETRY_MAIN_SOURCES}
    ${TSA_ROOT}/src/Geometry/CableGeometry3D.h
    ${TSA_ROOT}/src/Geometry/CableGeometry3D.cpp
    ${GRID_RENDERING_SOURCES}
    ${INTERACTION_SOURCES}
    ${UI_THEME_SOURCES}
    ${UI_RULER_SOURCES}
)
list(REMOVE_ITEM TSARALOHA_GRAPHICS_SOURCES ${TSA_ROOT}/src/Model/ModelElementCopy.h)


# --- IA Co-Engineering : interface (panneau, configuration), Start Center, fenêtre ---
set(UI_AI_SOURCES
    ${TSA_ROOT}/src/UI/AI/AICoEngineeringDock.h
    ${TSA_ROOT}/src/UI/AI/AICoEngineeringDock.cpp
    ${TSA_ROOT}/src/UI/AI/AIRuntimeDialog.h
    ${TSA_ROOT}/src/UI/AI/AIRuntimeDialog.cpp
    ${TSA_ROOT}/src/UI/MainWindow_AI.cpp
    ${TSA_ROOT}/src/UI/MainWindow_Preview.cpp
    ${TSA_ROOT}/src/UI/Home/StartCenter.h
    ${TSA_ROOT}/src/UI/Home/StartCenter.cpp
    ${TSA_ROOT}/src/UI/Home/NewProjectDialog.h
    ${TSA_ROOT}/src/UI/Home/NewProjectDialog.cpp
    ${TSA_ROOT}/src/UI/Home/SaveProjectTemplateDialog.h
    ${TSA_ROOT}/src/UI/Home/SaveProjectTemplateDialog.cpp
    ${TSA_ROOT}/src/UI/Shell/AppShell.h
    ${TSA_ROOT}/src/UI/Shell/AppShell.cpp
    ${TSA_ROOT}/src/UI/Shell/AppShell_Search.cpp
    ${TSA_ROOT}/src/UI/Search/GlobalSearchWidget.h
    ${TSA_ROOT}/src/UI/Search/GlobalSearchWidget.cpp
    ${TSA_ROOT}/src/UI/Shell/TitleBar.h
    ${TSA_ROOT}/src/UI/Shell/TitleBar.cpp
)

set(_tsa_all_ui_sources
    ${UI_EXTENSIONMANAGERDIALOG_SOURCES}
    ${UI_ANALYSIS_SOURCES}
    ${UI_TOOLS_SOURCES}
    ${UI_MODELTREE_SOURCES}
    ${UI_WINDOWMANAGER_SOURCES}
    ${UI_AI_SOURCES}
    ${APP_SOURCES}
    ${UI_MAINWINDOW_SOURCES}
    ${UI_PROPERTIES_SOURCES}
    ${UI_DIALOGS_SOURCES}
    ${UI_WIDGETS_SOURCES}
    ${UI_RIBBON_SOURCES}
    ${UI_DOCK_SOURCES}
    ${MODEL_PRESETS_SOURCES}
    ${RESOURCES_SOURCES}
)
# Fenêtre propre à TSA (même découpage que tools/check_layers.py, couche « app »)
set(_tsa_app_regex "/src/(UI/(MainWindow|Ribbon/|Shell/|Home/)|App/Application|main\\.cpp)|/resources/")
set(TSA_APP_SOURCES ${_tsa_all_ui_sources})
list(FILTER TSA_APP_SOURCES INCLUDE REGEX "${_tsa_app_regex}")
set(TSARALOHA_WIDGETS_SOURCES ${_tsa_all_ui_sources})
list(FILTER TSARALOHA_WIDGETS_SOURCES EXCLUDE REGEX "${_tsa_app_regex}")

# =============================================================================
# Options de build
# =============================================================================
option(TSA_BUILD_TESTS "Compiler la suite de tests du produit (<Produit>_Tests)" ON)
option(TSA_USE_CCACHE "Utiliser ccache comme lanceur de compilation (Ninja)" OFF)

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
    TKMeshVS
)

# Fichiers qui utilisent directement l'API Win32 (MessageBox, shell, dbghelp...) :
# ils incluent eux-mêmes <windows.h> avec leurs propres réglages, on les exclut
# donc du PCH (dont <windows.h> est déjà inclus via Qt/OCCT avec d'autres macros).
set(TSA_NO_PCH_SOURCES
    ${TSA_ROOT}/src/Diagnostics/CrashHandler.cpp
    ${TSA_ROOT}/src/Platform/WindowsAssociation.cpp
    ${TSA_ROOT}/src/AI/Hardware/HardwareProfiler.cpp
    ${TSA_ROOT}/src/AI/Core/SecretStore.cpp
    ${TSA_ROOT}/src/AI/Providers/ProcessLifetime.cpp
    ${TSA_ROOT}/src/UI/Shell/AppShell.cpp
    ${TSA_ROOT}/src/UI/Common/EcosystemApplication.cpp
)

set(TSA_TEST_SOURCES
    ${TSA_ROOT}/tests/test_common.h
    ${TSA_ROOT}/tests/test_main.cpp
    ${TSA_ROOT}/tests/test_coordinates.cpp
    ${TSA_ROOT}/tests/test_model.cpp
    ${TSA_ROOT}/tests/test_file_io.cpp
    ${TSA_ROOT}/tests/test_commands.cpp
    ${TSA_ROOT}/tests/test_grids.cpp
    ${TSA_ROOT}/tests/test_viewer.cpp
    ${TSA_ROOT}/tests/test_cables.cpp
    ${TSA_ROOT}/tests/test_extensions.cpp
    ${TSA_ROOT}/tests/test_workplane.cpp
    ${TSA_ROOT}/tests/test_window_manager.cpp
    ${TSA_ROOT}/tests/test_node_system.cpp
    ${TSA_ROOT}/tests/test_loads.cpp
    ${TSA_ROOT}/tests/test_opensees.cpp
    ${TSA_ROOT}/tests/test_supports.cpp
    ${TSA_ROOT}/tests/test_standards.cpp
    ${TSA_ROOT}/tests/test_ndc_report.cpp
    ${TSA_ROOT}/tests/test_opensees_extraction.cpp
    ${TSA_ROOT}/tests/test_ai.cpp
    ${TSA_ROOT}/tests/test_ai_operator.cpp
    ${TSA_ROOT}/tests/test_preview.cpp
    ${TSA_ROOT}/tests/test_thumbnail_provider.cpp
    ${TSA_ROOT}/tests/test_analysis_engines.cpp
    ${TSA_ROOT}/tests/test_modeling_tools.cpp
    ${TSA_ROOT}/tests/test_met_de_deplacement.cpp
    ${TSA_ROOT}/tests/test_model_cleanup.cpp
    ${TSA_ROOT}/tests/test_bim.cpp
    ${TSA_ROOT}/tests/test_ifc_files.cpp
    ${TSA_ROOT}/tests/test_drawing.cpp
    ${TSA_ROOT}/tests/test_views.cpp
    ${TSA_ROOT}/tests/test_snap.cpp
    ${TSA_ROOT}/tests/test_automation.cpp
    ${TSA_ROOT}/tests/test_help.cpp
    ${TSA_ROOT}/tests/test_appearance.cpp
    ${TSA_ROOT}/tests/test_group_edit.cpp
    ${TSA_ROOT}/tests/test_topology.cpp
    ${TSA_ROOT}/tests/test_dimensions.cpp
    ${TSA_ROOT}/tests/test_display_modes.cpp
    ${TSA_ROOT}/tests/test_shortcuts.cpp
    ${TSA_ROOT}/tests/test_blueprint.cpp
    ${TSA_ROOT}/tests/test_tsa3d.cpp
    ${TSA_ROOT}/tests/test_modules.cpp
    ${TSA_ROOT}/tests/test_templates.cpp
    ${TSA_ROOT}/tests/test_deployment.cpp
    ${TSA_ROOT}/tests/test_mesh.cpp
    ${TSA_ROOT}/tests/test_surfaces.cpp
    ${TSA_ROOT}/tests/test_audit.cpp
    ${TSA_ROOT}/tests/test_benchmarks.cpp
    ${TSA_ROOT}/tests/test_display.cpp
    ${TSA_ROOT}/tests/test_multidoc.cpp
    ${TSA_ROOT}/tests/test_torsion.cpp
    ${TSA_ROOT}/tests/test_meshview.cpp
    ${TSA_ROOT}/tests/test_demo_models.cpp
    ${TSA_ROOT}/tests/test_i18n.cpp
    ${TSA_ROOT}/tests/test_project_templates.cpp
    ${TSA_ROOT}/tests/undo_characterization.h
    ${TSA_ROOT}/tests/test_undo_characterization.cpp
    # Ruban (couche application) : composants autonomes testés par la suite « ribbon »
    ${UI_RIBBON_SOURCES}
    ${TSA_ROOT}/resources/resources.qrc
    ${TSA_ROOT}/resources/templates.qrc
)

# Lanceurs de compilation (page de code MSVC, ccache) : à appeler une fois, avant les cibles.
#
# Dépendances d'en-têtes Ninja + MSVC : Ninja reconnaît les lignes /showIncludes grâce au préfixe
# détecté à la configuration. Avec un MSVC localisé (« Remarque : inclusion du fichier : »), ce
# préfixe contient une espace insécable que CMake écrit dans rules.ninja dans la page de code de la
# console qui GÉNÈRE (0xFF en CP850, C2 A0 en UTF-8), et que cl.exe écrit dans celle de la console qui
# COMPILE. Si elles diffèrent, AUCUNE dépendance d'en-tête n'est enregistrée et modifier un .h ne
# recompile pas les .cpp qui l'incluent (BUG-011, réapparu le 2026-10-06 après une reconfiguration
# lancée en CP850 alors que le lanceur imposait UTF-8).
# Correction : à chaque génération, la page de code active est relevée et le lanceur généré
# (msvc_codepage.cmd) l'impose à chaque compilation : les deux restent toujours accordées.
macro(tsa_setup_compiler_launchers)
    set(TSA_COMPILER_LAUNCHERS)
    if(CMAKE_GENERATOR MATCHES "Ninja" AND MSVC AND CMAKE_CXX_CL_SHOWINCLUDES_PREFIX MATCHES "[^	 -~]")
        execute_process(COMMAND cmd /c chcp OUTPUT_VARIABLE _tsa_chcp ERROR_QUIET)
        string(REGEX MATCH "[0-9]+" TSA_CONSOLE_CODEPAGE "${_tsa_chcp}")
        if(NOT TSA_CONSOLE_CODEPAGE)
            set(TSA_CONSOLE_CODEPAGE 65001)
        endif()
        configure_file("${TSA_ROOT}/cmake/msvc_codepage.cmd.in" "${CMAKE_BINARY_DIR}/msvc_codepage.cmd"
                       @ONLY NEWLINE_STYLE CRLF)
        list(APPEND TSA_COMPILER_LAUNCHERS "${CMAKE_BINARY_DIR}/msvc_codepage.cmd")
        message(STATUS "MSVC localisé : compilation via msvc_codepage.cmd (page de code ${TSA_CONSOLE_CODEPAGE})")
    endif()

    # ccache (optionnel, ignoré avec les générateurs Visual Studio).
    # Avec MSVC, ccache exige des infos de debug embarquées (/Z7) et son support
    # des en-têtes précompilés est limité : à activer seulement si vous le testez.
    if(TSA_USE_CCACHE AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        find_program(CCACHE_PROGRAM ccache)
        if(CCACHE_PROGRAM)
            list(APPEND TSA_COMPILER_LAUNCHERS "${CCACHE_PROGRAM}")
            if(MSVC)
                set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
                cmake_policy(SET CMP0141 NEW)
            endif()
            message(STATUS "ccache activé : ${CCACHE_PROGRAM}")
        endif()
    endif()
    if(TSA_COMPILER_LAUNCHERS)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${TSA_COMPILER_LAUNCHERS})
    endif()
endmacro()

# Options de compilation communes (Core, exécutable et tests).
# IMPORTANT : elles doivent être identiques pour pouvoir partager le PCH.
#   - src/ commun, puis les dossiers propres au produit (overlay) : un produit n'y redéfinit jamais
#     un chemin de src/ commun (contrôlé par tsa_add_product) ;
#   - product/ du produit : <ProductIdentity.h>, <ProductShellIds.h> (inclus avec des chevrons).
function(tsa_apply_common_settings target product_dir)
    # SYSTEM : les en-têtes OCCT sont traités comme "externes" (plus rapide,
    # et supprime les milliers d'avertissements /W4 venant d'OCCT).
    target_include_directories(${target} PRIVATE ${TSA_ROOT}/src ${ARGN} ${product_dir})
    target_include_directories(${target} SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR})
    # Ressources de développement (Extensions/, thirdparty/OpenSees) : sources communes. Ressources d'OCCT (Shaders…) :
    # celles de l'OCCT trouvé par find_package (share/opencascade/resources sous Linux, src du SDK Windows), BUG-120.
    target_compile_definitions(${target} PRIVATE TSA_SOURCE_DIR="${TSA_ROOT}" TSA_OCCT_RESOURCE_DIR="${OpenCASCADE_RESOURCE_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        # Compilation parallèle des fichiers (générateurs Visual Studio uniquement ;
        # Ninja parallélise déjà de lui-même).
        if(CMAKE_GENERATOR MATCHES "Visual Studio")
            target_compile_options(${target} PRIVATE /MP)
        endif()
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
endfunction()

# =============================================================================
# Bibliothèques partagées TSA / TSALab, compilées avec l'identité du produit (PRODUCT_DIR) :
#   <PREFIX>_Core     (STATIC) : TSARALOHA_CORE_SOURCES — noyau sans widget, sans rendu, sans moteur (QtCore, TKernel,
#                                TKMath) ; lié en PUBLIC par <PREFIX>_Model, donc transmis à tous ses consommateurs
#   <PREFIX>_Model    (OBJECT) : TSARALOHA_MODEL_SOURCES (+ MODEL_SOURCES du produit) — porte le PCH
#   <PREFIX>_Graphics (OBJECT) : TSARALOHA_GRAPHICS_SOURCES
#   <PREFIX>_Widgets  (OBJECT) : TSARALOHA_WIDGETS_SOURCES
# Une cible qui les utilise les lie toutes (les objets d'une OBJECT ne sont pas transitifs) :
#   target_link_libraries(<cible> PRIVATE <PREFIX>_Model <PREFIX>_Graphics <PREFIX>_Widgets)
#   target_precompile_headers(<cible> REUSE_FROM <PREFIX>_Model)
# =============================================================================
function(tsaraloha_add_shared_libraries)
    cmake_parse_arguments(P "" "PREFIX;PRODUCT_DIR" "OVERLAY_DIRS;MODEL_SOURCES" ${ARGN})
    set(core ${P_PREFIX}_Core)
    set(model ${P_PREFIX}_Model)
    set(graphics ${P_PREFIX}_Graphics)
    set(widgets ${P_PREFIX}_Widgets)

    # Un dossier propre au produit ne doit jamais masquer un fichier commun : avec MSVC, un
    # #include "X/Y.h" peut se résoudre différemment selon le fichier qui l'inclut (ODR).
    foreach(_overlay IN LISTS P_OVERLAY_DIRS)
        file(GLOB_RECURSE _own RELATIVE "${_overlay}" "${_overlay}/*")
        foreach(_f IN LISTS _own)
            if(EXISTS "${TSA_ROOT}/src/${_f}")
                message(FATAL_ERROR "${P_PREFIX} : ${_overlay}/${_f} masque le fichier commun ${TSA_ROOT}/src/${_f}. "
                                    "Corriger le fichier commun (ou y ajouter un point d'extension) au lieu de le copier.")
            endif()
        endforeach()
    endforeach()

    # Cœur scientifique (dépôt TSALab, C++ pur) : numerics, solveurs d'ossatures planes (MetDeDeplacement),
    # validation. Utilisé par les deux applications (moteur « custom2d » de TSA, laboratoire de TSALab).
    if(NOT TARGET tsalab_science)
        if(NOT EXISTS "${TSALAB_ROOT_DIR}/science/CMakeLists.txt")
            message(FATAL_ERROR "Cœur scientifique TSALab introuvable dans « ${TSALAB_ROOT_DIR} ».
"
                                "Cloner le dépôt TSALab à côté de TSA (../TSALab) ou indiquer -DTSALAB_ROOT_DIR=<chemin>.")
        endif()
        add_subdirectory(${TSALAB_ROOT_DIR}/science ${CMAKE_BINARY_DIR}/tsalab_science)
    endif()
    # Moteur EF 3D Fem3D (facultatif, voir TSA_FEM3D_DIR) : bibliothèque seule, sans ses tests.
    if(TSA_HAS_FEM3D AND NOT TARGET fem3d)
        set(FEM3D_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(${TSA_FEM3D_DIR} ${CMAKE_BINARY_DIR}/fem3d)
    endif()

    # TSA Core : bibliothèque STATIQUE, transmise aux exécutables par le lien PUBLIC de ${model} (les objets d'une
    # OBJECT ne le sont pas). Aucun de ses objets n'a d'initialisation statique (vérifié à l'étape 1) : un éventuel
    # enregistrement par initialisation statique n'a pas sa place dans le noyau (il serait perdu à l'édition de liens).
    # Dépendances limitées à QtCore et à la géométrie d'OCCT ; PCH propre, sans QtGui ni QtWidgets.
    add_library(${core} STATIC ${TSARALOHA_CORE_SOURCES})
    tsa_apply_common_settings(${core} ${P_PRODUCT_DIR} ${P_OVERLAY_DIRS})
    target_link_libraries(${core} PUBLIC Qt6::Core TKernel TKMath)
    target_precompile_headers(${core} PRIVATE
        <QtCore/QtCore>
        <Standard_Handle.hxx>
        <gp_Pnt.hxx>
        <gp_Vec.hxx>
        <gp_Dir.hxx>
        <gp_Trsf.hxx>
        <algorithm>
        <memory>
        <string>
        <vector>
        <map>
        <functional>
    )

    # Bibliothèques OBJECT : les .obj sont réutilisés tels quels par les exécutables et les tests
    # (pas de risque de perdre des initialisations statiques comme avec une .lib).
    add_library(${model} OBJECT ${TSARALOHA_MODEL_SOURCES} ${P_MODEL_SOURCES})
    tsa_apply_common_settings(${model} ${P_PRODUCT_DIR} ${P_OVERLAY_DIRS})
    # Le PCH de ${model} (REUSE_FROM) inclut QtWidgets et est compilé avec QT_WIDGETS_LIB : on garde les exigences de
    # COMPILATION de QtWidgets, à la même place (options identiques pour toutes les cibles qui partagent le PCH), sans le
    # LIEN. CMake < 3.27 (pas de $<COMPILE_ONLY>) : comportement antérieur.
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.27)
        set(_model_widgets $<COMPILE_ONLY:Qt6::Widgets>)
    else()
        set(_model_widgets Qt6::Widgets)
    endif()
    # Dépendances réellement utilisées par les objets du modèle (nm sur les objets ; tsa-mesh et tsa-ifc liés sans QtWidgets
    # ni rendu OCCT). QtWidgets et OCCT de rendu / B-Rep : portés par ${graphics}.
    target_link_libraries(${model} PUBLIC
        ${core}
        tsalab_science
        Qt6::Core
        Qt6::Gui
        ${_model_widgets}
        Qt6::Network
        TKernel
        TKMath
    )
    if(WIN32)
        target_link_libraries(${model} PUBLIC DbgHelp Crypt32)
    endif()
    if(TSA_HAS_FEM3D)
        target_link_libraries(${model} PUBLIC fem3d)
        target_compile_definitions(${model} PUBLIC TSA_HAS_FEM3D=1)
    endif()
    if(TARGET TSA::Gmsh)
        target_link_libraries(${model} PUBLIC TSA::Gmsh)
        # GmshMeshEngine inclut gmsh.h_cwrap (en-tête de 350 ko) : hors en-têtes précompilés
        set_source_files_properties(${TSA_ROOT}/src/Mesh/Gmsh/GmshMeshEngine.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    endif()
    # Import IFC complet (cmake/IfcOpenShell.cmake), si la bibliothèque est présente : pont compilé à part
    tsa_link_ifcopenshell(${model} ${P_PRODUCT_DIR} ${P_OVERLAY_DIRS})

    # En-têtes précompilés : Qt et OCCT sont très lourds à parser.
    # Les autres cibles réutilisent ce PCH (REUSE_FROM).
    target_precompile_headers(${model} PRIVATE
        <QtCore/QtCore>
        <QtGui/QtGui>
        <QtWidgets/QtWidgets>
        <Standard_Handle.hxx>
        <gp_Pnt.hxx>
        <gp_Vec.hxx>
        <gp_Dir.hxx>
        <gp_Ax1.hxx>
        <gp_Ax2.hxx>
        <gp_Trsf.hxx>
        <TopoDS_Shape.hxx>
        <algorithm>
        <memory>
        <string>
        <vector>
        <map>
        <unordered_map>
        <functional>
    )

    add_library(${graphics} OBJECT ${TSARALOHA_GRAPHICS_SOURCES})
    tsa_apply_common_settings(${graphics} ${P_PRODUCT_DIR} ${P_OVERLAY_DIRS})
    target_precompile_headers(${graphics} REUSE_FROM ${model})
    # Dépendances propres à graphics (symboles constatés) : QtWidgets (OccView, ThemeManager, Viewport*), rendu et B-Rep
    # OCCT. PUBLIC : widgets, applications et tests les utilisent aussi directement.
    target_link_libraries(${graphics} PUBLIC ${model} Qt6::Widgets Qt6::Svg ${OCCT_LIBS})

    add_library(${widgets} OBJECT ${TSARALOHA_WIDGETS_SOURCES})
    tsa_apply_common_settings(${widgets} ${P_PRODUCT_DIR} ${P_OVERLAY_DIRS})
    target_precompile_headers(${widgets} REUSE_FROM ${model})
    target_link_libraries(${widgets} PUBLIC ${model} ${graphics} Qt6::Widgets)

    set_source_files_properties(${TSA_NO_PCH_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endfunction()

# Extension Explorateur Windows (miniatures des fichiers projet du produit) : DLL autonome chargée par
# l'Explorateur, ni Qt ni OpenCASCADE, runtime C statique. Voir docs/THUMBNAIL_PROVIDER.md.
function(tsaraloha_add_thumbnail_provider target product_dir)
    if(NOT WIN32)
        return()
    endif()
    add_library(${target} SHARED
        ${TSA_ROOT}/src/ShellExtension/TSAThumbnailProvider.cpp
        ${TSA_ROOT}/src/ShellExtension/TSAThumbnailProvider.def
        ${TSA_ROOT}/src/IO/TSAPreviewBlock.h
        ${TSA_ROOT}/src/IO/TSAFileFormat.h
    )
    target_include_directories(${target} PRIVATE ${product_dir})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
    set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(${target} PRIVATE windowscodecs shlwapi ole32 shell32 advapi32)
endfunction()

# Pont MCP (tsaraloha-mcp.exe) : serveur Model Context Protocol (stdio) lancé par Claude Code ou tout client MCP,
# relayé vers l'AutomationServer de TSA / TSALab ouvert sur le poste. Qt Core + Network seulement ; placé à côté
# de l'application (mêmes DLL Qt). Voir docs/MCP.md.
function(tsaraloha_add_mcp_bridge target)
    add_executable(${target} ${TSA_ROOT}/tools/mcp/TsaralohaMcp.cpp)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Network)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "tsaraloha-mcp")
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
        target_link_options(${target} PRIVATE /SUBSYSTEM:CONSOLE)
    endif()
endfunction()

# windeployqt de la Qt retenue (bin/ de Qt6::qmake). Jamais par find_program en cache : après un changement de Qt, un
# windeployqt d'une autre installation déploierait des DLL d'une autre version (mélange).
function(tsa_find_windeployqt out)
    get_target_property(_qmake_loc Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_loc}" DIRECTORY)
    set(_exe "${_qt_bin_dir}/windeployqt${CMAKE_EXECUTABLE_SUFFIX}")
    if(EXISTS "${_exe}")
        set(${out} "${_exe}" PARENT_SCOPE)
        tsa_dependency_record(windeployqt "TROUVÉE" "${Qt6_VERSION}" "${_exe}")
    else()
        set(${out} "" PARENT_SCOPE)
        tsa_dependency_record(windeployqt "ABSENTE" "" "" "DLL Qt non déployées à côté de l'exécutable : lancer depuis Qt Creator ou ajouter ${_qt_bin_dir} au PATH")
    endif()
endfunction()

# Exécutable Qt d'une application de l'écosystème : sous-système Windows et déploiement des DLL
# (Qt, OCCT, 3rdparty, Extensions/ de la base commune) à côté de l'exécutable.
function(tsaraloha_configure_application target)
    tsa_copy_gmsh_runtime(${target})
    tsa_copy_ifcopenshell_runtime(${target})
    if(MSVC)
        # Debug : console conservée pour les journaux ; autres configurations : fenêtré pur.
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
            $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>
            $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
        )
    endif()
    if(WIN32)
        tsa_find_windeployqt(_windeployqt)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DTARGET_DIR=$<TARGET_FILE_DIR:${target}>
                -DSOURCE_DIR=${TSA_ROOT}
                -DTARGET_FILE=$<TARGET_FILE:${target}>
                -DWINDEPLOYQT_EXECUTABLE=${_windeployqt}
                "-DOCCT_BIN_DIR=${OpenCASCADE_BINARY_DIR}"
                -P "${TSA_ROOT}/cmake/DeployDependencies.cmake"
            COMMENT "Copie automatique de toutes les DLLs (Qt, OCCT, 3rdparty) vers le dossier du .exe..."
        )
    endif()
endfunction()

# Outils et exemples du SDK (docs/SDK.md) : construits avec le produit pour garantir qu'ils compilent.
#   tsa3d-validate        validateur TSA3D en ligne de commande (Qt Core seul), livré dans le paquet
#   tsa-template          validation, emballage et rendu de templates (Qt Core seul), livré dans le paquet
#   tsa3d_generator_example  programme externe C++17 sans Qt ni TSA produisant un fichier TSA3D
#   tsa3d_roundtrip_example  programme C++/Qt utilisant la bibliothèque TSA (lecture, modèle, export)
#   sample_csv2tsa3d      convertisseur du module d'exemple (processus séparé, modules/sample.csvimport)
function(tsaraloha_add_sdk_tools product model product_dir)
    if(TARGET tsa3d-validate)
        return()
    endif()
    add_executable(tsa3d-validate ${TSA_ROOT}/sdk/tools/tsa3d-validate/main.cpp
        ${TSA_ROOT}/src/IO/Tsa3d/Tsa3dCommon.cpp ${TSA_ROOT}/src/IO/Tsa3d/Tsa3dValidate.cpp)
    target_compile_features(tsa3d-validate PRIVATE cxx_std_20)
    target_include_directories(tsa3d-validate PRIVATE ${TSA_ROOT}/src ${product_dir})
    target_link_libraries(tsa3d-validate PRIVATE Qt6::Core)
    add_executable(tsa-template ${TSA_ROOT}/sdk/tools/tsa-template/main.cpp
        ${TSA_ROOT}/src/Templates/TemplateEngine.cpp ${TSA_ROOT}/src/Templates/TemplatePackage.cpp)
    target_compile_features(tsa-template PRIVATE cxx_std_20)
    target_include_directories(tsa-template PRIVATE ${TSA_ROOT}/src)
    target_link_libraries(tsa-template PRIVATE Qt6::Core)
    add_executable(tsa3d_generator_example ${TSA_ROOT}/sdk/examples/tsa3d-generator/portal_generator.cpp)
    target_compile_features(tsa3d_generator_example PRIVATE cxx_std_17)
    add_executable(tsa3d_roundtrip_example ${TSA_ROOT}/sdk/examples/tsa3d-roundtrip/main.cpp)
    tsa_apply_common_settings(tsa3d_roundtrip_example ${product_dir})
    target_link_libraries(tsa3d_roundtrip_example PRIVATE ${model})
    add_executable(tsa-mesh ${TSA_ROOT}/sdk/tools/tsa-mesh/main.cpp)
    tsa_apply_common_settings(tsa-mesh ${product_dir})
    target_link_libraries(tsa-mesh PRIVATE ${model})
    tsa_copy_gmsh_runtime(tsa-mesh)
    # tsa-ifc : import / export IFC en ligne de commande (essais sur fichiers réels, tests, publication)
    add_executable(tsa-ifc ${TSA_ROOT}/sdk/tools/tsa-ifc/main.cpp)
    tsa_apply_common_settings(tsa-ifc ${product_dir})
    target_link_libraries(tsa-ifc PRIVATE ${model})
    tsa_copy_ifcopenshell_runtime(tsa-ifc)
    if(MSVC)
        target_compile_options(tsa-ifc PRIVATE /utf-8)
        target_link_options(tsa-ifc PRIVATE /SUBSYSTEM:CONSOLE)
    endif()
    add_executable(sample_csv2tsa3d ${TSA_ROOT}/sdk/modules/sample.csvimport/src/csv2tsa3d.cpp)
    target_compile_features(sample_csv2tsa3d PRIVATE cxx_std_17)
    foreach(t tsa3d-validate tsa-template tsa-mesh tsa3d_generator_example tsa3d_roundtrip_example sample_csv2tsa3d)
        if(MSVC)
            target_compile_options(${t} PRIVATE /utf-8)
            target_link_options(${t} PRIVATE /SUBSYSTEM:CONSOLE)
        endif()
    endforeach()
    set_target_properties(tsa3d_generator_example tsa3d_roundtrip_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sdk)
    # Module d'exemple assemblé dans <build>/modules/sample.csvimport (manifeste, exemples, bin/) : c'est la
    # disposition livrée (AppPaths::shippedModulesDir), utilisée telle quelle en développement et par le paquet.
    # Runtime C statique : le convertisseur est lancé dans un processus séparé, sans les DLL de l'hôte.
    set(_mod ${CMAKE_BINARY_DIR}/modules/sample.csvimport)
    set_target_properties(sample_csv2tsa3d tsa3d_generator_example PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_target_properties(sample_csv2tsa3d PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${_mod}/bin)
    add_custom_command(TARGET sample_csv2tsa3d POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TSA_ROOT}/sdk/modules/sample.csvimport/module.json ${_mod}/module.json
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${TSA_ROOT}/sdk/modules/sample.csvimport/examples ${_mod}/examples
        COMMENT "Module d'exemple sample.csvimport assemblé")
    add_dependencies(${product} sample_csv2tsa3d)
    add_dependencies(${product} tsa3d-validate tsa-template tsa-mesh)
endfunction()

# Paquet distribuable autonome : cible package_<produit> (cmake/PackageTSA.cmake, docs/DEPLOYMENT.md).
# Dossier produit : <build>/dist/<produit> ; à vérifier avec tools/verify_package.py.
set(TSA_VCREDIST_DIR "$ENV{VCToolsRedistDir}" CACHE PATH "Runtime MSVC redistribuable (VCToolsRedistDir)")
option(TSA_PACKAGE_OPENSEES "Inclure OpenSees (thirdparty/OpenSees) dans le paquet — licence UC : usage éducatif, recherche, non lucratif ; diffusion commerciale non autorisée sans accord (docs/THIRD_PARTY_LICENSES.md)" OFF)
# Révision du code source citée par licenses/Gmsh/SOURCE.txt : commit exact construit (flux de publication de
# Setup-Livrable) ; vide : étiquette v<version>.
set(TSA_SOURCE_REF "" CACHE STRING "Commit ou étiquette du code source de la version distribuée")
# Autres codes compilés dans la version (dépôt voisin : TSALab pour TSA, TSA pour TSALab), adresses séparées par « ; ».
set(TSA_EXTRA_SOURCE_URLS "" CACHE STRING "Adresses du code source des autres dépôts compilés dans la version")

# Lit les constantes k<clé> de product/ProductIdentity.h du produit (seule source d'identité) : variables <prefix><clé>.
function(tsaraloha_read_identity prefix)
    file(READ "${CMAKE_SOURCE_DIR}/product/ProductIdentity.h" _identity)
    foreach(key ${ARGN})
        if(NOT _identity MATCHES "k${key}\\[\\] = L?\"([^\"]*)\"")
            message(FATAL_ERROR "ProductIdentity.h : k${key} introuvable")
        endif()
        set(${prefix}${key} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    endforeach()
endfunction()

function(tsaraloha_add_package_target target binaries)
    if(NOT WIN32)
        return()
    endif()
    # Gmsh (GPL) livré : le paquet indique où obtenir le code source correspondant (licenses/Gmsh/SOURCE.txt).
    tsaraloha_read_identity(_ SourceRepositoryUrl)
    string(REGEX MATCH "gmsh-([0-9]+\\.[0-9]+\\.[0-9]+)" _gmsh_match "${TSA_GMSH_LICENSE_DIR}")
    set(_gmsh_version "${CMAKE_MATCH_1}")
    tsa_find_windeployqt(_windeployqt)
    list(REMOVE_ITEM binaries "${target}.exe")
    string(REPLACE ";" "|" _extra "${binaries}")
    add_custom_target(package_${target}
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=$<TARGET_FILE_DIR:${target}>
            -DSOURCE_DIR=${TSA_ROOT}
            -DDIST_DIR=${CMAKE_BINARY_DIR}/dist/${target}
            -DPRODUCT=${target}
            -DPRODUCT_LICENSE=${CMAKE_SOURCE_DIR}/LICENSE
            -DVERSION=${PROJECT_VERSION}
            -DCONFIG=$<CONFIG>
            -DWINDEPLOYQT_EXECUTABLE=${_windeployqt}
            "-DOCCT_BIN_DIR=${OpenCASCADE_BINARY_DIR}"
            -DVCREDIST_DIR=${TSA_VCREDIST_DIR}
            -DPACKAGE_OPENSEES=${TSA_PACKAGE_OPENSEES}
            "-DGMSH_DLL=$<$<BOOL:${TSA_PACKAGE_GMSH}>:${TSA_GMSH_DLL}>"
            "-DGMSH_LICENSE_DIR=${TSA_GMSH_LICENSE_DIR}"
            "-DGMSH_VERSION=${_gmsh_version}"
            "-DSOURCE_URL=${_SourceRepositoryUrl}"
            "-DSOURCE_REF=${TSA_SOURCE_REF}"
            "-DEXTRA_SOURCE_URLS=${TSA_EXTRA_SOURCE_URLS}"
            "-DEXTRA_EXECUTABLES=${_extra}"
            "-DIFCOPENSHELL_ROOT=$<$<BOOL:${TSA_HAVE_IFCOPENSHELL}>:${TSA_IFCOPENSHELL_ROOT}>"
            -P "${TSA_ROOT}/cmake/PackageTSA.cmake"
        DEPENDS ${target}
        COMMENT "Paquet distribuable ${target} → ${CMAKE_BINARY_DIR}/dist/${target}"
        VERBATIM)
    tsaraloha_add_installer_target(${target})
endfunction()

# Installateur Windows : cible installer_<produit> (Inno Setup 6, packaging/installer.iss), construite à partir du
# paquet package_<produit>. Les valeurs viennent de product/ProductIdentity.h du produit, seule source d'identité.
# Sans Inno Setup (ISCC), la cible n'existe pas : le paquet portable reste disponible.
function(tsaraloha_add_installer_target target)
    find_program(TSA_ISCC_EXECUTABLE ISCC
        HINTS "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6" "$ENV{ProgramFiles}/Inno Setup 6" "C:/Program Files (x86)/Inno Setup 6")
    if(NOT TSA_ISCC_EXECUTABLE)
        message(STATUS "Inno Setup 6 (ISCC) introuvable : pas de cible installer_${target}")
        return()
    endif()
    tsaraloha_read_identity(_ Name Version ProjectExtension ProgId ProjectFriendlyName ProjectContentType ThumbnailProviderDll
                              AppUserModelId OrganizationName SettingsApplication InstallerAppId)
    if(NOT _Name STREQUAL target)
        message(FATAL_ERROR "ProductIdentity.h : kName « ${_Name} » ≠ cible « ${target} »")
    endif()
    string(TOLOWER "${target}" _slug)
    add_custom_target(installer_${target}
        COMMAND "${TSA_ISCC_EXECUTABLE}" /Q
            "/DProduct=${target}"
            "/DProductSlug=${_slug}"
            "/DVersion=${_Version}"
            "/DNumericVersion=${PROJECT_VERSION}"
            "/DAppId=${_InstallerAppId}"
            "/DAppUserModelId=${_AppUserModelId}"
            "/DExtension=${_ProjectExtension}"
            "/DProgId=${_ProgId}"
            "/DFriendlyName=${_ProjectFriendlyName}"
            "/DContentType=${_ProjectContentType}"
            "/DThumbnailDll=${_ThumbnailProviderDll}"
            "/DDataDir=${_OrganizationName}\\${_SettingsApplication}"
            "/DSourceDir=${CMAKE_BINARY_DIR}/dist/${target}"
            "/DOutputDir=${CMAKE_BINARY_DIR}/dist"
            "/DSetupIcon=${CMAKE_SOURCE_DIR}/resources/icons/${target}.ico"
            "/DLicenseFile=${CMAKE_SOURCE_DIR}/LICENSE"
            "${TSA_ROOT}/packaging/installer.iss"
        DEPENDS package_${target}
        COMMENT "Installateur ${target} → ${CMAKE_BINARY_DIR}/dist/${target}-${_Version}-windows-x64-setup.exe"
        VERBATIM)
endfunction()

# =============================================================================
# tsa_add_product : application TSA (ruban, AppShell, Start Center) sur les bibliothèques partagées.
#   NAME <nom>  PRODUCT_DIR <dossier>  RESOURCES <.qrc/.rc>
# Cibles : <nom>_Model, <nom>_Graphics, <nom>_Widgets, <nom>, <nom>_Tests (<nom>_TestSuite.exe),
#          <nom>ThumbnailProvider.
# =============================================================================
function(tsa_add_product)
    cmake_parse_arguments(P "" "NAME;PRODUCT_DIR" "RESOURCES" ${ARGN})
    set(name ${P_NAME})
    set(model ${name}_Model)
    set(graphics ${name}_Graphics)
    set(widgets ${name}_Widgets)
    set(tests ${name}_Tests)
    set(thumbs ${name}ThumbnailProvider)

    tsaraloha_add_shared_libraries(PREFIX ${name} PRODUCT_DIR ${P_PRODUCT_DIR})

    # Traductions de l'interface (docs/I18N.md) : translations/tsa_<langue>.ts → .qm intégrés aux ressources (:/i18n/),
    # dans l'application et dans la suite de tests. Mise à jour des .ts : tools/i18n/update_translations.py.
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    set(_tsa_qm_files "")
    if(TARGET Qt6::lrelease AND Qt6_VERSION VERSION_LESS TSA_QT_TRANSLATIONS_MIN_VERSION)
        # Forme qt_add_lrelease(TS_FILES …) : 6.7 ; QM_OUTPUT_DIRECTORY : 6.9 (sinon .qm hors de BASE, ressources fausses).
        message(WARNING "Qt ${Qt6_VERSION} < ${TSA_QT_TRANSLATIONS_MIN_VERSION} : ${name} sera compilé sans traductions "
                        "anglaise et malgache (qt_add_lrelease de cette version incompatible).")
    elseif(TARGET Qt6::lrelease)
        qt_add_lrelease(TS_FILES ${TSA_ROOT}/translations/tsa_en.ts ${TSA_ROOT}/translations/tsa_mg.ts
            LRELEASE_TARGET ${name}_lrelease NO_GLOBAL_TARGET
            QM_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/i18n_${name}
            QM_FILES_OUTPUT_VARIABLE _tsa_qm_files
            OPTIONS -silent)
    else()
        message(WARNING "Qt LinguistTools (lrelease) introuvable : ${name} sera compilé sans traductions anglaise et "
                        "malgache (textes en français ; la suite « i18n » le signale).")
    endif()
    if(_tsa_qm_files)
        tsa_dependency_record("Qt LinguistTools" "TROUVÉE" "${Qt6_VERSION}" "" "traductions en, mg")
    else()
        tsa_dependency_record("Qt LinguistTools" "DÉSACTIVÉE" "" "" "facultative : interface en français seulement")
    endif()

    add_executable(${name} ${TSA_APP_SOURCES} ${P_RESOURCES})
    if(_tsa_qm_files)
        qt_add_resources(${name} "${name}_i18n" PREFIX "/i18n" BASE ${CMAKE_CURRENT_BINARY_DIR}/i18n_${name} FILES ${_tsa_qm_files})
    endif()
    tsa_apply_common_settings(${name} ${P_PRODUCT_DIR})
    target_precompile_headers(${name} REUSE_FROM ${model})
    target_link_libraries(${name} PRIVATE ${model} ${graphics} ${widgets} Qt6::Widgets Qt6::Svg)
    if(WIN32)
        # AppShell : barre de titre personnalisée (ombre DWM de la fenêtre sans cadre système)
        target_link_libraries(${name} PRIVATE dwmapi)
    endif()
    tsaraloha_configure_application(${name})

    tsaraloha_add_thumbnail_provider(${thumbs} ${P_PRODUCT_DIR})
    tsaraloha_add_sdk_tools(${name} ${model} ${P_PRODUCT_DIR})
    tsaraloha_add_package_target(${name} "${name}.exe;tsaraloha-mcp.exe;tsa3d-validate.exe;tsa-template.exe;tsa-mesh.exe;${name}_Benchmark.exe;${thumbs}.dll")
    if(TARGET ${thumbs})
        add_dependencies(${name} ${thumbs})
    endif()
    tsaraloha_add_mcp_bridge(${name}_Mcp)
    add_dependencies(${name} ${name}_Mcp)

    # --- Tests unitaires : bibliothèques partagées -------------------------
    if(TSA_BUILD_TESTS)   # enable_testing() : CMakeLists.txt racine du produit (portée du répertoire)
        add_executable(${tests} ${TSA_TEST_SOURCES})
        tsa_apply_common_settings(${tests} ${P_PRODUCT_DIR})
        target_include_directories(${tests} PRIVATE ${TSA_ROOT}/tests)
        target_precompile_headers(${tests} REUSE_FROM ${model})
        # QtWidgets utilisé directement ; QT_WIDGETS_LIB requis par les surcharges widgets de QtTest
        target_link_libraries(${tests} PRIVATE ${model} ${graphics} ${widgets} Qt6::Widgets)
        if(TARGET Qt6::Test)   # frappes clavier simulées (suite « shortcuts »)
            target_link_libraries(${tests} PRIVATE Qt6::Test)
            target_compile_definitions(${tests} PRIVATE TSA_HAVE_QTTEST=1)
            add_custom_command(TARGET ${tests} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt6::Test> $<TARGET_FILE_DIR:${tests}>)
        endif()
        set_target_properties(${tests} PROPERTIES OUTPUT_NAME "${name}_TestSuite")
        if(_tsa_qm_files)
            qt_add_resources(${tests} "${tests}_i18n" PREFIX "/i18n" BASE ${CMAKE_CURRENT_BINARY_DIR}/i18n_${name} FILES ${_tsa_qm_files})
        endif()
        tsa_copy_gmsh_runtime(${tests})
        tsa_copy_ifcopenshell_runtime(${tests})
        add_dependencies(${tests} ${name}_Mcp sample_csv2tsa3d)
        target_compile_definitions(${tests} PRIVATE TSA_SAMPLE_MODULE_DIR="${CMAKE_BINARY_DIR}/modules/sample.csvimport")
        target_compile_definitions(${tests} PRIVATE TSARALOHA_MCP_EXE="$<TARGET_FILE:${name}_Mcp>")
        if(WIN32)
            add_dependencies(${tests} ${thumbs})
            target_link_libraries(${tests} PRIVATE shlwapi ole32)
            set_source_files_properties(${TSA_ROOT}/tests/test_thumbnail_provider.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
        endif()

        # Banc de validation en ligne de commande (docs/BENCHMARKS.md) : cas JSON tests/benchmarks, références externes
        add_executable(${name}_Benchmark ${TSA_ROOT}/tools/benchmark/tsa_benchmark.cpp)
        tsa_apply_common_settings(${name}_Benchmark ${P_PRODUCT_DIR})
        target_precompile_headers(${name}_Benchmark REUSE_FROM ${model})
        target_link_libraries(${name}_Benchmark PRIVATE ${model} ${graphics} ${widgets})
        tsa_copy_gmsh_runtime(${name}_Benchmark)
        if(MSVC)
            target_link_options(${name}_Benchmark PRIVATE /SUBSYSTEM:CONSOLE)
        endif()
        add_test(NAME ${name}_Benchmarks COMMAND ${name}_Benchmark --out ${CMAKE_BINARY_DIR}/validation-reports)

        # TSA Core seul (étape 1) : programme lié uniquement à <nom>_Core — preuve à l'édition de liens que le noyau ne
        # requiert ni QtGui, ni QtWidgets, ni le rendu OCCT, ni un moteur de calcul.
        add_executable(${name}_CoreTests ${TSA_ROOT}/tests/core/test_core_link.cpp)
        tsa_apply_common_settings(${name}_CoreTests ${P_PRODUCT_DIR})
        target_link_libraries(${name}_CoreTests PRIVATE ${name}_Core)
        add_test(NAME ${name}_CoreLink COMMAND ${name}_CoreTests)
        # Noyau STATIQUE : aucune initialisation statique dynamique (elle serait perdue à l'édition de liens). Contrôle par nm
        # (GCC / Clang) ; non enregistré avec MSVC (pas d'équivalent fiable), la règle portant sur les mêmes sources.
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_FOUND AND CMAKE_NM AND NOT MSVC)
            add_test(NAME ${name}_CoreStaticInit
                     COMMAND Python3::Interpreter ${TSA_ROOT}/tools/check_core_static_init.py ${CMAKE_NM} $<TARGET_FILE:${name}_Core>)
        endif()

        # Tests complets et suites thématiques enregistrés sous CTest
        add_test(NAME ${name}_AllTests COMMAND ${tests})
        foreach(_suite coordinates model io commands grids viewer cables extensions workplane window node
                       loads opensees supports standards bim ifc drawing views tsa3d modules templates deployment mesh surfaces audit benchmarks display multidoc i18n ribbon undo)
            add_test(NAME ${name}_${_suite}Tests COMMAND ${tests} --suite=${_suite})
        endforeach()
        # Couches de la base commune (docs/TSARALOHA_ARCHITECTURE.md)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_FOUND)
            add_test(NAME ${name}_Layers COMMAND Python3::Interpreter ${TSA_ROOT}/tools/check_layers.py ${TSA_ROOT}/src)
            # Traductions : paramètres, raccourcis &, doublons, caractères corrompus (docs/I18N.md §5)
            add_test(NAME ${name}_I18nCheck COMMAND Python3::Interpreter ${TSA_ROOT}/tools/i18n/check_translations.py --strict)
        endif()
    endif()
endfunction()
