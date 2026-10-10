#include "ElementResultsPanel.h"
#include "PropertyPanel.h"
#include <QAbstractSpinBox>
#include "NodePropertiesView.h"
#include "BeamPropertiesView.h"
#include "ColumnPropertiesView.h"
#include "CablePropertiesView.h"
#include "SlabPropertiesView.h"
#include "WallPropertiesView.h"
#include "FoundationPropertiesView.h"
#include "TrussMemberPropertiesView.h"
#include "WorkPlanePropertiesView.h"
#include "LoadPropertiesView.h"
#include "../../Coordinate/WorkPlane.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QScrollBar>
#include <QWheelEvent>

namespace TSA::UI
{

PropertyPanel::PropertyPanel(TSA::Model::Model* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setupUi();
    if (m_model)
    {
        m_model->addObserver(this);
    }
}

PropertyPanel::~PropertyPanel()
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
}

void PropertyPanel::setModel(TSA::Model::Model* model)
{
    if (m_model == model)
        return;

    if (m_model)
    {
        m_model->removeObserver(this);
    }

    m_model = model;

    if (m_model)
    {
        m_model->addObserver(this);
    }

    m_nodeView->setModel(m_model);
    m_beamView->setModel(m_model);
    m_columnView->setModel(m_model);
    m_cableView->setModel(m_model);
    m_slabView->setModel(m_model);
    m_wallView->setModel(m_model);
    m_foundationView->setModel(m_model);
    m_trussView->setModel(m_model);
    if (m_workPlaneView)
    {
        m_workPlaneView->setModel(m_model);
    }
    if (m_loadView)
    {
        m_loadView->setModel(m_model);
    }

    clearProperties();
}

void PropertyPanel::setResultsModel(const std::shared_ptr<class TSA::Analysis::ResultsModel>& results)
{
    m_resultsModel = results;
    if (m_nodeView)
    {
        m_nodeView->setResultsModel(results);
    }
    if (m_elementResults)
    {
        m_elementResults->setResultsModel(results);
    }
}

void PropertyPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // En-tête titre du panneau
    m_titleLabel = new QLabel(tr("PROPRIÉTÉS STRUCTURALES"), this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("background-color: #1E293B; color: #F8FAFC; font-weight: bold; padding: 8px; border-bottom: 2px solid #3B82F6;");
    mainLayout->addWidget(m_titleLabel);

    // Zone avec défilement pour les vues de propriétés
    qApp->installEventFilter(this);
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContainer = new QWidget(scrollArea);
    auto* containerLayout = new QVBoxLayout(scrollContainer);
    containerLayout->setContentsMargins(4, 4, 4, 4);

    m_stack = new QStackedWidget(scrollContainer);

    // 0. Vue Vide (Aucune sélection)
    m_emptyView = new QWidget(m_stack);
    auto* emptyLayout = new QVBoxLayout(m_emptyView);
    emptyLayout->setContentsMargins(16, 32, 16, 16);
    auto* lblEmpty = new QLabel(tr("Sélectionnez un élément dans le Viewport 3D ou l'arborescence pour inspecter ses propriétés métier."), m_emptyView);
    lblEmpty->setWordWrap(true);
    lblEmpty->setAlignment(Qt::AlignCenter);
    lblEmpty->setStyleSheet("color: #94A3B8; font-style: italic; font-size: 13px;");
    emptyLayout->addWidget(lblEmpty);
    emptyLayout->addStretch();
    m_stack->addWidget(m_emptyView); // Index 0

    // Vues spécialisées par élément structural métier (Règle 14)
    m_nodeView = new NodePropertiesView(m_model, m_stack);
    connect(m_nodeView, &IElementPropertyView::elementModified, this, &PropertyPanel::onViewModified);
    m_stack->addWidget(m_nodeView); // Index 1

    m_beamView = new BeamPropertiesView(m_model, m_stack);
    connect(m_beamView, &IElementPropertyView::elementModified, this, &PropertyPanel::onViewModified);
    connect(m_beamView, &IElementPropertyView::nodeRequested, this, &PropertyPanel::showNodeProperties);
    m_stack->addWidget(m_beamView); // Index 2

    m_columnView = new ColumnPropertiesView(m_model, m_stack);
    connect(m_columnView, &IElementPropertyView::nodeRequested, this, &PropertyPanel::showNodeProperties);
    connect(m_columnView, &IElementPropertyView::elementModified, this, &PropertyPanel::onViewModified);
    m_stack->addWidget(m_columnView); // Index 3

    m_cableView = new CablePropertiesView(m_model, m_stack);
    connect(m_cableView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_cableView); // Index 4

    m_slabView = new SlabPropertiesView(m_model, m_stack);
    connect(m_slabView, &IElementPropertyView::elementModified, this, &PropertyPanel::onViewModified);
    m_stack->addWidget(m_slabView); // Index 5

    m_wallView = new WallPropertiesView(m_model, m_stack);
    connect(m_wallView, &IElementPropertyView::elementModified, this, &PropertyPanel::onViewModified);
    m_stack->addWidget(m_wallView); // Index 6

    m_foundationView = new FoundationPropertiesView(m_model, m_stack);
    connect(m_foundationView, &IElementPropertyView::elementModified, this, &PropertyPanel::onViewModified);
    m_stack->addWidget(m_foundationView); // Index 7

    m_trussView = new TrussMemberPropertiesView(m_model, m_stack);
    connect(m_trussView, &IElementPropertyView::nodeRequested, this, &PropertyPanel::showNodeProperties);
    connect(m_trussView, &IElementPropertyView::elementModified, this, &PropertyPanel::onViewModified);
    m_stack->addWidget(m_trussView); // Index 8

    m_workPlaneView = new WorkPlanePropertiesView(m_model, m_stack);
    connect(m_workPlaneView, &WorkPlanePropertiesView::workPlaneModified, this, &PropertyPanel::workPlaneModified);
    m_stack->addWidget(m_workPlaneView); // Index 9

    m_loadView = new LoadPropertiesView(m_model, m_stack);
    connect(m_loadView, &LoadPropertiesView::loadModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_loadView); // Index 10

    containerLayout->addWidget(m_stack);
    m_elementResults = new ElementResultsPanel(scrollContainer);
    containerLayout->addWidget(m_elementResults);
    connect(m_stack, &QStackedWidget::currentChanged, this, [this]() {
        auto* w = m_stack->currentWidget();
        if (w != m_beamView && w != m_columnView && w != m_trussView && w != m_cableView)
            m_elementResults->showElement(std::nullopt);
    });
    scrollContainer->setLayout(containerLayout);
    scrollArea->setWidget(scrollContainer);

    mainLayout->addWidget(scrollArea);
    clearProperties();

    // Les vues appliquent leurs modifications sur valueChanged : sans suivi clavier, une valeur
    // n'est validée qu'à Entrée / perte du focus / cran de flèche (taper « 0.45 » appliquait
    // auparavant 0, 0.4 puis 0.45 : géométries intermédiaires et entrées Undo parasites).
    for (QAbstractSpinBox* spin : m_stack->findChildren<QAbstractSpinBox*>())
    {
        spin->setKeyboardTracking(false);
    }

    // Un QStackedWidget prend la taille de sa page la plus large : la page vide (texte d'invite)
    // était rognée à droite. Seule la page courante contribue à la taille.
    auto fitToCurrentPage = [this](int current) {
        for (int i = 0; i < m_stack->count(); ++i)
        {
            auto* page = m_stack->widget(i);
            page->setSizePolicy(i == current ? QSizePolicy::Preferred : QSizePolicy::Ignored,
                                i == current ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        }
        m_stack->updateGeometry();
    };
    connect(m_stack, &QStackedWidget::currentChanged, this, fitToCurrentPage);
    fitToCurrentPage(m_stack->currentIndex());
}

namespace
{
QString entityTitle(const QString& what, const std::string& label, int id)
{
    if (label.empty()) return QStringLiteral("%1 #%2").arg(what).arg(id);
    return QStringLiteral("%1 %2 (id %3)").arg(what, QString::fromStdString(label)).arg(id);
}
} // namespace

void PropertyPanel::clearProperties()
{
    endMultiEdit();
    m_titleLabel->setText(tr("PROPRIÉTÉS STRUCTURALES"));
    m_stack->setCurrentWidget(m_emptyView);
}

void PropertyPanel::showLevelProperties(const QString& levelId)
{
    endMultiEdit();
    clearProperties();
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NIVEAU : %1").arg(levelId));
}

void PropertyPanel::showNodeProperties(int nodeId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DU NŒUD"), m_model && m_model->getNode(nodeId) ? m_model->getNode(nodeId)->formattedName() : std::string(), nodeId));
    m_nodeView->setElementId(nodeId);
    m_stack->setCurrentWidget(m_nodeView);
}

void PropertyPanel::showBeamProperties(int beamId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DE LA POUTRE"), m_model && m_model->getBeam(beamId) ? m_model->getBeam(beamId)->formattedName() : std::string(), beamId));
    m_beamView->setElementId(beamId);
    m_stack->setCurrentWidget(m_beamView);
    m_elementResults->showElement(TSA::Analysis::ElementKey{ TSA::Analysis::StructuralElementKind::Beam, beamId });
}

void PropertyPanel::showColumnProperties(int columnId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DU POTEAU"), m_model && m_model->getColumn(columnId) ? m_model->getColumn(columnId)->formattedName() : std::string(), columnId));
    m_columnView->setElementId(columnId);
    m_stack->setCurrentWidget(m_columnView);
    m_elementResults->showElement(TSA::Analysis::ElementKey{ TSA::Analysis::StructuralElementKind::Column, columnId });
}

void PropertyPanel::showCableProperties(int cableId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DU CÂBLE"), m_model && m_model->getCable(cableId) ? m_model->getCable(cableId)->name() : std::string(), cableId));
    m_cableView->setElementId(cableId);
    m_stack->setCurrentWidget(m_cableView);
    m_elementResults->showElement(TSA::Analysis::ElementKey{ TSA::Analysis::StructuralElementKind::Cable, cableId });
}

void PropertyPanel::showSlabProperties(int slabId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DE LA DALLE"), m_model && m_model->getSlab(slabId) ? m_model->getSlab(slabId)->formattedName() : std::string(), slabId));
    m_slabView->setElementId(slabId);
    m_stack->setCurrentWidget(m_slabView);
}

void PropertyPanel::showWallProperties(int wallId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DU VOILE"), m_model && m_model->getWall(wallId) ? m_model->getWall(wallId)->formattedName() : std::string(), wallId));
    m_wallView->setElementId(wallId);
    m_stack->setCurrentWidget(m_wallView);
}

void PropertyPanel::showFoundationProperties(int foundationId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DE LA FONDATION"), m_model && m_model->getFoundation(foundationId) ? m_model->getFoundation(foundationId)->formattedName() : std::string(), foundationId));
    m_foundationView->setElementId(foundationId);
    m_stack->setCurrentWidget(m_foundationView);
}

void PropertyPanel::showTrussMemberProperties(int memberId)
{
    endMultiEdit();
    m_titleLabel->setText(entityTitle(tr("PROPRIÉTÉS DU TREILLIS"), m_model && m_model->getTrussMember(memberId) ? m_model->getTrussMember(memberId)->formattedName() : std::string(), memberId));
    m_trussView->setElementId(memberId);
    m_stack->setCurrentWidget(m_trussView);
    m_elementResults->showElement(TSA::Analysis::ElementKey{ TSA::Analysis::StructuralElementKind::Truss, memberId });
}

void PropertyPanel::showWorkPlaneProperties(int workPlaneId)
{
    endMultiEdit();
    m_titleLabel->setText(tr("PROPRIÉTÉS DU PLAN DE TRAVAIL WP%1").arg(workPlaneId));
    if (m_model && m_model->workPlaneManager())
    {
        const auto* wp = m_model->workPlaneManager()->getWorkPlane(workPlaneId);
        if (wp)
        {
            m_workPlaneView->setWorkPlane(*wp);
        }
    }
    m_stack->setCurrentWidget(m_workPlaneView);
}

void PropertyPanel::showNodalLoadProperties(int loadId)
{
    endMultiEdit();
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA CHARGE NODALE #%1").arg(loadId));
    m_loadView->setNodalLoadId(loadId);
    m_stack->setCurrentWidget(m_loadView);
}

void PropertyPanel::showMemberLoadProperties(int loadId)
{
    endMultiEdit();
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA CHARGE SUR BARRE #%1").arg(loadId));
    m_loadView->setMemberLoadId(loadId);
    m_stack->setCurrentWidget(m_loadView);
}

void PropertyPanel::setWorkPlane(const TSA::Coordinate::WorkPlane& wp)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DU PLAN DE TRAVAIL [%1]").arg(QString::fromStdString(wp.name())));
    m_workPlaneView->setWorkPlane(wp);
    m_stack->setCurrentWidget(m_workPlaneView);
}

void PropertyPanel::refreshLibraryLists()
{
    m_beamView->refreshLibraries();
    m_columnView->refreshLibraries();
    m_cableView->refreshLibraries();
    m_slabView->refreshLibraries();
    m_wallView->refreshLibraries();
    m_trussView->refreshLibraries();
}

// -----------------------------------------------------------------------------
// IModelObserver Callbacks (Synchronisation bidirectionnelle 100% temps réel)
// -----------------------------------------------------------------------------
void PropertyPanel::onNodeModified(const TSA::Model::Node& node)
{
    if (m_stack->currentWidget() == m_nodeView && m_nodeView->elementId() == node.id())
    {
        m_nodeView->refreshView();
        return;
    }
    // Barre affichée dont un nœud a changé (appui, coordonnées) : ses nœuds d'extrémité et sa
    // longueur sont relus. Sans cela, l'appui posé sur le nœud n'apparaissait qu'au changement de sélection.
    if (!m_model) return;
    QWidget* current = m_stack->currentWidget();
    auto uses = [&node](int start, int end) { return node.id() == start || node.id() == end; };
    if (current == m_beamView)
    {
        if (const auto* b = m_model->getBeam(m_beamView->elementId()); b && uses(b->startNodeId(), b->endNodeId()))
            m_beamView->refreshView();
    }
    else if (current == m_columnView)
    {
        if (const auto* c = m_model->getColumn(m_columnView->elementId()); c && uses(c->startNodeId(), c->endNodeId()))
            m_columnView->refreshView();
    }
    else if (current == m_trussView)
    {
        if (const auto* t = m_model->getTrussMember(m_trussView->elementId()); t && uses(t->startNodeId(), t->endNodeId()))
            m_trussView->refreshView();
    }
}

void PropertyPanel::onNodeRemoved(int nodeId)
{
    if (m_stack->currentWidget() == m_nodeView && m_nodeView->elementId() == nodeId)
    {
        clearProperties();
    }
}

void PropertyPanel::onBeamModified(const TSA::Model::Beam& beam)
{
    if (m_stack->currentWidget() == m_beamView && m_beamView->elementId() == beam.id())
    {
        m_beamView->refreshView();
    }
}

void PropertyPanel::onBeamRemoved(int beamId)
{
    if (m_stack->currentWidget() == m_beamView && m_beamView->elementId() == beamId)
    {
        clearProperties();
    }
}

void PropertyPanel::onColumnModified(const TSA::Model::Column& column)
{
    if (m_stack->currentWidget() == m_columnView && m_columnView->elementId() == column.id())
    {
        m_columnView->refreshView();
    }
}

void PropertyPanel::onColumnRemoved(int columnId)
{
    if (m_stack->currentWidget() == m_columnView && m_columnView->elementId() == columnId)
    {
        clearProperties();
    }
}

void PropertyPanel::onSlabModified(const TSA::Model::Slab& slab)
{
    if (m_stack->currentWidget() == m_slabView && m_slabView->elementId() == slab.id())
    {
        m_slabView->refreshView();
    }
}

void PropertyPanel::onSlabRemoved(int slabId)
{
    if (m_stack->currentWidget() == m_slabView && m_slabView->elementId() == slabId)
    {
        clearProperties();
    }
}

void PropertyPanel::onWallModified(const TSA::Model::Wall& wall)
{
    if (m_stack->currentWidget() == m_wallView && m_wallView->elementId() == wall.id())
    {
        m_wallView->refreshView();
    }
}

void PropertyPanel::onWallRemoved(int wallId)
{
    if (m_stack->currentWidget() == m_wallView && m_wallView->elementId() == wallId)
    {
        clearProperties();
    }
}

void PropertyPanel::onFoundationModified(const TSA::Model::Foundation& foundation)
{
    if (m_stack->currentWidget() == m_foundationView && m_foundationView->elementId() == foundation.id())
    {
        m_foundationView->refreshView();
    }
}

void PropertyPanel::onFoundationRemoved(int foundationId)
{
    if (m_stack->currentWidget() == m_foundationView && m_foundationView->elementId() == foundationId)
    {
        clearProperties();
    }
}

void PropertyPanel::onTrussMemberModified(const TSA::Model::TrussMember& member)
{
    if (m_stack->currentWidget() == m_trussView && m_trussView->elementId() == member.id())
    {
        m_trussView->refreshView();
    }
}

void PropertyPanel::onTrussMemberRemoved(int memberId)
{
    if (m_stack->currentWidget() == m_trussView && m_trussView->elementId() == memberId)
    {
        clearProperties();
    }
}

void PropertyPanel::onCableModified(const TSA::Model::Cable& cable)
{
    if (m_stack->currentWidget() == m_cableView && m_cableView->elementId() == cable.id())
    {
        m_cableView->refreshView();
    }
}

void PropertyPanel::onCableRemoved(int cableId)
{
    if (m_stack->currentWidget() == m_cableView && m_cableView->elementId() == cableId)
    {
        clearProperties();
    }
}

void PropertyPanel::onNodalLoadModified(int loadId)
{
    if (m_stack->currentWidget() == m_loadView && m_loadView->currentLoadId() == loadId &&
        m_loadView->displayMode() == LoadPropertiesView::DisplayMode::Nodal)
    {
        m_loadView->refreshView();
    }
}

void PropertyPanel::onNodalLoadRemoved(int loadId)
{
    if (m_stack->currentWidget() == m_loadView && m_loadView->currentLoadId() == loadId &&
        m_loadView->displayMode() == LoadPropertiesView::DisplayMode::Nodal)
    {
        clearProperties();
    }
}

void PropertyPanel::onMemberLoadModified(int loadId)
{
    if (m_stack->currentWidget() == m_loadView && m_loadView->currentLoadId() == loadId &&
        m_loadView->displayMode() == LoadPropertiesView::DisplayMode::Member)
    {
        m_loadView->refreshView();
    }
}

void PropertyPanel::onMemberLoadRemoved(int loadId)
{
    if (m_stack->currentWidget() == m_loadView && m_loadView->currentLoadId() == loadId &&
        m_loadView->displayMode() == LoadPropertiesView::DisplayMode::Member)
    {
        clearProperties();
    }
}

void PropertyPanel::onModelDiffApplied(const TSA::Model::ModelDiff& /*diff*/)
{
    if (m_model && m_multiEdit.active()) m_multiEdit.resync(*m_model);   // Annuler / Rétablir
    // Rafraîchir la vue active si son élément a été affecté
    if (m_stack->currentWidget() == m_beamView) m_beamView->refreshView();
    else if (m_stack->currentWidget() == m_columnView) m_columnView->refreshView();
    else if (m_stack->currentWidget() == m_cableView) m_cableView->refreshView();
    else if (m_stack->currentWidget() == m_slabView) m_slabView->refreshView();
    else if (m_stack->currentWidget() == m_wallView) m_wallView->refreshView();
    else if (m_stack->currentWidget() == m_nodeView) m_nodeView->refreshView();
    else if (m_stack->currentWidget() == m_foundationView) m_foundationView->refreshView();
    else if (m_stack->currentWidget() == m_trussView) m_trussView->refreshView();
    else if (m_stack->currentWidget() == m_loadView) m_loadView->refreshView();
}

void PropertyPanel::onModelCleared()
{
    clearProperties();
}

void PropertyPanel::setMultiSelection(TSA::Model::ElementKind kind, int primaryId, const std::set<int>& sameKindIds)
{
    if (!m_model) return;
    m_multiEdit.begin(*m_model, kind, primaryId, sameKindIds);
    if (!m_multiEdit.active()) return;

    QString family;
    switch (kind)
    {
    case TSA::Model::ElementKind::Node: family = tr("NŒUDS (appuis)"); break;
    case TSA::Model::ElementKind::Beam: family = tr("POUTRES"); break;
    case TSA::Model::ElementKind::Column: family = tr("POTEAUX"); break;
    case TSA::Model::ElementKind::Slab: family = tr("DALLES"); break;
    case TSA::Model::ElementKind::Wall: family = tr("VOILES"); break;
    case TSA::Model::ElementKind::Foundation: family = tr("FONDATIONS"); break;
    case TSA::Model::ElementKind::TrussMember: family = tr("BARRES DE TREILLIS"); break;
    default: break;
    }
    m_titleLabel->setText(tr("ÉDITION GROUPÉE : %1 %2").arg(m_multiEdit.count()).arg(family));
    m_titleLabel->setToolTip(tr("Les champs modifiés ici sont appliqués aux %1 éléments sélectionnés de ce type "
                                "(nom et géométrie exceptés). Annuler (Ctrl+Z) les rétablit tous.")
                                 .arg(m_multiEdit.count()));
}

void PropertyPanel::endMultiEdit()
{
    m_multiEdit.clear();
    if (m_titleLabel) m_titleLabel->setToolTip(QString());
}

void PropertyPanel::onViewModified()
{
    if (m_model && m_multiEdit.active())
    {
        const TSA::Model::ModelDiff diff = m_multiEdit.propagate(*m_model);
        if (!diff.isEmpty()) m_model->notifyModelDiffApplied(diff);
    }
    emit elementModified();
}

bool PropertyPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::Wheel) return QWidget::eventFilter(watched, event);
    auto* w = qobject_cast<QWidget*>(watched);
    if (!w || !isAncestorOf(w)) return QWidget::eventFilter(watched, event);

    // Liste déroulante ou champ numérique visé (le champ texte interne d'un QSpinBox compte aussi).
    QWidget* field = nullptr;
    for (QWidget* p = w; p && p != this; p = p->parentWidget())
    {
        if (qobject_cast<QComboBox*>(p) || qobject_cast<QAbstractSpinBox*>(p))
        {
            field = p;
            break;
        }
    }
    // Une liste ouverte (vue déroulante) ou un champ en cours d'édition garde la molette.
    if (!field || field->hasFocus()) return QWidget::eventFilter(watched, event);

    for (QWidget* p = field->parentWidget(); p && p != this->parentWidget(); p = p->parentWidget())
    {
        if (auto* area = qobject_cast<QScrollArea*>(p))
        {
            QApplication::sendEvent(area->verticalScrollBar(), event);
            return true;
        }
    }
    event->ignore();
    return true;
}

} // namespace TSA::UI
