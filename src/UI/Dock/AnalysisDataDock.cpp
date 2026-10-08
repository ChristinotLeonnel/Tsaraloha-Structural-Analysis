#include "AnalysisDataDock.h"

#include "../../Analysis/ResultsModel.h"
#include "../../Analysis/ResultsExport.h"
#include "../../Analysis/ResultsContext.h"
#include "../../Model/Model.h"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace TSA::UI
{

using namespace TSA::Analysis;

/// Vue paresseuse d'une SparseMatrix : data() n'est appelée que pour les cellules visibles.
class SparseMatrixTableModel : public QAbstractTableModel
{
public:
    explicit SparseMatrixTableModel(QObject* parent) : QAbstractTableModel(parent) {}

    void setMatrix(const SparseMatrix* m, const DofMap* dofs)
    {
        beginResetModel();
        m_matrix = m;
        m_dofs = dofs;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() || !m_matrix ? 0 : m_matrix->rows;
    }
    int columnCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() || !m_matrix ? 0 : m_matrix->cols;
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!m_matrix || !index.isValid()) return {};
        const double v = m_matrix->at(index.row(), index.column());
        if (role == Qt::DisplayRole) return v == 0.0 ? QString() : QString::number(v, 'g', 11);
        if (role == Qt::TextAlignmentRole) return int(Qt::AlignRight | Qt::AlignVCenter);
        if (role == Qt::ToolTipRole)
            return QString("%1 × %2 = %3")
                .arg(label(index.row()), label(index.column())).arg(v, 0, 'g', 17);
        return {};
    }
    QVariant headerData(int section, Qt::Orientation, int role) const override
    {
        if (role != Qt::DisplayRole) return {};
        return label(section);
    }

private:
    QString label(int eq) const
    {
        return m_dofs ? QString("%1  %2").arg(eq).arg(QString::fromStdString(m_dofs->equationLabel(eq))) : QString::number(eq);
    }

    const SparseMatrix* m_matrix = nullptr;
    const DofMap* m_dofs = nullptr;
};

namespace
{
QTableWidget* makeTable(QWidget* parent, const QStringList& headers)
{
    auto* t = new QTableWidget(parent);
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    return t;
}

void setCell(QTableWidget* t, int r, int c, const QString& s)
{
    t->setItem(r, c, new QTableWidgetItem(s));
}

void setNum(QTableWidget* t, int r, int c, double v)
{
    auto* it = new QTableWidgetItem(QString::number(v, 'g', 10));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    it->setToolTip(QString::number(v, 'g', 17));
    t->setItem(r, c, it);
}

QString metaText(const MatrixMetadata& m)
{
    return QString("Source : %1\nType : %2 | Repère : %3 | Exact : %4 | Symétrique : %5\n"
                   "Ordre des DDL : %6\nContraintes : %7 | Système : %8 | Stockage : %9\nUnités : %10%11")
        .arg(QString::fromStdString(m.source), QString::fromStdString(m.matrixType),
             QString::fromStdString(m.coordinateSystem), m.exact ? "oui" : "NON", m.symmetric ? "oui" : "non",
             QString::fromStdString(m.dofOrdering), QString::fromStdString(m.constraints),
             QString::fromStdString(m.solver), QString::fromStdString(m.storage))
        .arg(QString::fromStdString(m.units),
             m.notes.empty() ? QString() : "\nNotes : " + QString::fromStdString(m.notes));
}
} // namespace

AnalysisDataDock::AnalysisDataDock(QWidget* parent)
    : QDockWidget(tr("Données d'analyse"), parent)
{
    setObjectName("AnalysisDataDock");
    auto* root = new QWidget(this);
    auto* lay = new QVBoxLayout(root);
    lay->setContentsMargins(4, 4, 4, 4);

    auto* top = new QHBoxLayout();
    m_status = new QLabel(tr("Aucun résultat."), root);
    m_status->setWordWrap(true);
    top->addWidget(m_status, 1);
    auto* exportBtn = new QPushButton(tr("Exporter…"), root);
    connect(exportBtn, &QPushButton::clicked, this, &AnalysisDataDock::exportData);
    top->addWidget(exportBtn);
    lay->addLayout(top);

    m_tabs = new QTabWidget(root);
    lay->addWidget(m_tabs, 1);

    m_dispTable = makeTable(m_tabs, { tr("Nœud"), "UX", "UY", "UZ", "RX", "RY", "RZ" });
    m_tabs->addTab(m_dispTable, tr("Déplacements"));

    m_reactTable = makeTable(m_tabs, { tr("Nœud"), "FX", "FY", "FZ", "MX", "MY", "MZ" });
    m_tabs->addTab(m_reactTable, tr("Réactions"));

    auto* forcePage = new QWidget(m_tabs);
    auto* forceLay = new QVBoxLayout(forcePage);
    forceLay->setContentsMargins(0, 0, 0, 0);
    m_forceSystem = new QComboBox(forcePage);
    m_forceSystem->addItem(tr("Extrémités — convention RDM TSA (N traction > 0)"));
    m_forceSystem->addItem(tr("Forces locales — OpenSees (forces sur l'élément)"));
    m_forceSystem->addItem(tr("Forces globales — OpenSees globalForce"));
    m_forceSystem->addItem(tr("Forces basiques — OpenSees basicForce"));
    connect(m_forceSystem, &QComboBox::currentIndexChanged, this, [this]() { fillForces(); });
    forceLay->addWidget(m_forceSystem);
    m_forceTable = makeTable(forcePage, {});
    forceLay->addWidget(m_forceTable, 1);
    m_tabs->addTab(forcePage, tr("Efforts"));

    m_dofTable = makeTable(m_tabs, { tr("Équation"), tr("Nœud"), tr("DDL") });
    m_tabs->addTab(m_dofTable, tr("DDL"));

    auto* kPage = new QWidget(m_tabs);
    auto* kLay = new QVBoxLayout(kPage);
    kLay->setContentsMargins(0, 0, 0, 0);
    m_kMeta = new QLabel(kPage);
    m_kMeta->setWordWrap(true);
    m_kMeta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    kLay->addWidget(m_kMeta);
    m_kView = new QTableView(kPage);
    m_kModel = new SparseMatrixTableModel(m_kView);
    m_kView->setModel(m_kModel);
    m_kView->horizontalHeader()->setDefaultSectionSize(110);
    kLay->addWidget(m_kView, 1);
    m_tabs->addTab(kPage, tr("K globale"));

    auto* sysPage = new QWidget(m_tabs);
    auto* sysLay = new QVBoxLayout(sysPage);
    sysLay->setContentsMargins(0, 0, 0, 0);
    m_systemMeta = new QLabel(sysPage);
    m_systemMeta->setWordWrap(true);
    sysLay->addWidget(m_systemMeta);
    m_systemTable = makeTable(sysPage, { tr("Équation"), tr("DDL"), "F", "U", "(K·U)", tr("Résidu K·U − F") });
    sysLay->addWidget(m_systemTable, 1);
    m_tabs->addTab(sysPage, tr("K·U = F"));

    auto* elPage = new QWidget(m_tabs);
    auto* elLay = new QVBoxLayout(elPage);
    elLay->setContentsMargins(0, 0, 0, 0);
    m_elementCombo = new QComboBox(elPage);
    connect(m_elementCombo, &QComboBox::currentIndexChanged, this, &AnalysisDataDock::showElement);
    elLay->addWidget(m_elementCombo);
    m_elementText = new QPlainTextEdit(elPage);
    m_elementText->setReadOnly(true);
    m_elementText->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_elementText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    elLay->addWidget(m_elementText, 1);
    m_tabs->addTab(elPage, tr("Élément"));

    auto* ctxPage = new QWidget(m_tabs);
    auto* ctxLay = new QVBoxLayout(ctxPage);
    ctxLay->setContentsMargins(0, 0, 0, 0);
    auto* ctxTop = new QHBoxLayout();
    ctxTop->addWidget(new QLabel(tr("Nœud :"), ctxPage));
    m_nodeSpin = new QSpinBox(ctxPage);
    m_nodeSpin->setRange(1, 1000000);
    ctxTop->addWidget(m_nodeSpin);
    auto* ctxBtn = new QPushButton(tr("Contexte structuré (JSON)"), ctxPage);
    connect(ctxBtn, &QPushButton::clicked, this, &AnalysisDataDock::showNodeContext);
    ctxTop->addWidget(ctxBtn);
    ctxTop->addStretch(1);
    ctxLay->addLayout(ctxTop);
    m_contextText = new QPlainTextEdit(ctxPage);
    m_contextText->setReadOnly(true);
    m_contextText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ctxLay->addWidget(m_contextText, 1);
    m_tabs->addTab(ctxPage, tr("Contexte IA"));

    setWidget(root);
}

void AnalysisDataDock::setModel(TSA::Model::Model* model)
{
    m_model = model;
}

void AnalysisDataDock::setResultsModel(const std::shared_ptr<ResultsModel>& results)
{
    m_results = results;
    refresh();
}

void AnalysisDataDock::refresh()
{
    if (!m_results)
    {
        m_status->setText(tr("Aucun résultat."));
    }
    else
    {
        const auto& u = m_results->units();
        const auto& adv = m_results->advanced();
        const auto& meta = m_results->executionMetadata();
        QString engine = QString::fromStdString(meta.solverEngine);
        if (!meta.solverVersion.empty()) engine += " " + QString::fromStdString(meta.solverVersion);
        if (!meta.analysisScope.empty()) engine += tr(" — portée : %1").arg(QString::fromStdString(meta.analysisScope));
        m_status->setText(tr("Moteur %1\n%2 | %3 | unités %4, %5 | équilibre : résidu relatif forces %6, moments %8%7")
            .arg(engine)
            .arg(m_results->isValid() ? tr("Résultats valides") : tr("RÉSULTATS OBSOLÈTES OU INVALIDES"))
            .arg(adv.available ? "ADVANCED" : "LIGHT")
            .arg(QString::fromStdString(u.force), QString::fromStdString(u.length))
            .arg(meta.relativeEquilibriumResidual, 0, 'g', 3)
            .arg(adv.warnings.empty() ? QString() : "\n⚠ " + QString::fromStdString(adv.warnings.front()))
            .arg(meta.relativeMomentResidual, 0, 'g', 3));
    }
    fillDisplacements();
    fillReactions();
    fillForces();
    fillDofMap();
    fillGlobalStiffness();
    fillSystemVectors();
    fillElementList();
    fillEngineTables();
    updateTabsFromAvailability();
}

void AnalysisDataDock::updateTabsFromAvailability()
{
    // Seules les catégories réellement fournies par le moteur pour ce calcul sont affichées.
    const bool any = m_results != nullptr;
    const ResultAvailability a = any ? m_results->availability() : ResultAvailability{};
    const bool adv = any && m_results->advanced().available;
    m_tabs->setTabVisible(m_tabs->indexOf(m_dispTable), !any || a.displacements);
    m_tabs->setTabVisible(m_tabs->indexOf(m_reactTable), !any || a.reactions);
    m_tabs->setTabVisible(m_tabs->indexOf(m_forceTable->parentWidget()), !any || a.elementForces);
    m_tabs->setTabVisible(m_tabs->indexOf(m_dofTable), !any || a.dofMapping);
    m_tabs->setTabVisible(m_tabs->indexOf(m_kView->parentWidget()), !any || a.globalStiffness);
    m_tabs->setTabVisible(m_tabs->indexOf(m_systemTable->parentWidget()),
                          !any || (a.globalStiffness && !m_results->advanced().loadVector.empty()));
    m_tabs->setTabVisible(m_tabs->indexOf(m_elementText->parentWidget()), !any || a.elementStiffness || adv);
    // Repères bruts (local / global / basique) : uniquement avec les forces avancées du moteur.
    if (!adv) m_forceSystem->setCurrentIndex(0);
    m_forceSystem->setEnabled(adv);
}

void AnalysisDataDock::fillEngineTables()
{
    for (QWidget* w : m_engineTabs)
    {
        m_tabs->removeTab(m_tabs->indexOf(w));
        delete w;
    }
    m_engineTabs.clear();
    if (!m_results) return;
    for (const auto& t : m_results->engineTables())
    {
        QStringList headers;
        for (const auto& c : t.columns) headers << QString::fromStdString(c);
        auto* table = makeTable(m_tabs, headers);
        table->setRowCount(static_cast<int>(t.rows.size()));
        for (int r = 0; r < static_cast<int>(t.rows.size()); ++r)
            for (int c = 0; c < static_cast<int>(t.rows[r].size()); ++c)
                setCell(table, r, c, QString::fromStdString(t.rows[r][c]));
        m_tabs->addTab(table, QString::fromStdString(t.title));
        m_engineTabs.push_back(table);
    }
}

void AnalysisDataDock::fillDisplacements()
{
    m_dispTable->setRowCount(0);
    if (!m_results) return;
    const auto& all = m_results->allDisplacements();
    m_dispTable->setRowCount(static_cast<int>(all.size()));
    int r = 0;
    for (const auto& [id, d] : all)
    {
        setCell(m_dispTable, r, 0, QString("N%1").arg(id));
        const double v[6] = { d.ux, d.uy, d.uz, d.rx, d.ry, d.rz };
        for (int c = 0; c < 6; ++c) setNum(m_dispTable, r, c + 1, v[c]);
        ++r;
    }
}

void AnalysisDataDock::fillReactions()
{
    m_reactTable->setRowCount(0);
    if (!m_results) return;
    const auto& all = m_results->allReactions();
    m_reactTable->setRowCount(static_cast<int>(all.size()) + 1);
    int r = 0;
    for (const auto& [id, x] : all)
    {
        setCell(m_reactTable, r, 0, QString("N%1").arg(id));
        const double v[6] = { x.rx, x.ry, x.rz, x.mx, x.my, x.mz };
        for (int c = 0; c < 6; ++c) setNum(m_reactTable, r, c + 1, v[c]);
        ++r;
    }
    const auto eq = m_results->equilibrium();
    setCell(m_reactTable, r, 0, tr("Σ F + Σ R"));
    setNum(m_reactTable, r, 1, eq.errorFx());
    setNum(m_reactTable, r, 2, eq.errorFy());
    setNum(m_reactTable, r, 3, eq.errorFz());
}

void AnalysisDataDock::fillForces()
{
    m_forceTable->clear();
    m_forceTable->setRowCount(0);
    if (!m_results) return;
    const int mode = m_forceSystem->currentIndex();
    const auto& adv = m_results->advanced();
    if (mode == 0)
    {
        m_forceTable->setColumnCount(9);
        m_forceTable->setHorizontalHeaderLabels({ tr("Élément"), tr("Tag"), tr("Extr."), "N", "Vy", "Vz", "Mx", "My", "Mz" });
        int r = 0;
        for (const auto& [key, er] : m_results->allElementResults())
        {
            for (int end = 0; end < 2; ++end)
            {
                const auto& f = end == 0 ? er.startForces : er.endForces;
                m_forceTable->insertRow(r);
                setCell(m_forceTable, r, 0, QString::fromStdString(key.label()));
                setCell(m_forceTable, r, 1, QString::number(er.opsTag));
                setCell(m_forceTable, r, 2, end == 0 ? "i" : "j");
                const double v[6] = { f.N, f.Vy, f.Vz, f.Mx, f.My, f.Mz };
                for (int c = 0; c < 6; ++c) setNum(m_forceTable, r, 3 + c, v[c]);
                ++r;
            }
        }
        return;
    }
    if (adv.elementForces.empty())
    {
        m_forceTable->setColumnCount(1);
        m_forceTable->setHorizontalHeaderLabels({ tr("Information") });
        m_forceTable->insertRow(0);
        setCell(m_forceTable, 0, 0, tr("Forces brutes disponibles en mode ADVANCED (Configuration du calcul)."));
        return;
    }
    if (mode == 3)
    {
        m_forceTable->setColumnCount(8);
        m_forceTable->setHorizontalHeaderLabels({ tr("Élément"), tr("Composantes"), "q1", "q2", "q3", "q4", "q5", "q6" });
        int r = 0;
        for (const auto& [key, f] : adv.elementForces)
        {
            m_forceTable->insertRow(r);
            setCell(m_forceTable, r, 0, QString::fromStdString(key.label()));
            QStringList labels;
            for (const auto& l : f.basicLabels) labels << QString::fromStdString(l);
            setCell(m_forceTable, r, 1, labels.join(", "));
            for (std::size_t c = 0; c < f.basic.size() && c < 6; ++c) setNum(m_forceTable, r, 2 + static_cast<int>(c), f.basic[c]);
            ++r;
        }
        return;
    }
    m_forceTable->setColumnCount(9);
    m_forceTable->setHorizontalHeaderLabels({ tr("Élément"), tr("Extr."), "Fx", "Fy", "Fz", "Mx", "My", "Mz", tr("Source") });
    int r = 0;
    for (const auto& [key, f] : adv.elementForces)
    {
        const auto& v = mode == 1 ? f.local : f.global;
        if (v.size() != 12) continue;
        for (int end = 0; end < 2; ++end)
        {
            m_forceTable->insertRow(r);
            setCell(m_forceTable, r, 0, QString::fromStdString(key.label()));
            setCell(m_forceTable, r, 1, end == 0 ? "i" : "j");
            for (int c = 0; c < 6; ++c) setNum(m_forceTable, r, 2 + c, v[end * 6 + c]);
            setCell(m_forceTable, r, 8, mode == 1 ? QString::fromStdString(f.localSource) : tr("OpenSees API: globalForce"));
            ++r;
        }
    }
}

void AnalysisDataDock::fillDofMap()
{
    m_dofTable->setRowCount(0);
    if (!m_results) return;
    const auto& dm = m_results->advanced().dofMap;
    m_dofTable->setRowCount(dm.equationCount());
    for (const auto& e : dm.equations)
    {
        setCell(m_dofTable, e.equation, 0, QString::number(e.equation));
        setCell(m_dofTable, e.equation, 1, QString("N%1").arg(e.nodeId));
        setCell(m_dofTable, e.equation, 2, QString::fromStdString(dm.dofLabels[e.dof]));
    }
}

void AnalysisDataDock::fillGlobalStiffness()
{
    if (!m_results || !m_results->advanced().available)
    {
        m_kModel->setMatrix(nullptr, nullptr);
        m_kMeta->setText(tr("K_global disponible en mode ADVANCED (Configuration du calcul → Extraction des résultats)."));
        return;
    }
    const auto& adv = m_results->advanced();
    if (!adv.hasGlobalStiffness)
    {
        m_kModel->setMatrix(nullptr, nullptr);
        m_kMeta->setText(tr("K_global indisponible : %1").arg(QString::fromStdString(adv.kGlobalUnavailableReason)));
        return;
    }
    m_kMeta->setText(tr("%1 × %1, %2 coefficients non nuls\n").arg(adv.kGlobal.rows).arg(adv.kGlobal.nonZeros())
                     + metaText(adv.kGlobalMeta));
    m_kModel->setMatrix(&adv.kGlobal, &adv.dofMap);
}

void AnalysisDataDock::fillSystemVectors()
{
    m_systemTable->setRowCount(0);
    const AdvancedResults* adv = m_results ? &m_results->advanced() : nullptr;
    if (!adv || !adv->available || adv->loadVector.empty())
    {
        m_systemMeta->setText(tr("Vecteurs F et U fournis par les moteurs qui exportent leur système résolu "
                                 "(Custom2D : option « Exporter le système K·U = F »)."));
        return;
    }
    const auto& F = adv->loadVector;
    const auto& U = adv->displacementVector;
    const int n = static_cast<int>(F.size());
    std::vector<double> KU;
    if (adv->hasGlobalStiffness && static_cast<int>(U.size()) == adv->kGlobal.cols) KU = adv->kGlobal.multiply(U);
    double maxResidual = 0.0, maxLoad = 0.0;
    m_systemTable->setRowCount(n);
    for (int i = 0; i < n; ++i)
    {
        setCell(m_systemTable, i, 0, QString::number(i));
        setCell(m_systemTable, i, 1, QString::fromStdString(adv->dofMap.equationLabel(i)));
        setNum(m_systemTable, i, 2, F[static_cast<std::size_t>(i)]);
        if (i < static_cast<int>(U.size())) setNum(m_systemTable, i, 3, U[static_cast<std::size_t>(i)]);
        if (i < static_cast<int>(KU.size()))
        {
            const double r = KU[static_cast<std::size_t>(i)] - F[static_cast<std::size_t>(i)];
            setNum(m_systemTable, i, 4, KU[static_cast<std::size_t>(i)]);
            setNum(m_systemTable, i, 5, r);
            maxResidual = std::max(maxResidual, std::abs(r));
        }
        maxLoad = std::max(maxLoad, std::abs(F[static_cast<std::size_t>(i)]));
    }
    m_systemMeta->setText(tr("%1 équation(s). Résidu max |K·U − F| = %2 (relatif %3). Les réactions R sont dans "
                             "l'onglet Réactions.")
                              .arg(n).arg(maxResidual, 0, 'g', 3)
                              .arg(maxLoad > 0.0 ? maxResidual / maxLoad : 0.0, 0, 'g', 3));
}

void AnalysisDataDock::fillElementList()
{
    m_elementCombo->blockSignals(true);
    m_elementCombo->clear();
    if (m_results)
    {
        for (const auto& [key, er] : m_results->allElementResults())
            m_elementCombo->addItem(QString("%1  (tag OpenSees %2, L = %3)").arg(QString::fromStdString(key.label()))
                                        .arg(er.opsTag).arg(er.length, 0, 'f', 3),
                                    QVariant::fromValue(static_cast<int>(key.kind) * 1000000000LL + key.id));
    }
    m_elementCombo->blockSignals(false);
    showElement(m_elementCombo->currentIndex());
}

void AnalysisDataDock::showElement(int index)
{
    m_elementText->clear();
    if (!m_results || index < 0) return;
    const qlonglong code = m_elementCombo->itemData(index).toLongLong();
    const ElementKey key { static_cast<StructuralElementKind>(code / 1000000000LL), static_cast<int>(code % 1000000000LL) };
    const auto& adv = m_results->advanced();
    QString out = QString("Element %1\n\n").arg(QString::fromStdString(key.label()));
    static const std::vector<std::string> labels {
        "UX_i", "UY_i", "UZ_i", "RX_i", "RY_i", "RZ_i", "UX_j", "UY_j", "UZ_j", "RX_j", "RY_j", "RZ_j" };

    auto mIt = adv.elementMatrices.find(key);
    if (mIt == adv.elementMatrices.end())
    {
        out += tr("Matrices disponibles en mode ADVANCED.\n");
    }
    else if (!mIt->second.available)
    {
        out += tr("Matrices indisponibles : %1\n").arg(QString::fromStdString(mIt->second.unavailableReason));
    }
    else
    {
        const auto& em = mIt->second;
        out += QString("Classe OpenSees : %1 | nœuds N%2 → N%3 | L = %4\n")
                   .arg(QString::fromStdString(em.opsClass)).arg(em.nodeI).arg(em.nodeJ).arg(em.length, 0, 'g', 10);
        out += QString("Axes locaux (global) : x = (%1, %2, %3)  y = (%4, %5, %6)  z = (%7, %8, %9)\n\n")
                   .arg(em.rotation[0], 0, 'f', 6).arg(em.rotation[1], 0, 'f', 6).arg(em.rotation[2], 0, 'f', 6)
                   .arg(em.rotation[3], 0, 'f', 6).arg(em.rotation[4], 0, 'f', 6).arg(em.rotation[5], 0, 'f', 6)
                   .arg(em.rotation[6], 0, 'f', 6).arg(em.rotation[7], 0, 'f', 6).arg(em.rotation[8], 0, 'f', 6);
        out += "BASIC STIFFNESS\n" + metaText(em.kBasicMeta) + "\n"
             + QString::fromStdString(ResultsExport::formatMatrix(em.kBasic, {})) + "\n";
        out += "LOCAL STIFFNESS\n" + metaText(em.kLocalMeta) + "\n"
             + QString::fromStdString(ResultsExport::formatMatrix(em.kLocal, labels)) + "\n";
        out += "GLOBAL STIFFNESS\n" + metaText(em.kGlobalMeta) + "\n"
             + QString::fromStdString(ResultsExport::formatMatrix(em.kGlobal, labels)) + "\n";
    }
    auto fIt = adv.elementForces.find(key);
    if (fIt != adv.elementForces.end())
    {
        auto line = [](const std::vector<double>& v) {
            QStringList s;
            for (double x : v) s << QString::number(x, 'g', 10);
            return s.join("  ");
        };
        out += "LOCAL FORCES (" + QString::fromStdString(fIt->second.localSource) + ")\n  " + line(fIt->second.local) + "\n";
        out += "GLOBAL FORCES (OpenSees API: globalForce)\n  " + line(fIt->second.global) + "\n";
        out += "BASIC FORCES (OpenSees API: basicForce)\n  " + line(fIt->second.basic) + "\n";
    }
    m_elementText->setPlainText(out);
}

void AnalysisDataDock::showNodeContext()
{
    if (!m_results || !m_model)
    {
        m_contextText->setPlainText(tr("Aucun résultat."));
        return;
    }
    const QJsonObject ctx = ResultsContext::nodeContext(*m_model, *m_results, m_nodeSpin->value());
    m_contextText->setPlainText(QString::fromUtf8(QJsonDocument(ctx).toJson(QJsonDocument::Indented)));
}

void AnalysisDataDock::exportData()
{
    if (!m_results)
    {
        QMessageBox::information(this, tr("Export"), tr("Aucun résultat à exporter."));
        return;
    }
    QMenu menu(this);
    struct Entry { QString label; ResultsDataset ds; ResultsExportFormat fmt; QString filter; };
    const std::vector<Entry> entries {
        { tr("Déplacements (CSV)"), ResultsDataset::Displacements, ResultsExportFormat::Csv, "CSV (*.csv)" },
        { tr("Réactions (CSV)"), ResultsDataset::Reactions, ResultsExportFormat::Csv, "CSV (*.csv)" },
        { tr("Efforts d'éléments (CSV)"), ResultsDataset::ElementForces, ResultsExportFormat::Csv, "CSV (*.csv)" },
        { tr("Mapping des DDL (CSV)"), ResultsDataset::DofMap, ResultsExportFormat::Csv, "CSV (*.csv)" },
        { tr("K globale (CSV, COO)"), ResultsDataset::GlobalStiffness, ResultsExportFormat::Csv, "CSV (*.csv)" },
        { tr("Rigidités élémentaires (CSV)"), ResultsDataset::ElementStiffness, ResultsExportFormat::Csv, "CSV (*.csv)" },
        { tr("Rigidités élémentaires (TXT)"), ResultsDataset::ElementStiffness, ResultsExportFormat::Txt, "Texte (*.txt)" },
        { tr("Tout (JSON)"), ResultsDataset::All, ResultsExportFormat::Json, "JSON (*.json)" },
        { tr("Tout (TXT)"), ResultsDataset::All, ResultsExportFormat::Txt, "Texte (*.txt)" },
    };
    for (std::size_t i = 0; i < entries.size(); ++i)
        menu.addAction(entries[i].label)->setData(static_cast<int>(i));
    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen) return;
    const Entry& e = entries[static_cast<std::size_t>(chosen->data().toInt())];
    const QString path = QFileDialog::getSaveFileName(this, tr("Exporter les résultats"), QString(), e.filter);
    if (path.isEmpty()) return;
    std::string err;
    if (!ResultsExport::write(*m_results, e.ds, e.fmt, path.toStdString(), &err))
        QMessageBox::warning(this, tr("Export"), QString::fromStdString(err));
}

} // namespace TSA::UI
