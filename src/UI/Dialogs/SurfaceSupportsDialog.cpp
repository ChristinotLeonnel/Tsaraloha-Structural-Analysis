#include "SurfaceSupportsDialog.h"

#include "../../Model/Model.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace TSA::UI
{

using TSA::Model::SurfaceEdgeSupport;
using TSA::Model::SurfaceTargetKind;

namespace
{
const char* kDof[6] = { "Tx", "Ty", "Tz", "Rx", "Ry", "Rz" };

QString edgeName(SurfaceTargetKind kind, int edge)
{
    if (edge < 0) return QObject::tr("tout le contour");
    if (kind == SurfaceTargetKind::Wall)
    {
        static const char* names[4] = { "pied", "extrémité de fin", "tête", "extrémité de début" };
        return edge < 4 ? QObject::tr(names[edge]) : QObject::tr("bord %1").arg(edge + 1);
    }
    return QObject::tr("bord %1").arg(edge + 1);
}

QString surfaceRef(SurfaceTargetKind kind, int id)
{
    return (kind == SurfaceTargetKind::Wall ? QStringLiteral("W") : QStringLiteral("SL")) + QString::number(id);
}
} // namespace

QString describeEdgeSupport(SurfaceTargetKind kind, const SurfaceEdgeSupport& s)
{
    QStringList fixed, springs;
    for (int d = 0; d < 6; ++d)
    {
        if (s.fixed[d]) fixed << QString::fromLatin1(kDof[d]);
        else if (s.stiffness[d] > 0.0)
            springs << QStringLiteral("k%1 = %2 %3").arg(QString::fromLatin1(kDof[d])).arg(s.stiffness[d], 0, 'g', 6).arg(d < 3 ? QStringLiteral("kN/m/m") : QStringLiteral("kN·m/rad/m"));
    }
    QString t = edgeName(kind, s.edge) + QStringLiteral(" : ");
    t += fixed.isEmpty() ? QObject::tr("aucun DDL bloqué") : QObject::tr("bloqués %1").arg(fixed.join(QStringLiteral(", ")));
    if (!springs.isEmpty()) t += QStringLiteral(" ; ") + springs.join(QStringLiteral(", "));
    return t;
}

SurfaceSupportsDialog::SurfaceSupportsDialog(const TSA::Model::Model& model, const std::vector<std::pair<SurfaceTargetKind, int>>& surfaces, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_surfaces(surfaces)
{
    int maxEdges = 0;
    for (const auto& [kind, id] : surfaces)
    {
        const std::vector<SurfaceEdgeSupport>* list = nullptr;
        if (kind == SurfaceTargetKind::Slab)
        {
            if (const auto* s = model.getSlab(id)) list = &s->edgeSupports(), maxEdges = std::max(maxEdges, int(s->nodeIds().size()));
        }
        else if (const auto* w = model.getWall(id))
            list = &w->edgeSupports(), maxEdges = std::max(maxEdges, 4);
        if (list)
            for (const auto& sp : *list) m_entries.push_back({ kind, id, sp });
    }
    const bool walls = !surfaces.empty() && std::all_of(surfaces.begin(), surfaces.end(), [](const auto& s) { return s.first == SurfaceTargetKind::Wall; });

    setWindowTitle(tr("Appuis linéiques des surfaces (%1 surface(s))").arg(surfaces.size()));
    resize(780, 560);
    auto* v = new QVBoxLayout(this);
    auto* info = new QLabel(tr("Appuis répartis le long d'un bord (ou de tout le contour) : au calcul, chaque nœud du maillage situé sur le bord reçoit les "
                               "blocages choisis et la raideur linéique multipliée par sa longueur d'influence. Axes globaux. Bords d'une dalle : côté "
                               "allant du sommet i au sommet i+1 du contour ; bords d'un voile : pied, extrémité de fin, tête, extrémité de début "
                               "(une porte au pied interrompt l'appui)."),
                            this);
    info->setWordWrap(true);
    v->addWidget(info);
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({ tr("Surface"), tr("Nom"), tr("Appui") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_table, 1);

    auto* form = new QFormLayout;
    m_edge = new QComboBox(this);
    m_edge->addItem(tr("Tout le contour"), -1);
    for (int e = 0; e < maxEdges; ++e) m_edge->addItem(edgeName(walls ? SurfaceTargetKind::Wall : SurfaceTargetKind::Slab, e), e);
    form->addRow(tr("Bord :"), m_edge);
    m_preset = new QComboBox(this);
    m_preset->addItem(tr("Personnalisé"));
    m_preset->addItem(tr("Appui simple (Tz bloqué)"));
    m_preset->addItem(tr("Articulé (Tx, Ty, Tz bloqués)"));
    m_preset->addItem(tr("Encastré (6 DDL bloqués)"));
    m_preset->addItem(tr("Appui élastique vertical (raideur kTz)"));
    form->addRow(tr("Type :"), m_preset);
    auto* grid = new QGridLayout;
    for (int d = 0; d < 6; ++d)
    {
        m_fixed[d] = new QCheckBox(tr("%1 bloqué").arg(QString::fromLatin1(kDof[d])), this);
        m_k[d] = new QDoubleSpinBox(this);
        m_k[d]->setRange(0, 1e12);
        m_k[d]->setDecimals(2);
        m_k[d]->setSuffix(d < 3 ? QStringLiteral(" kN/m/m") : QStringLiteral(" kN·m/rad/m"));
        m_k[d]->setToolTip(tr("Raideur par unité de longueur de bord (0 : libre si non bloqué)"));
        grid->addWidget(m_fixed[d], d, 0);
        grid->addWidget(m_k[d], d, 1);
        connect(m_fixed[d], &QCheckBox::toggled, m_k[d], [this, d](bool on) { m_k[d]->setEnabled(!on); });
    }
    form->addRow(tr("Degrés de liberté :"), grid);
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("ex. appui sur mur porteur, encastrement du voile"));
    form->addRow(tr("Libellé :"), m_name);
    v->addLayout(form);
    connect(m_preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SurfaceSupportsDialog::applyPreset);
    applyPreset(1);
    m_preset->setCurrentIndex(1);

    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("Ajouter à chaque surface sélectionnée"), this);
    auto* remove = new QPushButton(tr("Supprimer"), this);
    row->addWidget(add), row->addWidget(remove), row->addStretch();
    v->addLayout(row);
    connect(add, &QPushButton::clicked, this, &SurfaceSupportsDialog::addFromForm);
    connect(remove, &QPushButton::clicked, this, [this] {
        const int r = m_table->currentRow();
        if (r < 0 || r >= int(m_entries.size())) return;
        m_entries.erase(m_entries.begin() + r);
        fill();
    });
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);
    fill();
}

void SurfaceSupportsDialog::applyPreset(int preset)
{
    if (preset <= 0) return;
    for (int d = 0; d < 6; ++d)
    {
        m_fixed[d]->setChecked((preset == 1 && d == 2) || (preset == 2 && d < 3) || preset == 3);
        m_k[d]->setValue(0);
    }
    if (preset == 4)
    {
        m_fixed[0]->setChecked(true), m_fixed[1]->setChecked(true);
        m_k[2]->setValue(10000.0);
    }
}

void SurfaceSupportsDialog::addFromForm()
{
    SurfaceEdgeSupport sp;
    sp.edge = m_edge->currentData().toInt();
    for (int d = 0; d < 6; ++d)
    {
        sp.fixed[d] = m_fixed[d]->isChecked();
        sp.stiffness[d] = sp.fixed[d] ? 0.0 : m_k[d]->value();
    }
    sp.name = m_name->text().toStdString();
    for (const auto& [kind, id] : m_surfaces) m_entries.push_back({ kind, id, sp });
    fill();
}

std::vector<SurfaceEdgeSupport> SurfaceSupportsDialog::supportsOf(SurfaceTargetKind kind, int id) const
{
    std::vector<SurfaceEdgeSupport> out;
    for (const auto& e : m_entries)
        if (e.kind == kind && e.surfaceId == id) out.push_back(e.support);
    return out;
}

void SurfaceSupportsDialog::fill()
{
    m_table->setRowCount(0);
    for (const auto& e : m_entries)
    {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(surfaceRef(e.kind, e.surfaceId)));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(e.support.name)));
        m_table->setItem(r, 2, new QTableWidgetItem(describeEdgeSupport(e.kind, e.support)));
    }
}

} // namespace TSA::UI
