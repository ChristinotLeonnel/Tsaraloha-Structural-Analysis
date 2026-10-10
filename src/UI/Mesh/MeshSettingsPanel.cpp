#include "MeshSettingsPanel.h"

#include "MeshController.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::Mesh;

namespace
{
QDoubleSpinBox* spin(QWidget* parent, double min, double max, int decimals, const QString& suffix)
{
    auto* s = new QDoubleSpinBox(parent);
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setSingleStep(max > 10 ? 0.1 : 0.01);
    s->setSuffix(suffix);
    return s;
}
QString num(double v) { return QString::number(v, 'g', 4); }
} // namespace

MeshSettingsPanel::MeshSettingsPanel(MeshController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    m_engine = new QLabel(this);
    m_engine->setWordWrap(true);
    root->addWidget(m_engine);

    auto* profileRow = new QHBoxLayout;
    m_profile = new QComboBox(this);
    auto* applyProfile = new QPushButton(tr("Appliquer"), this);
    auto* saveProfile = new QPushButton(tr("Enregistrer…"), this);
    auto* profileMenu = new QPushButton(tr("…"), this);
    profileMenu->setToolTip(tr("Supprimer, importer ou exporter un profil"));
    profileRow->addWidget(new QLabel(tr("Profil :"), this));
    profileRow->addWidget(m_profile, 1);
    profileRow->addWidget(applyProfile);
    profileRow->addWidget(saveProfile);
    profileRow->addWidget(profileMenu);
    root->addLayout(profileRow);

    auto* tabs = new QTabWidget(this);
    buildGeneral(tabs);
    buildSurfaces(tabs);
    buildRefinement(tabs);
    buildLocals(tabs);
    buildQuality(tabs);
    root->addWidget(tabs, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->hide();
    root->addWidget(m_progress);

    auto* grid = new QGridLayout;
    auto* estimate = new QPushButton(tr("Estimer"), this);
    m_btnPreview = new QPushButton(tr("Aperçu"), this);
    m_btnGenerate = new QPushButton(tr("Générer"), this);
    m_btnRegenerate = new QPushButton(tr("Régénérer"), this);
    auto* quality = new QPushButton(tr("Vérifier la qualité"), this);
    auto* clear = new QPushButton(tr("Supprimer"), this);
    auto* exportBtn = new QPushButton(tr("Exporter…"), this);
    auto* importBtn = new QPushButton(tr("Importer…"), this);
    m_btnCancel = new QPushButton(tr("Annuler"), this);
    m_btnCancel->hide();
    m_btnPreview->setToolTip(tr("Maillage prévisionnel affiché sans remplacer le maillage du projet ; ne remplace pas la génération ni son contrôle de qualité"));
    m_btnGenerate->setToolTip(tr("Génère le maillage du projet avec Gmsh, l'enregistre et contrôle sa qualité"));
    m_btnRegenerate->setToolTip(tr("Régénère le maillage avec les paramètres enregistrés (après une modification de la géométrie)"));
    grid->addWidget(estimate, 0, 0);
    grid->addWidget(m_btnPreview, 0, 1);
    grid->addWidget(m_btnGenerate, 0, 2);
    grid->addWidget(m_btnRegenerate, 1, 0);
    grid->addWidget(quality, 1, 1);
    grid->addWidget(clear, 1, 2);
    grid->addWidget(exportBtn, 2, 0);
    grid->addWidget(importBtn, 2, 1);
    grid->addWidget(m_btnCancel, 2, 2);
    root->addLayout(grid);

    connect(estimate, &QPushButton::clicked, this, [this] {
        readWidgets();
        const auto e = m_controller->estimate();
        QMessageBox::information(this, tr("Estimation du maillage"),
                                 tr("Estimation indicative (Gmsh donne le nombre réel à la génération) :\n\n"
                                    "• éléments 1D : %1\n• éléments 2D : %2\n• éléments 3D : %3\n• nœuds : ≈ %4")
                                     .arg(e.elements1D)
                                     .arg(e.elements2D)
                                     .arg(e.elements3D)
                                     .arg(e.nodes));
    });
    connect(m_btnPreview, &QPushButton::clicked, this, [this] {
        readWidgets();
        m_controller->generate(true);
    });
    connect(m_btnGenerate, &QPushButton::clicked, this, [this] {
        readWidgets();
        m_controller->generate(false);
    });
    connect(m_btnRegenerate, &QPushButton::clicked, this, [this] { m_controller->generate(false); });
    connect(quality, &QPushButton::clicked, this, &MeshSettingsPanel::qualityRequested);
    connect(exportBtn, &QPushButton::clicked, this, &MeshSettingsPanel::exportRequested);
    connect(importBtn, &QPushButton::clicked, this, &MeshSettingsPanel::importRequested);
    connect(m_btnCancel, &QPushButton::clicked, m_controller, &MeshController::cancel);
    connect(clear, &QPushButton::clicked, this, [this] {
        if (m_controller->hasMesh() &&
            QMessageBox::question(this, tr("Supprimer le maillage"), tr("Supprimer le maillage du projet ? Les paramètres sont conservés.")) == QMessageBox::Yes)
            m_controller->clearMesh();
    });
    connect(applyProfile, &QPushButton::clicked, this, [this] {
        const auto p = MeshProfileLibrary().find(m_profile->currentData().toString().toStdString());
        if (!p) return;
        m_controller->settings().global = p->global;
        m_controller->settings().profile = p->name;
        m_controller->commitSettings();
        refresh();
    });
    connect(saveProfile, &QPushButton::clicked, this, [this] {
        readWidgets();
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Enregistrer le profil"), tr("Nom du profil :"), QLineEdit::Normal,
                                                   QString::fromStdString(m_controller->settings().profile), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        MeshProfile p;
        p.name = name.trimmed().toStdString();
        p.global = m_controller->settings().global;
        const QStringList cats { "general", "beam", "slab", "wall", "bridge", "volume" };
        p.category = QInputDialog::getItem(this, tr("Enregistrer le profil"), tr("Catégorie :"), cats, 0, false, &ok).toStdString();
        QString err;
        MeshProfileLibrary lib;
        if (!lib.save(p, &err)) QMessageBox::critical(this, tr("Profil"), err);
        m_controller->settings().profile = p.name;
        m_controller->commitSettings();
        refreshProfiles();
    });
    connect(profileMenu, &QPushButton::clicked, this, [this] {
        const QStringList actions { tr("Supprimer le profil sélectionné"), tr("Importer un profil…"), tr("Exporter le profil sélectionné…") };
        bool ok = false;
        const QString a = QInputDialog::getItem(this, tr("Profils de maillage"), tr("Action :"), actions, 0, false, &ok);
        if (!ok) return;
        MeshProfileLibrary lib;
        QString err;
        const std::string name = m_profile->currentData().toString().toStdString();
        if (a == actions[0] && !lib.remove(name, &err)) QMessageBox::warning(this, tr("Profil"), err);
        if (a == actions[1])
        {
            const QString path = QFileDialog::getOpenFileName(this, tr("Importer un profil"), QString(), tr("Profils (*.json)"));
            std::string imported;
            if (!path.isEmpty() && !lib.importProfile(path, &imported, &err)) QMessageBox::warning(this, tr("Profil"), err);
        }
        if (a == actions[2])
        {
            const QString path = QFileDialog::getSaveFileName(this, tr("Exporter le profil"), QString::fromStdString(name) + ".json", tr("Profils (*.json)"));
            if (!path.isEmpty() && !lib.exportProfile(name, path, &err)) QMessageBox::warning(this, tr("Profil"), err);
        }
        refreshProfiles();
    });
    connect(m_controller, &MeshController::generationStarted, this, [this](bool) {
        m_progress->setValue(0);
        m_progress->show();
        m_btnCancel->show();
        m_btnPreview->setEnabled(false), m_btnGenerate->setEnabled(false), m_btnRegenerate->setEnabled(false);
    });
    connect(m_controller, &MeshController::progress, this, [this](int p, const QString& s) {
        m_progress->setValue(p);
        m_progress->setFormat(QStringLiteral("%1 % — %2").arg(p).arg(s));
    });
    connect(m_controller, &MeshController::generationFinished, this, [this](bool preview, bool ok, const QString& message) {
        m_progress->hide();
        m_btnCancel->hide();
        m_btnPreview->setEnabled(true), m_btnGenerate->setEnabled(true), m_btnRegenerate->setEnabled(true);
        refreshStatus();
        if (!ok) QMessageBox::warning(this, preview ? tr("Aperçu du maillage") : tr("Génération du maillage"), message);
    });
    connect(m_controller, &MeshController::meshChanged, this, &MeshSettingsPanel::refresh);
    connect(m_controller, &MeshController::previewChanged, this, &MeshSettingsPanel::refreshStatus);
    refresh();
}

void MeshSettingsPanel::buildGeneral(QTabWidget* tabs)
{
    auto* w = new QWidget(tabs);
    auto* f = new QFormLayout(w);
    auto* dims = new QHBoxLayout;
    m_d1 = new QCheckBox(tr("1D (barres)"), w);
    m_d2 = new QCheckBox(tr("2D (dalles, voiles)"), w);
    m_d3 = new QCheckBox(tr("3D (semelles)"), w);
    dims->addWidget(m_d1), dims->addWidget(m_d2), dims->addWidget(m_d3);
    f->addRow(tr("Dimensions :"), dims);
    m_size = spin(w, 0.001, 100, 3, tr(" m"));
    m_min = spin(w, 0.0001, 100, 4, tr(" m"));
    m_max = spin(w, 0.001, 1000, 3, tr(" m"));
    f->addRow(tr("Taille globale :"), m_size);
    f->addRow(tr("Taille minimale :"), m_min);
    f->addRow(tr("Taille maximale :"), m_max);
    m_fineness = new QComboBox(w);
    m_fineness->addItems({ tr("Très grossier (× 2)"), tr("Grossier (× 1,5)"), tr("Standard (× 1)"), tr("Fin (× 0,5)"), tr("Très fin (× 0,25)"), tr("Personnalisé (× 1)") });
    f->addRow(tr("Finesse :"), m_fineness);
    m_factor = spin(w, 0.01, 10, 3, QString());
    m_factor->setToolTip(tr("Facteur de raffinement appliqué en plus de la finesse (< 1 : plus fin)"));
    f->addRow(tr("Facteur de raffinement :"), m_factor);
    m_rate = spin(w, 0.05, 5, 2, QString());
    m_rate->setToolTip(tr("Croissance de la taille hors d'une zone de raffinement (m de taille par m de distance)"));
    f->addRow(tr("Transition :"), m_rate);
    m_order = new QComboBox(w);
    m_order->addItems({ tr("Premier ordre"), tr("Second ordre (Serendip : line3, tri6, quad8, tet10, hex20)") });
    f->addRow(tr("Ordre des éléments :"), m_order);
    m_keepPoints = new QCheckBox(tr("Nœuds aux charges ponctuelles et aux nœuds du modèle situés dans les surfaces"), w);
    f->addRow(QString(), m_keepPoints);
    m_maxElements = new QSpinBox(w);
    m_maxElements->setRange(100, 50000000);
    m_maxElements->setSingleStep(10000);
    f->addRow(tr("Nombre maximal d'éléments :"), m_maxElements);
    tabs->addTab(w, tr("Général"));
    for (auto* s : { m_size, m_min, m_max, m_factor, m_rate }) connect(s, &QDoubleSpinBox::editingFinished, this, &MeshSettingsPanel::readWidgets);
    for (auto* c : { m_d1, m_d2, m_d3, m_keepPoints }) connect(c, &QCheckBox::toggled, this, &MeshSettingsPanel::readWidgets);
    for (auto* c : { m_fineness, m_order }) connect(c, &QComboBox::currentIndexChanged, this, &MeshSettingsPanel::readWidgets);
    connect(m_maxElements, &QSpinBox::editingFinished, this, &MeshSettingsPanel::readWidgets);
}

void MeshSettingsPanel::buildSurfaces(QTabWidget* tabs)
{
    const MesherCapabilities caps = meshEngineCapabilities();
    auto* w = new QWidget(tabs);
    auto* v = new QVBoxLayout(w);
    auto* g2 = new QGroupBox(tr("Surfaces (2D)"), w);
    auto* f2 = new QFormLayout(g2);
    m_algo2D = new QComboBox(g2);
    for (auto a : caps.algorithms2D) m_algo2D->addItem(QString::fromUtf8(algorithm2DName(a)), static_cast<int>(a));
    f2->addRow(tr("Algorithme :"), m_algo2D);
    m_structured = new QCheckBox(tr("Maillage structuré (transfini) quand c'est possible : surface à 4 côtés, sans ouverture, nœud plongé ni zone de raffinement"), g2);
    m_structured->setStyleSheet(QStringLiteral("QCheckBox { white-space: normal; }"));
    f2->addRow(QString(), m_structured);
    m_recombine = new QCheckBox(tr("Recombiner les triangles en quadrangles"), g2);
    f2->addRow(QString(), m_recombine);
    m_recombAlgo = new QComboBox(g2);
    for (auto a : caps.recombinationAlgorithms) m_recombAlgo->addItem(QString::fromUtf8(recombinationName(a)), static_cast<int>(a));
    f2->addRow(tr("Recombinaison :"), m_recombAlgo);
    m_smoothing = new QSpinBox(g2);
    m_smoothing->setRange(0, 100);
    f2->addRow(tr("Lissage (itérations) :"), m_smoothing);
    v->addWidget(g2);
    auto* g3 = new QGroupBox(tr("Volumes (3D)"), w);
    auto* f3 = new QFormLayout(g3);
    m_volume = new QComboBox(g3);
    m_volume->addItem(tr("Tétraèdres (non structuré)"));
    if (caps.structuredHexahedra) m_volume->addItem(tr("Hexaèdres (transfini : volumes à 6 faces uniquement)"));
    f3->addRow(tr("Éléments :"), m_volume);
    m_algo3D = new QComboBox(g3);
    for (auto a : caps.algorithms3D) m_algo3D->addItem(QString::fromUtf8(algorithm3DName(a)), static_cast<int>(a));
    f3->addRow(tr("Algorithme (tétraèdres) :"), m_algo3D);
    m_optimize = new QCheckBox(tr("Optimiser les tétraèdres"), g3);
    f3->addRow(QString(), m_optimize);
    v->addWidget(g3);
    v->addStretch();
    tabs->addTab(w, tr("Surfaces / volumes"));
    for (auto* c : { m_algo2D, m_recombAlgo, m_volume, m_algo3D }) connect(c, &QComboBox::currentIndexChanged, this, &MeshSettingsPanel::readWidgets);
    for (auto* c : { m_structured, m_recombine, m_optimize }) connect(c, &QCheckBox::toggled, this, &MeshSettingsPanel::readWidgets);
    connect(m_smoothing, &QSpinBox::editingFinished, this, &MeshSettingsPanel::readWidgets);
}

void MeshSettingsPanel::buildRefinement(QTabWidget* tabs)
{
    auto* w = new QWidget(tabs);
    auto* v = new QVBoxLayout(w);
    auto* g = new QGroupBox(tr("Raffinement automatique"), w);
    auto* grid = new QGridLayout(g);
    const QStringList names { tr("Appuis"), tr("Charges ponctuelles"), tr("Changements de section"), tr("Connexions"), tr("Ouvertures") };
    grid->addWidget(new QLabel(tr("Taille"), g), 0, 1);
    grid->addWidget(new QLabel(tr("Rayon"), g), 0, 2);
    for (int i = 0; i < 5; ++i)
    {
        m_auto[i].on = new QCheckBox(names[i], g);
        m_auto[i].size = spin(g, 0.001, 100, 3, tr(" m"));
        m_auto[i].radius = spin(g, 0, 100, 2, tr(" m"));
        grid->addWidget(m_auto[i].on, i + 1, 0);
        grid->addWidget(m_auto[i].size, i + 1, 1);
        grid->addWidget(m_auto[i].radius, i + 1, 2);
        connect(m_auto[i].on, &QCheckBox::toggled, this, &MeshSettingsPanel::readWidgets);
        connect(m_auto[i].size, &QDoubleSpinBox::editingFinished, this, &MeshSettingsPanel::readWidgets);
        connect(m_auto[i].radius, &QDoubleSpinBox::editingFinished, this, &MeshSettingsPanel::readWidgets);
    }
    v->addWidget(g);
    auto* gz = new QGroupBox(tr("Zones de raffinement"), w);
    auto* vz = new QVBoxLayout(gz);
    m_zones = new QTableWidget(0, 5, gz);
    m_zones->setHorizontalHeaderLabels({ tr("Nom"), tr("Type"), tr("Centre / nœud"), tr("Rayon"), tr("Taille / facteur") });
    m_zones->horizontalHeader()->setStretchLastSection(true);
    m_zones->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_zones->setSelectionBehavior(QAbstractItemView::SelectRows);
    vz->addWidget(m_zones);
    auto* row = new QHBoxLayout;
    auto* point = new QPushButton(tr("En un point…"), gz);
    auto* fromSel = new QPushButton(tr("Autour de la sélection…"), gz);
    fromSel->setToolTip(tr("Zone autour des nœuds sélectionnés dans la vue 3D"));
    auto* supports = new QPushButton(tr("Appuis…"), gz);
    auto* loads = new QPushButton(tr("Charges…"), gz);
    auto* remove = new QPushButton(tr("Supprimer"), gz);
    for (auto* b : { point, fromSel, supports, loads, remove }) row->addWidget(b);
    vz->addLayout(row);
    v->addWidget(gz, 1);
    tabs->addTab(w, tr("Raffinement"));

    auto askZone = [this](RefinementZone z) {
        bool ok = false;
        const double size = QInputDialog::getDouble(this, tr("Zone de raffinement"), tr("Taille des éléments dans la zone (m) :"), 0.1, 0.001, 100, 3, &ok);
        if (!ok) return;
        z.radius = QInputDialog::getDouble(this, tr("Zone de raffinement"), tr("Rayon de la zone (m) :"), 0.5, 0, 100, 2, &ok);
        if (!ok) return;
        z.size = size;
        m_controller->settings().zones.push_back(z);
        m_controller->commitSettings();
        refreshTables();
    };
    connect(point, &QPushButton::clicked, this, [this, askZone] {
        bool ok = false;
        const QString t = QInputDialog::getText(this, tr("Zone en un point"), tr("Coordonnées x ; y ; z (m) :"), QLineEdit::Normal, QStringLiteral("0 ; 0 ; 0"), &ok);
        if (!ok) return;
        const QStringList p = t.split(';');
        if (p.size() != 3) return;
        RefinementZone z;
        z.kind = RefinementZone::Kind::Point;
        z.name = tr("point").toStdString();
        z.x = p[0].trimmed().replace(',', '.').toDouble(), z.y = p[1].trimmed().replace(',', '.').toDouble(), z.z = p[2].trimmed().replace(',', '.').toDouble();
        askZone(z);
    });
    connect(fromSel, &QPushButton::clicked, this, &MeshSettingsPanel::zoneFromSelectionRequested);
    connect(supports, &QPushButton::clicked, this, [askZone, this] {
        RefinementZone z;
        z.kind = RefinementZone::Kind::Support;
        z.name = tr("appuis").toStdString();
        askZone(z);
    });
    connect(loads, &QPushButton::clicked, this, [askZone, this] {
        RefinementZone z;
        z.kind = RefinementZone::Kind::Load;
        z.name = tr("charges ponctuelles").toStdString();
        askZone(z);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int r = m_zones->currentRow();
        auto& zones = m_controller->settings().zones;
        if (r < 0 || r >= int(zones.size())) return;
        zones.erase(zones.begin() + r);
        m_controller->commitSettings();
        refreshTables();
    });
}

void MeshSettingsPanel::buildLocals(QTabWidget* tabs)
{
    auto* w = new QWidget(tabs);
    auto* v = new QVBoxLayout(w);
    m_locals = new QTableWidget(0, 6, w);
    m_locals->setHorizontalHeaderLabels({ tr("Nom"), tr("Objets"), tr("Taille"), tr("Divisions"), tr("Répartition"), tr("Autres") });
    m_locals->horizontalHeader()->setStretchLastSection(true);
    m_locals->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_locals->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->addWidget(m_locals);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("Appliquer à la sélection…"), w);
    add->setToolTip(tr("Paramètres propres aux objets sélectionnés (barres, dalles, voiles, semelles), sans modifier les autres"));
    auto* remove = new QPushButton(tr("Supprimer"), w);
    row->addWidget(add), row->addWidget(remove), row->addStretch();
    v->addLayout(row);
    tabs->addTab(w, tr("Paramètres locaux"));
    connect(add, &QPushButton::clicked, this, &MeshSettingsPanel::localForSelectionRequested);
    connect(remove, &QPushButton::clicked, this, [this] {
        const int r = m_locals->currentRow();
        auto& locals = m_controller->settings().locals;
        if (r < 0 || r >= int(locals.size())) return;
        locals.erase(locals.begin() + r);
        m_controller->commitSettings();
        refreshTables();
    });
}

void MeshSettingsPanel::buildQuality(QTabWidget* tabs)
{
    auto* w = new QWidget(tabs);
    auto* f = new QFormLayout(w);
    m_qAspect = spin(w, 1, 1000, 2, QString());
    m_qMinAngle = spin(w, 0, 90, 1, tr(" °"));
    m_qMaxAngle = spin(w, 90, 180, 1, tr(" °"));
    m_qJac = spin(w, 0, 1, 2, QString());
    m_qGrowth = spin(w, 1, 100, 2, QString());
    m_qSicn = spin(w, 0, 1, 2, QString());
    f->addRow(tr("Rapport d'aspect maximal :"), m_qAspect);
    f->addRow(tr("Angle minimal :"), m_qMinAngle);
    f->addRow(tr("Angle maximal :"), m_qMaxAngle);
    f->addRow(tr("Jacobien normalisé minimal :"), m_qJac);
    f->addRow(tr("Rapport de tailles 1D maximal :"), m_qGrowth);
    f->addRow(tr("Qualité Gmsh minSICN minimale :"), m_qSicn);
    m_boundary = new QCheckBox(tr("Éléments de frontière (contours des surfaces, faces des volumes)"), w);
    m_groups = new QCheckBox(tr("Groupes physiques (objets, appuis, matériaux, interfaces)"), w);
    f->addRow(QString(), m_boundary);
    f->addRow(QString(), m_groups);
    tabs->addTab(w, tr("Qualité"));
    for (auto* s : { m_qAspect, m_qMinAngle, m_qMaxAngle, m_qJac, m_qGrowth, m_qSicn }) connect(s, &QDoubleSpinBox::editingFinished, this, &MeshSettingsPanel::readWidgets);
    for (auto* c : { m_boundary, m_groups }) connect(c, &QCheckBox::toggled, this, &MeshSettingsPanel::readWidgets);
}

void MeshSettingsPanel::refresh()
{
    m_loading = true;
    const auto caps = meshEngineCapabilities();
    m_engine->setText(caps.available ? tr("<b>Moteur : %1 %2</b> — %3").arg(QString::fromStdString(caps.name), QString::fromStdString(caps.version), QString::fromStdString(caps.license))
                                     : tr("<b>Maillage indisponible</b> : %1").arg(QString::fromStdString(caps.unavailableReason)));
    const auto& g = m_controller->settings().global;
    m_d1->setChecked(g.mesh1D), m_d2->setChecked(g.mesh2D), m_d3->setChecked(g.mesh3D);
    m_size->setValue(g.globalSize), m_min->setValue(g.minSize), m_max->setValue(g.maxSize);
    m_fineness->setCurrentIndex(int(g.fineness));
    m_factor->setValue(g.refinementFactor), m_rate->setValue(g.transitionRate);
    m_order->setCurrentIndex(g.order == ElementOrder::Second ? 1 : 0);
    m_keepPoints->setChecked(g.keepCharacteristicPoints);
    m_maxElements->setValue(g.maxElements);
    m_algo2D->setCurrentIndex(std::max(0, m_algo2D->findData(static_cast<int>(g.algorithm2D))));
    m_structured->setChecked(g.preferStructured), m_recombine->setChecked(g.recombine);
    m_recombAlgo->setCurrentIndex(std::max(0, m_recombAlgo->findData(static_cast<int>(g.recombination))));
    m_smoothing->setValue(g.smoothing);
    m_volume->setCurrentIndex(g.volumeElement == VolumeElement::Hexahedra && m_volume->count() > 1 ? 1 : 0);
    m_algo3D->setCurrentIndex(std::max(0, m_algo3D->findData(static_cast<int>(g.algorithm3D))));
    m_optimize->setChecked(g.optimize);
    const AutoRefinement* autos[5] = { &g.nearSupports, &g.nearPointLoads, &g.nearSectionChanges, &g.nearConnections, &g.nearOpenings };
    for (int i = 0; i < 5; ++i) m_auto[i].on->setChecked(autos[i]->enabled), m_auto[i].size->setValue(autos[i]->size), m_auto[i].radius->setValue(autos[i]->radius);
    m_qAspect->setValue(g.quality.maxAspectRatio), m_qMinAngle->setValue(g.quality.minAngleDeg), m_qMaxAngle->setValue(g.quality.maxAngleDeg);
    m_qJac->setValue(g.quality.minScaledJacobian), m_qGrowth->setValue(g.quality.maxGrowthRatio), m_qSicn->setValue(g.quality.minSICN);
    m_boundary->setChecked(g.boundaryElements), m_groups->setChecked(g.physicalGroups);
    m_loading = false;
    refreshProfiles();
    refreshTables();
    refreshStatus();
    setEnabled(true);
    for (auto* b : { m_btnPreview, m_btnGenerate, m_btnRegenerate }) b->setEnabled(caps.available && !m_controller->busy());
}

void MeshSettingsPanel::readWidgets()
{
    if (m_loading) return;
    auto& g = m_controller->settings().global;
    g.mesh1D = m_d1->isChecked(), g.mesh2D = m_d2->isChecked(), g.mesh3D = m_d3->isChecked();
    g.globalSize = m_size->value(), g.minSize = m_min->value(), g.maxSize = m_max->value();
    g.fineness = static_cast<Fineness>(m_fineness->currentIndex());
    g.refinementFactor = m_factor->value(), g.transitionRate = m_rate->value();
    g.order = m_order->currentIndex() == 1 ? ElementOrder::Second : ElementOrder::First;
    g.keepCharacteristicPoints = m_keepPoints->isChecked();
    g.maxElements = m_maxElements->value();
    g.algorithm2D = static_cast<Algorithm2D>(m_algo2D->currentData().toInt());
    g.preferStructured = m_structured->isChecked(), g.recombine = m_recombine->isChecked();
    g.recombination = static_cast<RecombinationAlgorithm>(m_recombAlgo->currentData().toInt());
    g.smoothing = m_smoothing->value();
    g.volumeElement = m_volume->currentIndex() == 1 ? VolumeElement::Hexahedra : VolumeElement::Tetrahedra;
    g.algorithm3D = static_cast<Algorithm3D>(m_algo3D->currentData().toInt());
    g.optimize = m_optimize->isChecked();
    AutoRefinement* autos[5] = { &g.nearSupports, &g.nearPointLoads, &g.nearSectionChanges, &g.nearConnections, &g.nearOpenings };
    for (int i = 0; i < 5; ++i) *autos[i] = { m_auto[i].on->isChecked(), m_auto[i].size->value(), m_auto[i].radius->value() };
    g.quality.maxAspectRatio = m_qAspect->value(), g.quality.minAngleDeg = m_qMinAngle->value(), g.quality.maxAngleDeg = m_qMaxAngle->value();
    g.quality.minScaledJacobian = m_qJac->value(), g.quality.maxGrowthRatio = m_qGrowth->value(), g.quality.minSICN = m_qSicn->value();
    g.boundaryElements = m_boundary->isChecked(), g.physicalGroups = m_groups->isChecked();
    m_controller->commitSettings();
    refreshStatus();
}

void MeshSettingsPanel::refreshTables()
{
    const auto& s = m_controller->settings();
    static const char* kinds[] = { "point", "nœud", "appuis", "charges" };
    m_zones->setRowCount(0);
    for (const auto& z : s.zones)
    {
        const int r = m_zones->rowCount();
        m_zones->insertRow(r);
        const QString where = z.kind == RefinementZone::Kind::Point ? QStringLiteral("(%1 ; %2 ; %3)").arg(num(z.x), num(z.y), num(z.z))
                            : z.kind == RefinementZone::Kind::Node  ? tr("N%1").arg(z.nodeId)
                                                                    : (z.nodeId ? tr("N%1").arg(z.nodeId) : tr("tous"));
        const QString val = z.size ? tr("%1 m").arg(num(*z.size)) : tr("× %1").arg(num(z.factor.value_or(1)));
        const QStringList cells { QString::fromStdString(z.name), QString::fromUtf8(kinds[int(z.kind)]), where, tr("%1 m").arg(num(z.radius)), val };
        for (int c = 0; c < cells.size(); ++c) m_zones->setItem(r, c, new QTableWidgetItem(cells[c]));
    }
    static const char* dist[] = { "uniforme", "progressive", "symétrique" };
    m_locals->setRowCount(0);
    for (const auto& l : s.locals)
    {
        const int r = m_locals->rowCount();
        m_locals->insertRow(r);
        QStringList targets;
        for (const auto& t : l.targets) targets << QString::fromStdString(t);
        QStringList other;
        if (l.maxLength) other << tr("long. max %1 m").arg(num(*l.maxLength));
        if (l.endSize) other << tr("extrémités %1 m").arg(num(*l.endSize));
        if (!l.portions.empty()) other << tr("%1 portion(s)").arg(l.portions.size());
        if (l.divisionsU || l.divisionsV) other << tr("divisions %1 × %2").arg(l.divisionsU.value_or(0)).arg(l.divisionsV.value_or(0));
        if (l.shape) other << (*l.shape == SurfaceShape::Quadrilaterals ? tr("quadrangles") : tr("triangles"));
        if (l.structured) other << (*l.structured ? tr("structuré imposé") : tr("non structuré"));
        const QStringList cells { QString::fromStdString(l.name), targets.join(QStringLiteral(", ")), l.size ? tr("%1 m").arg(num(*l.size)) : QString(),
                                  l.divisions ? QString::number(*l.divisions) : QString(),
                                  l.distribution == Distribution::Uniform ? QString::fromUtf8(dist[0]) : QStringLiteral("%1 (%2)").arg(QString::fromUtf8(dist[int(l.distribution)])).arg(num(l.progression)),
                                  other.join(QStringLiteral(" ; ")) };
        for (int c = 0; c < cells.size(); ++c) m_locals->setItem(r, c, new QTableWidgetItem(cells[c]));
    }
}

void MeshSettingsPanel::refreshStatus()
{
    QString s = meshStatusText(*m_controller);
    if (const auto* p = m_controller->preview())
        s += tr("\nAperçu affiché : %1 nœuds, %2 éléments (non enregistré)").arg(p->nodes.size()).arg(p->countByDimension(1) + p->countByDimension(2) + p->countByDimension(3));
    m_status->setText(s);
}

void MeshSettingsPanel::refreshProfiles()
{
    const QString current = QString::fromStdString(m_controller->settings().profile);
    m_profile->clear();
    for (const auto& p : MeshProfileLibrary().all())
        m_profile->addItem(QString::fromStdString(p.name) + (p.builtIn ? QString() : tr(" (utilisateur)")), QString::fromStdString(p.name));
    const int i = m_profile->findData(current);
    if (i >= 0) m_profile->setCurrentIndex(i);
}

} // namespace TSA::UI
