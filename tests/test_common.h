#pragma once

#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <memory>
#include <algorithm>

// OpenCASCADE
#include <gp_Circ.hxx>
#include <gp_Trsf.hxx>
#include <gp_Ax3.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Surface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <Graphic3d_Camera.hxx>
#include <AIS_Shape.hxx>

// Qt
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

// TSA Core & Modules
#include "Coordinate/Point3D.h"
#include "Coordinate/LevelManager.h"
#include "Coordinate/CoordinateSystem.h"
#include "Coordinate/WorkPlane.h"
#include "Coordinate/WorkPlaneManager.h"
#include "Coordinate/CoordinateTransformationService.h"

#include "Model/Model.h"
#include "Model/SelectionQuery.h"
#include "Coordinate/GeometryTolerance.h"
#include "Analysis/ResultsModel.h"
#include "Analysis/ResultsValidityGuard.h"
#include "Model/ModelDiff.h"
#include "Model/Material.h"
#include "Model/MaterialLibrary.h"
#include "Model/Section.h"
#include "Model/Wall.h"
#include "Model/Foundation.h"
#include "Model/TrussMember.h"
#include "Model/StructuralClipboard.h"

#include "Model/Cable/CableTypes.h"
#include "Model/Cable/CableStandards.h"
#include "Model/Cable/CableAnchor.h"
#include "Model/Cable/CablePrestress.h"
#include "Model/Cable/CableAnalysisProperties.h"
#include "Model/Cable/CableDefinition.h"
#include "Model/Cable/CableGeometry.h"
#include "Model/Cable/Cable.h"
#include "Model/Cable/StayCable.h"
#include "Model/Cable/SuspensionSystem.h"

#include "Geometry/CableGeometry3D.h"
#include "Geometry/BeamGeometry.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

#include "Grid/CableGrid.h"
#include "Grid/CartesianGrid.h"
#include "Grid/CylindricalGrid.h"
#include "Grid/ArbitraryGrid.h"
#include "Grid/GridDefinition.h"
#include "Grid/GridSystem.h"
#include "Grid/GridManager.h"
#include "Grid/GridSnapManager.h"

#include "Library/CableLibrary.h"
#include "Library/LibraryManager.h"

#include "IO/TSAFile.h"
#include "IO/TSAFileFormat.h"
#include "IO/TSAPreviewGenerator.h"

#include "Project/ProjectManager.h"

#include "Commands/ICommand.h"
#include "Commands/CommandCategory.h"
#include "Commands/CommandCatalog.h"
#include "Commands/CreateElementCommands.h"
#include "Commands/CreateBeamCommand.h"
#include "Commands/GridCommands.h"
#include "Commands/ModifyCommands.h"

#include "UndoRedo/UndoManager.h"
#include "UndoRedo/CommandManager.h"
#include "UndoRedo/EditTransaction.h"

#include "Interaction/InteractionManager.h"

#include "Diagnostics/LogLevel.h"
#include "Diagnostics/LogEntry.h"
#include "Diagnostics/RingBuffer.h"
#include "Diagnostics/Logger.h"
#include "Diagnostics/CrashHandler.h"
#include "Diagnostics/DiagnosticReport.h"

#include "ExtensionSystem/ExtensionTypes.h"
#include "ExtensionSystem/DefinitionModels.h"
#include "ExtensionSystem/LibraryRegistry.h"
#include "ExtensionSystem/LibraryValidator.h"
#include "ExtensionSystem/LibraryLoader.h"
#include "ExtensionSystem/LibraryCache.h"
#include "ExtensionSystem/LibraryVersionManager.h"
#include "ExtensionSystem/LibraryDependencyManager.h"
#include "ExtensionSystem/LibraryManager.h"
#include "ExtensionSystem/ExtensionManager.h"
#include "ExtensionSystem/ExtensionPackager.h"

#include "Viewer/MaterialVisual.h"
#include "Viewer/TextureManager.h"

#include "UI/Dialogs/ExtensionManagerDialog.h"

using namespace TSA::Coordinate;
using namespace TSA::Model;
using namespace TSA::Grid;
using namespace TSA::Geometry;
using namespace TSA::IO;
using namespace TSA::Viewer;

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (" << #cond << ") at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

inline bool approxEqual(double a, double b, double eps = 1e-4)
{
    return std::abs(a - b) <= eps;
}

// Declarations of individual test suite runners
bool runSuite_Coordinates(int& passed);
bool runSuite_Model(int& passed);
bool runSuite_FileIO(int& passed);
bool runSuite_Commands(int& passed);
bool runSuite_Grids(int& passed);
bool runSuite_Viewer(int& passed);
bool runSuite_Cables(int& passed);
bool runSuite_Extensions(int& passed);
bool runSuite_WorkPlane(int& passed);
bool runSuite_WindowManager(int& passed);
bool runSuite_NodeSystem(int& passed);
bool runSuite_Loads(int& passed);
bool runSuite_OpenSees(int& passed);
bool runSuite_Supports(int& passed);
bool runSuite_Standards(int& passed);
bool runSuite_NDCReport(int& passed);
bool runSuite_Extraction(int& passed);
bool runSuite_AI(int& passed);
bool runSuite_Preview(int& passed);
bool runSuite_Thumbnail(int& passed);
bool runSuite_Engines(int& passed);
bool runSuite_ModelingTools(int& passed);
bool runSuite_MetDeDeplacement(int& passed);
bool runSuite_ModelCleanup(int& passed);
bool runSuite_Bim(int& passed);
bool runSuite_Snap(int& passed);
bool runSuite_Automation(int& passed);
bool runSuite_Blueprint(int& passed);
bool runSuite_Help(int& passed);
bool runSuite_Appearance(int& passed);
bool runSuite_GroupEdit(int& passed);
