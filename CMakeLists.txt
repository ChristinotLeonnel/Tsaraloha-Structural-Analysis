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
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg Network)

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
    src/UI/MainWindow_Bim.cpp
    src/UI/MainWindow_ModelingTools.cpp
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
    src/UI/Properties/ElementResultsPanel.h
    src/UI/Properties/ElementResultsPanel.cpp
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
    src/UI/Properties/LoadPropertiesView.h
    src/UI/Properties/LoadPropertiesView.cpp
)

# --- Boîtes de dialogue -------------------------------------------------------
set(UI_DIALOGS_SOURCES
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
    src/UI/Dialogs/NodeSelectionDialog.h
    src/UI/Dialogs/NodeSelectionDialog.cpp
    src/UI/Dialogs/NewNodeDialog.h
    src/UI/Dialogs/NewNodeDialog.cpp
    src/UI/Dialogs/NodalLoadDialog.h
    src/UI/Dialogs/NodalLoadDialog.cpp
    src/UI/Dialogs/MemberLoadDialog.h
    src/UI/Dialogs/MemberLoadDialog.cpp
    src/UI/Dialogs/LoadCaseDialog.h
    src/UI/Dialogs/LoadCaseDialog.cpp
)

# Cette boîte de dialogue dépend d'ExtensionManager, testé indépendamment de
# l'UI : on l'isole pour pouvoir la réutiliser dans CORE_SOURCES. — [CORE]
# Fenêtre Analysis multi-moteurs et panneaux d'options par moteur — [CORE] (testée par TSA_Tests)
set(UI_TOOLS_SOURCES
    src/UI/Tools/ModelingToolDialog.h
    src/UI/Tools/ModelingToolDialog.cpp
    src/UI/Tools/ModelCleanupDialog.h
    src/UI/Tools/ModelCleanupDialog.cpp
)

set(UI_ANALYSIS_SOURCES
    src/UI/Analysis/AnalysisEngineOptions.h
    src/UI/Analysis/AnalysisEngineOptions.cpp
    src/UI/Analysis/OpenSeesOptionsWidget.h
    src/UI/Analysis/OpenSeesOptionsWidget.cpp
    src/UI/Analysis/Custom2DOptionsWidget.h
    src/UI/Analysis/Custom2DOptionsWidget.cpp
    src/UI/Analysis/AnalysisDialog.h
    src/UI/Analysis/AnalysisDialog.cpp
)

set(UI_EXTENSIONMANAGERDIALOG_SOURCES
    src/UI/Dialogs/ExtensionManagerDialog.h
    src/UI/Dialogs/ExtensionManagerDialog.cpp
)

# --- Gestionnaire centralisé des fenêtres et disposition (Window Manager) — [CORE] ---
set(UI_WINDOWMANAGER_SOURCES
    src/UI/WindowManager/WindowItem.h
    src/UI/WindowManager/WindowItem.cpp
    src/UI/WindowManager/WindowRegistry.h
    src/UI/WindowManager/WindowRegistry.cpp
    src/UI/WindowManager/LayoutManager.h
    src/UI/WindowManager/LayoutManager.cpp
    src/UI/WindowManager/WindowManager.h
    src/UI/WindowManager/WindowManager.cpp
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
    src/UI/Widgets/PointSelector.h
    src/UI/Widgets/PointSelector.cpp
    src/UI/Diagrams/Diagram2DWidget.h
    src/UI/Diagrams/Diagram2DWidget.cpp
    src/NDC/NDCViewerWidget.h
    src/NDC/NDCViewerWidget.cpp
    src/NDC/ReportConfigDialog.h
    src/NDC/ReportConfigDialog.cpp
    src/UI/Widgets/ProjectStatusOverlay.h
    src/UI/Widgets/ProjectStatusOverlay.cpp
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
    src/UI/Dock/ProjectionViewDock.h
    src/UI/Dock/ProjectionViewDock.cpp
    src/UI/Dock/ResultsDockWidget.h
    src/UI/Dock/ResultsDockWidget.cpp
    src/UI/Dock/AnalysisDataDock.h
    src/UI/Dock/AnalysisDataDock.cpp
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
    src/Coordinate/GeometryTolerance.h
    src/Coordinate/WorkPlane.cpp
    src/Coordinate/WorkPlaneCoordinateSystem.h
    src/Coordinate/WorkPlaneCoordinateSystem.cpp
    src/Coordinate/CoordinateTransform.h
    src/Coordinate/CoordinateTransform.cpp
    src/Coordinate/AxisColorConfig.h
    src/Coordinate/AxisColorConfig.cpp
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
    src/Grid/SnapEngine.h
    src/Grid/SnapEngine.cpp
    src/Grid/SnapManager.h
    src/Grid/SnapManager.cpp
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
    src/Grid/SnapMarker.h
    src/Grid/SnapMarker.cpp
)

# --- Viewer 3D OCCT (UI uniquement) --------------------------------------------
set(VIEWER_MAIN_SOURCES
    src/Viewer/OccView.h
    src/Viewer/OccView.cpp
    src/Viewer/OccView_Shapes.cpp
    src/Viewer/OccView_Navigation.cpp
    src/Viewer/OccView_Events.cpp
    src/Viewer/OccView_Tools.cpp
    src/Viewer/SelectionManager.h
    src/Viewer/SelectionManager.cpp
    src/Viewer/ResultsVisualManager.h
    src/Viewer/ResultsVisualManager.cpp
)

# --- Matériaux visuels, projection et navigation de vue — [CORE] --------------
set(VIEWER_CORE_SOURCES
    src/Viewer/MaterialVisual.h
    src/Viewer/MaterialVisual.cpp
    src/Viewer/TextureManager.h
    src/Viewer/TextureManager.cpp
    src/Viewer/ProjectionManager.h
    src/Viewer/ProjectionManager.cpp
    src/Viewer/ViewManager.h
    src/Viewer/ViewManager.cpp
)

# --- Géométrie des poutres et résultats (utilisée par les tests) — [CORE] ----
set(GEOMETRY_CORE_SOURCES
    src/Geometry/BeamGeometry.h
    src/Geometry/BeamGeometry.cpp
    src/Geometry/DeformedGeometry.h
    src/Geometry/DeformedGeometry.cpp
    src/Geometry/DiagramGeometry.h
    src/Geometry/DiagramGeometry.cpp
    src/Geometry/SupportGeometry.h
    src/Geometry/SupportGeometry.cpp
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
    src/Model/SupportDefinition.h
    src/Model/SupportDefinition.cpp
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
    src/Model/Model_Transformations.cpp
    src/Model/Model_Topology.cpp
    src/Model/ModelCleanup.h
    src/Model/ModelCleanup.cpp
    # Couche BIM (docs/BIM_ARCHITECTURE.md)
    src/Core/Units.h
    src/BIM/Core/BimTypes.h
    src/BIM/Core/BimTypes.cpp
    src/BIM/Core/IfcGuid.h
    src/BIM/Core/IfcGuid.cpp
    src/BIM/Core/BimModel.h
    src/BIM/Core/BimModel.cpp
    src/BIM/IFC/IfcStepWriter.h
    src/BIM/IFC/IfcStepWriter.cpp
    src/BIM/IFC/IfcExportContext.h
    src/BIM/IFC/IfcMapper.h
    src/BIM/IFC/IfcMapper.cpp
    src/BIM/IFC/IfcGeometryMapper.h
    src/BIM/IFC/IfcGeometryMapper.cpp
    src/BIM/IFC/IfcPropertyMapper.h
    src/BIM/IFC/IfcPropertyMapper.cpp
    src/BIM/IFC/IfcRelationshipMapper.h
    src/BIM/IFC/IfcRelationshipMapper.cpp
    src/BIM/IFC/IfcExporter.h
    src/BIM/IFC/IfcExporter.cpp
    src/BIM/IFC/IfcStepReader.h
    src/BIM/IFC/IfcStepReader.cpp
    src/BIM/IFC/IfcImporter.h
    src/BIM/IFC/IfcImporter.cpp
    src/Model/Model_Snapshots.cpp
    src/Model/ModelDiff.h
    src/Model/ModelDiff.cpp
    src/Model/StructuralClipboard.h
    src/Model/StructuralClipboard.cpp
    src/Model/SelectionQuery.h
    src/Model/SelectionQuery.cpp
)

# --- Système de chargement et solveur OpenSees — [CORE] ------------------------
set(LOAD_CORE_SOURCES
    src/Model/Load/LoadEnums.h
    src/Model/Load/NodalLoad.h
    src/Model/Load/NodalLoad.cpp
    src/Model/Load/MemberLoad.h
    src/Model/Load/MemberLoad.cpp
    src/Model/Load/LoadCase.h
    src/Model/Load/LoadCase.cpp
    src/Model/Load/LoadCombination.h
    src/Model/Load/LoadCombination.cpp
    src/Model/Load/LoadManager.h
    src/Model/Load/LoadManager.cpp
    src/Analysis/LoadResolver.h
    src/Analysis/LoadResolver.cpp
    src/Analysis/LoadValidation.h
    src/Analysis/LoadValidation.cpp
    src/Analysis/OpenSeesAdapter.h
    src/Analysis/OpenSeesAdapter.cpp
    src/Analysis/OpenSeesManager.h
    src/Analysis/OpenSeesManager.cpp
    src/Analysis/CalculationSnapshot.h
    src/Analysis/CalculationSnapshot.cpp
    src/Analysis/ResultsModel.h
    src/Analysis/ResultsModel.cpp
    src/Analysis/AnalysisTypes.h
    src/Analysis/ElementTransformation.h
    src/Analysis/ElementTransformation.cpp
    src/Analysis/OpenSeesModelMap.h
    src/Analysis/OpenSeesModelMap.cpp
    src/Analysis/ResultsContext.h
    src/Analysis/ResultsContext.cpp
    src/Analysis/ResultsExport.h
    src/Analysis/ResultsExport.cpp
    src/Analysis/ResultsValidityGuard.h
    src/Analysis/ResultsValidityGuard.cpp
    src/Analysis/OpenSeesAnalysisBuilder.h
    src/Analysis/OpenSeesAnalysisBuilder.cpp
    src/Analysis/OpenSeesResultsReader.h
    src/Analysis/OpenSeesResultsReader.cpp
    src/Analysis/OpenSeesSolver.h
    src/Analysis/OpenSeesSolver.cpp
)

# --- Analyse multi-moteurs (contexte, portée, modèle d'analyse, registre, adaptateurs) — [CORE] ---
set(ANALYSIS_ENGINE_SOURCES
    src/Analysis/Engine/AnalysisValidation.h
    src/Analysis/Engine/AnalysisContext.h
    src/Analysis/Engine/AnalysisContext.cpp
    src/Analysis/Engine/AnalysisModel.h
    src/Analysis/Engine/AnalysisModel.cpp
    src/Analysis/Engine/AnalysisEngine.h
    src/Analysis/Engine/AnalysisEngineRegistry.h
    src/Analysis/Engine/AnalysisEngineRegistry.cpp
    src/Analysis/Engine/AnalysisManager.h
    src/Analysis/Engine/AnalysisManager.cpp
    src/Analysis/Engines/BuiltInEngines.cpp
    src/Analysis/Engines/OpenSees/OpenSeesEngine.h
    src/Analysis/Engines/OpenSees/OpenSeesEngine.cpp
    src/Analysis/Engines/Custom2D/Custom2DSolver.h
    src/Analysis/Engines/Custom2D/Custom2DAdapter.h
    src/Analysis/Engines/Custom2D/Custom2DAdapter.cpp
    src/Analysis/Engines/Custom2D/Custom2DEngine.h
    src/Analysis/Engines/Custom2D/Custom2DEngine.cpp
    src/Analysis/Engines/Custom2D/MetDeDeplacementSolver.h
    src/Analysis/Engines/Custom2D/MetDeDeplacementSolver.cpp
)

# --- IA Co-Engineering (matériel, modèles, fournisseurs, contexte, outils, RAG) — [CORE] ---
set(AI_CORE_SOURCES
    src/AI/Hardware/HardwareProfiler.h
    src/AI/Hardware/HardwareProfiler.cpp
    src/AI/Models/ModelRegistry.h
    src/AI/Models/ModelRegistry.cpp
    src/AI/Models/ModelManager.h
    src/AI/Models/ModelManager.cpp
    src/AI/Providers/AIProvider.h
    src/AI/Providers/OpenAICompatibleProvider.h
    src/AI/Providers/OpenAICompatibleProvider.cpp
    src/AI/Providers/LocalLlamaServer.h
    src/AI/Providers/LocalLlamaServer.cpp
    src/AI/Providers/ProcessLifetime.h
    src/AI/Providers/ProcessLifetime.cpp
    src/AI/Checker/StructuralChecker.h
    src/AI/Checker/StructuralChecker.cpp
    src/AI/Context/EngineeringContext.h
    src/AI/Context/EngineeringContext.cpp
    src/AI/Tools/AIToolRegistry.h
    src/AI/Tools/AIToolRegistry.cpp
    src/AI/RAG/EngineeringKnowledgeBase.h
    src/AI/RAG/EngineeringKnowledgeBase.cpp
    src/AI/Core/AISettings.h
    src/AI/Core/AISettings.cpp
    src/AI/Core/SecretStore.cpp
    src/AI/Core/AILog.h
    src/AI/Core/AILog.cpp
    src/AI/Core/AIOrchestrator.h
    src/AI/Core/AIOrchestrator.cpp
)

# --- Note de Calcul (NDC) — [CORE] ---------------------------------------------
set(NDC_CORE_SOURCES
    src/NDC/NDCDocumentModel.h
    src/NDC/NDCDocumentModel.cpp
    src/NDC/ReportConfiguration.h
    src/NDC/ReportConfiguration.cpp
    src/NDC/ReportTemplate.h
    src/NDC/ReportTemplate.cpp
    src/NDC/NormativeReferenceDetector.h
    src/NDC/NormativeReferenceDetector.cpp
    src/NDC/ResultAnalyzer.h
    src/NDC/ResultAnalyzer.cpp
    src/NDC/NDCGenerator.h
    src/NDC/NDCGenerator.cpp
    src/NDC/NDCExporter.h
    src/NDC/NDCExporter.cpp
    src/NDC/ReportManager.h
    src/NDC/ReportManager.cpp
    src/NDC/NDCPlanarCurves.h
    src/NDC/NDCPlanarCurves.cpp
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
    src/Project/RecentProjects.h
    src/Project/RecentProjects.cpp
    src/Project/ModelPreviewCache.h
    src/Project/ModelPreviewCache.cpp
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
    src/UndoRedo/EditRecord.h
    src/UndoRedo/EditTransaction.h
    src/UndoRedo/CommandManager.h
    src/UndoRedo/CommandManager.cpp
)

# --- Gestion des interactions utilisateur — [CORE] ---------------------------
set(INTERACTION_SOURCES
    src/Interaction/InteractionManager.h
    src/Interaction/InteractionManager.cpp
    src/Interaction/Tools/ModelingTool.h
    src/Interaction/Tools/ModelingTool.cpp
    src/Interaction/Tools/ModifyTools.h
    src/Interaction/Tools/ModifyTools.cpp
    src/Interaction/Tools/DrawTools.cpp
    src/Model/ModelElementCopy.h
)

# --- Format de fichier TSA (sauvegarde / chargement) — [CORE] ----------------
set(IO_SOURCES
    src/IO/TSAFileFormat.h
    src/IO/TSAFile_BinaryUtils.h
    src/IO/TSAFile.h
    src/IO/TSAFile.cpp
    src/IO/TSAFileWriter_Chunks.cpp
    src/IO/TSAFileReader_Chunks.cpp
    src/IO/TSAPreviewGenerator.h
    src/IO/TSAPreviewGenerator.cpp
)

# --- Système de traçabilité des exigences et normes internationales — [CORE] ---
set(STANDARDS_SOURCES
    src/Standards/NormativeTypes.h
    src/Standards/NormativeTypes.cpp
    src/Standards/RequirementsCatalog.h
    src/Standards/RequirementsCatalog.cpp
    src/Standards/NationalAnnexConfig.h
    src/Standards/NationalAnnexConfig.cpp
    src/Standards/ExternalLibraryCatalog.h
    src/Standards/ExternalLibraryCatalog.cpp
    src/Standards/DataDefinition.h
    src/Standards/ModelValidator.h
    src/Standards/ModelValidator.cpp
    src/Standards/AnalyticalBenchmark.h
    src/Standards/AnalyticalBenchmark.cpp
    src/Standards/Design/ConcreteDesignEC2.h
    src/Standards/Design/ConcreteDesignEC2.cpp
    src/Standards/Design/SteelDesignEC3.h
    src/Standards/Design/SteelDesignEC3.cpp
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
    ${UI_ANALYSIS_SOURCES}
    ${UI_TOOLS_SOURCES}
    ${UI_WINDOWMANAGER_SOURCES}
    ${EXTENSIONSYSTEM_SOURCES}
    ${LIBRARY_SOURCES}
    ${COORDINATE_SOURCES}
    ${GRID_CORE_SOURCES}
    ${VIEWER_CORE_SOURCES}
    ${GEOMETRY_CORE_SOURCES}
    ${MODEL_CORE_SOURCES}
    ${LOAD_CORE_SOURCES}
    ${ANALYSIS_ENGINE_SOURCES}
    ${NDC_CORE_SOURCES}
    ${AI_CORE_SOURCES}
    ${CABLE_SOURCES}
    ${CABLE_GEOMETRY_SOURCES}
    ${PROJECT_SOURCES}
    ${COMMANDS_SOURCES}
    ${UNDOREDO_SOURCES}
    ${INTERACTION_SOURCES}
    ${IO_SOURCES}
    ${STANDARDS_SOURCES}
)

# --- Liste complète des sources de l'exécutable principal --------------------
# --- IA Co-Engineering : interface (panneau, configuration) ---
set(UI_AI_SOURCES
    src/UI/AI/AICoEngineeringDock.h
    src/UI/AI/AICoEngineeringDock.cpp
    src/UI/AI/AIRuntimeDialog.h
    src/UI/AI/AIRuntimeDialog.cpp
    src/UI/MainWindow_AI.cpp
    src/UI/MainWindow_Preview.cpp
    src/UI/Home/StartCenter.h
    src/UI/Home/StartCenter.cpp
    src/UI/Home/NewProjectDialog.h
    src/UI/Home/NewProjectDialog.cpp
    src/UI/Shell/AppShell.h
    src/UI/Shell/AppShell.cpp
    src/UI/Shell/TitleBar.h
    src/UI/Shell/TitleBar.cpp
)

set(SOURCES
    ${UI_AI_SOURCES}
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
)

# =============================================================================
# Options de build
# =============================================================================
option(TSA_BUILD_TESTS "Compiler la suite de tests TSA_Tests" ON)

# Dépendances d'en-têtes Ninja + MSVC : Ninja reconnaît les lignes /showIncludes grâce au préfixe
# détecté à la configuration. Avec un MSVC localisé (« Remarque : inclusion du fichier : »), ce
# préfixe contient une espace insécable dont l'encodage dépend de la page de code de la console
# (0xFF en CP850, C2 A0 en UTF-8/65001). Si la configuration et le build ne tournent pas sous la
# même page de code, AUCUNE dépendance d'en-tête n'est enregistrée : modifier un .h ne recompile
# pas les .cpp qui l'incluent, d'où des objets périmés (dispositions mémoire incohérentes, crashs
# aléatoires). Remède durable : pack de langue anglais de MSVC + VSLANG=1033 (presets ninja-*).
if(CMAKE_GENERATOR MATCHES "Ninja" AND MSVC AND CMAKE_CXX_CL_SHOWINCLUDES_PREFIX MATCHES "[^	 -~]")
    message(WARNING
        "Préfixe /showIncludes localisé détecté (non ASCII) : « ${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX} ».
"
        "Lancez la configuration ET les builds depuis des consoles de MÊME page de code (chcp), sinon "
        "Ninja n'enregistre aucune dépendance d'en-tête (vérification : ninja -t deps <fichier.obj>). "
        "Solution pérenne : installer le pack de langue anglais de Visual Studio (VSLANG=1033).")
endif()

# ccache (optionnel, ignoré avec les générateurs Visual Studio).
# Avec MSVC, ccache exige des infos de debug embarquées (/Z7) et son support
# des en-têtes précompilés est limité : à activer seulement si vous le testez.
option(TSA_USE_CCACHE "Utiliser ccache comme lanceur de compilation (Ninja)" OFF)
if(TSA_USE_CCACHE AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        if(MSVC)
            set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
            cmake_policy(SET CMP0141 NEW)
        endif()
        message(STATUS "ccache activé : ${CCACHE_PROGRAM}")
    endif()
endif()

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

# Options de compilation communes (Core, exécutable et tests).
# IMPORTANT : elles doivent être identiques pour pouvoir partager le PCH.
function(tsa_apply_common_settings target)
    # SYSTEM : les en-têtes OCCT sont traités comme "externes" (plus rapide,
    # et supprime les milliers d'avertissements /W4 venant d'OCCT).
    target_include_directories(${target} PRIVATE src)
    target_include_directories(${target} SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR})
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
# TSA_Core : logique "métier", compilée UNE SEULE FOIS
# -----------------------------------------------------------------------------
# Avant, CORE_SOURCES était compilé deux fois (exécutable + tests).
# Bibliothèque OBJECT : les .obj sont réutilisés tels quels par les deux cibles
# (pas de risque de perdre des initialisations statiques comme avec une .lib).
# =============================================================================
# Moteur 2D « méthode des déplacements » (bibliothèque autonome du même auteur)
add_subdirectory(thirdparty/MetDeDeplacement)

add_library(TSA_Core OBJECT ${CORE_SOURCES})
tsa_apply_common_settings(TSA_Core)
target_link_libraries(TSA_Core PUBLIC
    MetDeDeplacement
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    ${OCCT_LIBS}
)

# En-têtes précompilés : Qt et OCCT sont très lourds à parser.
# Les autres cibles réutilisent ce PCH (REUSE_FROM).
target_precompile_headers(TSA_Core PRIVATE
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

if(WIN32)
    target_link_libraries(TSA_Core PUBLIC DbgHelp Crypt32)
endif()

# Fichiers qui utilisent directement l'API Win32 (MessageBox, shell, dbghelp...) :
# ils incluent eux-mêmes <windows.h> avec leurs propres réglages, on les exclut
# donc du PCH (dont <windows.h> est déjà inclus via Qt/OCCT avec d'autres macros).
set_source_files_properties(
    src/Diagnostics/CrashHandler.cpp
    src/Platform/WindowsAssociation.cpp
    src/AI/Hardware/HardwareProfiler.cpp
    src/AI/Core/SecretStore.cpp
    src/AI/Providers/ProcessLifetime.cpp
    src/UI/Shell/AppShell.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# =============================================================================
# Exécutable principal
# =============================================================================
add_executable(${PROJECT_NAME} ${SOURCES})
tsa_apply_common_settings(${PROJECT_NAME})
target_precompile_headers(${PROJECT_NAME} REUSE_FROM TSA_Core)

target_link_libraries(${PROJECT_NAME} PRIVATE
    TSA_Core
    Qt6::Svg
)
if(WIN32)
    # AppShell : barre de titre personnalisée (ombre DWM de la fenêtre sans cadre système)
    target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi)
endif()

# Propriétés spécifiques MSVC / Windows
if(MSVC)
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
if(TSA_BUILD_TESTS)
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
    tests/test_window_manager.cpp
    tests/test_node_system.cpp
    tests/test_loads.cpp
    tests/test_opensees.cpp
    tests/test_supports.cpp
    tests/test_standards.cpp
    tests/test_ndc_report.cpp
    tests/test_opensees_extraction.cpp
    tests/test_ai.cpp
    tests/test_preview.cpp
    tests/test_thumbnail_provider.cpp
    tests/test_analysis_engines.cpp
    tests/test_modeling_tools.cpp
    tests/test_met_de_deplacement.cpp
    tests/test_model_cleanup.cpp
    tests/test_bim.cpp
    tests/test_snap.cpp
    resources/resources.qrc
)

add_executable(TSA_Tests ${TEST_SOURCES})
tsa_apply_common_settings(TSA_Tests)
target_include_directories(TSA_Tests PRIVATE tests)
target_precompile_headers(TSA_Tests REUSE_FROM TSA_Core)
target_link_libraries(TSA_Tests PRIVATE TSA_Core)
set_target_properties(TSA_Tests PROPERTIES OUTPUT_NAME "TSA_TestSuite")

# =============================================================================
# Extension Explorateur Windows : miniatures des fichiers .tsa
# -----------------------------------------------------------------------------
# DLL autonome chargée par l'Explorateur : ni Qt, ni OpenCASCADE, runtime C statique (aucune
# dépendance à installer). Placée à côté de TSA.exe, enregistrée par TSA au lancement (HKCU) ou par
# l'installeur (regsvr32 /i:machine). Voir docs/THUMBNAIL_PROVIDER.md.
# =============================================================================
if(WIN32)
    add_library(TSAThumbnailProvider SHARED
        src/ShellExtension/TSAThumbnailProvider.cpp
        src/ShellExtension/TSAThumbnailProvider.def
        src/IO/TSAPreviewBlock.h
        src/IO/TSAFileFormat.h
    )
    target_compile_features(TSAThumbnailProvider PRIVATE cxx_std_20)
    target_compile_definitions(TSAThumbnailProvider PRIVATE UNICODE _UNICODE)
    set_target_properties(TSAThumbnailProvider PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(TSAThumbnailProvider PRIVATE windowscodecs shlwapi ole32 shell32 advapi32)
    add_dependencies(${PROJECT_NAME} TSAThumbnailProvider)
    add_dependencies(TSA_Tests TSAThumbnailProvider)
    target_link_libraries(TSA_Tests PRIVATE shlwapi ole32)
    set_source_files_properties(tests/test_thumbnail_provider.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

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
add_test(NAME TSA_WindowManagerTests COMMAND TSA_Tests --suite=window)
add_test(NAME TSA_NodeSystemTests COMMAND TSA_Tests --suite=node)
add_test(NAME TSA_LoadsTests COMMAND TSA_Tests --suite=loads)
add_test(NAME TSA_OpenSeesTests COMMAND TSA_Tests --suite=opensees)
add_test(NAME TSA_SupportsTests COMMAND TSA_Tests --suite=supports)
add_test(NAME TSA_StandardsTests COMMAND TSA_Tests --suite=standards)
endif()

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