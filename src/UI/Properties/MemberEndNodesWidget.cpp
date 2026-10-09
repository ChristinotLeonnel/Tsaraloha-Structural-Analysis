#include "MemberEndNodesWidget.h"

#include "../../Model/Model.h"
#include "../../Model/SupportDefinition.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>

namespace TSA::UI
{

namespace
{
using TSA::Model::SupportDefinition;
using TSA::Model::SupportType;

// Appuis proposés directement depuis la barre ; les autres (glissant, élastique, personnalisé…) se
// règlent dans les propriétés du nœud, ils sont seulement affichés ici.
constexpr int kOtherSupport = -1;

bool presetFor(SupportType type, SupportDefinition& out)
{
    switch (type)
    {
    case SupportType::Free: out = SupportDefinition::free(); return true;
    case SupportType::Fixed: out = SupportDefinition::fixed(); return true;
    case SupportType::Pinned: out = SupportDefinition::pinned(); return true;
    case SupportType::Roller: out = SupportDefinition::roller(); return true;
    default: return false;
    }
}
} // namespace

MemberEndNodesWidget::MemberEndNodesWidget(QWidget* parent)
    : QGroupBox(tr("NŒUDS D'EXTRÉMITÉ"), parent)
{
    setStyleSheet("QGroupBox { font-weight: bold; color: #38bdf8; border: 1px solid #334155; margin-top: 6px; "
                  "padding-top: 8px; border-radius: 4px; }");
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1);

    for (int end = 0; end < 2; ++end)
    {
        Row& r = m_rows[end];
        r.title = new QLabel(this);
        r.title->setStyleSheet("color: #94a3b8; font-weight: normal;");
        r.node = new QLabel(this);
        r.node->setStyleSheet("font-family: Consolas, monospace; color: #f8fafc; font-weight: normal;");
        r.node->setTextInteractionFlags(Qt::TextSelectableByMouse);
        r.support = new QComboBox(this);
        r.support->setToolTip(tr("Appui du nœud : modifié dans le modèle (Ctrl+Z pour annuler), visible partout où le nœud apparaît."));
        r.open = new QToolButton(this);
        r.open->setText(QStringLiteral("↗"));
        r.open->setAutoRaise(true);
        r.open->setToolTip(tr("Afficher les propriétés complètes du nœud (orientation, ressorts, degrés de liberté)"));

        // Bouton du nœud à gauche, sous le libellé : le panneau peut être plus étroit que son contenu,
        // une colonne à droite serait coupée.
        const int row = end * 2;
        grid->addWidget(r.title, row, 0);
        grid->addWidget(r.node, row, 1);
        grid->addWidget(r.open, row + 1, 0, Qt::AlignRight);
        grid->addWidget(r.support, row + 1, 1);

        connect(r.support, &QComboBox::activated, this, [this, end] { onSupportChosen(end); });
        connect(r.open, &QToolButton::clicked, this, [this, end] {
            if (m_nodeIds[end] > 0) emit nodeRequested(m_nodeIds[end]);
        });
    }
    setEndLabels(tr("Début :"), tr("Fin :"));
}

void MemberEndNodesWidget::setEndLabels(const QString& startLabel, const QString& endLabel)
{
    m_rows[0].title->setText(startLabel);
    m_rows[1].title->setText(endLabel);
}

void MemberEndNodesWidget::setNodes(int startNodeId, int endNodeId)
{
    m_nodeIds[0] = startNodeId;
    m_nodeIds[1] = endNodeId;
    refreshRow(0);
    refreshRow(1);
}

void MemberEndNodesWidget::refreshRow(int end)
{
    Row& r = m_rows[end];
    const TSA::Model::Node* node = m_model ? m_model->getNode(m_nodeIds[end]) : nullptr;
    m_loading = true;
    r.support->clear();
    if (!node)
    {
        r.node->setText(m_nodeIds[end] > 0 ? tr("N%1 (introuvable)").arg(m_nodeIds[end]) : QStringLiteral("-"));
        r.support->setEnabled(false);
        r.open->setEnabled(false);
        m_loading = false;
        return;
    }

    r.node->setText(QStringLiteral("N%1  (%2 ; %3 ; %4) m")
                        .arg(node->id())
                        .arg(node->x(), 0, 'f', 2)
                        .arg(node->y(), 0, 'f', 2)
                        .arg(node->z(), 0, 'f', 2));
    r.support->addItem(tr("Libre (aucun appui)"), static_cast<int>(SupportType::Free));
    r.support->addItem(tr("Encastrement"), static_cast<int>(SupportType::Fixed));
    r.support->addItem(tr("Articulation"), static_cast<int>(SupportType::Pinned));
    r.support->addItem(tr("Appui simple"), static_cast<int>(SupportType::Roller));

    const SupportDefinition& current = node->support();
    SupportDefinition preset;
    int index = r.support->findData(static_cast<int>(current.supportType()));
    if (index < 0 || !presetFor(current.supportType(), preset) || !(preset == current))
    {
        // Appui réglé dans le nœud (orientation, ressorts, DDL) : montré tel quel, jamais écrasé ici.
        r.support->addItem(tr("%1 (voir le nœud)").arg(QString::fromStdString(current.typeName())), kOtherSupport);
        index = r.support->count() - 1;
    }
    r.support->setCurrentIndex(index);
    r.support->setEnabled(true);
    r.open->setEnabled(true);
    m_loading = false;
}

void MemberEndNodesWidget::onSupportChosen(int end)
{
    if (m_loading || !m_model) return;
    TSA::Model::Node* node = m_model->getNode(m_nodeIds[end]);
    const int choice = m_rows[end].support->currentData().toInt();
    SupportDefinition chosen;
    if (!node || choice == kOtherSupport || !presetFor(static_cast<SupportType>(choice), chosen) || chosen == node->support())
        return;

    const std::string label = tr("Appui : %1 (N%2)").arg(QString::fromStdString(chosen.typeName())).arg(node->id()).toStdString();
    m_model->pushUndoState(label, label);
    node->setSupport(chosen);
    // Notification du nœud : vue 3D, arbre, panneau (cette barre comprise) et résultats invalidés.
    m_model->notifyNodeModified(node->id());
}

} // namespace TSA::UI
