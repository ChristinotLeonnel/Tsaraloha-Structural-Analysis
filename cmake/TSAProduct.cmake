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
# Cœur scientifique : dépôt TSALab voisin (TSALab/science).
set(TSALAB_ROOT_DIR "${TSA_ROOT}/../TSALab" CACHE PATH "Racine du dépôt TSALab (cœur scientifique tsalab_science)")
get_filename_component(TSALAB_ROOT_DIR "${TSALAB_ROOT_DIR}" ABSOLUTE)
# Outils et tests du cœur scientifique : compilés par TSALab, pas par défaut dans le build de TSA.
if(NOT DEFINED TSALAB_SCIENCE_TOOLS)
    set(TSALAB_SCIENCE_TOOLS OFF CACHE BOOL "Compiler tsalab-bench et les tests du cœur scientifique")
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
    ${TSA_ROOT}/src/UI/Properties/PropertyPanel.h
    ${TSA_ROOT}/src/UI/Properties/PropertyPanel.cpp
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
)

# --- Boîtes de dialogue -------------------------------------------------------
set(UI_DIALOGS_SOURCES
    ${TSA_ROOT}/src/UI/HelpLauncher.h
    ${TSA_ROOT}/src/UI/HelpLauncher.cpp
    ${TSA_ROOT}/src/UI/Dialogs/GridDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/GridDialog.cpp
    ${TSA_ROOT}/src/UI/Dialogs/GridSettingsDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/GridSettingsDialog.cpp
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
    ${TSA_ROOT}/src/UI/Analysis/AnalysisDialog.h
    ${TSA_ROOT}/src/UI/Analysis/AnalysisDialog.cpp
    ${TSA_ROOT}/src/UI/Analysis/AnalysisManagerPanel.h
    ${TSA_ROOT}/src/UI/Analysis/AnalysisManagerPanel.cpp
)

set(UI_EXTENSIONMANAGERDIALOG_SOURCES
    ${TSA_ROOT}/src/UI/Dialogs/ExtensionManagerDialog.h
    ${TSA_ROOT}/src/UI/Dialogs/ExtensionManagerDialog.cpp
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
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintScene.h
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintScene.cpp
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintEditor.h
    ${TSA_ROOT}/src/UI/Blueprint/BlueprintEditor.cpp
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
    ${TSA_ROOT}/src/Viewer/OccView_Navigation.cpp
    ${TSA_ROOT}/src/Viewer/OccView_Events.cpp
    ${TSA_ROOT}/src/Viewer/OccView_Tools.cpp
    ${TSA_ROOT}/src/Viewer/SelectionManager.h
    ${TSA_ROOT}/src/Viewer/SelectionManager.cpp
    ${TSA_ROOT}/src/Viewer/ResultsVisualManager.h
    ${TSA_ROOT}/src/Viewer/ResultsVisualManager.cpp
)

# --- Matériaux visuels, projection et navigation de vue — [CORE] --------------
set(VIEWER_CORE_SOURCES
    ${TSA_ROOT}/src/Viewer/MaterialVisual.h
    ${TSA_ROOT}/src/Viewer/MaterialVisual.cpp
    ${TSA_ROOT}/src/Viewer/TextureManager.h
    ${TSA_ROOT}/src/Viewer/TextureManager.cpp
    ${TSA_ROOT}/src/Viewer/ProjectionManager.h
    ${TSA_ROOT}/src/Viewer/ProjectionManager.cpp
    ${TSA_ROOT}/src/Viewer/ViewManager.h
    ${TSA_ROOT}/src/Viewer/ViewManager.cpp
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
    ${TSA_ROOT}/src/BIM/Core/BimTypes.h
    ${TSA_ROOT}/src/BIM/Core/BimTypes.cpp
    ${TSA_ROOT}/src/BIM/Core/IfcGuid.h
    ${TSA_ROOT}/src/BIM/Core/IfcGuid.cpp
    ${TSA_ROOT}/src/BIM/Core/BimModel.h
    ${TSA_ROOT}/src/BIM/Core/BimModel.cpp
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
    ${TSA_ROOT}/src/Analysis/Engines/BuiltInEngines.cpp
    ${TSA_ROOT}/src/Analysis/Engines/OpenSees/OpenSeesEngine.h
    ${TSA_ROOT}/src/Analysis/Engines/OpenSees/OpenSeesEngine.cpp
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DSolver.h
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DAdapter.h
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DAdapter.cpp
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DEngine.h
    ${TSA_ROOT}/src/Analysis/Engines/Custom2D/Custom2DEngine.cpp
)

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
    ${TSA_ROOT}/src/Interaction/Tools/ModelingTool.h
    ${TSA_ROOT}/src/Interaction/Tools/ModelingTool.cpp
    ${TSA_ROOT}/src/Interaction/Tools/ModifyTools.h
    ${TSA_ROOT}/src/Interaction/Tools/ModifyTools.cpp
    ${TSA_ROOT}/src/Interaction/Tools/DrawTools.cpp
    ${TSA_ROOT}/src/Model/ModelElementCopy.h
)

# --- Format de fichier TSA (sauvegarde / chargement) — [CORE] ----------------
set(IO_SOURCES
    ${TSA_ROOT}/src/IO/TSAFileFormat.h
    ${TSA_ROOT}/src/IO/TSAFile_BinaryUtils.h
    ${TSA_ROOT}/src/IO/TSAFile.h
    ${TSA_ROOT}/src/IO/TSAFile.cpp
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
    ${TSA_ROOT}/src/UI/Shell/AppShell.h
    ${TSA_ROOT}/src/UI/Shell/AppShell.cpp
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
    ${TSA_ROOT}/tests/test_preview.cpp
    ${TSA_ROOT}/tests/test_thumbnail_provider.cpp
    ${TSA_ROOT}/tests/test_analysis_engines.cpp
    ${TSA_ROOT}/tests/test_modeling_tools.cpp
    ${TSA_ROOT}/tests/test_met_de_deplacement.cpp
    ${TSA_ROOT}/tests/test_model_cleanup.cpp
    ${TSA_ROOT}/tests/test_bim.cpp
    ${TSA_ROOT}/tests/test_snap.cpp
    ${TSA_ROOT}/tests/test_automation.cpp
    ${TSA_ROOT}/tests/test_help.cpp
    ${TSA_ROOT}/tests/test_appearance.cpp
    ${TSA_ROOT}/tests/test_blueprint.cpp
    ${TSA_ROOT}/resources/resources.qrc
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
    # Ressources de développement (Extensions/, thirdparty/OpenSees) : sources communes.
    target_compile_definitions(${target} PRIVATE TSA_SOURCE_DIR="${TSA_ROOT}")
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
#   <PREFIX>_Model    (OBJECT) : TSARALOHA_MODEL_SOURCES (+ MODEL_SOURCES du produit) — porte le PCH
#   <PREFIX>_Graphics (OBJECT) : TSARALOHA_GRAPHICS_SOURCES
#   <PREFIX>_Widgets  (OBJECT) : TSARALOHA_WIDGETS_SOURCES
# Une cible qui les utilise les lie toutes (les objets d'une OBJECT ne sont pas transitifs) :
#   target_link_libraries(<cible> PRIVATE <PREFIX>_Model <PREFIX>_Graphics <PREFIX>_Widgets)
#   target_precompile_headers(<cible> REUSE_FROM <PREFIX>_Model)
# =============================================================================
function(tsaraloha_add_shared_libraries)
    cmake_parse_arguments(P "" "PREFIX;PRODUCT_DIR" "OVERLAY_DIRS;MODEL_SOURCES" ${ARGN})
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

    # Bibliothèques OBJECT : les .obj sont réutilisés tels quels par les exécutables et les tests
    # (pas de risque de perdre des initialisations statiques comme avec une .lib).
    add_library(${model} OBJECT ${TSARALOHA_MODEL_SOURCES} ${P_MODEL_SOURCES})
    tsa_apply_common_settings(${model} ${P_PRODUCT_DIR} ${P_OVERLAY_DIRS})
    target_link_libraries(${model} PUBLIC
        tsalab_science
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        ${OCCT_LIBS}
    )
    if(WIN32)
        target_link_libraries(${model} PUBLIC DbgHelp Crypt32)
    endif()

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
    target_link_libraries(${graphics} PUBLIC ${model} Qt6::Svg)

    add_library(${widgets} OBJECT ${TSARALOHA_WIDGETS_SOURCES})
    tsa_apply_common_settings(${widgets} ${P_PRODUCT_DIR} ${P_OVERLAY_DIRS})
    target_precompile_headers(${widgets} REUSE_FROM ${model})
    target_link_libraries(${widgets} PUBLIC ${model} ${graphics})

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

# Exécutable Qt d'une application de l'écosystème : sous-système Windows et déploiement des DLL
# (Qt, OCCT, 3rdparty, Extensions/ de la base commune) à côté de l'exécutable.
function(tsaraloha_configure_application target)
    if(MSVC)
        # Debug : console conservée pour les journaux ; autres configurations : fenêtré pur.
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>
            $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>
            $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
        )
    endif()
    if(WIN32)
        get_target_property(_qmake_loc Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qmake_loc}" DIRECTORY)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DTARGET_DIR=$<TARGET_FILE_DIR:${target}>
                -DSOURCE_DIR=${TSA_ROOT}
                -DTARGET_FILE=$<TARGET_FILE:${target}>
                -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
                -P "${TSA_ROOT}/cmake/DeployDependencies.cmake"
            COMMENT "Copie automatique de toutes les DLLs (Qt, OCCT, 3rdparty) vers le dossier du .exe..."
        )
    endif()
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

    add_executable(${name} ${TSA_APP_SOURCES} ${P_RESOURCES})
    tsa_apply_common_settings(${name} ${P_PRODUCT_DIR})
    target_precompile_headers(${name} REUSE_FROM ${model})
    target_link_libraries(${name} PRIVATE ${model} ${graphics} ${widgets} Qt6::Svg)
    if(WIN32)
        # AppShell : barre de titre personnalisée (ombre DWM de la fenêtre sans cadre système)
        target_link_libraries(${name} PRIVATE dwmapi)
    endif()
    tsaraloha_configure_application(${name})

    tsaraloha_add_thumbnail_provider(${thumbs} ${P_PRODUCT_DIR})
    if(TARGET ${thumbs})
        add_dependencies(${name} ${thumbs})
    endif()
    tsaraloha_add_mcp_bridge(${name}_Mcp)
    add_dependencies(${name} ${name}_Mcp)

    # --- Tests unitaires : bibliothèques partagées -------------------------
    if(TSA_BUILD_TESTS)
        enable_testing()
        add_executable(${tests} ${TSA_TEST_SOURCES})
        tsa_apply_common_settings(${tests} ${P_PRODUCT_DIR})
        target_include_directories(${tests} PRIVATE ${TSA_ROOT}/tests)
        target_precompile_headers(${tests} REUSE_FROM ${model})
        target_link_libraries(${tests} PRIVATE ${model} ${graphics} ${widgets})
        set_target_properties(${tests} PROPERTIES OUTPUT_NAME "${name}_TestSuite")
        add_dependencies(${tests} ${name}_Mcp)
        target_compile_definitions(${tests} PRIVATE TSARALOHA_MCP_EXE="$<TARGET_FILE:${name}_Mcp>")
        if(WIN32)
            add_dependencies(${tests} ${thumbs})
            target_link_libraries(${tests} PRIVATE shlwapi ole32)
            set_source_files_properties(${TSA_ROOT}/tests/test_thumbnail_provider.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
        endif()

        # Tests complets et suites thématiques enregistrés sous CTest
        add_test(NAME ${name}_AllTests COMMAND ${tests})
        foreach(_suite coordinates model io commands grids viewer cables extensions workplane window node
                       loads opensees supports standards)
            add_test(NAME ${name}_${_suite}Tests COMMAND ${tests} --suite=${_suite})
        endforeach()
        # Couches de la base commune (docs/TSARALOHA_ARCHITECTURE.md)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_FOUND)
            add_test(NAME ${name}_Layers COMMAND Python3::Interpreter ${TSA_ROOT}/tools/check_layers.py ${TSA_ROOT}/src)
        endif()
    endif()
endfunction()
