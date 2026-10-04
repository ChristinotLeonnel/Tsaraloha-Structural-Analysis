#include "VisibilityDock.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QAction>
#include <QLineEdit>
#include <QScrollArea>
#include <QPushButton>
#include <QHBoxLayout>
#include "../../Viewer/OccView.h"

namespace TSA::UI
{

VisibilityDock::VisibilityDock(QWidget* parent)
    : QDockWidget(tr("CALQUES & VISIBILITÉ"), parent)
{
    setObjectName("VisibilityDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUi();
}

void VisibilityDock::setupUi()
{
    auto* container = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Recherche + actions globales
    m_filter = new QLineEdit(container);
    m_filter->setPlaceholderText(tr("Rechercher un calque…"));
    m_filter->setClearButtonEnabled(true);
    mainLayout->addWidget(m_filter);

    auto* bulk = new QHBoxLayout();
    bulk->setSpacing(6);
    auto* btnShowAll = new QPushButton(tr("Tout afficher"), container);
    btnShowAll->setToolTip(tr("Affiche toutes les familles d'éléments de structure"));
    auto* btnHideAll = new QPushButton(tr("Tout masquer"), container);
    btnHideAll->setToolTip(tr("Masque toutes les familles d'éléments de structure (guides et repères inchangés)"));
    bulk->addWidget(btnShowAll);
    bulk->addWidget(btnHideAll);
    mainLayout->addLayout(bulk);
    connect(btnShowAll, &QPushButton::clicked, this, [this] { setAllStructureVisible(true); });
    connect(btnHideAll, &QPushButton::clicked, this, [this] { setAllStructureVisible(false); });
    connect(m_filter, &QLineEdit::textChanged, this, &VisibilityDock::applyFilter);

    auto addCheck = [this](QGroupBox* group, const QString& text, bool checked, bool structure) {
        auto* chk = new QCheckBox(text, group);
        chk->setChecked(checked);
        group->layout()->addWidget(chk);
        m_allChecks.push_back(chk);
        if (structure) m_structureChecks.push_back(chk);
        return chk;
    };

    // Groupe Guides & Repères
    m_guidesGroup = new QGroupBox(tr("Guides & Repères 3D"), container);
    auto* guidesLayout = new QVBoxLayout(m_guidesGroup);
    guidesLayout->setSpacing(6);
    m_chkGrid = addCheck(m_guidesGroup, tr("Grille 3D (G)"), true, false);
    m_chkLevels = addCheck(m_guidesGroup, tr("Plans d'étages & Altimétrie"), true, false);
    m_chkLabels = addCheck(m_guidesGroup, tr("Bulles & Libellés d'axes"), true, false);
    m_chkRulers = addCheck(m_guidesGroup, tr("Règles graduées du viewport"), true, false);
    m_chkCoords = addCheck(m_guidesGroup, tr("Repères locaux (LCS)"), false, false);
    m_chkWorkPlane = addCheck(m_guidesGroup, tr("Plan de travail 3D (W)"), true, false);
    mainLayout->addWidget(m_guidesGroup);

    // Groupe Modèle & Structure
    m_modelGroup = new QGroupBox(tr("Composants de Structure"), container);
    auto* modelLayout = new QVBoxLayout(m_modelGroup);
    modelLayout->setSpacing(6);
    m_chkNodes = addCheck(m_modelGroup, tr("Nœuds structurels"), true, true);
    m_chkNodeLabels = addCheck(m_modelGroup, tr("Numéros des nœuds (labels 3D)"), false, false);
    m_chkLoads = addCheck(m_modelGroup, tr("Charges & Actions (3D)"), true, true);
    m_chkLoadValues = addCheck(m_modelGroup, tr("Valeurs des charges (kN, kN/m)"), true, false);

    using Cat = OccView::ElementCategory;
    auto addElement = [&](const QString& text, Cat cat) {
        auto* chk = addCheck(m_modelGroup, text, true, true);
        connect(chk, &QCheckBox::toggled, this, [this, cat](bool on) { emit elementCategoryToggled(static_cast<int>(cat), on); });
        return chk;
    };
    m_chkBeams = addElement(tr("Poutres"), Cat::Beams);
    m_chkColumns = addElement(tr("Poteaux"), Cat::Columns);
    m_chkSlabs = addElement(tr("Dalles / Planchers"), Cat::Slabs);
    addElement(tr("Voiles"), Cat::Walls);
    addElement(tr("Fondations"), Cat::Foundations);
    addElement(tr("Treillis"), Cat::Trusses);
    addElement(tr("Câbles"), Cat::Cables);
    mainLayout->addWidget(m_modelGroup);
    mainLayout->addStretch();

    // Défilement vertical : la liste des familles ne doit jamais être coupée dans un dock bas.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(container);
    setWidget(scroll);
    setMinimumWidth(240);
}

void VisibilityDock::setAllStructureVisible(bool visible)
{
    for (auto* chk : m_structureChecks) chk->setChecked(visible);
}

void VisibilityDock::applyFilter(const QString& text)
{
    const QString needle = text.trimmed();
    for (auto* group : { m_guidesGroup, m_modelGroup })
    {
        bool any = false;
        for (auto* chk : group->findChildren<QCheckBox*>())
        {
            const bool show = needle.isEmpty() || chk->text().contains(needle, Qt::CaseInsensitive);
            chk->setVisible(show);
            any = any || show;
        }
        group->setVisible(any);
    }
}

void VisibilityDock::bindGridVisibleAction(QAction* act)
{
    if (!act || !m_chkGrid) return;
    m_chkGrid->setChecked(act->isChecked());
    connect(m_chkGrid, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkGrid, &QCheckBox::setChecked);
}

void VisibilityDock::bindLevelsVisibleAction(QAction* act)
{
    if (!act || !m_chkLevels) return;
    m_chkLevels->setChecked(act->isChecked());
    connect(m_chkLevels, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkLevels, &QCheckBox::setChecked);
}

void VisibilityDock::bindGridLabelsAction(QAction* act)
{
    if (!act || !m_chkLabels) return;
    m_chkLabels->setChecked(act->isChecked());
    connect(m_chkLabels, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkLabels, &QCheckBox::setChecked);
}

void VisibilityDock::bindRulersVisibleAction(QAction* act)
{
    if (!act || !m_chkRulers) return;
    m_chkRulers->setChecked(act->isChecked());
    connect(m_chkRulers, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkRulers, &QCheckBox::setChecked);
}

void VisibilityDock::bindCoordSystemAction(QAction* act)
{
    if (!act || !m_chkCoords) return;
    m_chkCoords->setChecked(act->isChecked());
    connect(m_chkCoords, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkCoords, &QCheckBox::setChecked);
}

void VisibilityDock::bindWorkPlaneVisibleAction(QAction* act)
{
    if (!act || !m_chkWorkPlane) return;
    m_chkWorkPlane->setChecked(act->isChecked());
    connect(m_chkWorkPlane, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkWorkPlane, &QCheckBox::setChecked);
}

void VisibilityDock::bindNodesVisibleAction(QAction* act)
{
    if (!act || !m_chkNodes) return;
    m_chkNodes->setChecked(act->isChecked());
    connect(m_chkNodes, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkNodes, &QCheckBox::setChecked);
}

void VisibilityDock::bindNodeLabelsAction(QAction* act)
{
    if (!act || !m_chkNodeLabels) return;
    m_chkNodeLabels->setChecked(act->isChecked());
    connect(m_chkNodeLabels, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkNodeLabels, &QCheckBox::setChecked);
}

void VisibilityDock::bindLoadsVisibleAction(QAction* act)
{
    if (!act || !m_chkLoads) return;
    m_chkLoads->setChecked(act->isChecked());
    connect(m_chkLoads, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkLoads, &QCheckBox::setChecked);
}

void VisibilityDock::bindLoadValuesVisibleAction(QAction* act)
{
    if (!act || !m_chkLoadValues) return;
    m_chkLoadValues->setChecked(act->isChecked());
    connect(m_chkLoadValues, &QCheckBox::toggled, act, &QAction::setChecked);
    connect(act, &QAction::toggled, m_chkLoadValues, &QCheckBox::setChecked);
}

} // namespace TSA::UI
