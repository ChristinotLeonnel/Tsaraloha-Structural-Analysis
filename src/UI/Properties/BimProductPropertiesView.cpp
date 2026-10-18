#include "BimProductPropertiesView.h"

#include "../../BIM/Core/BimModel.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Model.h"

#include <QHeaderView>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace TSA::UI
{

using TSA::BIM::AnalyticalStatus;

namespace
{
QString statusText(AnalyticalStatus s)
{
    switch (s)
    {
    case AnalyticalStatus::Analytical: return QObject::tr("Relié au calcul");
    case AnalyticalStatus::NeedsPreparation: return QObject::tr("À préparer : aucun élément de calcul (barres, surfaces, appuis à définir)");
    case AnalyticalStatus::NotStructural: return QObject::tr("Non porteur : hors du modèle de calcul");
    }
    return {};
}

QTreeWidgetItem* row(QTreeWidgetItem* parent, const QString& key, const QString& value = {})
{
    auto* item = new QTreeWidgetItem(parent, { key, value });
    item->setToolTip(1, value);
    return item;
}
} // namespace

BimProductPropertiesView::BimProductPropertiesView(TSA::Model::Model* model, QWidget* parent) : QWidget(parent), m_model(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({ tr("Propriété"), tr("Valeur") });
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::ContiguousSelection);   // copie possible (Ctrl+C du système)
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->setMinimumHeight(360);
    layout->addWidget(m_tree);
}

bool BimProductPropertiesView::setProduct(int productId)
{
    m_tree->clear();
    const auto* e = m_model ? m_model->bim().element(productId) : nullptr;
    if (!e) return false;

    auto* identity = new QTreeWidgetItem(m_tree, { tr("Identité") });
    row(identity, tr("Classe IFC"), QString::fromStdString(e->ifcClass.empty() ? std::string("—") : e->ifcClass));
    row(identity, tr("Nom"), QString::fromStdString(e->name));
    if (!e->objectType.empty()) row(identity, tr("Type d'objet"), QString::fromStdString(e->objectType));
    if (!e->predefinedType.empty()) row(identity, tr("Type prédéfini"), QString::fromStdString(e->predefinedType));
    if (!e->tag.empty()) row(identity, tr("Repère"), QString::fromStdString(e->tag));
    if (!e->description.empty()) row(identity, tr("Description"), QString::fromStdString(e->description));
    row(identity, tr("GlobalId"), QString::fromStdString(e->globalId));
    QString storey = QString::fromStdString(m_model->bim().resolvedStorey(*m_model, *e));
    if (const auto* lm = m_model->levelManager(); lm && !storey.isEmpty())
        if (const auto* level = lm->getLevel(storey.toStdString())) storey = QString::fromStdString(level->name);
    row(identity, tr("Étage"), storey.isEmpty() ? QStringLiteral("—") : storey);

    auto* analysis = new QTreeWidgetItem(m_tree, { tr("Calcul") });
    row(analysis, tr("État"), statusText(e->status));
    if (!e->analytical.empty())
    {
        QStringList refs;
        for (const auto& r : e->analytical) refs << QString::fromStdString(r.label());
        row(analysis, tr("Éléments de calcul"), refs.join(", "));
    }
    if (e->geometry)
        row(analysis, tr("Géométrie importée"),
            tr("%1 triangles (%2)").arg(e->geometry->triangleCount()).arg(QString::fromStdString(e->geometry->representation)));

    if (!e->classifications.empty())
    {
        auto* classification = new QTreeWidgetItem(m_tree, { tr("Classification") });
        for (const auto& c : e->classifications)
            row(classification, QString::fromStdString(c.system), QString::fromStdString(c.identification + (c.name.empty() ? "" : " — " + c.name)));
    }
    if (!e->importNotes.empty())
    {
        auto* notes = new QTreeWidgetItem(m_tree, { tr("Remarques d'import (%1)").arg(e->importNotes.size()) });
        for (const auto& n : e->importNotes) row(notes, QStringLiteral("•"), QString::fromStdString(n));
    }
    for (const auto& pset : e->propertySets)
    {
        auto* set = new QTreeWidgetItem(m_tree, { QString::fromStdString(pset.name) });
        for (const auto& [name, value] : pset.properties) row(set, QString::fromStdString(name), QString::fromStdString(value.toString()));
    }
    m_tree->expandToDepth(0);
    return true;
}

} // namespace TSA::UI
