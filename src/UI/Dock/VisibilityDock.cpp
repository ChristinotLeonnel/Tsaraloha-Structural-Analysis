#include "VisibilityDock.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QAction>

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
    mainLayout->setSpacing(12);

    // Groupe Guides & Repères
    auto* guidesGroup = new QGroupBox(tr("Guides & Repères 3D"), container);
    auto* guidesLayout = new QVBoxLayout(guidesGroup);
    guidesLayout->setSpacing(6);

    m_chkGrid = new QCheckBox(tr("Grille 3D (G)"), guidesGroup);
    m_chkGrid->setChecked(true);
    guidesLayout->addWidget(m_chkGrid);

    m_chkLevels = new QCheckBox(tr("Plans d'étages & Altimétrie"), guidesGroup);
    m_chkLevels->setChecked(true);
    guidesLayout->addWidget(m_chkLevels);

    m_chkLabels = new QCheckBox(tr("Bulles & Libellés d'axes"), guidesGroup);
    m_chkLabels->setChecked(true);
    guidesLayout->addWidget(m_chkLabels);

    m_chkRulers = new QCheckBox(tr("Règles graduées du viewport"), guidesGroup);
    m_chkRulers->setChecked(true);
    guidesLayout->addWidget(m_chkRulers);

    m_chkCoords = new QCheckBox(tr("Repères locaux (LCS)"), guidesGroup);
    m_chkCoords->setChecked(false);
    guidesLayout->addWidget(m_chkCoords);

    mainLayout->addWidget(guidesGroup);

    // Groupe Modèle & Structure
    auto* modelGroup = new QGroupBox(tr("Composants de Structure"), container);
    auto* modelLayout = new QVBoxLayout(modelGroup);
    modelLayout->setSpacing(6);

    m_chkNodes = new QCheckBox(tr("Nœuds structurels"), modelGroup);
    m_chkNodes->setChecked(true);
    modelLayout->addWidget(m_chkNodes);

    m_chkBeams = new QCheckBox(tr("Poutres"), modelGroup);
    m_chkBeams->setChecked(true);
    modelLayout->addWidget(m_chkBeams);

    m_chkColumns = new QCheckBox(tr("Poteaux"), modelGroup);
    m_chkColumns->setChecked(true);
    modelLayout->addWidget(m_chkColumns);

    m_chkSlabs = new QCheckBox(tr("Dalles / Planchers"), modelGroup);
    m_chkSlabs->setChecked(true);
    modelLayout->addWidget(m_chkSlabs);

    mainLayout->addWidget(modelGroup);
    mainLayout->addStretch();

    setWidget(container);
    setMinimumWidth(240);
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

} // namespace TSA::UI
