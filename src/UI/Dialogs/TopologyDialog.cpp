#include "TopologyDialog.h"

#include "../../Model/Model.h"
#include "../../Model/ModelDiff.h"
#include "../HelpLauncher.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::Topology;

namespace
{
const char* kAxisCodes[] = { "XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX" };

constexpr EntityFamily kElementFamilies[] = { EntityFamily::Beam,  EntityFamily::Column, EntityFamily::Truss,
                                              EntityFamily::Cable, EntityFamily::Slab,   EntityFamily::Wall,
                                              EntityFamily::Foundation };

QComboBox* axisCombo(QWidget* parent)
{
    auto* c = new QComboBox(parent);
    for (const char* code : kAxisCodes)
    {
        const QString s = QString::fromLatin1(code);
        c->addItem(QStringLiteral("%1 puis %2 puis %3").arg(s[0]).arg(s[1]).arg(s[2]), s);
    }
    c->setToolTip(QObject::tr("Axe le plus significatif en premier : « Z puis Y puis X » numérote couche par couche, "
                              "puis rangée par rangée."));
    return c;
}

void setAxisCombo(QComboBox* c, const AxisOrder& o)
{
    const int i = c->findData(QString::fromStdString(o.code()));
    c->setCurrentIndex(i >= 0 ? i : 0);
}

QSpinBox* intSpin(QWidget* parent, int min, int max, const QString& tip)
{
    auto* s = new QSpinBox(parent);
    s->setRange(min, max);
    s->setToolTip(tip);
    return s;
}

/// Désactive un élément de liste et lui associe la raison (infobulle).
void setItemEnabled(QComboBox* combo, int index, bool enabled, const QString& tip)
{
    if (auto* model = qobject_cast<QStandardItemModel*>(combo->model()))
    {
        if (QStandardItem* item = model->item(index))
        {
            item->setEnabled(enabled);
            item->setToolTip(tip);
        }
    }
}
} // namespace

TopologyDialog::TopologyDialog(TSA::Model::Model* model, std::function<NumberingInput()> inputProvider, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_inputProvider(std::move(inputProvider))
{
    setWindowTitle(tr("Paramètres du projet — Topologie et numérotation"));
    setObjectName(QStringLiteral("topologyDialog"));
    setSizeGripEnabled(true);
    resize(900, 640);
    buildUi();
    m_previousSettings = TopologySettings::fromJson(m_model ? m_model->topologySettingsJson() : std::string());
    loadSettings(m_previousSettings);
    refreshAvailability();
    refreshMapping();
}

void TopologyDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("La renumérotation ne change que les <b>étiquettes visibles</b> (N001, A1, B012…). "
                                "Les identifiants internes, la géométrie, les connectivités, les charges, les appuis et "
                                "les résultats de calcul ne sont jamais modifiés. Prévisualisez avant d'appliquer ; "
                                "Ctrl+Z annule une renumérotation."),
                             this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildGeneralTab(), tr("Général"));
    m_tabs->addTab(buildNodesTab(), tr("Nœuds"));
    m_tabs->addTab(buildElementsTab(), tr("Éléments"));
    m_tabs->addTab(buildMeshTab(), tr("Maillage"));
    m_tabs->addTab(buildPreviewTab(), tr("Aperçu"));
    m_tabs->addTab(buildMappingTab(), tr("Correspondances solveur"));
    root->addWidget(m_tabs, 1);

    auto* buttons = new QHBoxLayout();
    auto* btnReset = new QPushButton(tr("Réinitialiser les paramètres"), this);
    btnReset->setToolTip(tr("Revenir aux valeurs par défaut (étiquettes historiques N001, B001…)"));
    auto* btnRestore = new QPushButton(tr("Restaurer la configuration précédente"), this);
    btnRestore->setToolTip(tr("Recharger les paramètres enregistrés dans le projet"));
    auto* btnPreview = new QPushButton(tr("Prévisualiser"), this);
    btnPreview->setObjectName(QStringLiteral("previewButton"));
    m_btnApply = new QPushButton(tr("Appliquer"), this);
    m_btnApply->setObjectName(QStringLiteral("applyButton"));
    m_btnApply->setToolTip(tr("Renumérote (étiquettes seulement) et enregistre les paramètres dans le projet"));
    auto* btnClose = new QPushButton(tr("Fermer"), this);
    if (auto* help = createHelpButton(this, QStringLiteral("project.topology"))) buttons->addWidget(help);
    buttons->addWidget(btnReset);
    buttons->addWidget(btnRestore);
    buttons->addStretch(1);
    buttons->addWidget(btnPreview);
    buttons->addWidget(m_btnApply);
    buttons->addWidget(btnClose);
    root->addLayout(buttons);

    connect(btnReset, &QPushButton::clicked, this, &TopologyDialog::resetToDefaults);
    connect(btnRestore, &QPushButton::clicked, this, &TopologyDialog::restorePrevious);
    connect(btnPreview, &QPushButton::clicked, this, [this] {
        preview();
        m_tabs->setCurrentIndex(4);
    });
    connect(m_btnApply, &QPushButton::clicked, this, [this] { apply(); });
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
}

QWidget* TopologyDialog::buildGeneralTab()
{
    auto* w = new QWidget(this);
    auto* form = new QFormLayout(w);
    m_lblDimension = new QLabel(w);
    form->addRow(tr("Dimension du modèle :"), m_lblDimension);

    m_comboScope = new QComboBox(w);
    m_comboScope->setObjectName(QStringLiteral("scopeCombo"));
    m_comboScope->addItem(tr("Projet entier"), static_cast<int>(NumberingScope::Project));
    m_comboScope->addItem(tr("Entités sélectionnées"), static_cast<int>(NumberingScope::Selection));
    m_comboScope->setToolTip(tr("Sélection : seules les entités sélectionnées sont renumérotées ; les autres gardent "
                                "leur étiquette et sont prises en compte pour détecter les doublons."));
    form->addRow(tr("Portée :"), m_comboScope);

    auto* mode = new QLabel(tr("Manuel : la numérotation est appliquée à la demande (bouton Appliquer). "
                               "Les nouvelles entités reçoivent l'étiquette historique (N012, B004…) jusqu'à la "
                               "prochaine renumérotation. Le mode automatique n'est pas encore disponible."),
                            w);
    mode->setWordWrap(true);
    form->addRow(tr("Mode :"), mode);

    m_chkShowLabels = new QCheckBox(tr("Afficher les étiquettes des nœuds dans la vue 3D"), w);
    m_chkShowLabels->setObjectName(QStringLiteral("showLabelsCheck"));
    form->addRow(QString(), m_chkShowLabels);

    auto* tokens = new QLabel(tr("Jetons de format : <b>{p}</b> préfixe, <b>{n}</b> numéro (complété au nombre de "
                                 "chiffres), <b>{g}</b> repère de grille (nœuds), <b>{l}</b> numéro de couche ou de "
                                 "niveau. Exemples : « {p}{n} » → N001 ; « {g} » → A1 ; « {p}{l}-{n} » → N2-01."),
                              w);
    tokens->setWordWrap(true);
    form->addRow(tr("Formats :"), tokens);
    return w;
}

QWidget* TopologyDialog::buildNodesTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    auto* top = new QFormLayout();
    m_comboNodeStrategy = new QComboBox(w);
    m_comboNodeStrategy->setObjectName(QStringLiteral("nodeStrategyCombo"));
    for (const auto& s : nodeStrategies())
        m_comboNodeStrategy->addItem(QString::fromStdString(s->name()), QString::fromStdString(s->id()));
    top->addRow(tr("Stratégie :"), m_comboNodeStrategy);
    m_lblNodeDescription = new QLabel(w);
    m_lblNodeDescription->setWordWrap(true);
    top->addRow(QString(), m_lblNodeDescription);
    lay->addLayout(top);

    auto* grpOrder = new QGroupBox(tr("Ordre de parcours"), w);
    auto* fo = new QFormLayout(grpOrder);
    m_comboNodeAxes = axisCombo(grpOrder);
    fo->addRow(tr("Ordre des axes :"), m_comboNodeAxes);
    auto* desc = new QHBoxLayout();
    const char* axes[] = { "X", "Y", "Z" };
    for (int i = 0; i < 3; ++i)
    {
        m_chkNodeDesc[i] = new QCheckBox(tr("%1 décroissant").arg(axes[i]), grpOrder);
        desc->addWidget(m_chkNodeDesc[i]);
    }
    fo->addRow(tr("Sens :"), desc);
    m_spinNodeTol = new QDoubleSpinBox(grpOrder);
    m_spinNodeTol->setDecimals(4);
    m_spinNodeTol->setRange(0.0001, 10.0);
    m_spinNodeTol->setSuffix(QStringLiteral(" m"));
    m_spinNodeTol->setToolTip(tr("Coordonnées à moins de cette distance : même rangée, colonne, couche ou intersection de grille."));
    fo->addRow(tr("Tolérance de regroupement :"), m_spinNodeTol);
    m_spinStartNode = intSpin(grpOrder, 0, 99999999, tr("Identifiant interne du nœud de départ des parcours (0 : choix automatique)."));
    m_spinStartNode->setSpecialValueText(tr("Automatique"));
    fo->addRow(tr("Nœud de départ :"), m_spinStartNode);
    lay->addWidget(grpOrder);

    auto* grpLabels = new QGroupBox(tr("Étiquettes"), w);
    auto* fl = new QFormLayout(grpLabels);
    m_editNodePrefix = new QLineEdit(grpLabels);
    m_editNodePrefix->setObjectName(QStringLiteral("nodePrefixEdit"));
    fl->addRow(tr("Préfixe {p} :"), m_editNodePrefix);
    m_editNodePattern = new QLineEdit(grpLabels);
    m_editNodePattern->setObjectName(QStringLiteral("nodePatternEdit"));
    fl->addRow(tr("Format :"), m_editNodePattern);
    m_spinNodeWidth = intSpin(grpLabels, 0, 9, tr("Nombre minimum de chiffres de {n} (complété par des zéros)."));
    fl->addRow(tr("Chiffres :"), m_spinNodeWidth);
    m_spinNodeStart = intSpin(grpLabels, 0, 100000000, tr("Premier numéro."));
    m_spinNodeStart->setObjectName(QStringLiteral("nodeStartSpin"));
    fl->addRow(tr("Numéro de départ :"), m_spinNodeStart);
    m_spinNodeIncrement = intSpin(grpLabels, 1, 100000, tr("Écart entre deux numéros successifs."));
    fl->addRow(tr("Incrément :"), m_spinNodeIncrement);
    m_chkRestartPerLayer = new QCheckBox(tr("Recommencer la numérotation à chaque couche ou niveau (à combiner avec {l})"), grpLabels);
    fl->addRow(QString(), m_chkRestartPerLayer);
    m_chkPreserveNodes = new QCheckBox(tr("Conserver les étiquettes personnalisées (saisies à la main)"), grpLabels);
    m_chkPreserveNodes->setToolTip(tr("Une étiquette qui ne suit ni le format historique ni le format choisi est conservée."));
    fl->addRow(QString(), m_chkPreserveNodes);
    lay->addWidget(grpLabels);
    lay->addStretch(1);

    connect(m_comboNodeStrategy, &QComboBox::currentIndexChanged, this, [this] {
        updateStrategyControls();
        // Format conseillé (grille : {g}) seulement s'il n'a pas été personnalisé.
        const auto* s = findNodeStrategy(m_comboNodeStrategy->currentData().toString().toStdString());
        if (s && (m_editNodePattern->text() == QLatin1String("{p}{n}") || m_editNodePattern->text() == QLatin1String("{g}")))
            m_editNodePattern->setText(QString::fromStdString(s->suggestedPattern()));
    });
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(w);
    return scroll;
}

QWidget* TopologyDialog::buildElementsTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    auto* top = new QFormLayout();
    m_comboElemStrategy = new QComboBox(w);
    m_comboElemStrategy->setObjectName(QStringLiteral("elemStrategyCombo"));
    for (const auto& s : elementStrategies())
        m_comboElemStrategy->addItem(QString::fromStdString(s->name()), QString::fromStdString(s->id()));
    top->addRow(tr("Stratégie :"), m_comboElemStrategy);
    m_lblElemDescription = new QLabel(w);
    m_lblElemDescription->setWordWrap(true);
    top->addRow(QString(), m_lblElemDescription);
    m_chkSharedSequence = new QCheckBox(tr("Une seule suite de numéros pour toutes les familles"), w);
    m_chkSharedSequence->setToolTip(tr("Décoché : chaque famille (poutres, poteaux…) a sa propre suite B001, C001…"));
    top->addRow(QString(), m_chkSharedSequence);
    m_comboElemAxes = axisCombo(w);
    top->addRow(tr("Ordre des axes (proximité) :"), m_comboElemAxes);
    m_spinElemTol = new QDoubleSpinBox(w);
    m_spinElemTol->setDecimals(4);
    m_spinElemTol->setRange(0.0001, 10.0);
    m_spinElemTol->setSuffix(QStringLiteral(" m"));
    top->addRow(tr("Tolérance (proximité) :"), m_spinElemTol);
    lay->addLayout(top);

    auto* grpPrefix = new QGroupBox(tr("Préfixes par famille {p}"), w);
    auto* fp = new QFormLayout(grpPrefix);
    for (EntityFamily f : kElementFamilies)
    {
        auto* edit = new QLineEdit(grpPrefix);
        m_prefixEdits[f] = edit;
        fp->addRow(QString::fromStdString(familyDisplayName(f)) + QStringLiteral(" :"), edit);
    }
    auto* note = new QLabel(tr("Les barres de rôle « poteau » utilisent le préfixe des poteaux."), grpPrefix);
    note->setWordWrap(true);
    fp->addRow(QString(), note);
    lay->addWidget(grpPrefix);

    auto* grpLabels = new QGroupBox(tr("Étiquettes"), w);
    auto* fl = new QFormLayout(grpLabels);
    m_editElemPattern = new QLineEdit(grpLabels);
    fl->addRow(tr("Format :"), m_editElemPattern);
    m_spinElemWidth = intSpin(grpLabels, 0, 9, tr("Nombre minimum de chiffres de {n}."));
    fl->addRow(tr("Chiffres :"), m_spinElemWidth);
    m_spinElemStart = intSpin(grpLabels, 0, 100000000, tr("Premier numéro."));
    fl->addRow(tr("Numéro de départ :"), m_spinElemStart);
    m_spinElemIncrement = intSpin(grpLabels, 1, 100000, tr("Écart entre deux numéros successifs."));
    fl->addRow(tr("Incrément :"), m_spinElemIncrement);
    m_chkPreserveElems = new QCheckBox(tr("Conserver les étiquettes personnalisées"), grpLabels);
    fl->addRow(QString(), m_chkPreserveElems);
    lay->addWidget(grpLabels);
    auto* orient = new QLabel(tr("La renumérotation ne modifie jamais les extrémités, l'orientation ni le repère local "
                                 "des éléments."),
                              w);
    orient->setWordWrap(true);
    lay->addWidget(orient);
    lay->addStretch(1);

    connect(m_comboElemStrategy, &QComboBox::currentIndexChanged, this, [this] { updateStrategyControls(); });
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(w);
    return scroll;
}

QWidget* TopologyDialog::buildMeshTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    auto* text = new QLabel(tr("<b>Aucun mailleur dans cette version de TSA.</b><br><br>"
                               "Les barres sont transmises au solveur sans maillage ; la commande « Générer maillage » "
                               "ne fait qu'une estimation et les dalles et voiles ne sont pas calculés. Il n'existe donc "
                               "pas de nœuds ni d'éléments de maillage à numéroter.<br><br>"
                               "Les options propres au maillage (dimension 1D/2D/3D, maillage structuré, non structuré ou "
                               "hybride, ordre des couches, nœuds de frontière, nœuds partagés) seront proposées ici quand "
                               "un mailleur sera disponible. Les stratégies « orientées maillage » sont affichées comme "
                               "indisponibles dans les onglets Nœuds et Éléments."),
                            w);
    text->setWordWrap(true);
    text->setTextFormat(Qt::RichText);
    lay->addWidget(text);
    lay->addStretch(1);
    return w;
}

QWidget* TopologyDialog::buildPreviewTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    auto* top = new QHBoxLayout();
    m_lblSummary = new QLabel(tr("Cliquez sur « Prévisualiser » : le modèle n'est pas modifié."), w);
    m_lblSummary->setObjectName(QStringLiteral("previewSummary"));
    m_lblSummary->setWordWrap(true);
    top->addWidget(m_lblSummary, 1);
    m_chkChangedOnly = new QCheckBox(tr("Seulement les étiquettes modifiées"), w);
    top->addWidget(m_chkChangedOnly);
    lay->addLayout(top);

    m_table = new QTableWidget(0, 9, w);
    m_table->setObjectName(QStringLiteral("previewTable"));
    m_table->setHorizontalHeaderLabels({ tr("Type"), tr("Id interne"), tr("Étiquette actuelle"), tr("Nouvelle étiquette"),
                                         tr("Repère grille"), tr("X (m)"), tr("Y (m)"), tr("Z (m)"), tr("Tag solveur") });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    lay->addWidget(m_table, 1);

    m_messages = new QListWidget(w);
    m_messages->setObjectName(QStringLiteral("previewMessages"));
    m_messages->setMaximumHeight(110);
    lay->addWidget(m_messages);
    connect(m_chkChangedOnly, &QCheckBox::toggled, this, [this] { showPreview(); });
    return w;
}

QWidget* TopologyDialog::buildMappingTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    auto* text = new QLabel(tr("Correspondance entre l'identifiant interne (stable), l'étiquette visible et le numéro "
                               "transmis au solveur OpenSees. Les solveurs utilisent les identifiants internes : une "
                               "renumérotation des étiquettes ne change pas ces numéros."),
                            w);
    text->setWordWrap(true);
    lay->addWidget(text);
    m_mappingTable = new QTableWidget(0, 4, w);
    m_mappingTable->setObjectName(QStringLiteral("mappingTable"));
    m_mappingTable->setHorizontalHeaderLabels({ tr("Type"), tr("Id interne"), tr("Étiquette"), tr("Tag solveur OpenSees") });
    m_mappingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mappingTable->horizontalHeader()->setStretchLastSection(true);
    m_mappingTable->verticalHeader()->setVisible(false);
    lay->addWidget(m_mappingTable, 1);
    auto* btnExport = new QPushButton(tr("Exporter la correspondance (CSV)..."), w);
    lay->addWidget(btnExport, 0, Qt::AlignRight);
    connect(btnExport, &QPushButton::clicked, this, &TopologyDialog::exportMapping);
    return w;
}

void TopologyDialog::refreshAvailability()
{
    const NumberingInput input = m_inputProvider ? m_inputProvider() : NumberingInput{};
    static const char* dims[] = { "vide ou un seul point", "1D (nœuds alignés)", "2D (nœuds dans un plan)", "3D" };
    const int dim = m_model ? detail::modelDimension(*m_model) : 0;
    m_lblDimension->setText(tr(dims[std::clamp(dim, 0, 3)]) +
                            tr(" — %1 nœud(s)").arg(m_model ? static_cast<int>(m_model->nodes().size()) : 0));
    for (int i = 0; i < m_comboNodeStrategy->count(); ++i)
    {
        const auto* s = findNodeStrategy(m_comboNodeStrategy->itemData(i).toString().toStdString());
        const Availability a = (s && m_model) ? s->availability(*m_model, input) : Availability{};
        setItemEnabled(m_comboNodeStrategy, i, a.available,
                       a.available ? QString::fromStdString(s->description()) : tr("Indisponible : %1").arg(QString::fromStdString(a.reason)));
        if (s) m_comboNodeStrategy->setItemText(i, QString::fromStdString(s->name()) + (a.available ? QString() : tr(" (indisponible)")));
    }
    for (int i = 0; i < m_comboElemStrategy->count(); ++i)
    {
        const auto* s = findElementStrategy(m_comboElemStrategy->itemData(i).toString().toStdString());
        const Availability a = (s && m_model) ? s->availability(*m_model, input) : Availability{};
        setItemEnabled(m_comboElemStrategy, i, a.available,
                       a.available ? QString::fromStdString(s->description()) : tr("Indisponible : %1").arg(QString::fromStdString(a.reason)));
        if (s) m_comboElemStrategy->setItemText(i, QString::fromStdString(s->name()) + (a.available ? QString() : tr(" (indisponible)")));
    }
    updateStrategyControls();
}

void TopologyDialog::updateStrategyControls()
{
    const NumberingInput input = m_inputProvider ? m_inputProvider() : NumberingInput{};
    const std::string nid = m_comboNodeStrategy->currentData().toString().toStdString();
    if (const auto* s = findNodeStrategy(nid))
    {
        const Availability a = m_model ? s->availability(*m_model, input) : Availability{};
        m_lblNodeDescription->setText(a.available ? QString::fromStdString(s->description())
                                                  : tr("<b>Indisponible pour ce modèle :</b> %1").arg(QString::fromStdString(a.reason)));
    }
    const bool keep = nid == "keep";
    const bool graph = nid == "bfs" || nid == "dfs" || nid == "rcm";
    const bool axisSorted = nid == "coordinates" || nid == "xyz" || nid == "zyx" || nid == "rows" || nid == "columns" || nid == "layers";
    m_comboNodeAxes->setEnabled(nid == "coordinates");
    for (auto* c : m_chkNodeDesc) c->setEnabled(axisSorted);
    m_spinNodeTol->setEnabled(axisSorted || nid == "grid" || nid == "level" || graph);
    m_spinStartNode->setEnabled(graph);
    for (QWidget* wdg : std::initializer_list<QWidget*>{ m_editNodePrefix, m_editNodePattern, m_spinNodeWidth, m_spinNodeStart,
                                                        m_spinNodeIncrement, m_chkRestartPerLayer, m_chkPreserveNodes })
        wdg->setEnabled(!keep);

    const std::string eid = m_comboElemStrategy->currentData().toString().toStdString();
    if (const auto* s = findElementStrategy(eid))
    {
        const Availability a = m_model ? s->availability(*m_model, input) : Availability{};
        m_lblElemDescription->setText(a.available ? QString::fromStdString(s->description())
                                                  : tr("<b>Indisponible pour ce modèle :</b> %1").arg(QString::fromStdString(a.reason)));
    }
    const bool ekeep = eid == "keep";
    m_comboElemAxes->setEnabled(eid == "proximity");
    m_spinElemTol->setEnabled(eid == "proximity");
    for (QWidget* wdg : std::initializer_list<QWidget*>{ m_chkSharedSequence, m_editElemPattern, m_spinElemWidth, m_spinElemStart,
                                                        m_spinElemIncrement, m_chkPreserveElems })
        wdg->setEnabled(!ekeep);
    for (auto& [f, edit] : m_prefixEdits) edit->setEnabled(!ekeep);
}

TopologySettings TopologyDialog::settingsFromUi() const
{
    TopologySettings s = TopologySettings::defaults();
    s.scope = static_cast<NumberingScope>(m_comboScope->currentData().toInt());
    s.showNodeLabels = m_chkShowLabels->isChecked();

    s.nodes.strategy = m_comboNodeStrategy->currentData().toString().toStdString();
    AxisOrder::fromCode(m_comboNodeAxes->currentData().toString().toStdString(), s.nodes.order);
    for (int i = 0; i < 3; ++i) s.nodes.order.descending[i] = m_chkNodeDesc[i]->isChecked();
    s.nodes.tolerance = m_spinNodeTol->value();
    s.nodes.startNodeId = m_spinStartNode->value();
    s.nodes.restartPerLayer = m_chkRestartPerLayer->isChecked();
    s.nodes.preserveCustomNames = m_chkPreserveNodes->isChecked();
    s.nodes.format.prefix = m_editNodePrefix->text().toStdString();
    s.nodes.format.pattern = m_editNodePattern->text().toStdString();
    s.nodes.format.width = m_spinNodeWidth->value();
    s.nodes.format.start = m_spinNodeStart->value();
    s.nodes.format.increment = m_spinNodeIncrement->value();

    s.elements.strategy = m_comboElemStrategy->currentData().toString().toStdString();
    s.elements.sharedSequence = m_chkSharedSequence->isChecked();
    AxisOrder::fromCode(m_comboElemAxes->currentData().toString().toStdString(), s.elements.order);
    s.elements.tolerance = m_spinElemTol->value();
    s.elements.preserveCustomNames = m_chkPreserveElems->isChecked();
    s.elements.format.pattern = m_editElemPattern->text().toStdString();
    s.elements.format.width = m_spinElemWidth->value();
    s.elements.format.start = m_spinElemStart->value();
    s.elements.format.increment = m_spinElemIncrement->value();
    for (const auto& [f, edit] : m_prefixEdits) s.elements.prefixes[f] = edit->text().toStdString();
    return s;
}

void TopologyDialog::loadSettings(const TopologySettings& s)
{
    m_comboScope->setCurrentIndex(m_comboScope->findData(static_cast<int>(s.scope)));
    m_chkShowLabels->setChecked(s.showNodeLabels);

    const int ni = m_comboNodeStrategy->findData(QString::fromStdString(s.nodes.strategy));
    m_comboNodeStrategy->setCurrentIndex(ni >= 0 ? ni : 0);
    setAxisCombo(m_comboNodeAxes, s.nodes.order);
    for (int i = 0; i < 3; ++i) m_chkNodeDesc[i]->setChecked(s.nodes.order.descending[i]);
    m_spinNodeTol->setValue(s.nodes.tolerance);
    m_spinStartNode->setValue(s.nodes.startNodeId);
    m_chkRestartPerLayer->setChecked(s.nodes.restartPerLayer);
    m_chkPreserveNodes->setChecked(s.nodes.preserveCustomNames);
    m_editNodePrefix->setText(QString::fromStdString(s.nodes.format.prefix));
    m_editNodePattern->setText(QString::fromStdString(s.nodes.format.pattern));
    m_spinNodeWidth->setValue(s.nodes.format.width);
    m_spinNodeStart->setValue(s.nodes.format.start);
    m_spinNodeIncrement->setValue(s.nodes.format.increment);

    const int ei = m_comboElemStrategy->findData(QString::fromStdString(s.elements.strategy));
    m_comboElemStrategy->setCurrentIndex(ei >= 0 ? ei : 0);
    m_chkSharedSequence->setChecked(s.elements.sharedSequence);
    setAxisCombo(m_comboElemAxes, s.elements.order);
    m_spinElemTol->setValue(s.elements.tolerance);
    m_chkPreserveElems->setChecked(s.elements.preserveCustomNames);
    m_editElemPattern->setText(QString::fromStdString(s.elements.format.pattern));
    m_spinElemWidth->setValue(s.elements.format.width);
    m_spinElemStart->setValue(s.elements.format.start);
    m_spinElemIncrement->setValue(s.elements.format.increment);
    for (auto& [f, edit] : m_prefixEdits) edit->setText(QString::fromStdString(s.elementPrefix(f)));
    updateStrategyControls();
}

bool TopologyDialog::preview()
{
    if (!m_model) return false;
    const NumberingInput input = m_inputProvider ? m_inputProvider() : NumberingInput{};
    m_preview = computePreview(*m_model, settingsFromUi(), input);
    showPreview();
    return m_preview.valid();
}

void TopologyDialog::showPreview()
{
    const auto& p = m_preview;
    m_table->setRowCount(0);
    const bool changedOnly = m_chkChangedOnly->isChecked();
    int row = 0;
    m_table->setUpdatesEnabled(false);
    for (const auto& e : p.entries)
    {
        if (changedOnly && !e.changed()) continue;
        m_table->insertRow(row);
        const QStringList cells = { QString::fromStdString(familyDisplayName(e.family)), QString::number(e.id),
                                    QString::fromStdString(e.currentLabel),
                                    QString::fromStdString(e.newLabel) + (e.preserved ? tr(" (conservée)") : QString()),
                                    QString::fromStdString(e.gridRef), QString::number(e.x, 'f', 3), QString::number(e.y, 'f', 3),
                                    QString::number(e.z, 'f', 3), e.solverTag >= 0 ? QString::number(e.solverTag) : tr("non transmis") };
        for (int c = 0; c < cells.size(); ++c)
        {
            auto* item = new QTableWidgetItem(cells[c]);
            if (c == 3 && e.changed())
            {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }
            m_table->setItem(row, c, item);
        }
        ++row;
    }
    m_table->setUpdatesEnabled(true);
    m_table->resizeColumnsToContents();

    m_messages->clear();
    for (const auto& err : p.errors) m_messages->addItem(tr("Erreur : %1").arg(QString::fromStdString(err)));
    for (const auto& w : p.warnings) m_messages->addItem(tr("Avertissement : %1").arg(QString::fromStdString(w)));
    if (!p.valid())
        m_lblSummary->setText(tr("<b>Renumérotation impossible</b> : corrigez les erreurs ci-dessous."));
    else
        m_lblSummary->setText(tr("%1 entité(s), %2 étiquette(s) modifiée(s). Le modèle n'a pas été modifié.")
                                  .arg(p.entries.size())
                                  .arg(p.changedCount()));
    m_btnApply->setEnabled(true);
}

bool TopologyDialog::apply()
{
    if (!m_model) return false;
    if (!preview())
    {
        m_tabs->setCurrentIndex(4);
        QMessageBox::warning(this, windowTitle(),
                             tr("La renumérotation n'a pas été appliquée :\n%1").arg(QString::fromStdString(m_preview.errors.front())));
        return false;
    }
    const TopologySettings settings = settingsFromUi();
    std::string error;
    if (!applyPreview(*m_model, m_preview, tr("Renumérotation (topologie)").toStdString(), &error))
    {
        QMessageBox::warning(this, windowTitle(), QString::fromStdString(error));
        return false;
    }
    m_model->setTopologySettingsJson(settings.toJson());
    m_model->notifyLabelsChanged(TSA::Model::ModelDiff{}); // document modifié (paramètres), résultats conservés
    const int changed = m_preview.changedCount();
    preview(); // l'aperçu reflète maintenant le modèle renuméroté
    refreshMapping();
    m_lblSummary->setText(tr("<b>Appliqué</b> : %1 étiquette(s) modifiée(s), paramètres enregistrés dans le projet "
                             "(Ctrl+Z annule la renumérotation).").arg(changed));
    emit settingsApplied(settings);
    return true;
}

void TopologyDialog::resetToDefaults()
{
    loadSettings(TopologySettings::defaults());
}

void TopologyDialog::restorePrevious()
{
    loadSettings(m_previousSettings);
}

void TopologyDialog::refreshMapping()
{
    if (!m_model || !m_mappingTable) return;
    const auto rows = solverMapping(*m_model);
    m_mappingTable->setUpdatesEnabled(false);
    m_mappingTable->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
    {
        const auto& m = rows[r];
        m_mappingTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(familyDisplayName(m.family))));
        m_mappingTable->setItem(r, 1, new QTableWidgetItem(QString::number(m.id)));
        m_mappingTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(m.label)));
        m_mappingTable->setItem(r, 3, new QTableWidgetItem(m.solverTag >= 0 ? QString::number(m.solverTag) : tr("non transmis")));
    }
    m_mappingTable->setUpdatesEnabled(true);
    m_mappingTable->resizeColumnsToContents();
}

void TopologyDialog::exportMapping()
{
    if (!m_model) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Exporter la correspondance"), QStringLiteral("correspondance.csv"),
                                                      tr("CSV (*.csv)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, windowTitle(), tr("Impossible d'écrire « %1 ».").arg(path));
        return;
    }
    QTextStream out(&file);
    out << "famille;id_interne;etiquette;tag_solveur\n";
    for (const auto& m : solverMapping(*m_model))
        out << familyKey(m.family) << ';' << m.id << ';' << QString::fromStdString(m.label) << ';' << m.solverTag << '\n';
}

} // namespace TSA::UI
