#include "SelectionSynchronizer.h"

#include "../ModelTree/ModelTreeWidget.h"
#include "../Properties/PropertyPanel.h"
#include "../Ruler/ViewportContainer.h"
#include "../../Coordinate/WorkPlane.h"
#include "../../Coordinate/WorkPlaneManager.h"
#include "../../Model/Model.h"
#include "../../Viewer/OccView.h"
#include "../../Viewer/SelectionManager.h"

namespace TSA::UI
{

using TSA::Viewer::SelectionManager;
using TSA::Viewer::SelectionType;

SelectionSynchronizer::SelectionSynchronizer(TSA::Model::Model* model, SelectionManager* selection, OccView* view,
                                             ModelTreeWidget* tree, PropertyPanel* properties,
                                             ViewportContainer* container, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_selection(selection)
    , m_view(view)
    , m_tree(tree)
    , m_properties(properties)
    , m_container(container)
{
    connectTree();
    connectViewport();
    connectProperties();
}

// 1. Sélection depuis l'arbre du modèle
void SelectionSynchronizer::connectTree()
{
    connect(m_tree, &ModelTreeWidget::levelSelected, this, [this](const QString& levelId) {
        m_selection->clearSelection();
        m_view->clearHighlight();
        m_properties->showLevelProperties(levelId);
    });

    connect(m_tree, &ModelTreeWidget::workPlaneSelected, this, [this](int axis, double offset, const QString& name) {
        if (m_container)
            m_container->setActivePlane(static_cast<TSA::Coordinate::WorkPlaneAxis>(axis), offset);
        else
            m_view->setWorkPlaneAxisAndOffset(static_cast<TSA::Coordinate::WorkPlaneAxis>(axis), offset, name.toStdString());
    });

    connect(m_tree, &ModelTreeWidget::nodeSelected, this, [this](int id) {
        m_selection->selectNode(id);
        m_view->highlightNode(id);
        m_properties->showNodeProperties(id);
    });
    connect(m_tree, &ModelTreeWidget::beamSelected, this, [this](int id) {
        m_selection->selectBeam(id);
        m_view->highlightBeam(id);
        m_properties->showBeamProperties(id);
    });
    connect(m_tree, &ModelTreeWidget::columnSelected, this, [this](int id) {
        m_selection->selectColumn(id);
        m_view->highlightColumn(id);
        m_properties->showColumnProperties(id);
    });
    connect(m_tree, &ModelTreeWidget::slabSelected, this, [this](int id) {
        m_selection->selectSlab(id);
        m_view->highlightSlab(id);
        m_properties->showSlabProperties(id);
    });
    connect(m_tree, &ModelTreeWidget::wallSelected, this, [this](int id) {
        m_selection->selectWall(id);
        m_view->highlightWall(id);
        m_properties->showWallProperties(id);
    });
    connect(m_tree, &ModelTreeWidget::foundationSelected, this, [this](int id) {
        m_selection->selectFoundation(id);
        m_view->highlightFoundation(id);
        m_properties->showFoundationProperties(id);
    });
    connect(m_tree, &ModelTreeWidget::trussMemberSelected, this, [this](int id) {
        m_selection->selectTrussMember(id);
        m_view->highlightTrussMember(id);
        m_properties->showTrussMemberProperties(id);
    });
    connect(m_tree, &ModelTreeWidget::selectionCleared, this, [this]() {
        m_selection->clearSelection();
        m_view->clearHighlight();
        m_properties->clearProperties();
    });
    connect(m_tree, &ModelTreeWidget::cableSelected, this, [this](int id) {
        m_selection->clearSelection();
        m_selection->selectCable(id);
        m_view->highlightCable(id);
        m_properties->showCableProperties(id);
        emit statusMessage(tr("Câble sélectionné C%1").arg(id));
    });
    connect(m_tree, &ModelTreeWidget::loadSelected, this, [this](int id) {
        m_selection->clearSelection();
        m_view->clearHighlight();
        m_properties->showMemberLoadProperties(id);
        emit statusMessage(tr("Charge #%1 sélectionnée").arg(id));
    });
    connect(m_tree, &ModelTreeWidget::supportSelected, this, [this](int nodeId) {
        m_selection->clearSelection();
        m_selection->selectNode(nodeId);
        m_view->highlightNode(nodeId);
        m_properties->showNodeProperties(nodeId);
        emit statusMessage(tr("Appui sur Nœud N%1 sélectionné").arg(nodeId));
    });
    connect(m_tree, &ModelTreeWidget::resultsSelected, this, [this]() {
        emit resultsRequested();
        emit statusMessage(tr("Résultats d'analyse"));
    });
}

// 2. Sélection depuis le viewport (clic souris)
void SelectionSynchronizer::connectViewport()
{
    // Élément filaire : axes locaux affichés ; autres : masqués. Manipulateur détaché dans tous les cas.
    auto element = [this](bool localAxes) {
        m_view->detachManipulator();
        if (localAxes) m_view->updateSelectedElementLocalAxes();
        else m_view->clearSelectedElementLocalAxes();
    };

    connect(m_selection, &SelectionManager::nodeSelected, this, [this, element](int id) {
        m_tree->selectNodeItem(id);
        m_view->highlightNode(id);
        element(false);
        m_properties->showNodeProperties(id);
        emit statusMessage(tr("Selected Node %1").arg(id));
    });
    connect(m_selection, &SelectionManager::beamSelected, this, [this, element](int id) {
        m_tree->selectBeamItem(id);
        m_view->highlightBeam(id);
        element(true);
        m_properties->showBeamProperties(id);
        emit beamActivated(id);
        emit statusMessage(tr("Selected Beam %1").arg(id));
    });
    connect(m_selection, &SelectionManager::columnSelected, this, [this, element](int id) {
        m_tree->selectColumnItem(id);
        m_view->highlightColumn(id);
        element(true);
        m_properties->showColumnProperties(id);
        emit statusMessage(tr("Selected Column %1").arg(id));
    });
    connect(m_selection, &SelectionManager::slabSelected, this, [this, element](int id) {
        m_tree->selectSlabItem(id);
        m_view->highlightSlab(id);
        element(false);
        m_properties->showSlabProperties(id);
        emit statusMessage(tr("Selected Slab %1").arg(id));
    });
    connect(m_selection, &SelectionManager::wallSelected, this, [this, element](int id) {
        m_tree->selectWallItem(id);
        m_view->highlightWall(id);
        element(false);
        m_properties->showWallProperties(id);
        emit statusMessage(tr("Selected Wall %1").arg(id));
    });
    connect(m_selection, &SelectionManager::foundationSelected, this, [this, element](int id) {
        m_tree->selectFoundationItem(id);
        m_view->highlightFoundation(id);
        element(false);
        m_properties->showFoundationProperties(id);
        emit statusMessage(tr("Selected Foundation %1").arg(id));
    });
    connect(m_selection, &SelectionManager::trussMemberSelected, this, [this, element](int id) {
        m_tree->selectTrussMemberItem(id);
        m_view->highlightTrussMember(id);
        element(true);
        m_properties->showTrussMemberProperties(id);
        emit statusMessage(tr("Selected Truss Member %1").arg(id));
    });
    connect(m_selection, &SelectionManager::cableSelected, this, [this, element](int id) {
        m_tree->selectCableItem(id);
        m_view->highlightCable(id);
        element(true);
        m_properties->showCableProperties(id);
        emit cableActivated(id);
        emit statusMessage(tr("Câble sélectionné C%1").arg(id));
    });
    connect(m_selection, &SelectionManager::workPlaneSelected, this, [this](int id) {
        m_properties->showWorkPlaneProperties(id);
        m_view->attachManipulatorToWorkPlane();
        m_view->clearSelectedElementLocalAxes();
        emit statusMessage(tr("Plan de travail WP%1 sélectionné (Manipulateur 3D interactif)").arg(id));
    });
    connect(m_selection, &SelectionManager::nodalLoadSelected, this, [this, element](int id) {
        element(false);
        m_properties->showNodalLoadProperties(id);
        emit statusMessage(tr("Charge Nodale #%1 sélectionnée").arg(id));
    });
    connect(m_selection, &SelectionManager::memberLoadSelected, this, [this, element](int id) {
        element(false);
        m_properties->showMemberLoadProperties(id);
        emit statusMessage(tr("Charge sur Barre #%1 sélectionnée").arg(id));
    });
    connect(m_selection, &SelectionManager::selectionCleared, this, [this, element]() {
        m_tree->clearTreeSelection();
        m_view->clearHighlight();
        element(false);
        m_properties->clearProperties();
        emit statusMessage(tr("Ready"));
    });

    // Sélection ensembliste (tout sélectionner, inverser, par type...) : surbrillance de tout
    // l'ensemble en une passe, propriétés de l'élément principal, arbre désélectionné (sélectionner
    // des milliers d'items dans l'arbre serait coûteux et illisible).
    connect(m_selection, &SelectionManager::multipleSelectionChanged, this, [this, element]() {
        m_tree->clearTreeSelection();
        element(false);
        m_view->highlightSelection();
        const int id = m_selection->primarySelectedId();
        switch (m_selection->currentSelectionType())
        {
        case SelectionType::Node: m_properties->showNodeProperties(id); break;
        case SelectionType::Beam: m_properties->showBeamProperties(id); break;
        case SelectionType::Column: m_properties->showColumnProperties(id); break;
        case SelectionType::Slab: m_properties->showSlabProperties(id); break;
        case SelectionType::Wall: m_properties->showWallProperties(id); break;
        case SelectionType::Foundation: m_properties->showFoundationProperties(id); break;
        case SelectionType::TrussMember: m_properties->showTrussMemberProperties(id); break;
        case SelectionType::Cable: m_properties->showCableProperties(id); break;
        default: m_properties->clearProperties(); break;
        }
        // Édition groupée des éléments du même type que l'élément principal (BUG-005)
        using TSA::Model::ElementKind;
        const TSA::Model::ElementSet sel = m_selection->selectedElements();
        switch (m_selection->currentSelectionType())
        {
        case SelectionType::Node: m_properties->setMultiSelection(ElementKind::Node, id, sel.nodes); break;
        case SelectionType::Beam: m_properties->setMultiSelection(ElementKind::Beam, id, sel.beams); break;
        case SelectionType::Column: m_properties->setMultiSelection(ElementKind::Column, id, sel.columns); break;
        case SelectionType::Slab: m_properties->setMultiSelection(ElementKind::Slab, id, sel.slabs); break;
        case SelectionType::Wall: m_properties->setMultiSelection(ElementKind::Wall, id, sel.walls); break;
        case SelectionType::Foundation: m_properties->setMultiSelection(ElementKind::Foundation, id, sel.foundations); break;
        case SelectionType::TrussMember: m_properties->setMultiSelection(ElementKind::TrussMember, id, sel.trussMembers); break;
        default: break;
        }
    });

    connect(m_selection, &SelectionManager::selectionChanged, this, [this]() {
        const size_t total = m_selection->totalSelectedCount();
        if (total > 1)
        {
            emit statusMessage(tr("Sélection multiple : %1 éléments (%2 nœuds, %3 poutres, %4 poteaux, %5 dalles)")
                                   .arg(total)
                                   .arg(m_selection->selectedNodes().size())
                                   .arg(m_selection->selectedBeams().size())
                                   .arg(m_selection->selectedColumns().size())
                                   .arg(m_selection->selectedSlabs().size()));
        }
    });
}

// 3. Modifications depuis le panneau Propriétés
void SelectionSynchronizer::connectProperties()
{
    connect(m_properties, &PropertyPanel::elementModified, this, [this]() {
        // L'arbre est observateur du modèle (notify*Modified met à jour la ligne concernée) :
        // pas de reconstruction complète à chaque édition de propriété.
        m_view->update();
        emit modelEdited();
    });
    connect(m_properties, &PropertyPanel::workPlaneModified, this, [this](const TSA::Coordinate::WorkPlane& wp) {
        if (m_model && m_model->workPlaneManager()) m_model->workPlaneManager()->updateWorkPlane(wp);
        m_view->setActiveWorkPlane(wp);
        emit modelEdited();
    });
}

} // namespace TSA::UI
