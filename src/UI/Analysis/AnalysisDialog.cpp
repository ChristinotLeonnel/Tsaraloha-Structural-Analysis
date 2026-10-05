#include "AnalysisDialog.h"

#include "AnalysisEngineOptions.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Load/LoadManager.h"
#include "../../Model/Model.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::Analysis;

namespace
{
QString qs(const std::string& s) { return QString::fromStdString(s); }

QString typeLabel(AnalysisType t)
{
    switch (t)
    {
    case AnalysisType::LinearStatic: return QObject::tr("Statique linéaire");
    case AnalysisType::NonLinearStatic: return QObject::tr("Statique non linéaire");
    case AnalysisType::Modal: return QObject::tr("Modale (vibrations propres)");
    case AnalysisType::DynamicTimeHistory: return QObject::tr("Dynamique temporelle");
    case AnalysisType::Pushover: return QObject::tr("Pushover");
    }
    return {};
}

QString dimensionLabel(AnalysisDimension d)
{
    return d == AnalysisDimension::Plane2D ? QObject::tr("2D — ossature plane") : QObject::tr("3D — ossature spatiale");
}

QString capabilityText(const AnalysisCapabilities& c)
{
    auto row = [](const QString& name, bool ok) { return (ok ? QStringLiteral("✓ ") : QStringLiteral("✗ ")) + name; };
    const QStringList items {
        row(QObject::tr("2D"), c.supports2D), row(QObject::tr("3D"), c.supports3D),
        row(QObject::tr("Poutres/poteaux"), c.supportsFrame), row(QObject::tr("Treillis"), c.supportsTruss),
        row(QObject::tr("Câbles"), c.supportsCable), row(QObject::tr("Coques (dalles/voiles)"), c.supportsShell),
        row(QObject::tr("Ressorts"), c.supportsSprings), row(QObject::tr("Statique"), c.supportsStatic),
        row(QObject::tr("Non linéaire"), c.supportsNonlinear), row(QObject::tr("Modale"), c.supportsModal),
        row(QObject::tr("Déplacements"), c.providesDisplacements), row(QObject::tr("Réactions"), c.providesReactions),
        row(QObject::tr("Efforts"), c.providesElementForces), row(QObject::tr("K globale"), c.providesGlobalStiffness),
    };
    return items.join(QStringLiteral("   "));
}

QString loadKey(const AnalysisContext& c)
{
    if (c.combinationId > 0) return QStringLiteral("combo:%1").arg(c.combinationId);
    if (c.loadCaseIds.empty()) return QStringLiteral("all");
    QStringList ids;
    for (int id : c.loadCaseIds) ids << QString::number(id);
    return QStringLiteral("cases:") + ids.join(',');
}
} // namespace

AnalysisDialog::AnalysisDialog(AnalysisManager& manager, const AnalysisEngineOptionsRegistry& options,
                               const TSA::Model::Model* model, const TSA::Grid::GridManager* grids,
                               const TSA::Model::ElementSet& selection, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_options(options)
    , m_model(model)
    , m_grids(grids)
    , m_selection(selection)
{
    setWindowTitle(tr("Analyse structurelle"));
    setMinimumWidth(620);
    buildUi();
    populateEngines();
    populateScopes();
    populateLoads();
    if (!m_manager.registry().ids().empty()) m_context.engineId = m_manager.registry().ids().front();
    setContext(m_context);
}

void AnalysisDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    // Contenu défilant : le panneau d'options d'un moteur peut être long.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* lay = new QVBoxLayout(content);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto* groupEngine = new QGroupBox(tr("Moteur de calcul"), content);
    auto* fe = new QFormLayout(groupEngine);
    m_engineCombo = new QComboBox(groupEngine);
    fe->addRow(tr("Moteur :"), m_engineCombo);
    m_engineInfo = new QLabel(groupEngine);
    m_engineInfo->setWordWrap(true);
    fe->addRow(m_engineInfo);
    m_capabilities = new QLabel(groupEngine);
    m_capabilities->setWordWrap(true);
    m_capabilities->setStyleSheet("color: palette(mid);");
    fe->addRow(m_capabilities);
    lay->addWidget(groupEngine);

    auto* groupCommon = new QGroupBox(tr("Analyse"), content);
    auto* fc = new QFormLayout(groupCommon);
    m_dimensionCombo = new QComboBox(groupCommon);
    fc->addRow(tr("Dimension :"), m_dimensionCombo);
    m_typeCombo = new QComboBox(groupCommon);
    fc->addRow(tr("Type d'analyse :"), m_typeCombo);
    m_scopeCombo = new QComboBox(groupCommon);
    m_scopeCombo->setToolTip(tr("Partie du modèle calculée : un axe de grille, un niveau ou un plan de travail "
                                "définit le plan d'une analyse 2D."));
    fc->addRow(tr("Portée :"), m_scopeCombo);
    m_levelCombo = new QComboBox(groupCommon);
    m_levelCombo->setToolTip(tr("Ne garder que les éléments situés à la cote de ce niveau (intersection)."));
    fc->addRow(tr("Restreindre au niveau :"), m_levelCombo);
    m_loadCombo = new QComboBox(groupCommon);
    fc->addRow(tr("Chargement :"), m_loadCombo);
    m_selfWeight = new QCheckBox(tr("Inclure le poids propre"), groupCommon);
    fc->addRow(QString(), m_selfWeight);
    m_modeCount = new QSpinBox(groupCommon);
    m_modeCount->setRange(1, 50);
    fc->addRow(tr("Modes propres :"), m_modeCount);
    lay->addWidget(groupCommon);

    m_optionsGroup = new QGroupBox(content);
    m_optionsLayout = new QVBoxLayout(m_optionsGroup);
    m_noOptionsLabel = new QLabel(tr("Ce moteur n'a pas d'options spécifiques."), m_optionsGroup);
    m_optionsLayout->addWidget(m_noOptionsLabel);
    lay->addWidget(m_optionsGroup);

    auto* groupValidation = new QGroupBox(tr("Validation"), content);
    auto* fv = new QVBoxLayout(groupValidation);
    m_validationList = new QListWidget(groupValidation);
    m_validationList->setMinimumHeight(110);
    m_validationList->setWordWrap(true);
    fv->addWidget(m_validationList);
    lay->addWidget(groupValidation);
    lay->addStretch(1);

    auto* buttons = new QHBoxLayout();
    auto* validate = new QPushButton(tr("Valider le modèle"), this);
    m_runButton = new QPushButton(tr("Lancer le calcul"), this);
    m_runButton->setDefault(true);
    auto* ok = new QPushButton(tr("Enregistrer et fermer"), this);
    auto* cancel = new QPushButton(tr("Annuler"), this);
    buttons->addWidget(validate);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    buttons->addWidget(m_runButton);
    root->addLayout(buttons);

    connect(validate, &QPushButton::clicked, this, [this]() { validateNow(); });
    connect(m_runButton, &QPushButton::clicked, this, [this]() {
        m_runRequested = true;
        accept();
    });
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_engineCombo, &QComboBox::currentIndexChanged, this, [this]() { if (!m_updating) onEngineChanged(); });
    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_updating) return;
        m_modeCount->setEnabled(m_typeCombo->currentData().toInt() == static_cast<int>(AnalysisType::Modal));
        syncCommonToOptions();
    });
    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const int i = m_scopeCombo->currentIndex();
        const bool restrictable = i >= 0 && i < static_cast<int>(m_scopes.size()) &&
                                  m_scopes[i].type != ScopeType::EntireModel && m_scopes[i].type != ScopeType::Level;
        m_levelCombo->setEnabled(restrictable);
        // Une portée plane appelle naturellement une analyse 2D si le moteur la propose.
        if (!m_updating && i >= 0 && i < static_cast<int>(m_scopes.size()) && !m_scopes[i].isPlanar())
        {
            const int d3 = m_dimensionCombo->findData(static_cast<int>(AnalysisDimension::Space3D));
            if (d3 >= 0) m_dimensionCombo->setCurrentIndex(d3);
        }
    });
}

void AnalysisDialog::populateEngines()
{
    m_updating = true;
    m_engineCombo->clear();
    for (const auto& info : m_manager.registry().engines())
        m_engineCombo->addItem(qs(info.name), qs(info.id));
    m_updating = false;
}

void AnalysisDialog::populateScopes()
{
    m_scopeCombo->clear();
    m_scopes.clear();
    if (m_model)
    {
        for (const auto& opt : AnalysisScopeResolver::availableScopes(*m_model, m_grids))
        {
            m_scopes.push_back(opt.scope);
            m_scopeCombo->addItem(qs(opt.label));
        }
    }
    if (!m_selection.empty())
    {
        AnalysisScope s;
        s.type = ScopeType::SelectedElements;
        s.nodes = m_selection.nodes;
        s.beams = m_selection.beams;
        s.columns = m_selection.columns;
        s.trussMembers = m_selection.trussMembers;
        s.cables = m_selection.cables;
        s.slabs = m_selection.slabs;
        s.walls = m_selection.walls;
        s.foundations = m_selection.foundations;
        m_scopes.push_back(s);
        m_scopeCombo->addItem(tr("Sélection courante (%1 objet(s))").arg(m_selection.size()));
    }

    m_levelCombo->clear();
    m_levelCombo->addItem(tr("Aucune restriction"), QString());
    if (m_model && m_model->levelManager())
        for (const auto& l : m_model->levelManager()->levels())
            m_levelCombo->addItem(tr("Niveau %1 (%2 m)").arg(qs(l.name)).arg(l.elevation, 0, 'f', 2), qs(l.id));
}

void AnalysisDialog::populateLoads()
{
    m_loadCombo->clear();
    m_loadCombo->addItem(tr("Tous les cas (superposés)"), QStringLiteral("all"));
    if (!m_model) return;
    const auto& lm = m_model->loadManager();
    for (const auto& [id, lc] : lm.loadCases())
        m_loadCombo->addItem(tr("Cas : %1").arg(qs(lc.name())), QStringLiteral("cases:%1").arg(id));
    for (const auto& [id, c] : lm.combinations())
        m_loadCombo->addItem(tr("Combinaison : %1").arg(qs(c.name())), QStringLiteral("combo:%1").arg(id));
}

void AnalysisDialog::setContext(const AnalysisContext& context)
{
    m_context = context;
    if (!m_manager.registry().hasEngine(m_context.engineId) && !m_manager.registry().ids().empty())
        m_context.engineId = m_manager.registry().ids().front();

    m_updating = true;
    const int ei = m_engineCombo->findData(qs(m_context.engineId));
    if (ei >= 0) m_engineCombo->setCurrentIndex(ei);

    // Portée : retrouver l'option équivalente (sans la restriction de niveau), sinon l'ajouter.
    AnalysisScope base = m_context.scope;
    if (base.hasLevelRestriction()) base.levelId.clear();
    int si = -1;
    for (std::size_t i = 0; i < m_scopes.size(); ++i)
        if (m_scopes[i] == base) si = static_cast<int>(i);
    if (si < 0)
    {
        m_scopes.push_back(base);
        m_scopeCombo->addItem(tr("Portée enregistrée"));
        si = static_cast<int>(m_scopes.size()) - 1;
    }
    m_scopeCombo->setCurrentIndex(si);
    const int li = m_levelCombo->findData(m_context.scope.hasLevelRestriction() ? qs(m_context.scope.levelId) : QString());
    m_levelCombo->setCurrentIndex(li >= 0 ? li : 0);

    const QString lk = loadKey(m_context);
    int lo = m_loadCombo->findData(lk);
    if (lo < 0)
    {
        QStringList names;
        for (int id : m_context.loadCaseIds)
        {
            const TSA::Model::LoadCase* lc = nullptr;
            if (m_model)
            {
                const auto& cases = m_model->loadManager().loadCases();
                auto it = cases.find(id);
                if (it != cases.end()) lc = &it->second;
            }
            names << (lc ? qs(lc->name()) : QStringLiteral("#%1").arg(id));
        }
        m_loadCombo->addItem(tr("Cas : %1").arg(names.join(QStringLiteral(" + "))), lk);
        lo = m_loadCombo->count() - 1;
    }
    m_loadCombo->setCurrentIndex(lo);
    m_selfWeight->setChecked(m_context.common.includeSelfWeight);
    m_modeCount->setValue(m_context.common.modeCount);
    m_updating = false;

    m_shownEngine.clear();   // force la reconstruction du panneau d'options
    onEngineChanged();
}

void AnalysisDialog::storeOptions()
{
    if (m_optionsWidget && !m_shownEngine.empty())
        m_context.engineSettings[m_shownEngine] = m_optionsWidget->saveSettings();
}

void AnalysisDialog::onEngineChanged()
{
    storeOptions();
    const EngineId id = currentEngineId();
    const AnalysisEngine* engine = m_manager.registry().engine(id);
    if (!engine) return;

    // Panneau d'options fourni par le moteur (ou aucun).
    delete m_optionsWidget;   // hors de tout signal du panneau : suppression immédiate
    m_optionsWidget = nullptr;
    m_optionsWidget = m_options.create(id, m_optionsGroup);
    if (m_optionsWidget)
    {
        m_optionsLayout->addWidget(m_optionsWidget);
        const QJsonObject saved = m_context.settingsFor(id);
        m_optionsWidget->loadSettings(saved.isEmpty() ? engine->defaultSettings() : saved);
    }
    m_noOptionsLabel->setVisible(m_optionsWidget == nullptr);
    m_shownEngine = id;
    m_context.engineId = id;
    refreshEngineDependentWidgets();
}

void AnalysisDialog::refreshEngineDependentWidgets()
{
    const AnalysisEngine* engine = m_manager.registry().engine(currentEngineId());
    if (!engine) return;
    const EngineInfo info = engine->info();
    const AnalysisCapabilities caps = engine->capabilities();
    const EngineAvailability avail = engine->availability();

    QString text = QStringLiteral("<b>%1</b>").arg(qs(info.name).toHtmlEscaped());
    if (!info.version.empty()) text += tr(" — version %1").arg(qs(info.version).toHtmlEscaped());
    text += QStringLiteral("<br>") + qs(info.description).toHtmlEscaped() + QStringLiteral("<br>");
    text += avail.available ? tr("<span style='color:#10b981'>● Disponible</span>")
                            : tr("<span style='color:#ef4444'>● Indisponible</span> : %1").arg(qs(avail.message).toHtmlEscaped());
    m_engineInfo->setText(text);
    m_capabilities->setText(capabilityText(caps));
    m_optionsGroup->setTitle(tr("Options %1").arg(qs(info.name)));

    m_updating = true;
    // Dimensions et types d'analyse : uniquement ceux que le moteur déclare.
    const int wantedDim = static_cast<int>(m_context.dimension);
    m_dimensionCombo->clear();
    for (auto d : { AnalysisDimension::Plane2D, AnalysisDimension::Space3D })
        if (caps.supportsDimension(d)) m_dimensionCombo->addItem(dimensionLabel(d), static_cast<int>(d));
    const int di = m_dimensionCombo->findData(wantedDim);
    m_dimensionCombo->setCurrentIndex(di >= 0 ? di : 0);

    const int wantedType = static_cast<int>(m_context.type);
    m_typeCombo->clear();
    for (auto t : { AnalysisType::LinearStatic, AnalysisType::NonLinearStatic, AnalysisType::Modal,
                    AnalysisType::DynamicTimeHistory, AnalysisType::Pushover })
        if (caps.supportsAnalysisType(t)) m_typeCombo->addItem(typeLabel(t), static_cast<int>(t));
    const int ti = m_typeCombo->findData(wantedType);
    m_typeCombo->setCurrentIndex(ti >= 0 ? ti : 0);
    m_modeCount->setEnabled(m_typeCombo->currentData().toInt() == static_cast<int>(AnalysisType::Modal));
    m_updating = false;

    // Pas de bouton inutilisable : calcul possible si le moteur est disponible ou installable.
    m_runButton->setEnabled(avail.available || avail.canProvision);
    m_runButton->setToolTip(avail.available ? QString() : qs(avail.message));
    syncCommonToOptions();
    m_validationList->clear();
}

void AnalysisDialog::syncCommonToOptions()
{
    if (m_optionsWidget) m_optionsWidget->setAnalysisContext(context());
}

AnalysisContext AnalysisDialog::context() const
{
    AnalysisContext c = m_context;
    c.engineId = currentEngineId();
    if (m_dimensionCombo->currentIndex() >= 0)
        c.dimension = static_cast<AnalysisDimension>(m_dimensionCombo->currentData().toInt());
    if (m_typeCombo->currentIndex() >= 0) c.type = static_cast<AnalysisType>(m_typeCombo->currentData().toInt());

    const int si = m_scopeCombo->currentIndex();
    if (si >= 0 && si < static_cast<int>(m_scopes.size())) c.scope = m_scopes[si];
    const QString level = m_levelCombo->isEnabled() ? m_levelCombo->currentData().toString() : QString();
    if (c.scope.type != ScopeType::Level) c.scope.levelId = level.toStdString();

    const QString lk = m_loadCombo->currentData().toString();
    c.combinationId = 0;
    c.loadCaseIds.clear();
    if (lk.startsWith(QStringLiteral("combo:")))
        c.combinationId = lk.mid(6).toInt();
    else if (lk.startsWith(QStringLiteral("cases:")))
        for (const auto& id : lk.mid(6).split(',', Qt::SkipEmptyParts)) c.loadCaseIds.push_back(id.toInt());

    c.common.includeSelfWeight = m_selfWeight->isChecked();
    c.common.modeCount = m_modeCount->value();
    if (m_optionsWidget) c.engineSettings[c.engineId] = m_optionsWidget->saveSettings();
    return c;
}

EngineId AnalysisDialog::currentEngineId() const
{
    return m_engineCombo->currentData().toString().toStdString();
}

bool AnalysisDialog::setEngine(const EngineId& id)
{
    const int i = m_engineCombo->findData(qs(id));
    if (i < 0) return false;
    m_engineCombo->setCurrentIndex(i);   // déclenche onEngineChanged
    return currentEngineId() == id;
}

std::vector<AnalysisType> AnalysisDialog::offeredAnalysisTypes() const
{
    std::vector<AnalysisType> out;
    for (int i = 0; i < m_typeCombo->count(); ++i) out.push_back(static_cast<AnalysisType>(m_typeCombo->itemData(i).toInt()));
    return out;
}

std::vector<AnalysisDimension> AnalysisDialog::offeredDimensions() const
{
    std::vector<AnalysisDimension> out;
    for (int i = 0; i < m_dimensionCombo->count(); ++i)
        out.push_back(static_cast<AnalysisDimension>(m_dimensionCombo->itemData(i).toInt()));
    return out;
}

QStringList AnalysisDialog::scopeLabels() const
{
    QStringList l;
    for (int i = 0; i < m_scopeCombo->count(); ++i) l << m_scopeCombo->itemText(i);
    return l;
}

bool AnalysisDialog::selectScope(const QString& label)
{
    const int i = m_scopeCombo->findText(label);
    if (i < 0) return false;
    m_scopeCombo->setCurrentIndex(i);
    return true;
}

bool AnalysisDialog::isRunEnabled() const
{
    return m_runButton->isEnabled();
}

ValidationResult AnalysisDialog::validateNow()
{
    ValidationResult v;
    if (!m_model)
        v.addError("Modèle", "Aucun modèle.");
    else
        v = m_manager.prepare(*m_model, m_grids, context()).validation;
    showValidation(v);
    return v;
}

void AnalysisDialog::showValidation(const ValidationResult& v)
{
    m_validationList->clear();
    for (const auto& m : v.messages())
    {
        QString icon = QStringLiteral("✓ ");
        QColor color("#10b981");
        if (m.severity == ValidationSeverity::Warning) { icon = QStringLiteral("⚠ "); color = QColor("#d97706"); }
        if (m.severity == ValidationSeverity::Error) { icon = QStringLiteral("✗ "); color = QColor("#ef4444"); }
        auto* item = new QListWidgetItem(icon + qs(m.text), m_validationList);
        item->setForeground(color);
        item->setToolTip(qs(m.category));
    }
    auto* summary = new QListWidgetItem(v.isValid() ? tr("Prêt pour le calcul.")
                                                    : tr("Calcul impossible : corriger les erreurs ci-dessus."),
                                        m_validationList);
    QFont f = summary->font();
    f.setBold(true);
    summary->setFont(f);
}

} // namespace TSA::UI
