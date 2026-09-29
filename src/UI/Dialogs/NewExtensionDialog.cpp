#include "NewExtensionDialog.h"
#include "../../ExtensionSystem/LibraryValidator.h"
#include "../../ExtensionSystem/LibraryManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

namespace TSA::UI
{

NewExtensionDialog::NewExtensionDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    updateTargetPathPreview();
}

void NewExtensionDialog::setupUi()
{
    setWindowTitle(tr("Créer une nouvelle bibliothèque TSALib"));
    setMinimumWidth(560);
    setWindowIcon(QIcon(":/icons/add.svg"));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Groupe Identification
    auto* grpIdentity = new QGroupBox(tr("Identité de l'Extension"), this);
    auto* formIdentity = new QFormLayout(grpIdentity);
    formIdentity->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_editId = new QLineEdit(this);
    m_editId->setPlaceholderText(tr("ex: com.monentreprise.structures ou org.eurocode.timber"));
    m_editId->setToolTip(tr("Identifiant unique en minuscules, chiffres, tirets ou points."));

    m_lblIdValidation = new QLabel(this);
    m_lblIdValidation->setStyleSheet("font-size: 11px;");

    auto* idLayout = new QVBoxLayout();
    idLayout->addWidget(m_editId);
    idLayout->addWidget(m_lblIdValidation);
    formIdentity->addRow(tr("Identifiant (ID) * :"), idLayout);

    m_editName = new QLineEdit(this);
    m_editName->setPlaceholderText(tr("ex: Catalogue Charpente Bois & Profilés"));
    formIdentity->addRow(tr("Nom complet * :"), m_editName);

    m_editVersion = new QLineEdit("1.0.0", this);
    formIdentity->addRow(tr("Version initiale :"), m_editVersion);

    m_editAuthor = new QLineEdit("TSA Engineering", this);
    formIdentity->addRow(tr("Auteur / Organisation :"), m_editAuthor);

    m_comboLicense = new QComboBox(this);
    m_comboLicense->addItems({ "MIT", "Proprietary", "Apache-2.0", "GPL-3.0", "Creative Commons CC-BY-4.0" });
    formIdentity->addRow(tr("Licence :"), m_comboLicense);

    m_editDescription = new QTextEdit(this);
    m_editDescription->setPlaceholderText(tr("Description des matériaux, profilés, normes couvertes..."));
    m_editDescription->setMaximumHeight(55);
    formIdentity->addRow(tr("Description :"), m_editDescription);

    mainLayout->addWidget(grpIdentity);

    // 2. Groupe Catégories à initialiser
    auto* grpCategories = new QGroupBox(tr("Contenu et Modèles Types à Inclure"), this);
    auto* catLayout = new QGridLayout(grpCategories);

    m_chkMaterials = new QCheckBox(tr("Matériaux (Materials/)"), this);
    m_chkMaterials->setChecked(true);
    m_chkMaterials->setToolTip(tr("Initialise le dossier Materials/ avec un exemple de matériau"));
    catLayout->addWidget(m_chkMaterials, 0, 0);

    m_chkSections = new QCheckBox(tr("Sections Types (Sections/)"), this);
    m_chkSections->setChecked(true);
    m_chkSections->setToolTip(tr("Initialise le dossier Sections/ avec une section rectangulaire calculée"));
    catLayout->addWidget(m_chkSections, 0, 1);

    m_chkProfiles = new QCheckBox(tr("Profilés Métalliques (Profiles/)"), this);
    m_chkProfiles->setChecked(true);
    m_chkProfiles->setToolTip(tr("Initialise le dossier Profiles/ avec un profilé en I standard"));
    catLayout->addWidget(m_chkProfiles, 1, 0);

    m_chkCables = new QCheckBox(tr("Câbles & Haubans (Cables/)"), this);
    m_chkCables->setChecked(false);
    m_chkCables->setToolTip(tr("Initialise le dossier Cables/ avec une fiche de câble/toron"));
    catLayout->addWidget(m_chkCables, 1, 1);

    m_chkTextures = new QCheckBox(tr("Textures PBR (Textures/)"), this);
    m_chkTextures->setChecked(true);
    m_chkTextures->setToolTip(tr("Initialise le dossier Textures/ avec le manifeste textures.json"));
    catLayout->addWidget(m_chkTextures, 2, 0);

    m_chkStandards = new QCheckBox(tr("Normes de Référence (Standards/)"), this);
    m_chkStandards->setChecked(true);
    m_chkStandards->setToolTip(tr("Initialise le dossier Standards/"));
    catLayout->addWidget(m_chkStandards, 2, 1);

    mainLayout->addWidget(grpCategories);

    // 3. Groupe Emplacement
    auto* grpLocation = new QGroupBox(tr("Emplacement sur le Disque"), this);
    auto* locLayout = new QVBoxLayout(grpLocation);

    m_radioProjectExtensions = new QRadioButton(tr("Dossier Extensions du projet (recommandé pour distribution)"), this);
    m_radioProjectExtensions->setChecked(true);
    locLayout->addWidget(m_radioProjectExtensions);

    m_radioUserExtensions = new QRadioButton(tr("Dossier Extensions utilisateur (%LOCALAPPDATA%/TSA/Extensions)"), this);
    locLayout->addWidget(m_radioUserExtensions);

    m_radioCustomLocation = new QRadioButton(tr("Dossier personnalisé..."), this);
    locLayout->addWidget(m_radioCustomLocation);

    auto* customPathLayout = new QHBoxLayout();
    m_editCustomPath = new QLineEdit(this);
    m_editCustomPath->setEnabled(false);
    customPathLayout->addWidget(m_editCustomPath, 1);

    m_btnBrowseCustom = new QPushButton(tr("Parcourir..."), this);
    m_btnBrowseCustom->setEnabled(false);
    customPathLayout->addWidget(m_btnBrowseCustom);
    locLayout->addLayout(customPathLayout);

    m_lblPathPreview = new QLabel(this);
    m_lblPathPreview->setStyleSheet("font-size: 11px; color: #555;");
    m_lblPathPreview->setWordWrap(true);
    locLayout->addWidget(m_lblPathPreview);

    mainLayout->addWidget(grpLocation);

    // 4. Options post-création
    auto* optsLayout = new QHBoxLayout();
    m_chkCreatePackage = new QCheckBox(tr("Générer également le package autonome (.tsalib)"), this);
    m_chkCreatePackage->setChecked(true);
    optsLayout->addWidget(m_chkCreatePackage);

    m_chkLoadImmediately = new QCheckBox(tr("Charger immédiatement dans TSA (sans redémarrage)"), this);
    m_chkLoadImmediately->setChecked(true);
    optsLayout->addWidget(m_chkLoadImmediately);
    mainLayout->addLayout(optsLayout);

    // 5. Boutons d'action
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Annuler"), this);
    btnLayout->addWidget(m_btnCancel);

    m_btnCreate = new QPushButton(QIcon(":/icons/add.svg"), tr("Créer la bibliothèque"), this);
    m_btnCreate->setDefault(true);
    m_btnCreate->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 14px; }");
    btnLayout->addWidget(m_btnCreate);

    mainLayout->addLayout(btnLayout);

    // Connexions
    connect(m_editId, &QLineEdit::textChanged, this, &NewExtensionDialog::onIdTextChanged);
    connect(m_radioProjectExtensions, &QRadioButton::toggled, this, &NewExtensionDialog::onLocationTypeChanged);
    connect(m_radioUserExtensions, &QRadioButton::toggled, this, &NewExtensionDialog::onLocationTypeChanged);
    connect(m_radioCustomLocation, &QRadioButton::toggled, this, &NewExtensionDialog::onLocationTypeChanged);
    connect(m_btnBrowseCustom, &QPushButton::clicked, this, &NewExtensionDialog::onBrowseCustomFolder);
    connect(m_editCustomPath, &QLineEdit::textChanged, this, [this]() { updateTargetPathPreview(); });
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnCreate, &QPushButton::clicked, this, &NewExtensionDialog::onCreateClicked);

    onIdTextChanged(m_editId->text());
}

void NewExtensionDialog::onIdTextChanged(const QString& text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        m_lblIdValidation->setText(tr("<font color='#888'>Identifiant requis.</font>"));
        m_btnCreate->setEnabled(false);
    }
    else if (TSA::ExtensionSystem::LibraryValidator::isValidId(trimmed.toStdString()))
    {
        m_lblIdValidation->setText(tr("<font color='#27ae60'>✔ Format d'identifiant valide.</font>"));
        m_btnCreate->setEnabled(!m_editName->text().trimmed().isEmpty());
    }
    else
    {
        m_lblIdValidation->setText(tr("<font color='#e74c3c'>✖ Format invalide : minuscules, chiffres, tirets et points uniquement.</font>"));
        m_btnCreate->setEnabled(false);
    }

    updateTargetPathPreview();
}

void NewExtensionDialog::onLocationTypeChanged()
{
    bool isCustom = m_radioCustomLocation->isChecked();
    m_editCustomPath->setEnabled(isCustom);
    m_btnBrowseCustom->setEnabled(isCustom);
    updateTargetPathPreview();
}

void NewExtensionDialog::onBrowseCustomFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Choisir le dossier parent"), m_editCustomPath->text());
    if (!dir.isEmpty())
    {
        m_editCustomPath->setText(dir);
        updateTargetPathPreview();
    }
}

void NewExtensionDialog::updateTargetPathPreview()
{
    QString id = m_editId->text().trimmed();
    if (id.isEmpty()) id = "ma_bibliotheque";

    QString basePath;
    if (m_radioProjectExtensions->isChecked())
    {
        basePath = QDir::currentPath() + "/Extensions";
    }
    else if (m_radioUserExtensions->isChecked())
    {
        QString userLoc = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        basePath = QDir(userLoc).filePath("Extensions");
    }
    else
    {
        basePath = m_editCustomPath->text().trimmed();
        if (basePath.isEmpty()) basePath = QDir::currentPath();
    }

    QString finalPath = QDir(basePath).filePath(id);
    m_lblPathPreview->setText(tr("Chemin de destination prévu : <b>%1</b>").arg(finalPath));
}

bool NewExtensionDialog::validateInputs(QString* outMessage)
{
    QString id = m_editId->text().trimmed();
    if (id.isEmpty())
    {
        if (outMessage) *outMessage = tr("Veuillez renseigner un identifiant (ID) pour l'extension.");
        return false;
    }

    if (!TSA::ExtensionSystem::LibraryValidator::isValidId(id.toStdString()))
    {
        if (outMessage) *outMessage = tr("L'identifiant '%1' est invalide (utiliser minuscules, chiffres, points ou tirets).").arg(id);
        return false;
    }

    if (m_editName->text().trimmed().isEmpty())
    {
        if (outMessage) *outMessage = tr("Veuillez renseigner un nom complet pour la bibliothèque.");
        return false;
    }

    if (m_radioCustomLocation->isChecked() && m_editCustomPath->text().trimmed().isEmpty())
    {
        if (outMessage) *outMessage = tr("Veuillez sélectionner un dossier parent personnalisé.");
        return false;
    }

    return true;
}

void NewExtensionDialog::onCreateClicked()
{
    QString validationError;
    if (!validateInputs(&validationError))
    {
        QMessageBox::warning(this, tr("Champ invalide"), validationError);
        return;
    }

    TSA::ExtensionSystem::ExtensionTemplateOptions opts;
    opts.id = m_editId->text().trimmed().toStdString();
    opts.name = m_editName->text().trimmed().toStdString();
    opts.author = m_editAuthor->text().trimmed().toStdString();
    opts.license = m_comboLicense->currentText().toStdString();
    opts.description = m_editDescription->toPlainText().trimmed().toStdString();

    auto semVer = TSA::ExtensionSystem::SemanticVersion::fromString(m_editVersion->text().trimmed().toStdString());
    if (semVer) opts.version = *semVer;

    opts.includeMaterials = m_chkMaterials->isChecked();
    opts.includeSections = m_chkSections->isChecked();
    opts.includeProfiles = m_chkProfiles->isChecked();
    opts.includeCables = m_chkCables->isChecked();
    opts.includeTextures = m_chkTextures->isChecked();
    opts.includeStandards = m_chkStandards->isChecked();

    // Détermination du répertoire cible
    QString basePath;
    if (m_radioProjectExtensions->isChecked())
    {
        basePath = QDir::currentPath() + "/Extensions";
    }
    else if (m_radioUserExtensions->isChecked())
    {
        QString userLoc = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        basePath = QDir(userLoc).filePath("Extensions");
    }
    else
    {
        basePath = m_editCustomPath->text().trimmed();
    }
    opts.targetDirectory = QDir(basePath).filePath(QString::fromStdString(opts.id));

    opts.createPackage = m_chkCreatePackage->isChecked();
    if (opts.createPackage)
    {
        opts.packageOutputPath = opts.targetDirectory + ".tsalib";
    }

    QString createdDir;
    QString scaffoldError;
    bool ok = TSA::ExtensionSystem::ExtensionScaffolder::scaffold(opts, &createdDir, &scaffoldError);
    if (!ok)
    {
        QMessageBox::critical(this, tr("Erreur de création"),
                              tr("Impossible de créer la bibliothèque :\n%1").arg(scaffoldError));
        return;
    }

    m_createdId = QString::fromStdString(opts.id);
    m_createdDirectory = createdDir;

    // Chargement immédiat si demandé
    if (m_chkLoadImmediately->isChecked())
    {
        auto& mgr = TSA::ExtensionSystem::LibraryManager::instance();
        mgr.addSearchPath(basePath);
        mgr.discover();
        mgr.load(opts.id);
    }

    emit extensionCreated(m_createdId, m_createdDirectory);

    QString successMsg = tr("La bibliothèque TSALib <b>%1</b> a été créée avec succès dans :<br><code>%2</code>")
                             .arg(QString::fromStdString(opts.name), createdDir);
    if (opts.createPackage)
    {
        successMsg += tr("<br><br>Package autonome créé :<br><code>%1</code>").arg(opts.packageOutputPath);
    }

    QMessageBox msgBox(QMessageBox::Information, tr("Bibliothèque créée"), successMsg, QMessageBox::Ok, this);
    auto* btnOpen = msgBox.addButton(tr("Ouvrir le dossier"), QMessageBox::ActionRole);
    msgBox.exec();

    if (msgBox.clickedButton() == btnOpen)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(createdDir));
    }

    accept();
}

} // namespace TSA::UI
