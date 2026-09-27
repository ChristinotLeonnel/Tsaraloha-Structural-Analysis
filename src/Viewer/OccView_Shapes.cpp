#include "OccView.h"
#include "SelectionManager.h"
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

#include <AIS_Shape.hxx>
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
#include <QString>

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
    for (auto& [id, aisShape] : m_nodeShapes)
    {
        m_context->Remove(aisShape, false);
    }
    m_nodeShapes.clear();

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

    m_context->UpdateCurrentViewer();
    fitAll();
}

void OccView::updateNodeShape(int nodeId, bool redrawImmediately)
{
    if (m_context.IsNull() || !m_model)
        return;

    bool wasSelected = m_selectionManager && m_selectionManager->selectedNodes().count(nodeId) > 0;

    // 1. Supprimer l'ancienne forme (avant le contrôle de validité, pour ne jamais
    //    laisser un nœud fantôme affiché/sélectionnable)
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

    // 2. Créer la nouvelle forme 3D
    TopoDS_Shape shape = TSA::Geometry::BeamGeometry::createNodeShape(*node, 0.12);
    if (!shape.IsNull())
    {
        Handle(AIS_Shape) aisNode = new AIS_Shape(shape);
        Quantity_Color qc;
        if (parseHexColor(node->color(), qc))
        {
            aisNode->SetColor(qc);
        }
        else
        {
            aisNode->SetColor(Quantity_NOC_GOLD);
        }
        aisNode->SetMaterial(Graphic3d_NOM_COPPER);
        aisNode->SetDisplayMode(AIS_Shaded);

        m_context->Display(aisNode, false);
        m_nodeShapes[nodeId] = aisNode;
        if (m_selectionManager)
        {
            m_selectionManager->registerNode(nodeId, aisNode);
            if (wasSelected)
            {
                m_selectionManager->selectNode(nodeId, true);
                m_context->SetSelected(aisNode, false);
            }
        }
    }

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

