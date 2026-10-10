#include "MeshDialogs.h"

#include "../../Mesh/MeshGeometry.h"
#include "../../Model/Model.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace TSA::UI
{

using namespace TSA::Mesh;

namespace
{
QDoubleSpinBox* dspin(QWidget* p, double min, double max, int dec, const QString& suffix, double value)
{
    auto* s = new QDoubleSpinBox(p);
    s->setRange(min, max);
    s->setDecimals(dec);
    s->setSuffix(suffix);
    s->setValue(value);
    return s;
}
} // namespace

// --- Paramètres locaux ------------------------------------------------------------------------------

LocalMeshDialog::LocalMeshDialog(const std::vector<std::string>& refs, const LocalMeshSetting* ex, QWidget* parent)
    : QDialog(parent)
    , m_refs(refs)
{
    for (const auto& r : refs)
    {
        const char c = r.empty() ? ' ' : r[0];
        if (r.rfind("SL", 0) == 0 || c == 'W') m_surfaces = true;
        else if (c == 'F') m_volumes = true;
        else m_members = true;
    }
    setWindowTitle(tr("Maillage local de la sélection"));
    auto* v = new QVBoxLayout(this);
    QStringList shown;
    for (std::size_t i = 0; i < refs.size() && i < 12; ++i) shown << QString::fromStdString(refs[i]);
    if (refs.size() > 12) shown << tr("… (%1 au total)").arg(refs.size());
    auto* info = new QLabel(tr("Objets : %1\nLes autres objets gardent leurs paramètres.").arg(shown.join(QStringLiteral(", "))), this);
    info->setWordWrap(true);
    v->addWidget(info);
    auto* f = new QFormLayout;
    m_name = new QLineEdit(ex ? QString::fromStdString(ex->name) : tr("local"), this);
    f->addRow(tr("Nom :"), m_name);
    m_useSize = new QCheckBox(tr("Taille des éléments"), this);
    m_size = dspin(this, 0.001, 100, 3, tr(" m"), ex && ex->size ? *ex->size : 0.25);
    m_useSize->setChecked(ex && ex->size);
    f->addRow(m_useSize, m_size);
    v->addLayout(f);

    if (m_members)
    {
        auto* g = new QGroupBox(tr("Barres (1D)"), this);
        auto* gf = new QFormLayout(g);
        m_useDiv = new QCheckBox(tr("Nombre de divisions"), g);
        m_div = new QSpinBox(g);
        m_div->setRange(1, 100000);
        m_div->setValue(ex && ex->divisions ? *ex->divisions : 10);
        m_useDiv->setChecked(ex && ex->divisions);
        gf->addRow(m_useDiv, m_div);
        m_dist = new QComboBox(g);
        m_dist->addItems({ tr("Uniforme"), tr("Progressive (depuis le nœud de départ)"), tr("Symétrique (raffinée aux deux extrémités)") });
        m_dist->setCurrentIndex(ex ? int(ex->distribution) : 0);
        gf->addRow(tr("Répartition :"), m_dist);
        m_prog = dspin(g, 1.0, 10, 3, QString(), ex ? ex->progression : 1.2);
        gf->addRow(tr("Progression :"), m_prog);
        m_useMax = new QCheckBox(tr("Longueur maximale"), g);
        m_max = dspin(g, 0.001, 100, 3, tr(" m"), ex && ex->maxLength ? *ex->maxLength : 0.5);
        m_useMax->setChecked(ex && ex->maxLength);
        gf->addRow(m_useMax, m_max);
        m_useEnd = new QCheckBox(tr("Raffinement aux extrémités"), g);
        m_end = dspin(g, 0.001, 100, 3, tr(" m"), ex && ex->endSize ? *ex->endSize : 0.1);
        m_useEnd->setChecked(ex && ex->endSize);
        gf->addRow(m_useEnd, m_end);
        m_usePortion = new QCheckBox(tr("Taille sur une portion"), g);
        auto* row = new QHBoxLayout;
        m_pFrom = dspin(g, 0, 1, 3, QString(), ex && !ex->portions.empty() ? ex->portions[0].from : 0.4);
        m_pTo = dspin(g, 0, 1, 3, QString(), ex && !ex->portions.empty() ? ex->portions[0].to : 0.6);
        m_pSize = dspin(g, 0.001, 100, 3, tr(" m"), ex && !ex->portions.empty() ? ex->portions[0].size : 0.1);
        row->addWidget(new QLabel(tr("de"), g)), row->addWidget(m_pFrom), row->addWidget(new QLabel(tr("à"), g)), row->addWidget(m_pTo);
        row->addWidget(new QLabel(tr("(relatif), taille"), g)), row->addWidget(m_pSize);
        m_usePortion->setChecked(ex && !ex->portions.empty());
        gf->addRow(m_usePortion, row);
        v->addWidget(g);
    }
    if (m_surfaces)
    {
        auto* g = new QGroupBox(tr("Surfaces (2D)"), this);
        auto* gf = new QFormLayout(g);
        m_useUV = new QCheckBox(tr("Divisions par direction (surfaces structurées)"), g);
        auto* row = new QHBoxLayout;
        m_divU = new QSpinBox(g);
        m_divV = new QSpinBox(g);
        m_divU->setRange(1, 10000), m_divV->setRange(1, 10000);
        m_divU->setValue(ex && ex->divisionsU ? *ex->divisionsU : 10), m_divV->setValue(ex && ex->divisionsV ? *ex->divisionsV : 10);
        row->addWidget(m_divU), row->addWidget(new QLabel(QStringLiteral("×"), g)), row->addWidget(m_divV);
        m_useUV->setChecked(ex && (ex->divisionsU || ex->divisionsV));
        gf->addRow(m_useUV, row);
        m_shape = new QComboBox(g);
        m_shape->addItems({ tr("Réglage global"), tr("Triangles"), tr("Quadrangles") });
        m_shape->setCurrentIndex(ex && ex->shape ? (*ex->shape == SurfaceShape::Triangles ? 1 : 2) : 0);
        gf->addRow(tr("Éléments :"), m_shape);
        m_structured = new QComboBox(g);
        m_structured->addItems({ tr("Réglage global"), tr("Imposer le maillage structuré"), tr("Non structuré") });
        m_structured->setCurrentIndex(ex && ex->structured ? (*ex->structured ? 1 : 2) : 0);
        m_structured->setToolTip(tr("Structuré : surfaces à 4 côtés sans ouverture ni nœud plongé uniquement ; sinon le maillage reste non structuré et c'est signalé"));
        gf->addRow(tr("Structure :"), m_structured);
        v->addWidget(g);
    }
    if (m_volumes) v->addWidget(new QLabel(tr("Volumes : la taille locale s'applique aux semelles sélectionnées."), this));
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);
}

LocalMeshSetting LocalMeshDialog::result() const
{
    LocalMeshSetting l;
    l.name = m_name->text().toStdString();
    l.targets = m_refs;
    if (m_useSize->isChecked()) l.size = m_size->value();
    if (m_members)
    {
        if (m_useDiv->isChecked()) l.divisions = m_div->value();
        l.distribution = static_cast<Distribution>(m_dist->currentIndex());
        l.progression = m_prog->value();
        if (m_useMax->isChecked()) l.maxLength = m_max->value();
        if (m_useEnd->isChecked()) l.endSize = m_end->value();
        if (m_usePortion->isChecked() && m_pTo->value() > m_pFrom->value()) l.portions.push_back({ m_pFrom->value(), m_pTo->value(), m_pSize->value() });
    }
    if (m_surfaces)
    {
        if (m_useUV->isChecked()) l.divisionsU = m_divU->value(), l.divisionsV = m_divV->value();
        if (m_shape->currentIndex() > 0) l.shape = m_shape->currentIndex() == 1 ? SurfaceShape::Triangles : SurfaceShape::Quadrilaterals;
        if (m_structured->currentIndex() > 0) l.structured = m_structured->currentIndex() == 1;
    }
    return l;
}

// --- Qualité ----------------------------------------------------------------------------------------

MeshQualityPanel::MeshQualityPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    v->addWidget(m_summary);
    auto* row = new QHBoxLayout;
    m_filter = new QComboBox(this);
    m_filter->addItems({ tr("Tous les problèmes"), tr("Erreurs"), tr("Avertissements") });
    row->addWidget(new QLabel(tr("Afficher :"), this));
    row->addWidget(m_filter, 1);
    v->addLayout(row);
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({ tr("Gravité"), tr("Type"), tr("Objet"), tr("Élément / nœud"), tr("Message") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->hide();
    v->addWidget(m_table, 1);
    auto* note = new QLabel(tr("Une génération sans erreur ne prouve pas la convergence : comparez les résultats obtenus avec plusieurs finesses."), this);
    note->setWordWrap(true);
    v->addWidget(note);
    connect(m_filter, &QComboBox::currentIndexChanged, this, &MeshQualityPanel::fill);
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row) {
        if (row < 0) return;
        auto* it = m_table->item(row, 0);
        if (it) emit issueActivated(it->data(Qt::UserRole).toInt(), it->data(Qt::UserRole + 1).toInt());
    });
}

void MeshQualityPanel::setReport(const QualityReport& r, const FeMesh* mesh, const QString& context)
{
    m_report = r;
    if (!mesh)
    {
        m_summary->setText(tr("<i>Aucun maillage : générez le maillage puis vérifiez sa qualité.</i>"));
        m_table->setRowCount(0);
        return;
    }
    auto stat = [](const MetricStats& s, const char* unit) {
        return s.count ? QStringLiteral("%1 … %2 %3 (moy. %4)").arg(s.min, 0, 'f', 2).arg(s.max, 0, 'f', 2).arg(QString::fromUtf8(unit)).arg(s.mean, 0, 'f', 2)
                       : QStringLiteral("—");
    };
    m_summary->setText(tr("<b>%1</b><br>%2 nœuds, %3 éléments (1D %4, 2D %5, 3D %6), ≈ %7 DDL<br>"
                          "<b>%8 erreur(s)</b>, %9 avertissement(s)<br>"
                          "Rapport d'aspect : %10<br>Angles min / max : %11 / %12<br>Jacobien / minSICN : %13<br>Rapport de tailles 1D : %14")
                           .arg(context)
                           .arg(r.nodes)
                           .arg(r.elements)
                           .arg(r.elements1D)
                           .arg(r.elements2D)
                           .arg(r.elements3D)
                           .arg(r.estimatedDofs)
                           .arg(r.errors)
                           .arg(r.warnings)
                           .arg(stat(r.aspect, ""), stat(r.minAngle, "°"), stat(r.maxAngle, "°"), stat(r.jacobian, ""), stat(r.growth, "")));
    fill();
}

void MeshQualityPanel::fill()
{
    m_table->setRowCount(0);
    const int filter = m_filter->currentIndex();
    for (const auto& i : m_report.issues)
    {
        if (filter == 1 && i.severity != QualityIssue::Severity::Error) continue;
        if (filter == 2 && i.severity != QualityIssue::Severity::Warning) continue;
        const int r = m_table->rowCount();
        if (r >= 5000) break;   // liste interactive bornée ; la synthèse donne les totaux
        m_table->insertRow(r);
        auto* sev = new QTableWidgetItem(QString::fromUtf8(severityName(i.severity)));
        sev->setData(Qt::UserRole, i.element);
        sev->setData(Qt::UserRole + 1, i.node);
        if (i.severity == QualityIssue::Severity::Error) sev->setForeground(QColor(220, 60, 60));
        m_table->setItem(r, 0, sev);
        m_table->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(i.kind)));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(i.source)));
        m_table->setItem(r, 3, new QTableWidgetItem(i.element ? tr("élément %1").arg(i.element) : i.node ? tr("nœud %1").arg(i.node) : QString()));
        m_table->setItem(r, 4, new QTableWidgetItem(QString::fromStdString(i.message)));
    }
    m_table->resizeColumnsToContents();
}

// --- Ouvertures de dalle ----------------------------------------------------------------------------

SlabOpeningsDialog::SlabOpeningsDialog(const TSA::Model::Model& model, int slabId, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_slabId(slabId)
{
    const auto* slab = model.getSlab(slabId);
    if (slab) m_openings = slab->openings();
    setWindowTitle(tr("Ouvertures de la dalle %1").arg(slab ? QString::fromStdString(slab->formattedName()) : QString::number(slabId)));
    resize(560, 360);
    auto* v = new QVBoxLayout(this);
    auto* info = new QLabel(tr("Repère local de la dalle : origine au premier nœud, axe u vers le deuxième nœud, v perpendiculaire dans le plan. "
                               "Les ouvertures sont prises en compte par le maillage (et le restent si les nœuds de la dalle bougent)."),
                            this);
    info->setWordWrap(true);
    v->addWidget(info);
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({ tr("Nom"), tr("Forme"), tr("Sommets (u ; v)") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_table, 1);
    auto* row = new QHBoxLayout;
    auto* rect = new QPushButton(tr("Rectangle…"), this);
    auto* circle = new QPushButton(tr("Cercle…"), this);
    auto* remove = new QPushButton(tr("Supprimer"), this);
    row->addWidget(rect), row->addWidget(circle), row->addWidget(remove), row->addStretch();
    v->addLayout(row);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);

    auto addOpening = [this](TSA::Model::SlabOpening o) {
        const auto* s = m_model.getSlab(m_slabId);
        if (!s) return;
        TSA::Model::Slab probe = *s;
        probe.setOpenings(m_openings);
        const std::string err = TSA::Mesh::validateSlabOpening(m_model, probe, o.polygon);
        if (!err.empty())
        {
            QMessageBox::warning(this, tr("Ouverture refusée"), QString::fromStdString(err));
            return;
        }
        m_openings.push_back(o);
        fill();
    };
    connect(rect, &QPushButton::clicked, this, [this, addOpening] {
        bool ok = false;
        const QString t = QInputDialog::getText(this, tr("Ouverture rectangulaire"), tr("Centre u ; centre v ; largeur (u) ; longueur (v) en m :"), QLineEdit::Normal,
                                                QStringLiteral("1 ; 1 ; 0,8 ; 0,8"), &ok);
        if (!ok) return;
        const QStringList p = t.split(';');
        if (p.size() != 4) return;
        auto d = [&](int i) { return p[i].trimmed().replace(',', '.').toDouble(); };
        const double cu = d(0), cv = d(1), a = d(2) / 2, b = d(3) / 2;
        TSA::Model::SlabOpening o;
        o.name = tr("O%1").arg(m_openings.size() + 1).toStdString();
        o.shape = "rectangle";
        o.polygon = { { cu - a, cv - b }, { cu + a, cv - b }, { cu + a, cv + b }, { cu - a, cv + b } };
        addOpening(o);
    });
    connect(circle, &QPushButton::clicked, this, [this, addOpening] {
        bool ok = false;
        const QString t = QInputDialog::getText(this, tr("Ouverture circulaire"), tr("Centre u ; centre v ; diamètre en m :"), QLineEdit::Normal,
                                                QStringLiteral("1 ; 1 ; 0,5"), &ok);
        if (!ok) return;
        const QStringList p = t.split(';');
        if (p.size() != 3) return;
        auto d = [&](int i) { return p[i].trimmed().replace(',', '.').toDouble(); };
        TSA::Model::SlabOpening o;
        o.name = tr("O%1").arg(m_openings.size() + 1).toStdString();
        o.shape = "circle";
        for (int k = 0; k < 24; ++k)   // polygone à 24 côtés (le maillage suit ce contour)
            o.polygon.emplace_back(d(0) + d(2) / 2 * std::cos(2 * 3.14159265358979 * k / 24), d(1) + d(2) / 2 * std::sin(2 * 3.14159265358979 * k / 24));
        addOpening(o);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int r = m_table->currentRow();
        if (r < 0 || r >= int(m_openings.size())) return;
        m_openings.erase(m_openings.begin() + r);
        fill();
    });
    fill();
}

void SlabOpeningsDialog::fill()
{
    m_table->setRowCount(0);
    for (const auto& o : m_openings)
    {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        QStringList pts;
        for (std::size_t i = 0; i < o.polygon.size() && i < 6; ++i) pts << QStringLiteral("(%1 ; %2)").arg(o.polygon[i].first, 0, 'g', 4).arg(o.polygon[i].second, 0, 'g', 4);
        if (o.polygon.size() > 6) pts << tr("… %1 sommets").arg(o.polygon.size());
        m_table->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(o.name)));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(o.shape)));
        m_table->setItem(r, 2, new QTableWidgetItem(pts.join(QStringLiteral(" "))));
    }
}

} // namespace TSA::UI
