#include "LibraryDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QGroupBox>

namespace TSA::UI
{

LibraryDialog::LibraryDialog(TSA::Model::Model* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(tr("Bibliothèque Personnalisée TSA"));
    resize(820, 560);
    setupUi();
}

void LibraryDialog::selectTab(int index)
{
    if (m_tabWidget && index >= 0 && index < m_tabWidget->count())
    {
        m_tabWidget->setCurrentIndex(index);
    }
}

void LibraryDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createSectionsTab(), tr("Sections"));
    m_tabWidget->addTab(createMaterialsTab(), tr("Matériaux"));
    m_tabWidget->addTab(createColorsTab(), tr("Couleurs"));
    m_tabWidget->addTab(createTexturesTab(), tr("Textures"));
    m_tabWidget->addTab(createStructuresTab(), tr("Structures Types"));

    mainLayout->addWidget(m_tabWidget);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* closeBtn = new QPushButton(tr("Fermer"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);

    mainLayout->addLayout(btnLayout);

    refreshSectionsTable();
    refreshMaterialsTable();
    refreshColorsTable();
    refreshTexturesTable();
    refreshStructuresTable();
}

// -----------------------------------------------------------------------------
// Onglet Sections
// -----------------------------------------------------------------------------

QWidget* LibraryDialog::createSectionsTab()
{
    auto* widget = new QWidget(this);
    auto* layout = new QHBoxLayout(widget);

    // Table à gauche
    m_sectionsTable = new QTableWidget(this);
    m_sectionsTable->setColumnCount(6);
    m_sectionsTable->setHorizontalHeaderLabels({ tr("Nom"), tr("Forme"), tr("Larg. (m)"), tr("Haut. (m)"), tr("tw (m)"), tr("tf (m)") });
    m_sectionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_sectionsTable->horizontalHeader()->setStretchLastSection(true);
    m_sectionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sectionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_sectionsTable, 3);

    // Éditeur à droite
    auto* rightPanel = new QVBoxLayout();
    auto* group = new QGroupBox(tr("Créer / Modifier une Section"), this);
    auto* form = new QFormLayout(group);

    m_secNameEdit = new QLineEdit(this);
    m_secNameEdit->setPlaceholderText("MaSection_01");
    form->addRow(tr("Nom :"), m_secNameEdit);

    m_secShapeCombo = new QComboBox(this);
    m_secShapeCombo->addItem(tr("Rectangulaire"), static_cast<int>(TSA::Model::SectionShape::Rectangular));
    m_secShapeCombo->addItem(tr("Circulaire"), static_cast<int>(TSA::Model::SectionShape::Circular));
    m_secShapeCombo->addItem(tr("Profil en I"), static_cast<int>(TSA::Model::SectionShape::IShape));
    m_secShapeCombo->addItem(tr("Tube Rectangulaire / Carré"), static_cast<int>(TSA::Model::SectionShape::BoxHollow));
    m_secShapeCombo->addItem(tr("Tube Rond"), static_cast<int>(TSA::Model::SectionShape::Pipe));
    m_secShapeCombo->addItem(tr("UPN"), static_cast<int>(TSA::Model::SectionShape::UPN));
    m_secShapeCombo->addItem(tr("Cornière (L)"), static_cast<int>(TSA::Model::SectionShape::Angle));
    m_secShapeCombo->addItem(tr("Profil en T"), static_cast<int>(TSA::Model::SectionShape::TSection));
    connect(m_secShapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LibraryDialog::onSectionShapeChanged);
    form->addRow(tr("Forme :"), m_secShapeCombo);

    m_secWidthSpin = new QDoubleSpinBox(this);
    m_secWidthSpin->setRange(0.01, 10.0);
    m_secWidthSpin->setValue(0.30);
    m_secWidthSpin->setSingleStep(0.05);
    m_secWidthSpin->setSuffix(" m");
    form->addRow(tr("Largeur b :"), m_secWidthSpin);

    m_secHeightSpin = new QDoubleSpinBox(this);
    m_secHeightSpin->setRange(0.01, 10.0);
    m_secHeightSpin->setValue(0.50);
    m_secHeightSpin->setSingleStep(0.05);
    m_secHeightSpin->setSuffix(" m");
    form->addRow(tr("Hauteur h :"), m_secHeightSpin);

    m_secDiameterSpin = new QDoubleSpinBox(this);
    m_secDiameterSpin->setRange(0.01, 10.0);
    m_secDiameterSpin->setValue(0.30);
    m_secDiameterSpin->setSingleStep(0.05);
    m_secDiameterSpin->setSuffix(" m");
    m_secDiameterSpin->setEnabled(false);
    form->addRow(tr("Diamètre D :"), m_secDiameterSpin);

    m_secTwSpin = new QDoubleSpinBox(this);
    m_secTwSpin->setRange(0.001, 0.50);
    m_secTwSpin->setValue(0.010);
    m_secTwSpin->setDecimals(3);
    m_secTwSpin->setSingleStep(0.002);
    m_secTwSpin->setSuffix(" m");
    form->addRow(tr("Épaisseur âme tw :"), m_secTwSpin);

    m_secTfSpin = new QDoubleSpinBox(this);
    m_secTfSpin->setRange(0.001, 0.50);
    m_secTfSpin->setValue(0.015);
    m_secTfSpin->setDecimals(3);
    m_secTfSpin->setSingleStep(0.002);
    m_secTfSpin->setSuffix(" m");
    form->addRow(tr("Épaisseur aile tf :"), m_secTfSpin);

    rightPanel->addWidget(group);

    auto* addBtn = new QPushButton(tr("Enregistrer dans la bibliothèque"), this);
    connect(addBtn, &QPushButton::clicked, this, &LibraryDialog::onAddCustomSection);
    rightPanel->addWidget(addBtn);

    auto* delBtn = new QPushButton(tr("Supprimer la section sélectionnée"), this);
    connect(delBtn, &QPushButton::clicked, this, &LibraryDialog::onDeleteCustomSection);
    rightPanel->addWidget(delBtn);

    rightPanel->addStretch();
    layout->addLayout(rightPanel, 2);

    return widget;
}

void LibraryDialog::onSectionShapeChanged(int index)
{
    auto shape = static_cast<TSA::Model::SectionShape>(m_secShapeCombo->itemData(index).toInt());
    bool isCirc = (shape == TSA::Model::SectionShape::Circular || shape == TSA::Model::SectionShape::Pipe);
    bool hasTf = (shape == TSA::Model::SectionShape::IShape || shape == TSA::Model::SectionShape::UPN ||
                  shape == TSA::Model::SectionShape::TSection || shape == TSA::Model::SectionShape::BoxHollow);
    bool hasTw = (shape != TSA::Model::SectionShape::Rectangular && shape != TSA::Model::SectionShape::Circular);

    m_secDiameterSpin->setEnabled(isCirc);
    m_secWidthSpin->setEnabled(!isCirc);
    m_secHeightSpin->setEnabled(!isCirc);
    m_secTwSpin->setEnabled(hasTw);
    m_secTfSpin->setEnabled(hasTf);
}

void LibraryDialog::refreshSectionsTable()
{
    m_sectionsTable->setRowCount(0);
    const auto& sections = TSA::Library::LibraryManager::instance().allSections();
    m_sectionsTable->setRowCount(static_cast<int>(sections.size()));

    for (int i = 0; i < static_cast<int>(sections.size()); ++i)
    {
        const auto& s = sections[i];
        m_sectionsTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(s.name)));
        m_sectionsTable->setItem(i, 1, new QTableWidgetItem(QString::number(static_cast<int>(s.shape))));
        m_sectionsTable->setItem(i, 2, new QTableWidgetItem(QString::number(s.width, 'f', 3)));
        m_sectionsTable->setItem(i, 3, new QTableWidgetItem(QString::number(s.height, 'f', 3)));
        m_sectionsTable->setItem(i, 4, new QTableWidgetItem(QString::number(s.tw, 'f', 3)));
        m_sectionsTable->setItem(i, 5, new QTableWidgetItem(QString::number(s.tf, 'f', 3)));
    }
}

void LibraryDialog::onAddCustomSection()
{
    QString name = m_secNameEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Champ manquant"), tr("Veuillez indiquer un nom pour la section."));
        return;
    }

    TSA::Model::Section s;
    s.name = name.toStdString();
    s.shape = static_cast<TSA::Model::SectionShape>(m_secShapeCombo->currentData().toInt());
    s.width = m_secWidthSpin->value();
    s.height = m_secHeightSpin->value();
    s.diameter = m_secDiameterSpin->value();
    s.tw = m_secTwSpin->value();
    s.tf = m_secTfSpin->value();

    if (s.shape == TSA::Model::SectionShape::Circular)
    {
        s.width = s.diameter;
        s.height = s.diameter;
    }

    TSA::Library::LibraryManager::instance().addCustomSection(s);
    refreshSectionsTable();
    emit sectionLibraryUpdated();
}

void LibraryDialog::onDeleteCustomSection()
{
    int row = m_sectionsTable->currentRow();
    if (row < 0) return;
    QString name = m_sectionsTable->item(row, 0)->text();
    if (TSA::Library::LibraryManager::instance().removeCustomSection(name.toStdString()))
    {
        refreshSectionsTable();
        emit sectionLibraryUpdated();
    }
    else
    {
        QMessageBox::information(this, tr("Information"), tr("Les sections standard du catalogue ne peuvent pas être supprimées."));
    }
}

// -----------------------------------------------------------------------------
// Onglet Matériaux
// -----------------------------------------------------------------------------

QWidget* LibraryDialog::createMaterialsTab()
{
    auto* widget = new QWidget(this);
    auto* layout = new QHBoxLayout(widget);

    m_materialsTable = new QTableWidget(this);
    m_materialsTable->setColumnCount(5);
    m_materialsTable->setHorizontalHeaderLabels({ tr("Nom"), tr("E (GPa)"), tr("Poisson ν"), tr("Masse ρ (kg/m³)"), tr("fk (MPa)") });
    m_materialsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_materialsTable->horizontalHeader()->setStretchLastSection(true);
    m_materialsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_materialsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_materialsTable, 3);

    auto* rightPanel = new QVBoxLayout();
    auto* group = new QGroupBox(tr("Créer un Matériau"), this);
    auto* form = new QFormLayout(group);

    m_matNameEdit = new QLineEdit(this);
    m_matNameEdit->setPlaceholderText("MonAcier_S460");
    form->addRow(tr("Nom :"), m_matNameEdit);

    m_matESpin = new QDoubleSpinBox(this);
    m_matESpin->setRange(0.1, 1000.0);
    m_matESpin->setValue(210.0);
    m_matESpin->setSuffix(" GPa");
    form->addRow(tr("Module d'Young E :"), m_matESpin);

    m_matNuSpin = new QDoubleSpinBox(this);
    m_matNuSpin->setRange(0.0, 0.49);
    m_matNuSpin->setValue(0.30);
    m_matNuSpin->setDecimals(2);
    form->addRow(tr("Coeff. Poisson ν :"), m_matNuSpin);

    m_matRhoSpin = new QDoubleSpinBox(this);
    m_matRhoSpin->setRange(100.0, 20000.0);
    m_matRhoSpin->setValue(7850.0);
    m_matRhoSpin->setSuffix(" kg/m³");
    form->addRow(tr("Masse volumique ρ :"), m_matRhoSpin);

    m_matFkSpin = new QDoubleSpinBox(this);
    m_matFkSpin->setRange(1.0, 2000.0);
    m_matFkSpin->setValue(460.0);
    m_matFkSpin->setSuffix(" MPa");
    form->addRow(tr("Résistance fk :"), m_matFkSpin);

    rightPanel->addWidget(group);

    auto* addBtn = new QPushButton(tr("Enregistrer Matériau"), this);
    connect(addBtn, &QPushButton::clicked, this, &LibraryDialog::onAddCustomMaterial);
    rightPanel->addWidget(addBtn);

    auto* delBtn = new QPushButton(tr("Supprimer Matériau sélectionné"), this);
    connect(delBtn, &QPushButton::clicked, this, &LibraryDialog::onDeleteCustomMaterial);
    rightPanel->addWidget(delBtn);

    rightPanel->addStretch();
    layout->addLayout(rightPanel, 2);

    return widget;
}

void LibraryDialog::refreshMaterialsTable()
{
    m_materialsTable->setRowCount(0);
    const auto& mats = TSA::Library::LibraryManager::instance().allMaterials();
    m_materialsTable->setRowCount(static_cast<int>(mats.size()));

    for (int i = 0; i < static_cast<int>(mats.size()); ++i)
    {
        const auto& m = mats[i];
        m_materialsTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(m.name)));
        m_materialsTable->setItem(i, 1, new QTableWidgetItem(QString::number(m.E / 1e9, 'f', 1)));
        m_materialsTable->setItem(i, 2, new QTableWidgetItem(QString::number(m.nu, 'f', 2)));
        m_materialsTable->setItem(i, 3, new QTableWidgetItem(QString::number(m.density, 'f', 0)));
        m_materialsTable->setItem(i, 4, new QTableWidgetItem(QString::number(m.fk / 1e6, 'f', 1)));
    }
}

void LibraryDialog::onAddCustomMaterial()
{
    QString name = m_matNameEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Champ manquant"), tr("Veuillez indiquer un nom pour le matériau."));
        return;
    }

    TSA::Model::Material m;
    m.name = name.toStdString();
    m.type = TSA::Model::MaterialType::Custom;
    m.E = m_matESpin->value() * 1e9;
    m.nu = m_matNuSpin->value();
    m.density = m_matRhoSpin->value();
    m.fk = m_matFkSpin->value() * 1e6;

    TSA::Library::LibraryManager::instance().addCustomMaterial(m);
    refreshMaterialsTable();
    emit materialLibraryUpdated();
}

void LibraryDialog::onDeleteCustomMaterial()
{
    int row = m_materialsTable->currentRow();
    if (row < 0) return;
    QString name = m_materialsTable->item(row, 0)->text();
    if (TSA::Library::LibraryManager::instance().removeCustomMaterial(name.toStdString()))
    {
        refreshMaterialsTable();
        emit materialLibraryUpdated();
    }
    else
    {
        QMessageBox::information(this, tr("Information"), tr("Les matériaux standard ne peuvent pas être supprimés."));
    }
}

// -----------------------------------------------------------------------------
// Onglet Couleurs
// -----------------------------------------------------------------------------

QWidget* LibraryDialog::createColorsTab()
{
    auto* widget = new QWidget(this);
    auto* layout = new QHBoxLayout(widget);

    m_colorsTable = new QTableWidget(this);
    m_colorsTable->setColumnCount(3);
    m_colorsTable->setHorizontalHeaderLabels({ tr("Nom"), tr("Aperçu"), tr("Code Hex") });
    m_colorsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_colorsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_colorsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_colorsTable, 3);

    auto* rightPanel = new QVBoxLayout();
    auto* group = new QGroupBox(tr("Ajouter une Couleur"), this);
    auto* form = new QFormLayout(group);

    m_colorNameEdit = new QLineEdit(this);
    m_colorNameEdit->setPlaceholderText("Poutre Principale Bleue");
    form->addRow(tr("Nom :"), m_colorNameEdit);

    m_currentColorHex = "#007ACC";
    m_colorPickBtn = new QPushButton(m_currentColorHex, this);
    m_colorPickBtn->setStyleSheet(QString("background-color: %1; color: #ffffff; font-weight: bold; padding: 6px; border-radius: 4px;").arg(m_currentColorHex));
    connect(m_colorPickBtn, &QPushButton::clicked, this, &LibraryDialog::onPickColor);
    form->addRow(tr("Couleur :"), m_colorPickBtn);

    rightPanel->addWidget(group);

    auto* addBtn = new QPushButton(tr("Enregistrer la Couleur"), this);
    connect(addBtn, &QPushButton::clicked, this, &LibraryDialog::onAddCustomColor);
    rightPanel->addWidget(addBtn);

    auto* delBtn = new QPushButton(tr("Supprimer la Couleur sélectionnée"), this);
    connect(delBtn, &QPushButton::clicked, this, &LibraryDialog::onDeleteCustomColor);
    rightPanel->addWidget(delBtn);

    rightPanel->addStretch();
    layout->addLayout(rightPanel, 2);

    return widget;
}

void LibraryDialog::onPickColor()
{
    QColor c = QColorDialog::getColor(QColor(m_currentColorHex), this, tr("Choisir une Couleur"));
    if (c.isValid())
    {
        m_currentColorHex = c.name(QColor::HexRgb).toUpper();
        m_colorPickBtn->setText(m_currentColorHex);
        m_colorPickBtn->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold; padding: 6px; border-radius: 4px;")
            .arg(m_currentColorHex).arg(c.lightness() > 128 ? "#000000" : "#ffffff"));
    }
}

void LibraryDialog::refreshColorsTable()
{
    m_colorsTable->setRowCount(0);
    const auto& colors = TSA::Library::LibraryManager::instance().colors();
    m_colorsTable->setRowCount(static_cast<int>(colors.size()));

    for (int i = 0; i < static_cast<int>(colors.size()); ++i)
    {
        const auto& c = colors[i];
        m_colorsTable->setItem(i, 0, new QTableWidgetItem(c.name));

        auto* colorItem = new QTableWidgetItem();
        colorItem->setBackground(QColor(c.hexCode));
        m_colorsTable->setItem(i, 1, colorItem);

        m_colorsTable->setItem(i, 2, new QTableWidgetItem(c.hexCode));
    }
}

void LibraryDialog::onAddCustomColor()
{
    QString name = m_colorNameEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Champ manquant"), tr("Veuillez indiquer un nom pour la couleur."));
        return;
    }

    TSA::Library::LibraryManager::instance().addCustomColor(name, m_currentColorHex);
    refreshColorsTable();
    emit colorLibraryUpdated();
}

void LibraryDialog::onDeleteCustomColor()
{
    int row = m_colorsTable->currentRow();
    if (row < 0) return;
    QString name = m_colorsTable->item(row, 0)->text();
    TSA::Library::LibraryManager::instance().removeCustomColor(name);
    refreshColorsTable();
    emit colorLibraryUpdated();
}

// -----------------------------------------------------------------------------
// Onglet Textures
// -----------------------------------------------------------------------------

QWidget* LibraryDialog::createTexturesTab()
{
    auto* widget = new QWidget(this);
    auto* layout = new QHBoxLayout(widget);

    m_texturesTable = new QTableWidget(this);
    m_texturesTable->setColumnCount(3);
    m_texturesTable->setHorizontalHeaderLabels({ tr("Nom"), tr("Catégorie"), tr("Fichier Image") });
    m_texturesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_texturesTable->horizontalHeader()->setStretchLastSection(true);
    m_texturesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_texturesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_texturesTable, 3);

    auto* rightPanel = new QVBoxLayout();
    auto* group = new QGroupBox(tr("Importer une Texture"), this);
    auto* form = new QFormLayout(group);

    m_texNameEdit = new QLineEdit(this);
    m_texNameEdit->setPlaceholderText("Béton Coffrage Bois");
    form->addRow(tr("Nom :"), m_texNameEdit);

    auto* pathLayout = new QHBoxLayout();
    m_texPathEdit = new QLineEdit(this);
    pathLayout->addWidget(m_texPathEdit);
    auto* browseBtn = new QPushButton("...", this);
    connect(browseBtn, &QPushButton::clicked, this, &LibraryDialog::onBrowseTextureFile);
    pathLayout->addWidget(browseBtn);
    form->addRow(tr("Image :"), pathLayout);

    m_texRepeatUSpin = new QDoubleSpinBox(this);
    m_texRepeatUSpin->setRange(0.1, 100.0);
    m_texRepeatUSpin->setValue(1.0);
    form->addRow(tr("Répétition U :"), m_texRepeatUSpin);

    m_texRepeatVSpin = new QDoubleSpinBox(this);
    m_texRepeatVSpin->setRange(0.1, 100.0);
    m_texRepeatVSpin->setValue(1.0);
    form->addRow(tr("Répétition V :"), m_texRepeatVSpin);

    rightPanel->addWidget(group);

    auto* addBtn = new QPushButton(tr("Ajouter aux Textures"), this);
    connect(addBtn, &QPushButton::clicked, this, &LibraryDialog::onAddCustomTexture);
    rightPanel->addWidget(addBtn);

    auto* delBtn = new QPushButton(tr("Supprimer la Texture sélectionnée"), this);
    connect(delBtn, &QPushButton::clicked, this, &LibraryDialog::onDeleteCustomTexture);
    rightPanel->addWidget(delBtn);

    rightPanel->addStretch();
    layout->addLayout(rightPanel, 2);

    return widget;
}

void LibraryDialog::onBrowseTextureFile()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Choisir une image de texture"),
                                                QString(), tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (!file.isEmpty())
    {
        m_texPathEdit->setText(file);
    }
}

void LibraryDialog::refreshTexturesTable()
{
    m_texturesTable->setRowCount(0);
    const auto& texs = TSA::Library::LibraryManager::instance().textures();
    m_texturesTable->setRowCount(static_cast<int>(texs.size()));

    for (int i = 0; i < static_cast<int>(texs.size()); ++i)
    {
        const auto& t = texs[i];
        m_texturesTable->setItem(i, 0, new QTableWidgetItem(t.name));
        m_texturesTable->setItem(i, 1, new QTableWidgetItem(t.category));
        m_texturesTable->setItem(i, 2, new QTableWidgetItem(t.filePath));
    }
}

void LibraryDialog::onAddCustomTexture()
{
    QString name = m_texNameEdit->text().trimmed();
    QString path = m_texPathEdit->text().trimmed();
    if (name.isEmpty() || path.isEmpty())
    {
        QMessageBox::warning(this, tr("Champs manquants"), tr("Veuillez renseigner un nom et un fichier d'image."));
        return;
    }

    TSA::Library::LibraryManager::instance().addCustomTexture(name, path, "Mes Textures",
                                                              m_texRepeatUSpin->value(), m_texRepeatVSpin->value());
    refreshTexturesTable();
}

void LibraryDialog::onDeleteCustomTexture()
{
    int row = m_texturesTable->currentRow();
    if (row < 0) return;
    QString name = m_texturesTable->item(row, 0)->text();
    TSA::Library::LibraryManager::instance().removeCustomTexture(name);
    refreshTexturesTable();
}

// -----------------------------------------------------------------------------
// Onglet Structures Types (Templates)
// -----------------------------------------------------------------------------

QWidget* LibraryDialog::createStructuresTab()
{
    auto* widget = new QWidget(this);
    auto* layout = new QHBoxLayout(widget);

    m_templatesTable = new QTableWidget(this);
    m_templatesTable->setColumnCount(4);
    m_templatesTable->setHorizontalHeaderLabels({ tr("Nom"), tr("Catégorie"), tr("Description"), tr("Nœuds / Barres") });
    m_templatesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_templatesTable->horizontalHeader()->setStretchLastSection(true);
    m_templatesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_templatesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_templatesTable, 3);

    auto* rightPanel = new QVBoxLayout();
    auto* group = new QGroupBox(tr("Enregistrer la Structure Active"), this);
    auto* form = new QFormLayout(group);

    m_tplNameEdit = new QLineEdit(this);
    m_tplNameEdit->setPlaceholderText("Portique_MonProjet");
    form->addRow(tr("Nom :"), m_tplNameEdit);

    m_tplDescEdit = new QLineEdit(this);
    m_tplDescEdit->setPlaceholderText("Portique 2D 6m x 3m");
    form->addRow(tr("Description :"), m_tplDescEdit);

    rightPanel->addWidget(group);

    auto* saveBtn = new QPushButton(tr("Enregistrer le modèle comme Modèle Réutilisable"), this);
    connect(saveBtn, &QPushButton::clicked, this, &LibraryDialog::onSaveCurrentModelAsTemplate);
    rightPanel->addWidget(saveBtn);

    auto* insertBtn = new QPushButton(tr("Insérer la structure sélectionnée dans le projet"), this);
    insertBtn->setStyleSheet("background-color: #007ACC; color: white; font-weight: bold; padding: 6px;");
    connect(insertBtn, &QPushButton::clicked, this, &LibraryDialog::onInsertTemplateIntoModel);
    rightPanel->addWidget(insertBtn);

    auto* delBtn = new QPushButton(tr("Supprimer le modèle"), this);
    connect(delBtn, &QPushButton::clicked, this, &LibraryDialog::onDeleteTemplate);
    rightPanel->addWidget(delBtn);

    rightPanel->addStretch();
    layout->addLayout(rightPanel, 2);

    return widget;
}

void LibraryDialog::refreshStructuresTable()
{
    m_templatesTable->setRowCount(0);
    const auto& tpls = TSA::Library::LibraryManager::instance().templates();
    m_templatesTable->setRowCount(static_cast<int>(tpls.size()));

    for (int i = 0; i < static_cast<int>(tpls.size()); ++i)
    {
        const auto& t = tpls[i];
        m_templatesTable->setItem(i, 0, new QTableWidgetItem(t.name));
        m_templatesTable->setItem(i, 1, new QTableWidgetItem(t.category));
        m_templatesTable->setItem(i, 2, new QTableWidgetItem(t.description));
        m_templatesTable->setItem(i, 3, new QTableWidgetItem(QString("%1 N / %2 B")
            .arg(t.snapshot.nodes.size()).arg(t.snapshot.beams.size() + t.snapshot.columns.size())));
    }
}

void LibraryDialog::onSaveCurrentModelAsTemplate()
{
    if (!m_model) return;
    QString name = m_tplNameEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Champ manquant"), tr("Veuillez indiquer un nom pour le modèle de structure."));
        return;
    }

    auto snapshot = m_model->createSnapshot(name.toStdString());
    TSA::Library::LibraryManager::instance().addStructureTemplate(name, "Mes Structures",
                                                                  m_tplDescEdit->text().trimmed(), snapshot);
    refreshStructuresTable();
    QMessageBox::information(this, tr("Succès"), tr("La structure a été enregistrée dans la bibliothèque persistante."));
}

void LibraryDialog::onInsertTemplateIntoModel()
{
    if (!m_model) return;
    int row = m_templatesTable->currentRow();
    if (row < 0)
    {
        QMessageBox::warning(this, tr("Sélection"), tr("Veuillez sélectionner un modèle dans la liste."));
        return;
    }

    QString name = m_templatesTable->item(row, 0)->text();
    if (TSA::Library::LibraryManager::instance().instantiateTemplateInModel(name, m_model, 0.0, 0.0, 0.0))
    {
        QMessageBox::information(this, tr("Succès"), tr("La structure a été insérée dans le projet actif."));
    }
}

void LibraryDialog::onDeleteTemplate()
{
    int row = m_templatesTable->currentRow();
    if (row < 0) return;
    QString name = m_templatesTable->item(row, 0)->text();
    TSA::Library::LibraryManager::instance().removeStructureTemplate(name);
    refreshStructuresTable();
}

} // namespace TSA::UI
