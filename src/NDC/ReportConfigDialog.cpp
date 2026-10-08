#include "ReportConfigDialog.h"
#include "App/ProductInfo.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>

namespace TSA::NDC
{

ReportConfigDialog::ReportConfigDialog(const ReportConfiguration& config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(tr("Personnalisation de la Note de Calcul (NDC)"));
    resize(720, 600);
    setupUi();
    loadFromConfig(m_config);
}

void ReportConfigDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabs = new QTabWidget(this);

    // =========================================================================
    // ONGLET 1 : INFORMATIONS DU PROJET
    // =========================================================================
    auto* tabInfo = new QWidget(tabs);
    auto* formInfo = new QFormLayout(tabInfo);

    m_editTitle = new QLineEdit(this);
    m_editDesc = new QLineEdit(this);
    m_editProjNum = new QLineEdit(this);
    m_editDocNum = new QLineEdit(this);
    m_editRev = new QLineEdit(this);
    m_editStatus = new QLineEdit(this);
    m_editEngineer = new QLineEdit(this);
    m_editOrg = new QLineEdit(this);
    m_editClient = new QLineEdit(this);
    m_editDate = new QLineEdit(this);

    formInfo->addRow(tr("Intitulé du Projet :"), m_editTitle);
    formInfo->addRow(tr("Description :"), m_editDesc);
    formInfo->addRow(tr("N° de Projet :"), m_editProjNum);
    formInfo->addRow(tr("N° de Document :"), m_editDocNum);
    formInfo->addRow(tr("Indice de Révision :"), m_editRev);
    formInfo->addRow(tr("Statut du Document :"), m_editStatus);
    formInfo->addRow(tr("Ingénieur / Auteur :"), m_editEngineer);
    formInfo->addRow(tr("Bureau d'Études :"), m_editOrg);
    formInfo->addRow(tr("Maître d'Ouvrage / Client :"), m_editClient);
    formInfo->addRow(tr("Date d'Émission :"), m_editDate);
    tabs->addTab(tabInfo, tr("Informations Projet"));

    // =========================================================================
    // ONGLET 2 : MISE EN PAGE & STYLE
    // =========================================================================
    auto* tabLayout = new QWidget(tabs);
    auto* formLayout = new QFormLayout(tabLayout);

    m_comboPageFormat = new QComboBox(this);
    m_comboPageFormat->addItem(tr("A4 (210 × 297 mm)"), static_cast<int>(PageFormat::A4));
    m_comboPageFormat->addItem(tr("A3 (297 × 420 mm)"), static_cast<int>(PageFormat::A3));

    m_comboOrientation = new QComboBox(this);
    m_comboOrientation->addItem(tr("Portrait (Vertical)"), static_cast<int>(PageOrientation::Portrait));
    m_comboOrientation->addItem(tr("Paysage (Horizontal)"), static_cast<int>(PageOrientation::Landscape));

    m_spinMarginLeft = new QDoubleSpinBox(this);
    m_spinMarginLeft->setRange(5.0, 50.0);
    m_spinMarginLeft->setSuffix(" mm");

    m_spinMarginRight = new QDoubleSpinBox(this);
    m_spinMarginRight->setRange(5.0, 50.0);
    m_spinMarginRight->setSuffix(" mm");

    m_spinMarginTop = new QDoubleSpinBox(this);
    m_spinMarginTop->setRange(5.0, 50.0);
    m_spinMarginTop->setSuffix(" mm");

    m_spinMarginBottom = new QDoubleSpinBox(this);
    m_spinMarginBottom->setRange(5.0, 50.0);
    m_spinMarginBottom->setSuffix(" mm");

    m_chkShowLogo = new QCheckBox(tr("Afficher le logo officiel %1").arg(TSA::Product::name()), this);
    m_editPrimaryColor = new QLineEdit(this);
    m_spinFontSize = new QSpinBox(this);
    m_spinFontSize->setRange(8, 16);
    m_spinFontSize->setSuffix(" pt");

    formLayout->addRow(tr("Format de Page :"), m_comboPageFormat);
    formLayout->addRow(tr("Orientation :"), m_comboOrientation);
    formLayout->addRow(tr("Marge Gauche :"), m_spinMarginLeft);
    formLayout->addRow(tr("Marge Droite :"), m_spinMarginRight);
    formLayout->addRow(tr("Marge Supérieure :"), m_spinMarginTop);
    formLayout->addRow(tr("Marge Inférieure :"), m_spinMarginBottom);
    formLayout->addRow(tr("Couleur Primaire :"), m_editPrimaryColor);
    formLayout->addRow(tr("Taille de Police :"), m_spinFontSize);
    formLayout->addRow(QString(), m_chkShowLogo);
    tabs->addTab(tabLayout, tr("Mise en Page & Style"));

    // =========================================================================
    // ONGLET 3 : SECTIONS & RÉSULTATS
    // =========================================================================
    auto* tabSections = new QWidget(tabs);
    auto* gridSec = new QGridLayout(tabSections);

    m_chkCover = new QCheckBox(tr("Page de Couverture Officielle"), this);
    m_chkToc = new QCheckBox(tr("Table des Matières (Sommaire)"), this);
    m_chkLof = new QCheckBox(tr("Liste des Figures"), this);
    m_chkLot = new QCheckBox(tr("Liste des Tableaux"), this);
    m_chkIntro = new QCheckBox(tr("Introduction & Hypothèses"), this);
    m_chkStandards = new QCheckBox(tr("Normes Eurocodes Détectées"), this);
    m_chkGeom = new QCheckBox(tr("Description Géométrique"), this);
    m_chkMaterials = new QCheckBox(tr("Tableau des Matériaux"), this);
    m_chkSections = new QCheckBox(tr("Tableau des Sections"), this);
    m_chkBoundary = new QCheckBox(tr("Conditions aux Limites & Appuis"), this);
    m_chkLoads = new QCheckBox(tr("Charges & Combinaisons"), this);
    m_chkMethod = new QCheckBox(tr("Méthode de Calcul MEF"), this);
    m_chkQuality = new QCheckBox(tr("Contrôle Qualité & Équilibre"), this);
    m_chkSnapshots = new QCheckBox(tr("Vues Tridimensionnelles 3D"), this);
    m_chkDisp = new QCheckBox(tr("Déplacements Nodaux"), this);
    m_chkDefl = new QCheckBox(tr("Flèches en Travée (L/250)"), this);
    m_chkMoment = new QCheckBox(tr("Moments Fléchissants Mz / My"), this);
    m_chkShear = new QCheckBox(tr("Efforts Tranchants Vz / Vy"), this);
    m_chkAxial = new QCheckBox(tr("Efforts Normaux N (Traction/Comp.)"), this);
    m_chkTorsion = new QCheckBox(tr("Moments de Torsion Mx"), this);
    m_chkExtrema = new QCheckBox(tr("Synthèse & Localisation 3D Extrema"), this);
    m_chkDetailedTables = new QCheckBox(tr("Tableaux Détaillés par Barre"), this);
    m_chkEurocode = new QCheckBox(tr("Vérifications Eurocodes (EC2 / EC3)"), this);
    m_chkWarn = new QCheckBox(tr("Avertissements & Limitations"), this);
    m_chkConclusion = new QCheckBox(tr("Conclusion & Déclaration"), this);
    m_chkBiblio = new QCheckBox(tr("Bibliographie & Webographie"), this);

    gridSec->addWidget(m_chkCover, 0, 0);
    gridSec->addWidget(m_chkToc, 1, 0);
    gridSec->addWidget(m_chkLof, 2, 0);
    gridSec->addWidget(m_chkLot, 3, 0);
    gridSec->addWidget(m_chkIntro, 4, 0);
    gridSec->addWidget(m_chkStandards, 5, 0);
    gridSec->addWidget(m_chkGeom, 6, 0);
    gridSec->addWidget(m_chkMaterials, 7, 0);
    gridSec->addWidget(m_chkSections, 8, 0);
    gridSec->addWidget(m_chkBoundary, 9, 0);
    gridSec->addWidget(m_chkLoads, 10, 0);
    gridSec->addWidget(m_chkMethod, 11, 0);
    gridSec->addWidget(m_chkQuality, 12, 0);
    gridSec->addWidget(m_chkSnapshots, 13, 0);

    gridSec->addWidget(m_chkDisp, 0, 1);
    gridSec->addWidget(m_chkDefl, 1, 1);
    gridSec->addWidget(m_chkMoment, 2, 1);
    gridSec->addWidget(m_chkShear, 3, 1);
    gridSec->addWidget(m_chkAxial, 4, 1);
    gridSec->addWidget(m_chkTorsion, 5, 1);
    gridSec->addWidget(m_chkExtrema, 6, 1);
    gridSec->addWidget(m_chkDetailedTables, 7, 1);
    gridSec->addWidget(m_chkEurocode, 8, 1);
    gridSec->addWidget(m_chkWarn, 9, 1);
    gridSec->addWidget(m_chkConclusion, 10, 1);
    gridSec->addWidget(m_chkBiblio, 11, 1);

    tabs->addTab(tabSections, tr("Sections & Résultats"));

    // =========================================================================
    // ONGLET 4 : MODÈLES (TEMPLATES)
    // =========================================================================
    auto* tabTemplates = new QWidget(tabs);
    auto* vlayTemplates = new QVBoxLayout(tabTemplates);

    auto* grpPresets = new QGroupBox(tr("Modèles Prédéfinis"), tabTemplates);
    auto* fPreset = new QFormLayout(grpPresets);

    m_comboTemplate = new QComboBox(this);
    m_comboTemplate->addItem(tr("Bureau d'Études (BPE Complet)"), static_cast<int>(ReportTemplateType::BureauEtudes));
    m_comboTemplate->addItem(tr("Eurocode (Réglementaire)"), static_cast<int>(ReportTemplateType::Eurocode));
    m_comboTemplate->addItem(tr("Universitaire (Scientifique EF)"), static_cast<int>(ReportTemplateType::Universitaire));
    m_comboTemplate->addItem(tr("Minimal (Synthèse Exécutive)"), static_cast<int>(ReportTemplateType::Minimal));

    m_btnApplyTemplate = new QPushButton(tr("Appliquer ce modèle"), this);
    connect(m_btnApplyTemplate, &QPushButton::clicked, this, &ReportConfigDialog::onApplyTemplateClicked);

    fPreset->addRow(tr("Sélection du modèle :"), m_comboTemplate);
    fPreset->addRow(QString(), m_btnApplyTemplate);
    vlayTemplates->addWidget(grpPresets);

    auto* grpFile = new QGroupBox(tr("Fichiers de Modèle (.tsareport)"), tabTemplates);
    auto* hFile = new QHBoxLayout(grpFile);
    m_btnLoadFile = new QPushButton(tr("Charger un modèle..."), this);
    m_btnSaveFile = new QPushButton(tr("Enregistrer ce modèle..."), this);
    connect(m_btnLoadFile, &QPushButton::clicked, this, &ReportConfigDialog::onLoadFromFileClicked);
    connect(m_btnSaveFile, &QPushButton::clicked, this, &ReportConfigDialog::onSaveToFileClicked);
    hFile->addWidget(m_btnLoadFile);
    hFile->addWidget(m_btnSaveFile);
    vlayTemplates->addWidget(grpFile);
    vlayTemplates->addStretch();

    tabs->addTab(tabTemplates, tr("Modèles & Templates"));

    mainLayout->addWidget(tabs);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btnBox);
}

void ReportConfigDialog::loadFromConfig(const ReportConfiguration& cfg)
{
    m_editTitle->setText(cfg.projectTitle);
    m_editDesc->setText(cfg.projectDescription);
    m_editProjNum->setText(cfg.projectNumber);
    m_editDocNum->setText(cfg.documentNumber);
    m_editRev->setText(cfg.revision);
    m_editStatus->setText(cfg.documentStatus);
    m_editEngineer->setText(cfg.engineerName);
    m_editOrg->setText(cfg.organization);
    m_editClient->setText(cfg.clientName);
    m_editDate->setText(cfg.emissionDate);

    m_comboPageFormat->setCurrentIndex((cfg.pageFormat == PageFormat::A3) ? 1 : 0);
    m_comboOrientation->setCurrentIndex((cfg.pageOrientation == PageOrientation::Landscape) ? 1 : 0);
    m_spinMarginLeft->setValue(cfg.marginMmLeft);
    m_spinMarginRight->setValue(cfg.marginMmRight);
    m_spinMarginTop->setValue(cfg.marginMmTop);
    m_spinMarginBottom->setValue(cfg.marginMmBottom);
    m_chkShowLogo->setChecked(cfg.showTsaLogo);
    m_editPrimaryColor->setText(cfg.primaryColor);
    m_spinFontSize->setValue(cfg.baseFontSizePt);

    m_chkCover->setChecked(cfg.includeCoverPage);
    m_chkToc->setChecked(cfg.includeToc);
    m_chkLof->setChecked(cfg.includeLof);
    m_chkLot->setChecked(cfg.includeLot);
    m_chkIntro->setChecked(cfg.includeIntroduction);
    m_chkStandards->setChecked(cfg.includeStandards);
    m_chkGeom->setChecked(cfg.includeModelGeometry);
    m_chkMaterials->setChecked(cfg.includeMaterials);
    m_chkSections->setChecked(cfg.includeSections);
    m_chkBoundary->setChecked(cfg.includeBoundaryConditions);
    m_chkLoads->setChecked(cfg.includeLoadsAndCombinations);
    m_chkMethod->setChecked(cfg.includeCalculationMethod);
    m_chkQuality->setChecked(cfg.includeModelVerification);
    m_chkSnapshots->setChecked(cfg.include3DModelSnapshots);
    m_chkDisp->setChecked(cfg.includeDisplacements);
    m_chkDefl->setChecked(cfg.includeDeflections);
    m_chkMoment->setChecked(cfg.includeBendingMoment);
    m_chkShear->setChecked(cfg.includeShearForce);
    m_chkAxial->setChecked(cfg.includeAxialForce);
    m_chkTorsion->setChecked(cfg.includeTorsion);
    m_chkExtrema->setChecked(cfg.includeMostStressedSummary);
    m_chkDetailedTables->setChecked(cfg.includeDetailedElementTables);
    m_chkEurocode->setChecked(cfg.includeEurocodeDesignChecks);
    m_chkWarn->setChecked(cfg.includeWarningsAndLimitations);
    m_chkConclusion->setChecked(cfg.includeConclusion);
    m_chkBiblio->setChecked(cfg.includeBibliography);
}

ReportConfiguration ReportConfigDialog::configuration() const
{
    ReportConfiguration cfg = m_config;

    cfg.projectTitle = m_editTitle->text();
    cfg.projectDescription = m_editDesc->text();
    cfg.projectNumber = m_editProjNum->text();
    cfg.documentNumber = m_editDocNum->text();
    cfg.revision = m_editRev->text();
    cfg.documentStatus = m_editStatus->text();
    cfg.engineerName = m_editEngineer->text();
    cfg.organization = m_editOrg->text();
    cfg.clientName = m_editClient->text();
    cfg.emissionDate = m_editDate->text();

    cfg.pageFormat = static_cast<PageFormat>(m_comboPageFormat->currentData().toInt());
    cfg.pageOrientation = static_cast<PageOrientation>(m_comboOrientation->currentData().toInt());
    cfg.marginMmLeft = m_spinMarginLeft->value();
    cfg.marginMmRight = m_spinMarginRight->value();
    cfg.marginMmTop = m_spinMarginTop->value();
    cfg.marginMmBottom = m_spinMarginBottom->value();
    cfg.showTsaLogo = m_chkShowLogo->isChecked();
    cfg.primaryColor = m_editPrimaryColor->text();
    cfg.baseFontSizePt = m_spinFontSize->value();

    cfg.includeCoverPage = m_chkCover->isChecked();
    cfg.includeToc = m_chkToc->isChecked();
    cfg.includeLof = m_chkLof->isChecked();
    cfg.includeLot = m_chkLot->isChecked();
    cfg.includeIntroduction = m_chkIntro->isChecked();
    cfg.includeStandards = m_chkStandards->isChecked();
    cfg.includeModelGeometry = m_chkGeom->isChecked();
    cfg.includeMaterials = m_chkMaterials->isChecked();
    cfg.includeSections = m_chkSections->isChecked();
    cfg.includeBoundaryConditions = m_chkBoundary->isChecked();
    cfg.includeLoadsAndCombinations = m_chkLoads->isChecked();
    cfg.includeCalculationMethod = m_chkMethod->isChecked();
    cfg.includeModelVerification = m_chkQuality->isChecked();
    cfg.include3DModelSnapshots = m_chkSnapshots->isChecked();
    cfg.includeDisplacements = m_chkDisp->isChecked();
    cfg.includeDeflections = m_chkDefl->isChecked();
    cfg.includeBendingMoment = m_chkMoment->isChecked();
    cfg.includeShearForce = m_chkShear->isChecked();
    cfg.includeAxialForce = m_chkAxial->isChecked();
    cfg.includeTorsion = m_chkTorsion->isChecked();
    cfg.includeMostStressedSummary = m_chkExtrema->isChecked();
    cfg.includeExtremaSpatialTable = m_chkExtrema->isChecked();
    cfg.includeDetailedElementTables = m_chkDetailedTables->isChecked();
    cfg.includeEurocodeDesignChecks = m_chkEurocode->isChecked();
    cfg.includeWarningsAndLimitations = m_chkWarn->isChecked();
    cfg.includeConclusion = m_chkConclusion->isChecked();
    cfg.includeBibliography = m_chkBiblio->isChecked();

    return cfg;
}

void ReportConfigDialog::onApplyTemplateClicked()
{
    auto type = static_cast<ReportTemplateType>(m_comboTemplate->currentData().toInt());
    ReportConfiguration tpl = ReportTemplate::createTemplate(type);

    // Préserver les métadonnées de projet déjà saisies par l'utilisateur
    tpl.projectTitle = m_editTitle->text();
    tpl.projectNumber = m_editProjNum->text();
    tpl.engineerName = m_editEngineer->text();
    tpl.organization = m_editOrg->text();
    tpl.clientName = m_editClient->text();

    loadFromConfig(tpl);
    QMessageBox::information(this, tr("Modèle Appliqué"), tr("Le modèle '%1' a été appliqué avec succès.").arg(ReportTemplate::templateName(type)));
}

void ReportConfigDialog::onLoadFromFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Ouvrir un modèle de note de calcul"), QString(), tr("Modèles TSA (*.tsareport);;Tous les fichiers (*.*)"));
    if (filePath.isEmpty()) return;

    ReportConfiguration loaded;
    QString err;
    if (loaded.loadFromFile(filePath, &err))
    {
        loadFromConfig(loaded);
        QMessageBox::information(this, tr("Modèle Chargé"), tr("Configuration chargée depuis : %1").arg(filePath));
    }
    else
    {
        QMessageBox::critical(this, tr("Erreur"), tr("Impossible de charger le fichier : %1").arg(err));
    }
}

void ReportConfigDialog::onSaveToFileClicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, tr("Enregistrer le modèle de note de calcul"), QString(), tr("Modèles TSA (*.tsareport)"));
    if (filePath.isEmpty()) return;

    if (!filePath.endsWith(".tsareport", Qt::CaseInsensitive))
    {
        filePath += ".tsareport";
    }

    ReportConfiguration current = configuration();
    QString err;
    if (current.saveToFile(filePath, &err))
    {
        QMessageBox::information(this, tr("Modèle Enregistré"), tr("Configuration enregistrée avec succès dans : %1").arg(filePath));
    }
    else
    {
        QMessageBox::critical(this, tr("Erreur"), tr("Impossible d'enregistrer le fichier : %1").arg(err));
    }
}

} // namespace TSA::NDC
