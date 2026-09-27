#include "PropertyPanel.h"
#include "NodePropertiesView.h"
#include "BeamPropertiesView.h"
#include "ColumnPropertiesView.h"
#include "CablePropertiesView.h"
#include "SlabPropertiesView.h"
#include "WallPropertiesView.h"
#include "FoundationPropertiesView.h"
#include "TrussMemberPropertiesView.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QScrollArea>

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

    clearProperties();
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
    connect(m_nodeView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_nodeView); // Index 1

    m_beamView = new BeamPropertiesView(m_model, m_stack);
    connect(m_beamView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_beamView); // Index 2

    m_columnView = new ColumnPropertiesView(m_model, m_stack);
    connect(m_columnView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_columnView); // Index 3

    m_cableView = new CablePropertiesView(m_model, m_stack);
    connect(m_cableView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_cableView); // Index 4

    m_slabView = new SlabPropertiesView(m_model, m_stack);
    connect(m_slabView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_slabView); // Index 5

    m_wallView = new WallPropertiesView(m_model, m_stack);
    connect(m_wallView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_wallView); // Index 6

    m_foundationView = new FoundationPropertiesView(m_model, m_stack);
    connect(m_foundationView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_foundationView); // Index 7

    m_trussView = new TrussMemberPropertiesView(m_model, m_stack);
    connect(m_trussView, &IElementPropertyView::elementModified, this, &PropertyPanel::elementModified);
    m_stack->addWidget(m_trussView); // Index 8

    containerLayout->addWidget(m_stack);
    scrollContainer->setLayout(containerLayout);
    scrollArea->setWidget(scrollContainer);

    mainLayout->addWidget(scrollArea);
    clearProperties();
}

void PropertyPanel::clearProperties()
{
    m_titleLabel->setText(tr("PROPRIÉTÉS STRUCTURALES"));
    m_stack->setCurrentWidget(m_emptyView);
}

void PropertyPanel::showLevelProperties(const QString& levelId)
{
    clearProperties();
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NIVEAU : %1").arg(levelId));
}

void PropertyPanel::showNodeProperties(int nodeId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DU NŒUD N%1").arg(nodeId));
    m_nodeView->setElementId(nodeId);
    m_stack->setCurrentWidget(m_nodeView);
}

void PropertyPanel::showBeamProperties(int beamId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA POUTRE B%1").arg(beamId));
    m_beamView->setElementId(beamId);
    m_stack->setCurrentWidget(m_beamView);
}

void PropertyPanel::showColumnProperties(int columnId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DU POTEAU C%1").arg(columnId));
    m_columnView->setElementId(columnId);
    m_stack->setCurrentWidget(m_columnView);
}

void PropertyPanel::showCableProperties(int cableId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DU CÂBLE K%1").arg(cableId));
    m_cableView->setElementId(cableId);
    m_stack->setCurrentWidget(m_cableView);
}

void PropertyPanel::showSlabProperties(int slabId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA DALLE S%1").arg(slabId));
    m_slabView->setElementId(slabId);
    m_stack->setCurrentWidget(m_slabView);
}

void PropertyPanel::showWallProperties(int wallId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DU VOILE W%1").arg(wallId));
    m_wallView->setElementId(wallId);
    m_stack->setCurrentWidget(m_wallView);
}

void PropertyPanel::showFoundationProperties(int foundationId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DE LA FONDATION F%1").arg(foundationId));
    m_foundationView->setElementId(foundationId);
    m_stack->setCurrentWidget(m_foundationView);
}

void PropertyPanel::showTrussMemberProperties(int memberId)
{
    m_titleLabel->setText(tr("PROPRIÉTÉS DU TREILLIS T%1").arg(memberId));
    m_trussView->setElementId(memberId);
    m_stack->setCurrentWidget(m_trussView);
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

void PropertyPanel::onModelDiffApplied(const TSA::Model::ModelDiff& /*diff*/)
{
    // Rafraîchir la vue active si son élément a été affecté
    if (m_stack->currentWidget() == m_beamView) m_beamView->refreshView();
    else if (m_stack->currentWidget() == m_columnView) m_columnView->refreshView();
    else if (m_stack->currentWidget() == m_cableView) m_cableView->refreshView();
    else if (m_stack->currentWidget() == m_slabView) m_slabView->refreshView();
    else if (m_stack->currentWidget() == m_wallView) m_wallView->refreshView();
    else if (m_stack->currentWidget() == m_nodeView) m_nodeView->refreshView();
    else if (m_stack->currentWidget() == m_foundationView) m_foundationView->refreshView();
    else if (m_stack->currentWidget() == m_trussView) m_trussView->refreshView();
}

void PropertyPanel::onModelCleared()
{
    clearProperties();
}

} // namespace TSA::UI
