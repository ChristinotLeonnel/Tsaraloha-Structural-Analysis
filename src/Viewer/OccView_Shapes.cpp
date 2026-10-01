#include "OccView.h"
#include "SelectionManager.h"
#include "ResultsVisualManager.h"
#include "MaterialVisual.h"
#include "TextureManager.h"
#include "../Model/Model.h"
#include "../Model/ModelDiff.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/Slab.h"
#include "../Model/Wall.h"
#include "../Model/Foundation.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Geometry/BeamGeometry.h"
#include "../Geometry/SlabGeometry.h"
#include "../Geometry/WallGeometry.h"
#include "../Geometry/FoundationGeometry.h"
#include "../Geometry/CableGeometry3D.h"
#include "../Geometry/SupportGeometry.h"

#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <Font_FontAspect.hxx>
#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>
#include <Graphic3d_NameOfMaterial.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Quantity_Color.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <gp_Pnt.hxx>
#include <QColor>
#include "../Model/Load/LoadManager.h"
#include "../Analysis/LoadResolver.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <algorithm>
#include <cmath>

namespace
{
static bool parseHexColor(const std::string& hex, Quantity_Color& outColor)
{
    if (hex.empty())
        return false;
    QColor qc(QString::fromStdString(hex));
    if (!qc.isValid())
        return false;
    outColor = Quantity_Color(qc.redF(), qc.greenF(), qc.blueF(), Quantity_TOC_sRGB);
    return true;
}

static TopoDS_Shape makeArrowShape(const gp_Pnt& targetPnt, const gp_Vec& dir, double length, double shaftRadius, double headRadius, double headLength)
{
    if (dir.Magnitude() < 1e-6 || length <= headLength) return TopoDS_Shape();
    gp_Dir d(dir);
    gp_Pnt basePnt = targetPnt.Translated(-gp_Vec(d) * length);
    gp_Ax2 cylAxes(basePnt, d);
    BRepPrimAPI_MakeCylinder cyl(cylAxes, shaftRadius, length - headLength);

    gp_Pnt headBase = basePnt.Translated(gp_Vec(d) * (length - headLength));
    gp_Ax2 coneAxes(headBase, d);
    BRepPrimAPI_MakeCone cone(coneAxes, headRadius, 0.0, headLength);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    bb.Add(comp, cyl.Shape());
    bb.Add(comp, cone.Shape());
    return comp;
}

// [CONVENTION]
// Force vector is defined in the selected coordinate system.
// Positive/negative sign determines the vector direction.
// Do not invert the vector in the renderer.
//
// [TRACEABILITY]
// ChargeModel -> CoordinateSystem -> ForceVector -> Viewer
//
// Origin of the arrow is at originPnt (application point / node).
// The arrow extends from originPnt along dir over the given length.
static TopoDS_Shape makeLoadArrowShape(const gp_Pnt& originPnt, const gp_Vec& dir,
                                       double length, double shaftRadius, double headRadius, double headLength)
{
    if (dir.Magnitude() < 1e-6 || length <= headLength) return TopoDS_Shape();
    gp_Dir d(dir);
    gp_Ax2 cylAxes(originPnt, d);
    BRepPrimAPI_MakeCylinder cyl(cylAxes, shaftRadius, length - headLength);

    gp_Pnt headBase = originPnt.Translated(gp_Vec(d) * (length - headLength));
    gp_Ax2 coneAxes(headBase, d);
    BRepPrimAPI_MakeCone cone(coneAxes, headRadius, 0.0, headLength);

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);
    bb.Add(comp, cyl.Shape());
    bb.Add(comp, cone.Shape());
    return comp;
}

// [CONVENTION]
// Moment direction follows the right-hand rule around the corresponding coordinate axis.
//
// Mx > 0 : positive rotation around X (anti-clockwise looking from +X towards origin)
// Mx < 0 : opposite rotation
//
// [TRACEABILITY]
// NodalLoad::moments -> CoordinateSystem -> RotationAxis -> Right-Hand 3D Circular Arc -> Viewer
static TopoDS_Shape makeMomentShape(const gp_Pnt& centerPnt, const gp_Vec& momentVec,
                                    double radius, double tubeRadius, double headRadius, double headLength)
{
    double mag = momentVec.Magnitude();
    if (mag < 1e-6 || radius <= headLength) return TopoDS_Shape();

    gp_Dir axisDir(momentVec);

    // 1. Déterminer une base orthonormée directe (u, v, axisDir)
    gp_Vec refVec(0.0, 0.0, 1.0);
    if (std::abs(axisDir.Z()) > 0.85)
    {
        refVec = gp_Vec(1.0, 0.0, 0.0);
    }
    gp_Vec uVec = refVec.Crossed(gp_Vec(axisDir));
    if (uVec.SquareMagnitude() < 1e-6)
    {
        refVec = gp_Vec(0.0, 1.0, 0.0);
        uVec = refVec.Crossed(gp_Vec(axisDir));
    }
    uVec.Normalize();
    gp_Vec vVec = gp_Vec(axisDir).Crossed(uVec);
    vVec.Normalize();

    // 2. Arc de tore discrétisé : angle total de 270 degrés (1.5 * pi)
    // Parcouru dans le sens positif (règle de la main droite autour de axisDir)
    const int numSegments = 16;
    const double totalAngle = 1.5 * 3.14159265358979323846;
    const double dTheta = totalAngle / numSegments;

    BRep_Builder bb;
    TopoDS_Compound comp;
    bb.MakeCompound(comp);

    gp_Pnt prevPnt;
    for (int i = 0; i <= numSegments; ++i)
    {
        double theta = i * dTheta;
        gp_Vec radialVec = uVec * std::cos(theta) + vVec * std::sin(theta);
        gp_Pnt curPnt = centerPnt.Translated(radialVec * radius);

        gp_Ax2 sphAx(curPnt, axisDir);
        BRepPrimAPI_MakeSphere sph(sphAx, tubeRadius);
        bb.Add(comp, sph.Shape());

        if (i > 0)
        {
            gp_Vec segVec(prevPnt, curPnt);
            double segLen = segVec.Magnitude();
            if (segLen > 1e-5)
            {
                gp_Ax2 segAx(prevPnt, gp_Dir(segVec));
                BRepPrimAPI_MakeCylinder cyl(segAx, tubeRadius, segLen);
                bb.Add(comp, cyl.Shape());
            }
        }
        prevPnt = curPnt;
    }

    // 3. Tête de flèche à l'extrémité de l'arc (i = numSegments)
    // Vecteur tangent unitaire T = d(radialVec)/dTheta = -sin(theta)*u + cos(theta)*v
    double endTheta = totalAngle;
    gp_Vec tangent = -uVec * std::sin(endTheta) + vVec * std::cos(endTheta);
    tangent.Normalize();

    gp_Ax2 coneAx(prevPnt, gp_Dir(tangent));
    BRepPrimAPI_MakeCone cone(coneAx, headRadius, 0.0, headLength);
    bb.Add(comp, cone.Shape());

    return comp;
}
} // namespace

void OccView::onNodeAdded(const TSA::Model::Node& node)
{
    updateNodeShape(node.id());
}

void OccView::onNodeModified(const TSA::Model::Node& node)
{
    updateNodeShape(node.id());
}

void OccView::onNodeRemoved(int nodeId)
{
    removeNodeShape(nodeId);
}

void OccView::onBeamAdded(const TSA::Model::Beam& beam)
{
    updateBeamShape(beam.id());
}

void OccView::onBeamModified(const TSA::Model::Beam& beam)
{
    updateBeamShape(beam.id());
}

void OccView::onBeamRemoved(int beamId)
{
    removeBeamShape(beamId);
}

void OccView::onColumnAdded(const TSA::Model::Column& column)
{
    updateColumnShape(column.id());
}

void OccView::onColumnModified(const TSA::Model::Column& column)
{
    updateColumnShape(column.id());
}

void OccView::onColumnRemoved(int columnId)
{
    removeColumnShape(columnId);
}

void OccView::onSlabAdded(const TSA::Model::Slab& slab)
{
    updateSlabShape(slab.id());
}

void OccView::onSlabModified(const TSA::Model::Slab& slab)
{
    updateSlabShape(slab.id());
}

void OccView::onSlabRemoved(int slabId)
{
    removeSlabShape(slabId);
}

void OccView::onWallAdded(const TSA::Model::Wall& wall)
{
    updateWallShape(wall.id());
}

void OccView::onWallModified(const TSA::Model::Wall& wall)
{
    updateWallShape(wall.id());
}

void OccView::onWallRemoved(int wallId)
{
    removeWallShape(wallId);
}

void OccView::onFoundationAdded(const TSA::Model::Foundation& foundation)
{
    updateFoundationShape(foundation.id());
}

void OccView::onFoundationModified(const TSA::Model::Foundation& foundation)
{
    updateFoundationShape(foundation.id());
}

void OccView::onFoundationRemoved(int foundationId)
{
    removeFoundationShape(foundationId);
}

void OccView::onTrussMemberAdded(const TSA::Model::TrussMember& member)
{
    updateTrussMemberShape(member.id());
}

void OccView::onTrussMemberModified(const TSA::Model::TrussMember& member)
{
    updateTrussMemberShape(member.id());
}

void OccView::onTrussMemberRemoved(int memberId)
{
    removeTrussMemberShape(memberId);
}

void OccView::onCableAdded(const TSA::Model::Cable& cable)
{
    updateCableShape(cable.id());
}

void OccView::onCableModified(const TSA::Model::Cable& cable)
{
    updateCableShape(cable.id());
}

void OccView::onCableRemoved(int cableId)
{
    removeCableShape(cableId);
}

void OccView::onNodalLoadAdded(int loadId)
{
    updateNodalLoadShape(loadId);
}

void OccView::onNodalLoadModified(int loadId)
{
    updateNodalLoadShape(loadId);
}

void OccView::onNodalLoadRemoved(int loadId)
{
    removeNodalLoadShape(loadId);
}

void OccView::onMemberLoadAdded(int loadId)
{
    updateMemberLoadShape(loadId);
}

void OccView::onMemberLoadModified(int loadId)
{
    updateMemberLoadShape(loadId);
}

void OccView::onMemberLoadRemoved(int loadId)
{
    removeMemberLoadShape(loadId);
}

void OccView::onLoadAdded(int /*loadId*/)
{
    // Déjà traité de manière spécifique dans onNodalLoadAdded et onMemberLoadAdded
}

void OccView::onLoadModified(int /*loadId*/)
{
    // Déjà traité de manière spécifique dans onNodalLoadModified et onMemberLoadModified
}

void OccView::onLoadRemoved(int /*loadId*/)
{
    // Déjà traité de manière spécifique dans onNodalLoadRemoved et onMemberLoadRemoved
}

void OccView::onLoadCaseChanged(int /*caseId*/)
{
    updateAllLoadShapes();
}

void OccView::onModelDiffApplied(const TSA::Model::ModelDiff& diff)
{
    if (m_context.IsNull() || !m_model)
        return;

    // 1. Supprimer uniquement les objets supprimés (sans redraw intermédiaire)
    for (int id : diff.deletedNodeIds) removeNodeShape(id, false);
    for (int id : diff.deletedBeamIds) removeBeamShape(id, false);
    for (int id : diff.deletedColumnIds) removeColumnShape(id, false);
    for (int id : diff.deletedSlabIds) removeSlabShape(id, false);
    for (int id : diff.deletedWallIds) removeWallShape(id, false);
    for (int id : diff.deletedFoundationIds) removeFoundationShape(id, false);
    for (int id : diff.deletedTrussMemberIds) removeTrussMemberShape(id, false);
    for (int id : diff.deletedCableIds) removeCableShape(id, false);

    // 2. Mettre à jour uniquement les objets créés et modifiés (sans redraw intermédiaire)
    for (int id : diff.createdNodeIds) updateNodeShape(id, false);
    for (int id : diff.createdBeamIds) updateBeamShape(id, false);
    for (int id : diff.createdColumnIds) updateColumnShape(id, false);
    for (int id : diff.createdSlabIds) updateSlabShape(id, false);
    for (int id : diff.createdWallIds) updateWallShape(id, false);
    for (int id : diff.createdFoundationIds) updateFoundationShape(id, false);
    for (int id : diff.createdTrussMemberIds) updateTrussMemberShape(id, false);
    for (int id : diff.createdCableIds) updateCableShape(id, false);

    for (int id : diff.modifiedNodeIds) updateNodeShape(id, false);
    for (int id : diff.modifiedBeamIds) updateBeamShape(id, false);
    for (int id : diff.modifiedColumnIds) updateColumnShape(id, false);
    for (int id : diff.modifiedSlabIds) updateSlabShape(id, false);
    for (int id : diff.modifiedWallIds) updateWallShape(id, false);
    for (int id : diff.modifiedFoundationIds) updateFoundationShape(id, false);
    for (int id : diff.modifiedTrussMemberIds) updateTrussMemberShape(id, false);
    for (int id : diff.modifiedCableIds) updateCableShape(id, false);
    updateAllLoadShapes();

    // 3. Une SEULE passe d'actualisation de la vue graphique OCCT
    // AUCUN fitAll(), la caméra et le zoom sont rigoureusement préservés !
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::onModelCleared()
{
    if (m_resultsVisual)
    {
        m_resultsVisual->clearAllVisuals();
    }
    rebuildAllShapes();
}


void OccView::highlightNode(int nodeId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_nodeShapes.find(nodeId);
    if (it != m_nodeShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightBeam(int beamId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_beamShapes.find(beamId);
    if (it != m_beamShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightColumn(int columnId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_columnShapes.find(columnId);
    if (it != m_columnShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightSlab(int slabId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_slabShapes.find(slabId);
    if (it != m_slabShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightWall(int wallId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_wallShapes.find(wallId);
    if (it != m_wallShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightFoundation(int foundationId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_foundationShapes.find(foundationId);
    if (it != m_foundationShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightTrussMember(int memberId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_trussShapes.find(memberId);
    if (it != m_trussShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::highlightCable(int cableId)
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    auto it = m_cableShapes.find(cableId);
    if (it != m_cableShapes.end())
    {
        m_context->SetSelected(it->second, false);
    }
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::clearHighlight()
{
    if (m_context.IsNull())
        return;

    m_context->ClearSelected(false);
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::rebuildAllShapes()
{
    if (m_context.IsNull())
        return;

    // Nettoyer tous les objets existants
    clearLoadShapes();
    for (auto& [id, aisShape] : m_nodeShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_nodeShapes.clear();

    for (auto& [id, aisLbl] : m_nodeLabels)
    {
        m_context->Remove(aisLbl, false);
    }
    m_nodeLabels.clear();

    for (auto& [id, aisShape] : m_supportShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_supportShapes.clear();

    for (auto& [id, aisLbl] : m_supportLabels)
    {
        m_context->Remove(aisLbl, false);
    }
    m_supportLabels.clear();

    for (auto& [id, aisShape] : m_beamShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_beamShapes.clear();

    for (auto& [id, aisShape] : m_columnShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_columnShapes.clear();

    for (auto& [id, aisShape] : m_slabShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_slabShapes.clear();

    for (auto& [id, aisShape] : m_wallShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_wallShapes.clear();

    for (auto& [id, aisShape] : m_foundationShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_foundationShapes.clear();

    for (auto& [id, aisShape] : m_trussShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_trussShapes.clear();

    for (auto& [id, aisShape] : m_cableShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_cableShapes.clear();

    if (m_selectionManager)
    {
        m_selectionManager->clearRegistry();
    }

    if (!m_model)
        return;

    // 1. Créer les formes des nœuds
    for (const auto& [nodeId, node] : m_model->nodes())
    {
        updateNodeShape(nodeId);
    }

    // 2. Créer les formes des poutres
    for (const auto& [beamId, beam] : m_model->beams())
    {
        updateBeamShape(beamId);
    }

    // 3. Créer les formes des poteaux
    for (const auto& [columnId, col] : m_model->columns())
    {
        updateColumnShape(columnId);
    }

    // 4. Créer les formes des dalles
    for (const auto& [slabId, slab] : m_model->slabs())
    {
        updateSlabShape(slabId);
    }

    // 5. Créer les formes des voiles
    for (const auto& [wallId, wall] : m_model->walls())
    {
        updateWallShape(wallId);
    }

    // 6. Créer les formes des fondations
    for (const auto& [fId, f] : m_model->foundations())
    {
        updateFoundationShape(fId);
    }

    // 7. Créer les formes des treillis
    for (const auto& [trId, tr] : m_model->trussMembers())
    {
        updateTrussMemberShape(trId);
    }

    // 8. Créer les formes des câbles
    for (const auto& [cableId, cable] : m_model->cables())
    {
        updateCableShape(cableId, false);
    }

    // 9. Créer les formes des charges
    updateAllLoadShapes();

    m_context->UpdateCurrentViewer();
    fitAll();
}

void OccView::updateNodeShape(int nodeId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedNodes().count(nodeId) > 0;

    // 1. Supprimer l'ancienne forme et étiquette
    auto itLbl = m_nodeLabels.find(nodeId);
    if (itLbl != m_nodeLabels.end())
    {
        m_context->Remove(itLbl->second, false);
        m_nodeLabels.erase(itLbl);
    }

    auto it = m_nodeShapes.find(nodeId);
    if (it != m_nodeShapes.end())
    {
        m_context->Remove(it->second, false);
        m_nodeShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterNode(nodeId);
        }
    }

    const auto* node = m_model->getNode(nodeId);
    if (!node)
        return;

    // 2. Créer la nouvelle forme 3D (Sphère)
    bool isFree = m_model && m_model->isNodeFree(nodeId);
    double nodeRadius = isFree ? 0.16 : 0.12;
    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createNodeShape(*node, nodeRadius);
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisNode = new AIS_Shape(shape);
        Quantity_Color qc;
        if (isFree)
        {
            // Nœuds libres : mis en évidence en magenta pour signaler les instabilités
            aisNode->SetColor(Quantity_NOC_MAGENTA1);
        }
        else if (parseHexColor(node->color(), qc))
        {
            aisNode->SetColor(qc);
        }
        else
        {
            aisNode->SetColor(Quantity_NOC_GOLD);
        }
        aisNode->SetMaterial(Graphic3d_NOM_COPPER);
        aisNode->SetDisplayMode(AIS_Shaded);

        m_nodeShapes[nodeId] = aisNode;
        if (isNodeVisibleByFilter(nodeId))
        {
            m_context->Display(aisNode, false);
        }

        if (m_selectionManager)
        {
            m_selectionManager->registerNode(nodeId, aisNode);
            if (wasSelected)
            {
                m_selectionManager->selectNode(nodeId, true);
                if (isNodeVisibleByFilter(nodeId))
                {
                    m_context->SetSelected(aisNode, false);
                }
            }
        }
    }

    // 3. Créer l'étiquette 3D (AIS_TextLabel)
    Handle(AIS_TextLabel) aisLabel = new AIS_TextLabel();
    QString labelText = QString("N%1").arg(node->id());
    if (isFree)
    {
        labelText += " [LIBRE]";
    }
    else if (!node->name().empty() && node->name() != labelText.toStdString() && node->name() != node->formattedName())
    {
        labelText += QString(" (%1)").arg(QString::fromStdString(node->name()));
    }
    aisLabel->SetText(TCollection_ExtendedString(labelText.toUtf8().constData(), true));
    aisLabel->SetPosition(gp_Pnt(node->x(), node->y(), node->z() + 0.18));
    aisLabel->SetColor(isFree ? Quantity_Color(1.0, 0.2, 0.8, Quantity_TOC_RGB) : (m_isDarkMode ? Quantity_Color(0.2, 0.9, 0.9, Quantity_TOC_RGB) : Quantity_Color(0.0, 0.4, 0.6, Quantity_TOC_RGB)));
    aisLabel->SetHJustification(Graphic3d_HTA_CENTER);
    aisLabel->SetVJustification(Graphic3d_VTA_BOTTOM);
    aisLabel->SetHeight(13.0);
    aisLabel->SetFontAspect(Font_FA_Bold);

    m_context->Display(aisLabel, false);
    m_context->Deactivate(aisLabel);
    if (!m_nodeLabelsVisible || !isNodeVisibleByFilter(nodeId))
    {
        m_context->Erase(aisLabel, false);
    }
    m_nodeLabels[nodeId] = aisLabel;

    // 4. Mettre à jour l'appui 3D lié au nœud
    updateSupportShape(nodeId, false);

    // 3. Collecter les éléments connectés à ce nœud avant de les mettre à jour
    //    (uniquement si redrawImmediately est vrai, sinon c'est le diff global qui gère)
    if (redrawImmediately)
    {
        std::vector<int> connectedBeams;
        for (const auto& [beamId, beam] : m_model->beams())
        {
            if (beam.startNodeId() == nodeId || beam.endNodeId() == nodeId)
                connectedBeams.push_back(beamId);
        }

        std::vector<int> connectedCols;
        for (const auto& [colId, col] : m_model->columns())
        {
            if (col.startNodeId() == nodeId || col.endNodeId() == nodeId)
                connectedCols.push_back(colId);
        }

        std::vector<int> connectedSlabs;
        for (const auto& [slabId, slab] : m_model->slabs())
        {
            const auto& nids = slab.nodeIds();
            if (std::find(nids.begin(), nids.end(), nodeId) != nids.end())
                connectedSlabs.push_back(slabId);
        }

        std::vector<int> connectedWalls;
        for (const auto& [wallId, wall] : m_model->walls())
        {
            if (wall.startNodeId() == nodeId || wall.endNodeId() == nodeId)
                connectedWalls.push_back(wallId);
        }

        for (int bid : connectedBeams) updateBeamShape(bid, false);
        for (int cid : connectedCols)  updateColumnShape(cid, false);
        for (int sid : connectedSlabs) updateSlabShape(sid, false);
        for (int wid : connectedWalls) updateWallShape(wid, false);

        // 4. Actualiser immédiatement l'affichage 3D OpenCASCADE
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::setRenderDisplayMode(TSA::Viewer::RenderDisplayMode mode)
{
    m_renderDisplayMode = mode;
    if (m_context.IsNull() || !m_model) return;

    for (const auto& [id, shape] : m_beamShapes)
    {
        const auto* b = m_model->getBeam(id);
        if (b) TSA::Viewer::MaterialVisual::instance().applyToShape(shape, b->material(), b->color(), m_renderDisplayMode);
    }
    for (const auto& [id, shape] : m_columnShapes)
    {
        const auto* c = m_model->getColumn(id);
        if (c) TSA::Viewer::MaterialVisual::instance().applyToShape(shape, c->material(), c->color(), m_renderDisplayMode);
    }
    for (const auto& [id, shape] : m_slabShapes)
    {
        const auto* s = m_model->getSlab(id);
        if (s) TSA::Viewer::MaterialVisual::instance().applyToShape(shape, s->material(), s->color(), m_renderDisplayMode, 0.35);
    }
    for (const auto& [id, shape] : m_wallShapes)
    {
        const auto* w = m_model->getWall(id);
        if (w) TSA::Viewer::MaterialVisual::instance().applyToShape(shape, w->material(), w->color(), m_renderDisplayMode, 0.25);
    }
    for (const auto& [id, shape] : m_foundationShapes)
    {
        const auto* f = m_model->getFoundation(id);
        if (f) TSA::Viewer::MaterialVisual::instance().applyToShape(shape, f->material(), f->color(), m_renderDisplayMode);
    }
    for (const auto& [id, shape] : m_trussShapes)
    {
        const auto* tr = m_model->getTrussMember(id);
        if (tr) TSA::Viewer::MaterialVisual::instance().applyToShape(shape, tr->material(), tr->color(), m_renderDisplayMode);
    }
    for (const auto& [id, shape] : m_cableShapes)
    {
        const auto* c = m_model->getCable(id);
        if (c) TSA::Viewer::MaterialVisual::instance().applyToShape(shape, c->material(), c->color(), m_renderDisplayMode);
    }

    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull())
    {
        m_view->Redraw();
    }
}

void OccView::updateBeamShape(int beamId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedBeams().count(beamId) > 0;

    // 1. Supprimer l'ancienne forme (avant le contrôle de validité des nœuds, pour
    //    ne jamais laisser une forme fantôme affichée/sélectionnable si les nœuds
    //    référencés ne sont plus valides)
    auto it = m_beamShapes.find(beamId);
    if (it != m_beamShapes.end())
    {
        m_context->Remove(it->second, false);
        m_beamShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterBeam(beamId);
        }
    }

    const auto* beam = m_model->getBeam(beamId);
    if (!beam)
        return;

    const auto* nodeA = m_model->getNode(beam->startNodeId());
    const auto* nodeB = m_model->getNode(beam->endNodeId());
    if (!nodeA || !nodeB)
        return;

    // 2. Créer le nouveau solide 3D selon la forme réelle de la section et l'orientation
    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createBeamShape(
        *nodeA, *nodeB, beam->section(), beam->rotation(), beam->eccentricity()
    );

    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisBeam = new AIS_Shape(shape);
        TSA::Viewer::MaterialVisual::instance().applyToShape(aisBeam, beam->material(), beam->color(), m_renderDisplayMode);

        m_context->Display(aisBeam, false);
        m_beamShapes[beamId] = aisBeam;
        if (m_selectionManager)
        {
            m_selectionManager->registerBeam(beamId, aisBeam);
            if (wasSelected)
            {
                m_selectionManager->selectBeam(beamId, true);
                m_context->SetSelected(aisBeam, false);
            }
        }
    }

    // 3. Actualiser immédiatement l'affichage 3D si demandé
    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::updateColumnShape(int columnId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedColumns().count(columnId) > 0;

    auto it = m_columnShapes.find(columnId);
    if (it != m_columnShapes.end())
    {
        m_context->Remove(it->second, false);
        m_columnShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterColumn(columnId);
        }
    }

    const auto* col = m_model->getColumn(columnId);
    if (!col)
        return;

    const auto* nodeA = m_model->getNode(col->startNodeId());
    const auto* nodeB = m_model->getNode(col->endNodeId());
    if (!nodeA || !nodeB)
        return;

    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createBeamShape(
        *nodeA, *nodeB, col->section(), col->rotation()
    );

    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisCol = new AIS_Shape(shape);
        TSA::Viewer::MaterialVisual::instance().applyToShape(aisCol, col->material(), col->color(), m_renderDisplayMode);

        m_context->Display(aisCol, false);
        m_columnShapes[columnId] = aisCol;
        if (m_selectionManager)
        {
            m_selectionManager->registerColumn(columnId, aisCol);
            if (wasSelected)
            {
                m_selectionManager->selectColumn(columnId, true);
                m_context->SetSelected(aisCol, false);
            }
        }
    }

    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::updateSlabShape(int slabId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedSlabs().count(slabId) > 0;

    const auto* slab = m_model->getSlab(slabId);
    if (!slab)
        return;

    std::vector<const TSA::Model::Node*> contourNodes;
    for (int nid : slab->nodeIds())
    {
        const auto* n = m_model->getNode(nid);
        if (n)
        {
            contourNodes.push_back(n);
        }
    }

    auto it = m_slabShapes.find(slabId);
    if (it != m_slabShapes.end())
    {
        m_context->Remove(it->second, false);
        m_slabShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterSlab(slabId);
        }
    }

    if (contourNodes.size() < 3)
        return;

    TopoDS_Shape shape = TSA::Geometry::SlabGeometry::createSlabShape(contourNodes, slab->thickness());

    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisSlab = new AIS_Shape(shape);
        TSA::Viewer::MaterialVisual::instance().applyToShape(aisSlab, slab->material(), slab->color(), m_renderDisplayMode, 0.35);

        m_context->Display(aisSlab, false);
        m_slabShapes[slabId] = aisSlab;
        if (m_selectionManager)
        {
            m_selectionManager->registerSlab(slabId, aisSlab);
            if (wasSelected)
            {
                m_selectionManager->selectSlab(slabId, true);
                m_context->SetSelected(aisSlab, false);
            }
        }
    }

    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::updateWallShape(int wallId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedWalls().count(wallId) > 0;

    const auto* wall = m_model->getWall(wallId);
    if (!wall)
        return;

    auto it = m_wallShapes.find(wallId);
    if (it != m_wallShapes.end())
    {
        m_context->Remove(it->second, false);
        m_wallShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterWall(wallId);
        }
    }

    const auto* nodeA = m_model->getNode(wall->startNodeId());
    const auto* nodeB = m_model->getNode(wall->endNodeId());
    if (!nodeA || !nodeB)
        return;

    TopoDS_Shape shape = TSA::Geometry::WallGeometry::createWallShape(*nodeA, *nodeB, wall->height(), wall->thickness(), wall->offset());
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisWall = new AIS_Shape(shape);
        TSA::Viewer::MaterialVisual::instance().applyToShape(aisWall, wall->material(), wall->color(), m_renderDisplayMode, 0.25);

        m_context->Display(aisWall, false);
        m_wallShapes[wallId] = aisWall;
        if (m_selectionManager)
        {
            m_selectionManager->registerWall(wallId, aisWall);
            if (wasSelected)
            {
                m_selectionManager->selectWall(wallId, true);
                m_context->SetSelected(aisWall, false);
            }
        }
    }

    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::updateFoundationShape(int foundationId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedFoundations().count(foundationId) > 0;

    const auto* f = m_model->getFoundation(foundationId);
    if (!f)
        return;

    auto it = m_foundationShapes.find(foundationId);
    if (it != m_foundationShapes.end())
    {
        m_context->Remove(it->second, false);
        m_foundationShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterFoundation(foundationId);
        }
    }

    const auto* node = m_model->getNode(f->nodeId());
    if (!node)
        return;

    TopoDS_Shape shape = TSA::Geometry::FoundationGeometry::createFoundationShape(*node, f->widthA(), f->lengthB(), f->heightH());
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisF = new AIS_Shape(shape);
        TSA::Viewer::MaterialVisual::instance().applyToShape(aisF, f->material(), f->color(), m_renderDisplayMode);

        m_context->Display(aisF, false);
        m_foundationShapes[foundationId] = aisF;
        if (m_selectionManager)
        {
            m_selectionManager->registerFoundation(foundationId, aisF);
            if (wasSelected)
            {
                m_selectionManager->selectFoundation(foundationId, true);
                m_context->SetSelected(aisF, false);
            }
        }
    }

    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::updateTrussMemberShape(int memberId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedTrussMembers().count(memberId) > 0;

    const auto* tr = m_model->getTrussMember(memberId);
    if (!tr)
        return;

    auto it = m_trussShapes.find(memberId);
    if (it != m_trussShapes.end())
    {
        m_context->Remove(it->second, false);
        m_trussShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterTrussMember(memberId);
        }
    }

    const auto* nodeA = m_model->getNode(tr->startNodeId());
    const auto* nodeB = m_model->getNode(tr->endNodeId());
    if (!nodeA || !nodeB)
        return;

    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createBeamShape(
        *nodeA, *nodeB, tr->section(), 0.0
    );
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisTr = new AIS_Shape(shape);
        TSA::Viewer::MaterialVisual::instance().applyToShape(aisTr, tr->material(), tr->color(), m_renderDisplayMode);

        m_context->Display(aisTr, false);
        m_trussShapes[memberId] = aisTr;
        if (m_selectionManager)
        {
            m_selectionManager->registerTrussMember(memberId, aisTr);
            if (wasSelected)
            {
                m_selectionManager->selectTrussMember(memberId, true);
                m_context->SetSelected(aisTr, false);
            }
        }
    }

    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::updateCableShape(int cableId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedCables().count(cableId) > 0;

    const auto* cable = m_model->getCable(cableId);
    if (!cable)
        return;

    auto it = m_cableShapes.find(cableId);
    if (it != m_cableShapes.end())
    {
        m_context->Remove(it->second, false);
        m_cableShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterCable(cableId);
        }
    }

    TopoDS_Shape shape = TSA::Geometry::CableGeometry3D::createCableShape(*cable, *m_model, true);
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisCable = new AIS_Shape(shape);
        TSA::Viewer::MaterialVisual::instance().applyToShape(aisCable, cable->material(), cable->color(), m_renderDisplayMode);

        m_context->Display(aisCable, false);
        m_cableShapes[cableId] = aisCable;
        if (m_selectionManager)
        {
            m_selectionManager->registerCable(cableId, aisCable);
            if (wasSelected)
            {
                m_selectionManager->selectCable(cableId, true);
                m_context->SetSelected(aisCable, false);
            }
        }
    }

    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
        if (!m_view.IsNull())
        {
            m_view->ZFitAll();
            m_view->Redraw();
        }
    }
}

void OccView::removeNodeShape(int nodeId, bool redrawImmediately)
{
    auto itLbl = m_nodeLabels.find(nodeId);
    if (itLbl != m_nodeLabels.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(itLbl->second, false);
        }
        m_nodeLabels.erase(itLbl);
    }

    auto it = m_nodeShapes.find(nodeId);
    if (it != m_nodeShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_nodeShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterNode(nodeId);
        }
    }

    removeSupportShape(nodeId, false);

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeBeamShape(int beamId, bool redrawImmediately)
{
    auto it = m_beamShapes.find(beamId);
    if (it != m_beamShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_beamShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterBeam(beamId);
        }
    }

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeColumnShape(int columnId, bool redrawImmediately)
{
    auto it = m_columnShapes.find(columnId);
    if (it != m_columnShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_columnShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterColumn(columnId);
        }
    }

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeSlabShape(int slabId, bool redrawImmediately)
{
    auto it = m_slabShapes.find(slabId);
    if (it != m_slabShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_slabShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterSlab(slabId);
        }
    }

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeWallShape(int wallId, bool redrawImmediately)
{
    auto it = m_wallShapes.find(wallId);
    if (it != m_wallShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_wallShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterWall(wallId);
        }
    }

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeFoundationShape(int foundationId, bool redrawImmediately)
{
    auto it = m_foundationShapes.find(foundationId);
    if (it != m_foundationShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_foundationShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterFoundation(foundationId);
        }
    }

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeTrussMemberShape(int memberId, bool redrawImmediately)
{
    auto it = m_trussShapes.find(memberId);
    if (it != m_trussShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_trussShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterTrussMember(memberId);
        }
    }

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

void OccView::removeCableShape(int cableId, bool redrawImmediately)
{
    auto it = m_cableShapes.find(cableId);
    if (it != m_cableShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_cableShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterCable(cableId);
        }
    }

    if (redrawImmediately && !m_view.IsNull())
    {
        m_view->ZFitAll();
        m_view->Redraw();
    }
}

// =============================================================================
// Visualisation 3D des charges (Forces, Moments, Réparties)
// =============================================================================
void OccView::updateNodalLoadShape(int loadId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model) return;

    removeNodalLoadShape(loadId, false);

    const auto* nl = m_model->loadManager().getNodalLoad(loadId);
    if (!nl) return;

    const auto* node = m_model->getNode(nl->nodeId());
    if (!node) return;

    gp_Pnt p0(node->x(), node->y(), node->z());

    std::vector<Handle(AIS_Shape)> loadAisShapes;
    double scale = (m_loadScale > 0.05) ? m_loadScale : 1.0;

    // 1. Vecteur force
    // [CONVENTION]
    // Force vector is defined in the selected coordinate system.
    // Positive/negative sign determines the vector direction.
    // Do not invert the vector in the renderer.
    //
    // [TRACEABILITY]
    // NodalLoad::forces -> CoordinateSystem -> ForceVector -> Viewer
    gp_Vec fVec(nl->fx(), nl->fy(), nl->fz());
    double mag = fVec.Magnitude();

    if (mag > 1e-6 && m_forcesVisible)
    {
        gp_Dir fDir(fVec);
        double arrowLen = std::clamp((0.6 + mag * 0.02) * scale, 0.8 * scale, 2.5 * scale);
        double shaftR = 0.035 * scale;
        double headR = 0.09 * scale;
        double headL = 0.22 * scale;

        TopoDS_Shape arrow = makeLoadArrowShape(p0, fDir, arrowLen, shaftR, headR, headL);
        if (!arrow.IsNull())
        {
            Handle(AIS_Shape) aisLoad = new AIS_Shape(arrow);
            aisLoad->SetColor(Quantity_NOC_RED);
            aisLoad->SetMaterial(Graphic3d_NOM_PLASTIC);
            aisLoad->SetDisplayMode(AIS_Shaded);
            if (m_loadsVisible)
            {
                m_context->Display(aisLoad, false);
            }
            if (m_selectionManager)
            {
                m_selectionManager->registerNodalLoad(loadId, aisLoad);
            }
            loadAisShapes.push_back(aisLoad);
        }
    }

    // 2. Vecteur moment (Règle de la main droite)
    // [CONVENTION]
    // Moment direction follows the right-hand rule around the corresponding coordinate axis.
    // Mx > 0 : positive rotation around X
    // My > 0 : positive rotation around Y
    // Mz > 0 : positive rotation around Z
    //
    // [TRACEABILITY]
    // NodalLoad::moments -> CoordinateSystem -> RotationAxis -> Right-Hand 3D Circular Arc -> Viewer
    gp_Vec mVec(nl->mx(), nl->my(), nl->mz());
    double mMag = mVec.Magnitude();

    if (mMag > 1e-6 && m_momentsVisible)
    {
        double arcRadius = std::clamp((0.35 + mMag * 0.015) * scale, 0.35 * scale, 1.2 * scale);
        double tubeR = 0.025 * scale;
        double headR = 0.065 * scale;
        double headL = 0.16 * scale;

        TopoDS_Shape mMoment = makeMomentShape(p0, mVec, arcRadius, tubeR, headR, headL);
        if (!mMoment.IsNull())
        {
            Handle(AIS_Shape) aisMoment = new AIS_Shape(mMoment);
            // Couleur distincte pour le moment : Violet / Magenta
            aisMoment->SetColor(Quantity_Color(0.85, 0.35, 0.95, Quantity_TOC_RGB));
            aisMoment->SetMaterial(Graphic3d_NOM_PLASTIC);
            aisMoment->SetDisplayMode(AIS_Shaded);
            if (m_loadsVisible)
            {
                m_context->Display(aisMoment, false);
            }
            if (m_selectionManager)
            {
                m_selectionManager->registerNodalLoad(loadId, aisMoment);
            }
            loadAisShapes.push_back(aisMoment);
        }
    }

    if (!loadAisShapes.empty())
    {
        m_nodalLoadShapes[loadId] = loadAisShapes;
    }

    // 3. Texte de valeur
    Handle(AIS_TextLabel) aisLabel = new AIS_TextLabel();
    QString valTxt;
    if (mag > 1e-6)
    {
        valTxt = QString("F = %1 kN").arg(QString::number(mag, 'f', 1));
        if (std::abs(nl->fx()) > 1e-6) valTxt += QString(" (Fx=%1)").arg(QString::number(nl->fx(), 'f', 1));
        if (std::abs(nl->fy()) > 1e-6) valTxt += QString(" (Fy=%1)").arg(QString::number(nl->fy(), 'f', 1));
        if (std::abs(nl->fz()) > 1e-6) valTxt += QString(" (Fz=%1)").arg(QString::number(nl->fz(), 'f', 1));
    }
    if (mMag > 1e-6)
    {
        if (!valTxt.isEmpty()) valTxt += "\n";
        valTxt += QString("M = %1 kNm").arg(QString::number(mMag, 'f', 1));
        if (std::abs(nl->mx()) > 1e-6) valTxt += QString(" (Mx=%1)").arg(QString::number(nl->mx(), 'f', 1));
        if (std::abs(nl->my()) > 1e-6) valTxt += QString(" (My=%1)").arg(QString::number(nl->my(), 'f', 1));
        if (std::abs(nl->mz()) > 1e-6) valTxt += QString(" (Mz=%1)").arg(QString::number(nl->mz(), 'f', 1));
    }
    if (valTxt.isEmpty()) valTxt = QString("NL#%1").arg(loadId);

    aisLabel->SetText(TCollection_ExtendedString(valTxt.toUtf8().constData(), true));
    aisLabel->SetPosition(gp_Pnt(p0.X(), p0.Y(), p0.Z() + (0.35 * scale)));
    aisLabel->SetColor(m_isDarkMode ? Quantity_Color(1.0, 0.8, 0.2, Quantity_TOC_RGB) : Quantity_Color(0.8, 0.4, 0.0, Quantity_TOC_RGB));
    aisLabel->SetHJustification(Graphic3d_HTA_CENTER);
    aisLabel->SetVJustification(Graphic3d_VTA_BOTTOM);
    aisLabel->SetHeight(12.0);
    aisLabel->SetFontAspect(Font_FA_Bold);

    m_context->Display(aisLabel, false);
    m_context->Deactivate(aisLabel);
    if (!m_loadValuesVisible || !m_loadsVisible)
    {
        m_context->Erase(aisLabel, false);
    }
    m_nodalLoadLabels[loadId] = aisLabel;

    if (redrawImmediately && !m_view.IsNull())
    {
        m_context->UpdateCurrentViewer();
        m_view->Redraw();
    }
}

void OccView::removeNodalLoadShape(int loadId, bool redrawImmediately)
{
    auto itS = m_nodalLoadShapes.find(loadId);
    if (itS != m_nodalLoadShapes.end())
    {
        if (!m_context.IsNull())
        {
            for (auto& s : itS->second)
            {
                if (!s.IsNull()) m_context->Remove(s, false);
            }
        }
        m_nodalLoadShapes.erase(itS);
    }
    if (m_selectionManager)
    {
        m_selectionManager->unregisterNodalLoad(loadId);
    }
    auto itL = m_nodalLoadLabels.find(loadId);
    if (itL != m_nodalLoadLabels.end())
    {
        if (!m_context.IsNull()) m_context->Remove(itL->second, false);
        m_nodalLoadLabels.erase(itL);
    }
    if (redrawImmediately && !m_view.IsNull())
    {
        m_context->UpdateCurrentViewer();
        m_view->Redraw();
    }
}

void OccView::updateMemberLoadShape(int loadId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model) return;

    removeMemberLoadShape(loadId, false);

    const auto* ml = m_model->loadManager().getMemberLoad(loadId);
    if (!ml) return;

    int elemId = ml->elementId();
    int sNode = 0, eNode = 0;
    double rotDeg = 0.0;
    const auto* b = m_model->getBeam(elemId);
    if (b) { sNode = b->startNodeId(); eNode = b->endNodeId(); rotDeg = b->rotation(); }
    else
    {
        const auto* col = m_model->getColumn(elemId);
        if (col) { sNode = col->startNodeId(); eNode = col->endNodeId(); rotDeg = col->rotation(); }
        else
        {
            const auto* tr = m_model->getTrussMember(elemId);
            if (tr) { sNode = tr->startNodeId(); eNode = tr->endNodeId(); }
        }
    }

    const auto* n1 = m_model->getNode(sNode);
    const auto* n2 = m_model->getNode(eNode);
    if (!n1 || !n2) return;

    gp_Pnt p1(n1->x(), n1->y(), n1->z());
    gp_Pnt p2(n2->x(), n2->y(), n2->z());

    gp_Vec vAxis(p1, p2);
    double len = vAxis.Magnitude();
    if (len < 1e-4) return;

    // Direction de la charge
    // [CONVENTION]
    // Sign and coordinate system determine true physical orientation.
    // Negative q reverses the vector direction.
    //
    // [TRACEABILITY]
    // MemberLoad -> CoordinateSystem/LocalFrame -> DirectionVector -> Viewer
    gp_Vec dirVec(0.0, 0.0, -1.0);
    double qMag = ml->q1();
    double sign = (qMag < 0.0) ? -1.0 : 1.0;

    if (ml->direction() == TSA::Model::LoadDirection::GlobalX) dirVec = gp_Vec(1.0, 0.0, 0.0) * sign;
    else if (ml->direction() == TSA::Model::LoadDirection::GlobalY) dirVec = gp_Vec(0.0, 1.0, 0.0) * sign;
    else if (ml->direction() == TSA::Model::LoadDirection::GlobalZ) dirVec = gp_Vec(0.0, 0.0, 1.0) * sign;
    else if (ml->direction() == TSA::Model::LoadDirection::Gravity) dirVec = gp_Vec(0.0, 0.0, -1.0);
    else if (ml->direction() == TSA::Model::LoadDirection::LocalX)
    {
        gp_Ax3 frame = TSA::Analysis::LoadResolver::computeElementLocalAxes(p1, p2, rotDeg);
        dirVec = gp_Vec(frame.XDirection()) * sign;
    }
    else if (ml->direction() == TSA::Model::LoadDirection::LocalY)
    {
        gp_Ax3 frame = TSA::Analysis::LoadResolver::computeElementLocalAxes(p1, p2, rotDeg);
        dirVec = gp_Vec(frame.YDirection()) * sign;
    }
    else if (ml->direction() == TSA::Model::LoadDirection::LocalZ)
    {
        gp_Ax3 frame = TSA::Analysis::LoadResolver::computeElementLocalAxes(p1, p2, rotDeg);
        dirVec = gp_Vec(frame.Direction()) * sign;
    }
    if (dirVec.SquareMagnitude() > 1e-6) dirVec.Normalize();

    double scale = (m_loadScale > 0.05) ? m_loadScale : 1.0;
    std::vector<Handle(AIS_Shape)> shapes;
    int numArrows = 4;
    double arrowH = 0.6 * scale;

    for (int i = 0; i <= numArrows; ++i)
    {
        double t = static_cast<double>(i) / static_cast<double>(numArrows);
        gp_Pnt pt = p1.Translated(vAxis * t);
        TopoDS_Shape arr = makeArrowShape(pt, dirVec, arrowH, 0.025 * scale, 0.07 * scale, 0.16 * scale);
        if (!arr.IsNull())
        {
            Handle(AIS_Shape) aisArr = new AIS_Shape(arr);
            aisArr->SetColor(Quantity_NOC_CYAN);
            aisArr->SetMaterial(Graphic3d_NOM_PLASTIC);
            aisArr->SetDisplayMode(AIS_Shaded);
            if (m_loadsVisible && m_forcesVisible)
            {
                m_context->Display(aisArr, false);
            }
            if (m_selectionManager)
            {
                m_selectionManager->registerMemberLoad(loadId, aisArr);
            }
            shapes.push_back(aisArr);
        }
    }
    m_memberLoadShapes[loadId] = shapes;

    // Étiquette au milieu
    gp_Pnt midPnt = p1.Translated(vAxis * 0.5);
    gp_Pnt labelPos = midPnt.Translated(-dirVec * (arrowH + 0.15));

    Handle(AIS_TextLabel) aisLabel = new AIS_TextLabel();
    QString textVal = QString("q = %1 kN/m").arg(QString::number(ml->q1(), 'f', 1));
    const auto* lc = m_model->loadManager().getLoadCase(ml->loadCaseId());
    if (lc) textVal += QString(" [%1]").arg(QString::fromStdString(lc->name()));

    aisLabel->SetText(TCollection_ExtendedString(textVal.toUtf8().constData(), true));
    aisLabel->SetPosition(labelPos);
    aisLabel->SetColor(m_isDarkMode ? Quantity_Color(0.2, 0.9, 1.0, Quantity_TOC_RGB) : Quantity_Color(0.0, 0.5, 0.7, Quantity_TOC_RGB));
    aisLabel->SetHJustification(Graphic3d_HTA_CENTER);
    aisLabel->SetVJustification(Graphic3d_VTA_BOTTOM);
    aisLabel->SetHeight(12.0);
    aisLabel->SetFontAspect(Font_FA_Bold);

    m_context->Display(aisLabel, false);
    m_context->Deactivate(aisLabel);
    if (!m_loadValuesVisible || !m_loadsVisible)
    {
        m_context->Erase(aisLabel, false);
    }
    m_memberLoadLabels[loadId] = aisLabel;

    if (redrawImmediately && !m_view.IsNull())
    {
        m_context->UpdateCurrentViewer();
        m_view->Redraw();
    }
}

void OccView::removeMemberLoadShape(int loadId, bool redrawImmediately)
{
    auto itS = m_memberLoadShapes.find(loadId);
    if (itS != m_memberLoadShapes.end())
    {
        if (!m_context.IsNull())
        {
            for (auto& s : itS->second)
            {
                if (!s.IsNull()) m_context->Remove(s, false);
            }
        }
        m_memberLoadShapes.erase(itS);
    }
    if (m_selectionManager)
    {
        m_selectionManager->unregisterMemberLoad(loadId);
    }
    auto itL = m_memberLoadLabels.find(loadId);
    if (itL != m_memberLoadLabels.end())
    {
        if (!m_context.IsNull()) m_context->Remove(itL->second, false);
        m_memberLoadLabels.erase(itL);
    }
    if (redrawImmediately && !m_view.IsNull())
    {
        m_context->UpdateCurrentViewer();
        m_view->Redraw();
    }
}

void OccView::updateAllLoadShapes()
{
    if (!m_model) return;
    clearLoadShapes();
    for (const auto& [id, nl] : m_model->loadManager().nodalLoads())
    {
        updateNodalLoadShape(id, false);
    }
    for (const auto& [id, ml] : m_model->loadManager().memberLoads())
    {
        updateMemberLoadShape(id, false);
    }
    if (!m_context.IsNull())
    {
        m_context->UpdateCurrentViewer();
    }
}

void OccView::clearLoadShapes()
{
    if (m_context.IsNull()) return;
    for (auto& [id, shapes] : m_nodalLoadShapes)
    {
        for (auto& s : shapes)
        {
            if (!s.IsNull()) m_context->Remove(s, false);
        }
    }
    m_nodalLoadShapes.clear();

    for (auto& [id, l] : m_nodalLoadLabels)
    {
        if (!l.IsNull()) m_context->Remove(l, false);
    }
    m_nodalLoadLabels.clear();

    for (auto& [id, shapes] : m_memberLoadShapes)
    {
        for (auto& s : shapes)
        {
            if (!s.IsNull()) m_context->Remove(s, false);
        }
    }
    m_memberLoadShapes.clear();

    for (auto& [id, l] : m_memberLoadLabels)
    {
        if (!l.IsNull()) m_context->Remove(l, false);
    }
    m_memberLoadLabels.clear();
}


void OccView::updateSupportShape(int nodeId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    // 1. Supprimer l'ancienne forme et étiquette d'appui
    auto itLbl = m_supportLabels.find(nodeId);
    if (itLbl != m_supportLabels.end())
    {
        m_context->Remove(itLbl->second, false);
        m_supportLabels.erase(itLbl);
    }

    auto it = m_supportShapes.find(nodeId);
    if (it != m_supportShapes.end())
    {
        m_context->Remove(it->second, false);
        m_supportShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterSupport(nodeId);
        }
    }

    const auto* node = m_model->getNode(nodeId);
    if (!node || node->support().isFree())
    {
        if (redrawImmediately) m_context->UpdateCurrentViewer();
        return;
    }

    // 2. Déterminer la normale d'orientation
    gp_Dir normal(0, 0, 1);
    if (node->support().orientationType() == TSA::Model::SupportOrientationType::CustomVector)
    {
        gp_Vec cv(node->support().customDirX(), node->support().customDirY(), node->support().customDirZ());
        if (cv.Magnitude() > 1e-4) normal = gp_Dir(cv);
    }
    else if (node->support().orientationType() == TSA::Model::SupportOrientationType::LocalBar)
    {
        for (const auto& [cid, col] : m_model->columns())
        {
            if (col.startNodeId() == nodeId)
            {
                const auto* nEnd = m_model->getNode(col.endNodeId());
                if (nEnd) { gp_Vec v(node->x() - nEnd->x(), node->y() - nEnd->y(), node->z() - nEnd->z()); if (v.Magnitude() > 1e-4) { normal = gp_Dir(v); break; } }
            }
            else if (col.endNodeId() == nodeId)
            {
                const auto* nStart = m_model->getNode(col.startNodeId());
                if (nStart) { gp_Vec v(node->x() - nStart->x(), node->y() - nStart->y(), node->z() - nStart->z()); if (v.Magnitude() > 1e-4) { normal = gp_Dir(v); break; } }
            }
        }
    }

    // 3. Créer la géométrie 3D de l'appui
    TopoDS_Shape shape = TSA::Geometry::SupportGeometry::createSupportShape(*node, 0.40, normal);
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisSupport = new AIS_Shape(shape);

        // Palette de couleurs normalisée génie civil
        if (node->support().isFixed())
        {
            aisSupport->SetColor(Quantity_NOC_DARKSLATEBLUE);
        }
        else if (node->support().isPinned())
        {
            aisSupport->SetColor(Quantity_NOC_SPRINGGREEN);
        }
        else if (node->support().isRoller())
        {
            aisSupport->SetColor(Quantity_NOC_CYAN2);
        }
        else if (node->support().hasSprings())
        {
            aisSupport->SetColor(Quantity_NOC_GOLDENROD);
        }
        else
        {
            aisSupport->SetColor(Quantity_NOC_ORANGE);
        }

        aisSupport->SetMaterial(Graphic3d_NOM_STEEL);
        aisSupport->SetDisplayMode(AIS_Shaded);

        m_supportShapes[nodeId] = aisSupport;
        if (m_supportsVisible)
        {
            m_context->Display(aisSupport, false);
        }

        if (m_selectionManager)
        {
            m_selectionManager->registerSupport(nodeId, aisSupport);
        }
    }

    // 4. Étiquette d'appui (AIS_TextLabel)
    Handle(AIS_TextLabel) aisLabel = new AIS_TextLabel();
    QString labelText = QString::fromStdString(node->support().typeName());
    aisLabel->SetText(TCollection_ExtendedString(labelText.toUtf8().constData(), true));
    aisLabel->SetPosition(gp_Pnt(node->x(), node->y(), node->z() - 0.45));
    aisLabel->SetColor(Quantity_Color(0.95, 0.65, 0.15, Quantity_TOC_RGB));
    aisLabel->SetHJustification(Graphic3d_HTA_CENTER);
    aisLabel->SetVJustification(Graphic3d_VTA_TOP);
    aisLabel->SetHeight(12.0);
    aisLabel->SetFontAspect(Font_FA_Bold);

    m_context->Display(aisLabel, false);
    m_context->Deactivate(aisLabel);
    if (!m_supportLabelsVisible || !m_supportsVisible)
    {
        m_context->Erase(aisLabel, false);
    }
    m_supportLabels[nodeId] = aisLabel;

    if (redrawImmediately)
    {
        m_context->UpdateCurrentViewer();
    }
}

void OccView::removeSupportShape(int nodeId, bool redrawImmediately)
{
    auto itLbl = m_supportLabels.find(nodeId);
    if (itLbl != m_supportLabels.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(itLbl->second, false);
        }
        m_supportLabels.erase(itLbl);
    }

    auto it = m_supportShapes.find(nodeId);
    if (it != m_supportShapes.end())
    {
        if (!m_context.IsNull())
        {
            m_context->Remove(it->second, false);
            if (redrawImmediately)
            {
                m_context->UpdateCurrentViewer();
            }
        }
        m_supportShapes.erase(it);
        if (m_selectionManager)
        {
            m_selectionManager->unregisterSupport(nodeId);
        }
    }
}

void OccView::setSupportsVisible(bool visible)
{
    m_supportsVisible = visible;
    if (m_context.IsNull()) return;

    for (const auto& [id, shape] : m_supportShapes)
    {
        if (visible)
            m_context->Display(shape, false);
        else
            m_context->Erase(shape, false);
    }

    for (const auto& [id, lbl] : m_supportLabels)
    {
        if (visible && m_supportLabelsVisible)
            m_context->Display(lbl, false);
        else
            m_context->Erase(lbl, false);
    }

    m_context->UpdateCurrentViewer();
}

void OccView::setSupportLabelsVisible(bool visible)
{
    m_supportLabelsVisible = visible;
    if (m_context.IsNull()) return;

    for (const auto& [id, lbl] : m_supportLabels)
    {
        if (visible && m_supportsVisible)
            m_context->Display(lbl, false);
        else
            m_context->Erase(lbl, false);
    }

    m_context->UpdateCurrentViewer();
}



