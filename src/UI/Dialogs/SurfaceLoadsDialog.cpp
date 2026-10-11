#include "SurfaceLoadsDialog.h"

#include "../../Model/Load/LoadManager.h"
#include "../../Model/Model.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <set>

namespace TSA::UI
{

using TSA::Model::SurfaceLoad;
using TSA::Model::SurfaceLoadDirection;
using TSA::Model::SurfaceLoadDistribution;
using TSA::Model::SurfaceTargetKind;

namespace
{
QString directionName(SurfaceLoadDirection d)
{
    switch (d)
    {
    case SurfaceLoadDirection::GlobalX: return QObject::tr("X global");
    case SurfaceLoadDirection::GlobalY: return QObject::tr("Y global");
    case SurfaceLoadDirection::GlobalZ: return QObject::tr("Z global");
    case SurfaceLoadDirection::LocalNormal: return QObject::tr("normale locale (−N)");
    default: return QObject::tr("gravité (−Z)");
    }
}
} // namespace

QString describeSurfaceLoad(const TSA::Model::Model& model, const SurfaceLoad& l)
{
    const auto* lc = model.loadManager().getLoadCase(l.loadCaseId);
    const bool point = l.distribution == SurfaceLoadDistribution::Point;
    QString s = QStringLiteral("%1 %2 %3, %4").arg(point ? QObject::tr("P =") : QObject::tr("q ="), QString::number(l.value, 'f', 2),
                                                   point ? QStringLiteral("kN") : QStringLiteral("kN/m²"), directionName(l.direction));
    if (point) s += QObject::tr(" en (u = %1 ; v = %2) m").arg(l.u, 0, 'f', 2).arg(l.v, 0, 'f', 2);
    else
    {
        if (l.gradU != 0.0 || l.gradV != 0.0)
            s += QObject::tr(" + %1·u + %2·v (kN/m³)").arg(l.gradU, 0, 'g', 4).arg(l.gradV, 0, 'g', 4);
        if (l.zone)
            s += QObject::tr(", zone u ∈ [%1 ; %2], v ∈ [%3 ; %4] m").arg(l.zoneU0, 0, 'f', 2).arg(l.zoneU1, 0, 'f', 2).arg(l.zoneV0, 0, 'f', 2).arg(l.zoneV1, 0, 'f', 2);
        if (l.projected) s += QObject::tr(", surface projetée");
    }
    s += QObject::tr(" — cas %1").arg(lc ? QString::fromStdString(lc->name()) : QString::number(l.loadCaseId));
    return s;
}

SurfaceLoadsDialog::SurfaceLoadsDialog(const TSA::Model::Model& model, const std::vector<std::pair<SurfaceTargetKind, int>>& surfaces, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_surfaces(surfaces)
{
    std::set<std::pair<SurfaceTargetKind, int>> targets(surfaces.begin(), surfaces.end());
    for (const auto& [id, l] : model.loadManager().surfaceLoads())
        if (targets.count({ l.targetKind, l.surfaceId })) m_loads.push_back(l);
    setWindowTitle(tr("Charges surfaciques (%1 surface(s))").arg(surfaces.size()));
    resize(760, 520);
    auto* v = new QVBoxLayout(this);
    auto* info = new QLabel(tr("Charges appliquées aux dalles et voiles sélectionnés. Au calcul, elles sont converties en charges nodales cohérentes "
                               "sur le maillage (∫ Nᵢ p dA), résultante et moment contrôlés ; les ouvertures sont déduites. Le poids propre des "
                               "surfaces est calculé automatiquement (ρ·g·e) pour les cas qui l'incluent : ne pas le saisir ici en plus."),
                            this);
    info->setWordWrap(true);
    v->addWidget(info);
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({ tr("Surface"), tr("Nom"), tr("Charge") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_table, 1);

    auto* form = new QFormLayout;
    m_case = new QComboBox(this);
    for (const auto& [id, lc] : model.loadManager().loadCases()) m_case->addItem(QString::fromStdString(lc.name()), id);
    if (int i = m_case->findData(model.loadManager().activeLoadCaseId()); i >= 0) m_case->setCurrentIndex(i);
    form->addRow(tr("Cas de charge :"), m_case);
    m_distribution = new QComboBox(this);
    m_distribution->addItem(tr("Uniforme sur la surface nette (kN/m²)"), int(SurfaceLoadDistribution::Uniform));
    m_distribution->addItem(tr("Ponctuelle en un point de la surface (kN)"), int(SurfaceLoadDistribution::Point));
    form->addRow(tr("Répartition :"), m_distribution);
    m_direction = new QComboBox(this);
    for (auto d : { SurfaceLoadDirection::Gravity, SurfaceLoadDirection::LocalNormal, SurfaceLoadDirection::GlobalX, SurfaceLoadDirection::GlobalY,
                    SurfaceLoadDirection::GlobalZ })
        m_direction->addItem(directionName(d), int(d));
    form->addRow(tr("Direction :"), m_direction);
    m_value = new QDoubleSpinBox(this);
    m_value->setRange(-1e6, 1e6);
    m_value->setDecimals(3);
    m_value->setValue(2.5);
    m_value->setSuffix(QStringLiteral(" kN/m²"));
    form->addRow(tr("Valeur :"), m_value);
    m_projected = new QCheckBox(tr("Intensité rapportée à la surface projetée (direction globale ; toiture inclinée, neige)"), this);
    form->addRow(QString(), m_projected);
    auto* pos = new QHBoxLayout;
    m_u = new QDoubleSpinBox(this);
    m_v = new QDoubleSpinBox(this);
    for (auto* s : { m_u, m_v }) s->setRange(-1e5, 1e5), s->setDecimals(3), s->setSuffix(QStringLiteral(" m")), pos->addWidget(s);
    form->addRow(tr("Position (u ; v), repère local :"), pos);
    auto* grad = new QHBoxLayout;
    m_gradU = new QDoubleSpinBox(this);
    m_gradV = new QDoubleSpinBox(this);
    for (auto* s : { m_gradU, m_gradV }) s->setRange(-1e5, 1e5), s->setDecimals(3), s->setSuffix(QStringLiteral(" kN/m³")), grad->addWidget(s);
    m_gradU->setToolTip(tr("Variation de l'intensité le long de u (repère local)"));
    m_gradV->setToolTip(tr("Variation de l'intensité le long de v ; pression hydrostatique sur un voile : valeur = γ·h, gradient v = −γ, zone v ≤ h"));
    form->addRow(tr("Variation linéaire (g_u ; g_v) :"), grad);
    m_zone = new QCheckBox(tr("Limiter à une zone rectangulaire du repère local"), this);
    form->addRow(QString(), m_zone);
    auto* zone = new QHBoxLayout;
    m_zu0 = new QDoubleSpinBox(this), m_zv0 = new QDoubleSpinBox(this), m_zu1 = new QDoubleSpinBox(this), m_zv1 = new QDoubleSpinBox(this);
    for (auto* s : { m_zu0, m_zv0, m_zu1, m_zv1 }) s->setRange(-1e5, 1e5), s->setDecimals(3), s->setSuffix(QStringLiteral(" m")), zone->addWidget(s);
    m_zu1->setValue(1.0), m_zv1->setValue(1.0);
    form->addRow(tr("Zone u₀, v₀, u₁, v₁ :"), zone);
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("ex. Exploitation bureaux, cloisons, vent façade"));
    form->addRow(tr("Libellé :"), m_name);
    v->addLayout(form);
    auto updateForm = [this] {
        const bool point = m_distribution->currentData().toInt() == int(SurfaceLoadDistribution::Point);
        m_value->setSuffix(point ? QStringLiteral(" kN") : QStringLiteral(" kN/m²"));
        m_u->setEnabled(point), m_v->setEnabled(point);
        m_projected->setEnabled(!point);
        m_gradU->setEnabled(!point), m_gradV->setEnabled(!point), m_zone->setEnabled(!point);
        for (auto* s : { m_zu0, m_zv0, m_zu1, m_zv1 }) s->setEnabled(!point && m_zone->isChecked());
    };
    connect(m_zone, &QCheckBox::toggled, this, updateForm);
    connect(m_distribution, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateForm);
    updateForm();

    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("Ajouter à chaque surface sélectionnée"), this);
    auto* dup = new QPushButton(tr("Dupliquer"), this);
    auto* remove = new QPushButton(tr("Supprimer"), this);
    row->addWidget(add), row->addWidget(dup), row->addWidget(remove), row->addStretch();
    v->addLayout(row);
    connect(add, &QPushButton::clicked, this, &SurfaceLoadsDialog::addFromForm);
    connect(dup, &QPushButton::clicked, this, [this] {
        const int r = m_table->currentRow();
        if (r < 0 || r >= int(m_loads.size())) return;
        SurfaceLoad c = m_loads[r];
        c.id = 0;
        m_loads.push_back(c);
        fill();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int r = m_table->currentRow();
        if (r < 0 || r >= int(m_loads.size())) return;
        m_loads.erase(m_loads.begin() + r);
        fill();
    });
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);
    fill();
}

void SurfaceLoadsDialog::addFromForm()
{
    for (const auto& [kind, id] : m_surfaces)
    {
        SurfaceLoad l;
        l.targetKind = kind;
        l.surfaceId = id;
        l.loadCaseId = m_case->currentData().toInt();
        l.name = m_name->text().toStdString();
        l.distribution = static_cast<SurfaceLoadDistribution>(m_distribution->currentData().toInt());
        l.direction = static_cast<SurfaceLoadDirection>(m_direction->currentData().toInt());
        l.value = m_value->value();
        l.projected = m_projected->isChecked() && l.distribution == SurfaceLoadDistribution::Uniform;
        l.u = m_u->value();
        l.v = m_v->value();
        if (l.distribution == SurfaceLoadDistribution::Uniform)
        {
            l.gradU = m_gradU->value();
            l.gradV = m_gradV->value();
            l.zone = m_zone->isChecked();
            if (l.zone) l.zoneU0 = m_zu0->value(), l.zoneV0 = m_zv0->value(), l.zoneU1 = m_zu1->value(), l.zoneV1 = m_zv1->value();
        }
        m_loads.push_back(l);
    }
    fill();
}

void SurfaceLoadsDialog::fill()
{
    m_table->setRowCount(0);
    for (const auto& l : m_loads)
    {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(l.surfaceRef()) + (l.id ? QString() : tr(" (nouvelle)"))));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(l.name)));
        m_table->setItem(r, 2, new QTableWidgetItem(describeSurfaceLoad(m_model, l)));
    }
}

} // namespace TSA::UI
