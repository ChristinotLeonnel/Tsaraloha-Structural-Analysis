// Représentation du modèle dans la vue 3D : physique, filaire analytique, éléments finis, superposition
// (ModelDisplayMode.h). Purement visuel : le modèle, ses sections, matériaux, appuis, charges, résultats et
// révision ne sont jamais modifiés.

#include "OccView.h"

#include "DisplayModeRenderers.h"
#include "MaterialVisual.h"
#include "../Analysis/ResultsModel.h"
#include "../Diagnostics/Logger.h"

#include <Graphic3d_ZLayerId.hxx>
#include <QElapsedTimer>
#include <algorithm>
#include <QTimer>

using TSA::Analysis::ElementKey;
using TSA::Analysis::StructuralElementKind;
using TSA::Viewer::ModelDisplayMode;
namespace DisplayPolicy = TSA::Viewer::DisplayPolicy;

void OccView::setModelDisplayMode(ModelDisplayMode mode)
{
    if (mode == m_modelDisplayMode) return;
    const ModelDisplayMode previous = m_modelDisplayMode;
    m_modelDisplayMode = mode;
    if (m_context.IsNull() || !m_model) return;
    QElapsedTimer timer;
    timer.start();

    // L'atténuation des résultats mémorise la transparence des objets : levée avant la modification,
    // réappliquée ensuite.
    const ResultsStructureDisplay resultsDisplay = m_resultsStructureDisplay;
    if (resultsDisplay == ResultsStructureDisplay::Ghosted) setResultsStructureDisplay(ResultsStructureDisplay::Normal);

    if (DisplayPolicy::linearAsAxis(previous) != DisplayPolicy::linearAsAxis(mode))
    {
        // Axe ↔ section, solide ↔ arêtes : formes recréées. update*Shape conserve la sélection,
        // l'enregistrement auprès du gestionnaire de sélection et l'isolation.
        for (const auto& [id, b] : m_model->beams()) updateBeamShape(id, false);
        for (const auto& [id, c] : m_model->columns()) updateColumnShape(id, false);
        for (const auto& [id, t] : m_model->trussMembers()) updateTrussMemberShape(id, false);
        for (const auto& [id, c] : m_model->cables()) updateCableShape(id, false);
        for (const auto& [id, s] : m_model->slabs()) updateSlabShape(id, false);
        for (const auto& [id, w] : m_model->walls()) updateWallShape(id, false);
        for (const auto& [id, f] : m_model->foundations()) updateFoundationShape(id, false);
    }
    else if (!linearAsAxis())
    {
        // Physique ↔ Superposition : mêmes solides, seule la transparence change (pas de reconstruction).
        const double minimum = DisplayPolicy::physicalTransparency(mode);
        auto apply = [&](const auto& shapes, const auto& getter, double defaultTransparency) {
            for (const auto& [id, ais] : shapes)
            {
                const auto* e = getter(id);
                if (!e || ais.IsNull()) continue;
                const double t = std::max(minimum, TSA::Viewer::MaterialVisual::instance().getTransparency(e->material(), defaultTransparency));
                if (t > 0.01) m_context->SetTransparency(ais, t, false);
                else m_context->UnsetTransparency(ais, false);
            }
        };
        apply(m_beamShapes, [&](int id) { return m_model->getBeam(id); }, 0.0);
        apply(m_columnShapes, [&](int id) { return m_model->getColumn(id); }, 0.0);
        apply(m_trussShapes, [&](int id) { return m_model->getTrussMember(id); }, 0.0);
        apply(m_cableShapes, [&](int id) { return m_model->getCable(id); }, 0.0);
        apply(m_slabShapes, [&](int id) { return m_model->getSlab(id); }, 0.35);
        apply(m_wallShapes, [&](int id) { return m_model->getWall(id); }, 0.25);
        apply(m_foundationShapes, [&](int id) { return m_model->getFoundation(id); }, 0.0);
    }
    // Filaire analytique ↔ Éléments finis : mêmes axes ; seul le maillage superposé change.

    if (resultsDisplay == ResultsStructureDisplay::Ghosted) setResultsStructureDisplay(ResultsStructureDisplay::Ghosted);
    updateElementIsolation();
    refreshDisplayModeOverlays();
    m_context->UpdateCurrentViewer();
    if (!m_view.IsNull()) m_view->Redraw();
    TSA_LOG_INFO("OccView", "ModelDisplayModeTiming",
                 std::string("Représentation « ") + TSA::Viewer::modelDisplayModeName(mode) + " » : " +
                     std::to_string(timer.elapsed()) + " ms (" +
                     std::to_string(m_beamShapes.size() + m_columnShapes.size() + m_trussShapes.size() + m_cableShapes.size()) +
                     " barres, " + std::to_string(m_slabShapes.size() + m_wallShapes.size() + m_foundationShapes.size()) + " surfaces)");
}

TSA::Viewer::SolverMeshStatus OccView::solverMeshStatus() const
{
    const std::size_t planar = m_model ? m_model->slabs().size() + m_model->walls().size() : 0;
    return TSA::Viewer::solverMeshStatus(m_lastResults.get(), planar);
}

void OccView::scheduleDisplayModeOverlays()
{
    const bool needed = DisplayPolicy::analyticalOverlay(m_modelDisplayMode) || DisplayPolicy::solverMesh(m_modelDisplayMode) ||
                        (m_analyticalRenderer && m_analyticalRenderer->isShown()) || (m_meshRenderer && m_meshRenderer->isShown());
    if (!needed || m_displayOverlaysPending) return;
    m_displayOverlaysPending = true;
    QTimer::singleShot(0, this, [this] {
        refreshDisplayModeOverlays();
        if (!m_view.IsNull()) m_view->Redraw();
    });
}

void OccView::refreshDisplayModeOverlays()
{
    m_displayOverlaysPending = false;
    if (m_context.IsNull() || !m_model || !m_analyticalRenderer || !m_meshRenderer) return;

    // Un élément n'apparaît dans les surcouches que si son objet est affiché (isolation, filtres).
    auto shown = [this](StructuralElementKind kind, int id) {
        const std::map<int, Handle(AIS_Shape)>* shapes = nullptr;
        switch (kind)
        {
        case StructuralElementKind::Beam: shapes = &m_beamShapes; break;
        case StructuralElementKind::Column: shapes = &m_columnShapes; break;
        case StructuralElementKind::Truss: shapes = &m_trussShapes; break;
        case StructuralElementKind::Cable: shapes = &m_cableShapes; break;
        }
        auto it = shapes->find(id);
        return it != shapes->end() && !it->second.IsNull() && m_context->IsDisplayed(it->second);
    };

    if (DisplayPolicy::analyticalOverlay(m_modelDisplayMode))
    {
        std::map<StructuralElementKind, std::vector<std::pair<gp_Pnt, gp_Pnt>>> axes;
        // family : table des objets (visibilité) ; color : famille d'affichage (rôle d'une barre).
        auto add = [&](StructuralElementKind family, StructuralElementKind color, int id, int a, int b) {
            const auto* na = m_model->getNode(a);
            const auto* nb = m_model->getNode(b);
            if (na && nb && shown(family, id))
                axes[color].emplace_back(gp_Pnt(na->x(), na->y(), na->z()), gp_Pnt(nb->x(), nb->y(), nb->z()));
        };
        for (const auto& [id, e] : m_model->beams())
            add(StructuralElementKind::Beam, TSA::Viewer::analyticalKindOf(e.role()), id, e.startNodeId(), e.endNodeId());
        for (const auto& [id, e] : m_model->columns())
            add(StructuralElementKind::Column, StructuralElementKind::Column, id, e.startNodeId(), e.endNodeId());
        for (const auto& [id, e] : m_model->trussMembers())
            add(StructuralElementKind::Truss, StructuralElementKind::Truss, id, e.startNodeId(), e.endNodeId());
        for (const auto& [id, e] : m_model->cables())
            add(StructuralElementKind::Cable, StructuralElementKind::Cable, id, e.startNodeId(), e.endNodeId());
        m_analyticalRenderer->rebuild(axes);
    }
    else
        m_analyticalRenderer->clear();

    if (DisplayPolicy::solverMesh(m_modelDisplayMode) && solverMeshStatus().available)
    {
        const auto geometry = TSA::Viewer::buildSolverMeshGeometry(
            m_lastResults->solverMesh(), [&](const ElementKey& key) { return shown(key.kind, key.id); });
        m_meshRenderer->rebuild(geometry);
    }
    else
        m_meshRenderer->clear();
}

void OccView::styleLinearShape(const Handle(AIS_Shape)& ais, const TSA::Model::Material& material, const std::string& color,
                               StructuralElementKind kind)
{
    if (ais.IsNull()) return;
    if (linearAsAxis())
    {
        // Axe analytique : trait épais, couleur de la famille (poutres, poteaux, treillis, câbles). La couleur
        // propre à l'élément, attribuée par défaut à la création, rendrait les familles indiscernables.
        ais->SetDisplayMode(AIS_WireFrame);
        ais->SetColor(TSA::Viewer::analyticalColor(kind));
        ais->SetWidth(3.0);
        ais->UnsetTransparency();
        // Calque « Top » : dessiné après les lignes de grille confondues avec l'axe (poteau sur un
        // croisement d'axes : alternance de pointillés), tout en restant masqué par ce qui est devant.
        ais->SetZLayer(Graphic3d_ZLayerId_Top);
        return;
    }
    TSA::Viewer::MaterialVisual::instance().applyToShape(ais, material, color, m_renderDisplayMode);
    const double t = DisplayPolicy::physicalTransparency(m_modelDisplayMode);
    if (t > ais->Transparency()) ais->SetTransparency(t);
}

void OccView::stylePlanarShape(const Handle(AIS_Shape)& ais, const TSA::Model::Material& material, const std::string& color,
                               double defaultTransparency)
{
    if (ais.IsNull()) return;
    TSA::Viewer::MaterialVisual::instance().applyToShape(ais, material, color, m_renderDisplayMode, defaultTransparency);
    if (DisplayPolicy::planarAsEdges(m_modelDisplayMode))
    {
        // Arêtes seules : la surface ne masque ni les nœuds ni les axes ; la sélection reste possible.
        ais->SetDisplayMode(AIS_WireFrame);
        ais->UnsetTransparency();
        ais->SetZLayer(Graphic3d_ZLayerId_Top);
        return;
    }
    const double t = DisplayPolicy::physicalTransparency(m_modelDisplayMode);
    if (t > ais->Transparency()) ais->SetTransparency(t);
}
