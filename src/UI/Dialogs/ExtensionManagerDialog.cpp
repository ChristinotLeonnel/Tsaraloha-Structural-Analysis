#include "ExtensionManagerDialog.h"
#include "NewExtensionDialog.h"
#include "../../ExtensionSystem/LibraryManager.h"
#include "../../ExtensionSystem/LibraryRegistry.h"
#include "../../ExtensionSystem/LibraryCache.h"
#include "../../Viewer/TextureManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QColor>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace TSA::UI
{

ExtensionManagerDialog::ExtensionManagerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Gestionnaire de Bibliothèques & Extensions TSALib"));
    resize(1000, 680);
    setMinimumSize(850, 500);

    setupUi();
    populateCategoryTree();
    selectCategory("Materials");
    updateTelemetryBar();
}

void ExtensionManagerDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // -------------------------------------------------------------------------
    // 1. Barre d'outils et recherche supérieure
    // -------------------------------------------------------------------------
    auto* topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Rechercher un matériau, profilé, câble, texture ou norme..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumHeight(32);
    m_searchEdit->setStyleSheet("QLineEdit { padding: 4px 8px; border-radius: 4px; font-size: 13px; }");
    topLayout->addWidget(m_searchEdit, 1);

    m_btnNewExtension = new QPushButton(QIcon(":/icons/add.svg"), tr("Nouvelle bibliothèque..."), this);
    m_btnNewExtension->setToolTip(tr("Créer une nouvelle extension TSALib avec l'assistant interactif"));
    m_btnNewExtension->setMinimumHeight(32);
    topLayout->addWidget(m_btnNewExtension);

    m_btnReloadAll = new QPushButton(QIcon(":/icons/edit/redo.svg"), tr("Recharger tout"), this);
    m_btnReloadAll->setToolTip(tr("Recharger à chaud toutes les bibliothèques JSON et textures du disque sans recompiler"));
    m_btnReloadAll->setMinimumHeight(32);
    topLayout->addWidget(m_btnReloadAll);

    m_btnValidateAll = new QPushButton(QIcon(":/icons/apply.svg"), tr("Valider l'intégrité"), this);
    m_btnValidateAll->setToolTip(tr("Vérifier la stricte conformité syntaxique et normative de toutes les extensions"));
    m_btnValidateAll->setMinimumHeight(32);
    topLayout->addWidget(m_btnValidateAll);

    m_btnImport = new QPushButton(QIcon(":/icons/file_open.svg"), tr("Importer (.tsalib)..."), this);
    m_btnImport->setToolTip(tr("Importer et installer un package .tsalib ou un dossier d'extension"));
    m_btnImport->setMinimumHeight(32);
    topLayout->addWidget(m_btnImport);

    m_btnExport = new QPushButton(QIcon(":/icons/file_save.svg"), tr("Exporter (.tsalib)..."), this);
    m_btnExport->setToolTip(tr("Empaqueter et exporter une extension au format autonome .tsalib"));
    m_btnExport->setMinimumHeight(32);
    topLayout->addWidget(m_btnExport);

    m_btnOpenFolder = new QPushButton(QIcon(":/icons/structure_preset.svg"), tr("Ouvrir dossier"), this);
    m_btnOpenFolder->setToolTip(tr("Ouvrir le répertoire des extensions dans l'explorateur Windows"));
    m_btnOpenFolder->setMinimumHeight(32);
    topLayout->addWidget(m_btnOpenFolder);

    mainLayout->addLayout(topLayout);

    // -------------------------------------------------------------------------
    // 2. Zone Master-Detail avec QSplitter
    // -------------------------------------------------------------------------
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Volet Gauche : Arborescence des catégories
    m_categoryTree = new QTreeWidget(m_mainSplitter);
    m_categoryTree->setHeaderLabel(tr("Catégories TSALib"));
    m_categoryTree->setRootIsDecorated(true);
    m_categoryTree->setAnimated(true);
    m_categoryTree->setMinimumWidth(220);
    m_mainSplitter->addWidget(m_categoryTree);

    // Volet Droit : Splitter vertical (Tableau des éléments + Fiche Technique)
    auto* rightSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);

    m_itemsTable = new QTableWidget(rightSplitter);
    m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_itemsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_itemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_itemsTable->setAlternatingRowColors(true);
    m_itemsTable->horizontalHeader()->setStretchLastSection(true);
    m_itemsTable->verticalHeader()->setVisible(false);
    rightSplitter->addWidget(m_itemsTable);

    m_detailBrowser = new QTextBrowser(rightSplitter);
    m_detailBrowser->setOpenExternalLinks(true);
    m_detailBrowser->setStyleSheet("QTextBrowser { font-family: 'Segoe UI', sans-serif; font-size: 13px; line-height: 1.4; padding: 10px; }");
    rightSplitter->addWidget(m_detailBrowser);

    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 2);

    m_mainSplitter->addWidget(rightSplitter);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 4);

    mainLayout->addWidget(m_mainSplitter, 1);

    // -------------------------------------------------------------------------
    // 3. Pied de page & Télémétrie
    // -------------------------------------------------------------------------
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);

    m_statusLabel = new QLabel(tr("Prêt"), this);
    m_statusLabel->setStyleSheet("font-weight: 500; color: #4A5568;");
    bottomLayout->addWidget(m_statusLabel, 1);

    m_telemetryLabel = new QLabel(this);
    m_telemetryLabel->setStyleSheet("color: #718096; font-size: 12px;");
    bottomLayout->addWidget(m_telemetryLabel);

    m_btnClose = new QPushButton(tr("Fermer"), this);
    m_btnClose->setMinimumHeight(30);
    m_btnClose->setMinimumWidth(90);
    bottomLayout->addWidget(m_btnClose);

    mainLayout->addLayout(bottomLayout);

    // Connexions de signaux/slots
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ExtensionManagerDialog::onSearchTextChanged);
    connect(m_btnNewExtension, &QPushButton::clicked, this, &ExtensionManagerDialog::onNewExtension);
    connect(m_btnReloadAll, &QPushButton::clicked, this, [this]() { onReloadAll(true); });
    connect(m_btnValidateAll, &QPushButton::clicked, this, [this]() { onValidateAll(true); });
    connect(m_btnImport, &QPushButton::clicked, this, &ExtensionManagerDialog::onImportExtension);
    connect(m_btnExport, &QPushButton::clicked, this, &ExtensionManagerDialog::onExportExtension);
    connect(m_btnOpenFolder, &QPushButton::clicked, this, &ExtensionManagerDialog::onOpenExtensionsFolder);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_categoryTree, &QTreeWidget::itemSelectionChanged, this, &ExtensionManagerDialog::onCategoryTreeSelectionChanged);
    connect(m_itemsTable, &QTableWidget::itemSelectionChanged, this, &ExtensionManagerDialog::onItemTableSelectionChanged);
}

void ExtensionManagerDialog::populateCategoryTree()
{
    m_categoryTree->clear();
    auto& reg = TSA::ExtensionSystem::LibraryRegistry::instance();

    // 1. Extensions installées
    auto* extItem = new QTreeWidgetItem(m_categoryTree);
    extItem->setText(0, tr("Extensions Installées"));
    extItem->setData(0, Qt::UserRole, "Extensions");
    extItem->setIcon(0, QIcon(":/icons/file_new.svg"));

    // 2. Matériaux Eurocodes
    auto* matRoot = new QTreeWidgetItem(m_categoryTree);
    matRoot->setText(0, tr("Matériaux Eurocodes (%1)").arg(reg.materialCount()));
    matRoot->setData(0, Qt::UserRole, "Materials_All");
    matRoot->setIcon(0, QIcon(":/icons/material_concrete.svg"));

    QStringList matCats = { "Concrete", "Steel", "Timber", "Soil", "Masonry", "Glass", "Aluminum" };
    for (const auto& cat : matCats)
    {
        auto* catItem = new QTreeWidgetItem(matRoot);
        catItem->setText(0, cat);
        catItem->setData(0, Qt::UserRole, "Materials_" + cat);
    }
    matRoot->setExpanded(true);

    // 3. Sections & Profilés
    auto* secRoot = new QTreeWidgetItem(m_categoryTree);
    secRoot->setText(0, tr("Sections & Profilés (%1)").arg(reg.sectionCount()));
    secRoot->setData(0, Qt::UserRole, "Sections_All");
    secRoot->setIcon(0, QIcon(":/icons/section_i.svg"));

    QStringList secCats = { "IPE", "HEA", "HEB", "UPN", "TSection", "Angle" };
    for (const auto& cat : secCats)
    {
        auto* catItem = new QTreeWidgetItem(secRoot);
        catItem->setText(0, cat);
        catItem->setData(0, Qt::UserRole, "Sections_" + cat);
    }
    secRoot->setExpanded(true);

    // 4. Câbles & Torons
    auto* cabRoot = new QTreeWidgetItem(m_categoryTree);
    cabRoot->setText(0, tr("Câbles & Torons (%1)").arg(reg.cableCount()));
    cabRoot->setData(0, Qt::UserRole, "Cables_All");
    cabRoot->setIcon(0, QIcon(":/icons/draw_cable.svg"));

    QStringList cabCats = { "PrestressingStrand", "LockedCoil", "StayCable", "Hanger", "PrestressingBar", "ASTM" };
    for (const auto& cat : cabCats)
    {
        auto* catItem = new QTreeWidgetItem(cabRoot);
        catItem->setText(0, cat);
        catItem->setData(0, Qt::UserRole, "Cables_" + cat);
    }
    cabRoot->setExpanded(true);

    // 5. Textures PBR
    auto* texItem = new QTreeWidgetItem(m_categoryTree);
    texItem->setText(0, tr("Textures PBR (%1)").arg(TSA::Viewer::TextureManager::instance().count()));
    texItem->setData(0, Qt::UserRole, "Textures");
    texItem->setIcon(0, QIcon(":/icons/material_steel.svg"));

    // 6. Normes Eurocodes
    auto* stdItem = new QTreeWidgetItem(m_categoryTree);
    stdItem->setText(0, tr("Normes Eurocodes (EN)"));
    stdItem->setData(0, Qt::UserRole, "Standards");
    stdItem->setIcon(0, QIcon(":/icons/common/help.svg"));
}

void ExtensionManagerDialog::selectCategory(const QString& categoryName)
{
    for (int i = 0; i < m_categoryTree->topLevelItemCount(); ++i)
    {
        auto* topItem = m_categoryTree->topLevelItem(i);
        QString roleData = topItem->data(0, Qt::UserRole).toString();
        if (roleData.startsWith(categoryName, Qt::CaseInsensitive))
        {
            m_categoryTree->setCurrentItem(topItem);
            return;
        }
        for (int j = 0; j < topItem->childCount(); ++j)
        {
            auto* childItem = topItem->child(j);
            QString childRole = childItem->data(0, Qt::UserRole).toString();
            if (childRole.contains(categoryName, Qt::CaseInsensitive))
            {
                m_categoryTree->setCurrentItem(childItem);
                return;
            }
        }
    }
}

int ExtensionManagerDialog::displayedItemCount() const
{
    return m_itemsTable->rowCount();
}

void ExtensionManagerDialog::setSearchQuery(const QString& query)
{
    m_searchEdit->setText(query);
}

void ExtensionManagerDialog::onCategoryTreeSelectionChanged()
{
    auto* current = m_categoryTree->currentItem();
    if (!current) return;

    QString roleData = current->data(0, Qt::UserRole).toString();
    if (roleData == "Extensions")
    {
        m_currentView = CurrentViewType::Extensions;
        m_currentSubCategory.clear();
    }
    else if (roleData.startsWith("Materials"))
    {
        m_currentView = CurrentViewType::Materials;
        m_currentSubCategory = roleData.mid(10); // ex: "Concrete", "All"
        if (m_currentSubCategory == "All") m_currentSubCategory.clear();
    }
    else if (roleData.startsWith("Sections"))
    {
        m_currentView = CurrentViewType::Sections;
        m_currentSubCategory = roleData.mid(9); // ex: "IPE", "All"
        if (m_currentSubCategory == "All") m_currentSubCategory.clear();
    }
    else if (roleData.startsWith("Cables"))
    {
        m_currentView = CurrentViewType::Cables;
        m_currentSubCategory = roleData.mid(7); // ex: "StayCable", "All"
        if (m_currentSubCategory == "All") m_currentSubCategory.clear();
    }
    else if (roleData == "Textures")
    {
        m_currentView = CurrentViewType::Textures;
        m_currentSubCategory.clear();
    }
    else if (roleData == "Standards")
    {
        m_currentView = CurrentViewType::Standards;
        m_currentSubCategory.clear();
    }

    refreshCurrentCategory();
}

void ExtensionManagerDialog::onSearchTextChanged(const QString& /*text*/)
{
    refreshCurrentCategory();
}

void ExtensionManagerDialog::refreshCurrentCategory()
{
    QString filter = m_searchEdit->text().trimmed();

    switch (m_currentView)
    {
    case CurrentViewType::Extensions:
        populateExtensionsList(filter);
        break;
    case CurrentViewType::Materials:
        populateMaterialsList(m_currentSubCategory, filter);
        break;
    case CurrentViewType::Sections:
        populateSectionsList(m_currentSubCategory, filter);
        break;
    case CurrentViewType::Cables:
        populateCablesList(m_currentSubCategory, filter);
        break;
    case CurrentViewType::Textures:
        populateTexturesList(filter);
        break;
    case CurrentViewType::Standards:
        populateStandardsList(filter);
        break;
    }

    if (m_itemsTable->rowCount() > 0)
    {
        m_itemsTable->selectRow(0);
    }
    else
    {
        m_detailBrowser->setHtml(tr("<p style='color: #718096; text-align: center; margin-top: 40px;'>Aucun élément ne correspond aux critères.</p>"));
    }

    updateTelemetryBar();
}

void ExtensionManagerDialog::populateExtensionsList(const QString& filter)
{
    m_itemsTable->clear();
    m_itemsTable->setColumnCount(5);
    m_itemsTable->setHorizontalHeaderLabels({ tr("ID"), tr("Nom"), tr("Version"), tr("Auteur"), tr("Statut") });

    auto manifests = TSA::ExtensionSystem::LibraryManager::instance().installedExtensions();
    m_currentExtensions.clear();

    for (const auto& man : manifests)
    {
        if (!filter.isEmpty())
        {
            QString qid = QString::fromStdString(man.id);
            QString qname = QString::fromStdString(man.name);
            if (!qid.contains(filter, Qt::CaseInsensitive) && !qname.contains(filter, Qt::CaseInsensitive))
            {
                continue;
            }
        }
        m_currentExtensions.push_back(man);
    }

    m_itemsTable->setRowCount(static_cast<int>(m_currentExtensions.size()));
    for (int r = 0; r < static_cast<int>(m_currentExtensions.size()); ++r)
    {
        const auto& man = m_currentExtensions[r];
        m_itemsTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(man.id)));
        m_itemsTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(man.name)));
        m_itemsTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(man.version.toString())));
        m_itemsTable->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(man.author)));
        auto* statusItem = new QTableWidgetItem(tr("Actif & Validé"));
        statusItem->setForeground(QBrush(QColor("#2B6CB0")));
        m_itemsTable->setItem(r, 4, statusItem);
    }
    m_statusLabel->setText(tr("%1 extension(s) installée(s)").arg(m_currentExtensions.size()));
}

void ExtensionManagerDialog::populateMaterialsList(const QString& subCategory, const QString& filter)
{
    m_itemsTable->clear();
    m_itemsTable->setColumnCount(6);
    m_itemsTable->setHorizontalHeaderLabels({ tr("ID"), tr("Désignation"), tr("Catégorie"), tr("Module E (GPa)"), tr("Résistance fk/fy (MPa)"), tr("Norme") });

    auto& reg = TSA::ExtensionSystem::LibraryRegistry::instance();
    auto allMats = reg.allMaterials();
    m_currentMaterials.clear();

    for (const auto& mat : allMats)
    {
        if (!subCategory.isEmpty())
        {
            if (QString::fromStdString(mat.category).compare(subCategory, Qt::CaseInsensitive) != 0 &&
                !QString::fromStdString(mat.id).contains(subCategory, Qt::CaseInsensitive))
            {
                continue;
            }
        }

        if (!filter.isEmpty())
        {
            QString qid = QString::fromStdString(mat.id);
            QString qname = QString::fromStdString(mat.name);
            QString qcat = QString::fromStdString(mat.category);
            QString qstd = QString::fromStdString(mat.standard.name);
            if (!qid.contains(filter, Qt::CaseInsensitive) &&
                !qname.contains(filter, Qt::CaseInsensitive) &&
                !qcat.contains(filter, Qt::CaseInsensitive) &&
                !qstd.contains(filter, Qt::CaseInsensitive))
            {
                continue;
            }
        }
        m_currentMaterials.push_back(mat);
    }

    m_itemsTable->setRowCount(static_cast<int>(m_currentMaterials.size()));
    for (int r = 0; r < static_cast<int>(m_currentMaterials.size()); ++r)
    {
        const auto& mat = m_currentMaterials[r];
        m_itemsTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(mat.id)));
        m_itemsTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(mat.name)));
        m_itemsTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(mat.category)));

        double eGpa = mat.youngModulus.toSI() / 1.0e9;
        m_itemsTable->setItem(r, 3, new QTableWidgetItem(QString::number(eGpa, 'f', 1)));

        double fkMpa = (mat.fck.value > 0.0 ? mat.fck.toSI() : mat.fy.toSI()) / 1.0e6;
        m_itemsTable->setItem(r, 4, new QTableWidgetItem(QString::number(fkMpa, 'f', 1)));

        m_itemsTable->setItem(r, 5, new QTableWidgetItem(QString::fromStdString(mat.standard.name)));
    }
    m_statusLabel->setText(tr("%1 matériau(x) affiché(s)").arg(m_currentMaterials.size()));
}

void ExtensionManagerDialog::populateSectionsList(const QString& subCategory, const QString& filter)
{
    m_itemsTable->clear();
    m_itemsTable->setColumnCount(6);
    m_itemsTable->setHorizontalHeaderLabels({ tr("ID"), tr("Profilé"), tr("Forme"), tr("Hauteur (mm)"), tr("Largeur (mm)"), tr("Norme") });

    auto& reg = TSA::ExtensionSystem::LibraryRegistry::instance();
    auto allSecs = reg.allSections();
    m_currentSections.clear();

    for (const auto& sec : allSecs)
    {
        if (!subCategory.isEmpty())
        {
            QString qid = QString::fromStdString(sec.id);
            QString qname = QString::fromStdString(sec.name);
            QString qshape = QString::fromStdString(sec.shapeType);
            if (!qid.contains(subCategory, Qt::CaseInsensitive) &&
                !qname.contains(subCategory, Qt::CaseInsensitive) &&
                !qshape.contains(subCategory, Qt::CaseInsensitive))
            {
                continue;
            }
        }

        if (!filter.isEmpty())
        {
            QString qid = QString::fromStdString(sec.id);
            QString qname = QString::fromStdString(sec.name);
            QString qshape = QString::fromStdString(sec.shapeType);
            QString qstd = QString::fromStdString(sec.standard.name);
            if (!qid.contains(filter, Qt::CaseInsensitive) &&
                !qname.contains(filter, Qt::CaseInsensitive) &&
                !qshape.contains(filter, Qt::CaseInsensitive) &&
                !qstd.contains(filter, Qt::CaseInsensitive))
            {
                continue;
            }
        }
        m_currentSections.push_back(sec);
    }

    m_itemsTable->setRowCount(static_cast<int>(m_currentSections.size()));
    for (int r = 0; r < static_cast<int>(m_currentSections.size()); ++r)
    {
        const auto& sec = m_currentSections[r];
        m_itemsTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(sec.id)));
        m_itemsTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(sec.name)));
        m_itemsTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(sec.shapeType)));

        double hMm = sec.height * 1000.0;
        double bMm = sec.width * 1000.0;
        m_itemsTable->setItem(r, 3, new QTableWidgetItem(QString::number(hMm, 'f', 1)));
        m_itemsTable->setItem(r, 4, new QTableWidgetItem(QString::number(bMm, 'f', 1)));
        m_itemsTable->setItem(r, 5, new QTableWidgetItem(QString::fromStdString(sec.standard.name)));
    }
    m_statusLabel->setText(tr("%1 section(s) affichée(s)").arg(m_currentSections.size()));
}

void ExtensionManagerDialog::populateCablesList(const QString& subCategory, const QString& filter)
{
    m_itemsTable->clear();
    m_itemsTable->setColumnCount(6);
    m_itemsTable->setHorizontalHeaderLabels({ tr("ID"), tr("Désignation"), tr("Catégorie"), tr("Diamètre (mm)"), tr("F rupture (kN)"), tr("Norme") });

    auto& reg = TSA::ExtensionSystem::LibraryRegistry::instance();
    auto allCables = reg.allCables();
    m_currentCables.clear();

    for (const auto& cab : allCables)
    {
        if (!subCategory.isEmpty())
        {
            QString qcat = QString::fromStdString(cab.category);
            QString qid = QString::fromStdString(cab.id);
            if (!qcat.contains(subCategory, Qt::CaseInsensitive) &&
                !qid.contains(subCategory, Qt::CaseInsensitive))
            {
                continue;
            }
        }

        if (!filter.isEmpty())
        {
            QString qid = QString::fromStdString(cab.id);
            QString qname = QString::fromStdString(cab.name);
            QString qcat = QString::fromStdString(cab.category);
            QString qstd = QString::fromStdString(cab.standard.name);
            if (!qid.contains(filter, Qt::CaseInsensitive) &&
                !qname.contains(filter, Qt::CaseInsensitive) &&
                !qcat.contains(filter, Qt::CaseInsensitive) &&
                !qstd.contains(filter, Qt::CaseInsensitive))
            {
                continue;
            }
        }
        m_currentCables.push_back(cab);
    }

    m_itemsTable->setRowCount(static_cast<int>(m_currentCables.size()));
    for (int r = 0; r < static_cast<int>(m_currentCables.size()); ++r)
    {
        const auto& cab = m_currentCables[r];
        m_itemsTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(cab.id)));
        m_itemsTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(cab.name)));
        m_itemsTable->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(cab.category)));

        double dMm = cab.nominalDiameter * 1000.0;
        m_itemsTable->setItem(r, 3, new QTableWidgetItem(QString::number(dMm, 'f', 1)));

        double fbrKn = cab.minimumBreakingForce / 1000.0;
        m_itemsTable->setItem(r, 4, new QTableWidgetItem(QString::number(fbrKn, 'f', 0)));
        m_itemsTable->setItem(r, 5, new QTableWidgetItem(QString::fromStdString(cab.standard.name)));
    }
    m_statusLabel->setText(tr("%1 câble(s) affiché(s)").arg(m_currentCables.size()));
}

void ExtensionManagerDialog::populateTexturesList(const QString& filter)
{
    m_itemsTable->clear();
    m_itemsTable->setColumnCount(4);
    m_itemsTable->setHorizontalHeaderLabels({ tr("ID Texture"), tr("Nom"), tr("Catégorie"), tr("Fichier Image") });

    auto textures = TSA::Viewer::TextureManager::instance().allTextures();
    int count = 0;

    for (const auto& tex : textures)
    {
        QString qid = tex.id;
        QString qname = tex.name;
        QString qcat = tex.category;
        QString qfile = tex.filePath;

        if (!filter.isEmpty() &&
            !qid.contains(filter, Qt::CaseInsensitive) &&
            !qname.contains(filter, Qt::CaseInsensitive) &&
            !qcat.contains(filter, Qt::CaseInsensitive))
        {
            continue;
        }

        int row = m_itemsTable->rowCount();
        m_itemsTable->insertRow(row);
        m_itemsTable->setItem(row, 0, new QTableWidgetItem(qid));
        m_itemsTable->setItem(row, 1, new QTableWidgetItem(qname));
        m_itemsTable->setItem(row, 2, new QTableWidgetItem(qcat));
        m_itemsTable->setItem(row, 3, new QTableWidgetItem(qfile));
        count++;
    }
    m_statusLabel->setText(tr("%1 texture(s) PBR disponible(s)").arg(count));
}

void ExtensionManagerDialog::populateStandardsList(const QString& filter)
{
    m_itemsTable->clear();
    m_itemsTable->setColumnCount(4);
    m_itemsTable->setHorizontalHeaderLabels({ tr("Référence Normative"), tr("Titre"), tr("Édition"), tr("Domaine Eurocode") });

    struct StandardEntry { QString ref; QString title; QString year; QString domain; };
    std::vector<StandardEntry> stds = {
        { "EN 1990", "Bases de calcul des structures", "2002", "Actions & Sécurité" },
        { "EN 1991", "Actions sur les structures (Poids, Neige, Vent)", "2005", "Charges & Actions" },
        { "EN 1992", "Calcul des structures en béton (EC2)", "2004", "Béton Armé & Précontraint" },
        { "EN 1993", "Calcul des structures en acier (EC3)", "2005", "Structures Métalliques" },
        { "EN 1993-1-11", "Calcul des structures avec éléments tendus", "2006", "Câbles, Tirants & Haubans" },
        { "EN 10138-3", "Aciers de précontrainte - Torons", "2009", "Torons Précontrainte" },
        { "EN 10138-4", "Aciers de précontrainte - Barres", "2009", "Barres de Post-tension" },
        { "ASTM A416", "Standard Specification for Steel Strand, Uncoated Seven-Wire", "2018", "Torons Internationaux" }
    };

    int count = 0;
    for (const auto& entry : stds)
    {
        if (!filter.isEmpty() &&
            !entry.ref.contains(filter, Qt::CaseInsensitive) &&
            !entry.title.contains(filter, Qt::CaseInsensitive) &&
            !entry.domain.contains(filter, Qt::CaseInsensitive))
        {
            continue;
        }

        int row = m_itemsTable->rowCount();
        m_itemsTable->insertRow(row);
        m_itemsTable->setItem(row, 0, new QTableWidgetItem(entry.ref));
        m_itemsTable->setItem(row, 1, new QTableWidgetItem(entry.title));
        m_itemsTable->setItem(row, 2, new QTableWidgetItem(entry.year));
        m_itemsTable->setItem(row, 3, new QTableWidgetItem(entry.domain));
        count++;
    }
    m_statusLabel->setText(tr("%1 référence(s) normative(s)").arg(count));
}

void ExtensionManagerDialog::onItemTableSelectionChanged()
{
    int row = m_itemsTable->currentRow();
    if (row < 0) return;
    updateDetailView(row);
}

void ExtensionManagerDialog::updateDetailView(int row)
{
    if (m_currentView == CurrentViewType::Materials && row >= 0 && row < static_cast<int>(m_currentMaterials.size()))
    {
        m_detailBrowser->setHtml(formatMaterialHtml(m_currentMaterials[row]));
    }
    else if (m_currentView == CurrentViewType::Sections && row >= 0 && row < static_cast<int>(m_currentSections.size()))
    {
        m_detailBrowser->setHtml(formatSectionHtml(m_currentSections[row]));
    }
    else if (m_currentView == CurrentViewType::Cables && row >= 0 && row < static_cast<int>(m_currentCables.size()))
    {
        m_detailBrowser->setHtml(formatCableHtml(m_currentCables[row]));
    }
    else if (m_currentView == CurrentViewType::Extensions && row >= 0 && row < static_cast<int>(m_currentExtensions.size()))
    {
        m_detailBrowser->setHtml(formatExtensionHtml(m_currentExtensions[row]));
    }
    else
    {
        m_detailBrowser->setHtml(tr("<p style='color: #4A5568;'>Élément sélectionné : <b>%1</b></p>")
            .arg(m_itemsTable->item(row, 0) ? m_itemsTable->item(row, 0)->text() : ""));
    }
}

QString ExtensionManagerDialog::formatMaterialHtml(const TSA::ExtensionSystem::MaterialDefinition& mat) const
{
    QString html;
    html += "<div style='font-family: Segoe UI, sans-serif;'>";
    html += QString("<h2 style='margin-bottom: 4px; color: #2B6CB0;'>%1 <span style='font-size: 13px; font-weight: normal; color: #718096;'>[%2]</span></h2>")
        .arg(QString::fromStdString(mat.name))
        .arg(QString::fromStdString(mat.id));

    html += QString("<p style='margin: 4px 0;'><b>Catégorie :</b> %1 | <b>Norme :</b> %2 (%3) - %4</p>")
        .arg(QString::fromStdString(mat.category))
        .arg(QString::fromStdString(mat.standard.name))
        .arg(QString::fromStdString(mat.standard.edition))
        .arg(QString::fromStdString(mat.standard.clause));

    html += "<hr style='border: none; border-top: 1px solid #E2E8F0; margin: 8px 0;'/>";

    html += "<table style='width: 100%; border-collapse: collapse; font-size: 13px;'>";
    html += "<tr style='background: #EDF2F7;'><th style='padding: 6px; text-align: left;'>Propriété Physique</th><th style='padding: 6px; text-align: right;'>Valeur</th><th style='padding: 6px; text-align: left;'>Unité</th></tr>";

    html += QString("<tr><td style='padding: 4px 6px;'>Module d'Young (E)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>GPa</td></tr>")
        .arg(mat.youngModulus.toSI() / 1.0e9, 0, 'f', 1);

    html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Coefficient de Poisson (ν)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>-</td></tr>")
        .arg(mat.poissonRatio, 0, 'f', 2);

    html += QString("<tr><td style='padding: 4px 6px;'>Masse Volumique (ρ)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>kg/m³</td></tr>")
        .arg(mat.density.toSI(), 0, 'f', 0);

    if (mat.fck.value > 0.0)
    {
        html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Résistance caractéristique fck</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>MPa</td></tr>")
            .arg(mat.fck.toSI() / 1.0e6, 0, 'f', 1);
    }
    if (mat.fy.value > 0.0)
    {
        html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Limite d'élasticité fy</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>MPa</td></tr>")
            .arg(mat.fy.toSI() / 1.0e6, 0, 'f', 1);
    }

    html += QString("<tr><td style='padding: 4px 6px;'>Dilatation thermique (α)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>1/K</td></tr>")
        .arg(QString::number(mat.thermalCoeff.value, 'e', 2));

    html += "</table>";

    html += "<p style='margin: 8px 0 4px 0;'><b>Aspect Visuel PBR :</b></p>";
    html += QString("<div style='display: flex; align-items: center; gap: 10px;'>Couleur : <span style='display: inline-block; width: 20px; height: 20px; background-color: %1; border: 1px solid #718096; vertical-align: middle;'></span> %1 | Rugosité : %2 | Métallique : %3</div>")
        .arg(QString::fromStdString(mat.visual.baseColor))
        .arg(mat.visual.roughness, 0, 'f', 2)
        .arg(mat.visual.metallic, 0, 'f', 2);

    html += "</div>";
    return html;
}

QString ExtensionManagerDialog::formatSectionHtml(const TSA::ExtensionSystem::SectionDefinition& sec) const
{
    QString html;
    html += "<div style='font-family: Segoe UI, sans-serif;'>";
    html += QString("<h2 style='margin-bottom: 4px; color: #2B6CB0;'>%1 <span style='font-size: 13px; font-weight: normal; color: #718096;'>[%2]</span></h2>")
        .arg(QString::fromStdString(sec.name))
        .arg(QString::fromStdString(sec.id));

    html += QString("<p style='margin: 4px 0;'><b>Forme :</b> %1 | <b>Norme :</b> %2 (%3) | <b>Matériau par défaut :</b> %4</p>")
        .arg(QString::fromStdString(sec.shapeType))
        .arg(QString::fromStdString(sec.standard.name))
        .arg(QString::fromStdString(sec.standard.edition))
        .arg(QString::fromStdString(sec.defaultMaterialId));

    html += "<hr style='border: none; border-top: 1px solid #E2E8F0; margin: 8px 0;'/>";

    html += "<table style='width: 100%; border-collapse: collapse; font-size: 13px;'>";
    html += "<tr style='background: #EDF2F7;'><th style='padding: 6px; text-align: left;'>Caractéristique Géométrique</th><th style='padding: 6px; text-align: right;'>Valeur</th><th style='padding: 6px; text-align: left;'>Unité</th></tr>";

    html += QString("<tr><td style='padding: 4px 6px;'>Hauteur totale (h)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>mm</td></tr>")
        .arg(sec.height * 1000.0, 0, 'f', 1);

    html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Largeur (b)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>mm</td></tr>")
        .arg(sec.width * 1000.0, 0, 'f', 1);

    if (sec.webThickness > 0.0)
    {
        html += QString("<tr><td style='padding: 4px 6px;'>Épaisseur de l'âme (tw)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>mm</td></tr>")
            .arg(sec.webThickness * 1000.0, 0, 'f', 2);
    }
    if (sec.flangeThickness > 0.0)
    {
        html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Épaisseur de semelle (tf)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>mm</td></tr>")
            .arg(sec.flangeThickness * 1000.0, 0, 'f', 2);
    }

    html += QString("<tr><td style='padding: 4px 6px;'>Section Transversale (A)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>cm²</td></tr>")
        .arg(sec.area * 10000.0, 0, 'f', 2);

    html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Inertie Iy (axe fort)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>cm⁴</td></tr>")
        .arg(sec.ix * 1.0e8, 0, 'f', 1);

    html += QString("<tr><td style='padding: 4px 6px;'>Inertie Iz (axe faible)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>cm⁴</td></tr>")
        .arg(sec.iy * 1.0e8, 0, 'f', 1);

    html += "</table>";
    html += "</div>";
    return html;
}

QString ExtensionManagerDialog::formatCableHtml(const TSA::ExtensionSystem::CableCatalogDefinition& cab) const
{
    QString html;
    html += "<div style='font-family: Segoe UI, sans-serif;'>";
    html += QString("<h2 style='margin-bottom: 4px; color: #2B6CB0;'>%1 <span style='font-size: 13px; font-weight: normal; color: #718096;'>[%2]</span></h2>")
        .arg(QString::fromStdString(cab.name))
        .arg(QString::fromStdString(cab.id));

    html += QString("<p style='margin: 4px 0;'><b>Nuance :</b> %1 | <b>Type :</b> %2 | <b>Norme :</b> %3 (%4)</p>")
        .arg(QString::fromStdString(cab.grade))
        .arg(QString::fromStdString(cab.category))
        .arg(QString::fromStdString(cab.standard.name))
        .arg(QString::fromStdString(cab.standard.edition));

    html += "<hr style='border: none; border-top: 1px solid #E2E8F0; margin: 8px 0;'/>";

    html += "<table style='width: 100%; border-collapse: collapse; font-size: 13px;'>";
    html += "<tr style='background: #EDF2F7;'><th style='padding: 6px; text-align: left;'>Caractéristique Câble</th><th style='padding: 6px; text-align: right;'>Valeur</th><th style='padding: 6px; text-align: left;'>Unité</th></tr>";

    html += QString("<tr><td style='padding: 4px 6px;'>Diamètre Nominal (D)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>mm</td></tr>")
        .arg(cab.nominalDiameter * 1000.0, 0, 'f', 1);

    html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Section Acier Réelle (A)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>mm²</td></tr>")
        .arg(cab.metallicArea * 1.0e6, 0, 'f', 1);

    html += QString("<tr><td style='padding: 4px 6px;'>Masse Linéique</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>kg/m</td></tr>")
        .arg(cab.linearMass, 0, 'f', 3);

    html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Module d'Élasticité (E)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>GPa</td></tr>")
        .arg(cab.elasticModulus / 1.0e9, 0, 'f', 1);

    html += QString("<tr><td style='padding: 4px 6px;'>Résistance fpk</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>MPa</td></tr>")
        .arg(cab.characteristicStrength / 1.0e6, 0, 'f', 0);

    html += QString("<tr style='background: #F7FAFC;'><td style='padding: 4px 6px;'>Force de Rupture (Fm)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>kN</td></tr>")
        .arg(cab.minimumBreakingForce / 1000.0, 0, 'f', 1);

    html += QString("<tr><td style='padding: 4px 6px;'>Tension Initiale Prévue (P0)</td><td style='text-align: right;'>%1</td><td style='padding: 4px 6px;'>kN</td></tr>")
        .arg(cab.defaultInitialTension / 1000.0, 0, 'f', 1);

    html += "</table>";
    html += "</div>";
    return html;
}

QString ExtensionManagerDialog::formatExtensionHtml(const TSA::ExtensionSystem::ExtensionManifest& man) const
{
    QString html;
    html += "<div style='font-family: Segoe UI, sans-serif;'>";
    html += QString("<h2 style='margin-bottom: 4px; color: #2B6CB0;'>%1 <span style='font-size: 13px; font-weight: normal; color: #718096;'>v%2</span></h2>")
        .arg(QString::fromStdString(man.name))
        .arg(QString::fromStdString(man.version.toString()));

    html += QString("<p style='margin: 4px 0;'><b>Identifiant :</b> %1 | <b>Auteur :</b> %2</p>")
        .arg(QString::fromStdString(man.id))
        .arg(QString::fromStdString(man.author));

    html += QString("<p style='color: #4A5568;'>%1</p>").arg(QString::fromStdString(man.description));

    html += "<hr style='border: none; border-top: 1px solid #E2E8F0; margin: 8px 0;'/>";

    html += "<p><b>Catégories Fournies :</b></p><ul>";
    for (const auto& cat : man.categories)
    {
        html += QString("<li>%1</li>").arg(QString::fromStdString(cat));
    }
    html += "</ul>";
    html += "</div>";
    return html;
}

void ExtensionManagerDialog::updateTelemetryBar()
{
    auto& mgr = TSA::ExtensionSystem::LibraryManager::instance();
    auto& cache = TSA::ExtensionSystem::LibraryCache::instance();

    int totalIndexed = static_cast<int>(mgr.indexedDefinitionsCount());
    int shapesInCache = static_cast<int>(cache.shapeCount());
    double hitRatio = cache.hitRatio() * 100.0;

    m_telemetryLabel->setText(tr("Définitions Indexées : %1 | Solides 3D en Cache : %2 | Hit Ratio : %3%")
        .arg(totalIndexed)
        .arg(shapesInCache)
        .arg(hitRatio, 0, 'f', 1));
}

void ExtensionManagerDialog::onReloadAll(bool showMessage)
{
    TSA::ExtensionSystem::LibraryManager::instance().reloadAll();
    populateCategoryTree();
    refreshCurrentCategory();
    updateTelemetryBar();

    emit extensionsReloaded();

    if (showMessage)
    {
        QMessageBox::information(this, tr("Rechargement à Chaud"),
            tr("Toutes les extensions TSALib et bibliothèques JSON/Textures ont été rechargées avec succès depuis le disque.\nAucune recompilation n'a été nécessaire."));
    }
}

void ExtensionManagerDialog::onValidateAll(bool showMessage)
{
    auto result = TSA::ExtensionSystem::LibraryManager::instance().validateAll();
    if (showMessage)
    {
        if (result.isValid())
        {
            QMessageBox::information(this, tr("Validation TSALib"),
                tr("Toutes les définitions externes (Matériaux, Sections, Câbles, Normes) sont 100% conformes et validées avec succès sans aucune erreur !"));
        }
        else
        {
            QString msg = tr("Validation terminée avec des alertes :\n\n");
            for (const auto& err : result.errors)
            {
                msg += QString("❌ %1\n").arg(QString::fromStdString(err));
            }
            for (const auto& warn : result.warnings)
            {
                msg += QString("⚠️ %1\n").arg(QString::fromStdString(warn));
            }
            QMessageBox::warning(this, tr("Rapport de Validation TSALib"), msg);
        }
    }
}

void ExtensionManagerDialog::onImportExtension()
{
    QString selected = QFileDialog::getOpenFileName(this,
        tr("Importer un package TSALib ou dossier"),
        QString(),
        tr("Packages TSALib (*.tsalib);;Tous les fichiers (*.*)"));

    if (selected.isEmpty())
    {
        // Si l'utilisateur n'a pas sélectionné de fichier .tsalib, demander s'il souhaite importer un dossier
        QString dir = QFileDialog::getExistingDirectory(this, tr("Ou sélectionner un dossier d'extension à importer"));
        if (dir.isEmpty()) return;

        TSA::ExtensionSystem::LibraryManager::instance().addSearchPath(dir);
        auto discovered = TSA::ExtensionSystem::LibraryManager::instance().discover();

        populateCategoryTree();
        refreshCurrentCategory();
        updateTelemetryBar();

        QMessageBox::information(this, tr("Importation d'Extension"),
            tr("Le dossier a été ajouté aux chemins de recherche.\n%1 extension(s) candidate(s) découverte(s).").arg(discovered.size()));
        return;
    }

    if (selected.endsWith(".tsalib", Qt::CaseInsensitive))
    {
        QString err;
        if (TSA::ExtensionSystem::LibraryManager::instance().installPackage(selected, &err))
        {
            populateCategoryTree();
            refreshCurrentCategory();
            updateTelemetryBar();
            emit extensionsReloaded();

            QMessageBox::information(this, tr("Importation Réussie"),
                tr("Le package .tsalib a été validé, extrait et chargé à chaud avec succès dans TSA !"));
        }
        else
        {
            QMessageBox::critical(this, tr("Erreur d'Importation"),
                tr("Échec de l'installation du package .tsalib :\n%1").arg(err));
        }
    }
}

void ExtensionManagerDialog::onExportExtension()
{
    auto exts = TSA::ExtensionSystem::LibraryManager::instance().installedExtensions();
    if (exts.empty())
    {
        QMessageBox::warning(this, tr("Exportation"), tr("Aucune extension n'est disponible pour l'exportation."));
        return;
    }

    std::string extId = exts.front().id;
    if (m_currentView == CurrentViewType::Extensions && !m_currentExtensions.empty())
    {
        int row = m_itemsTable->currentRow();
        if (row >= 0 && row < static_cast<int>(m_currentExtensions.size()))
        {
            extId = m_currentExtensions[row].id;
        }
    }

    QString defaultName = QString::fromStdString(extId) + ".tsalib";
    QString outPath = QFileDialog::getSaveFileName(this,
        tr("Exporter le package TSALib"),
        defaultName,
        tr("Packages TSALib (*.tsalib)"));

    if (outPath.isEmpty()) return;

    QString err;
    if (TSA::ExtensionSystem::LibraryManager::instance().exportPackage(extId, outPath, &err))
    {
        QMessageBox::information(this, tr("Exportation Réussie"),
            tr("L'extension '%1' a été exportée avec succès sous forme de package autonome :\n%2")
            .arg(QString::fromStdString(extId)).arg(outPath));
    }
    else
    {
        QMessageBox::critical(this, tr("Erreur d'Exportation"),
            tr("Impossible d'exporter l'extension :\n%1").arg(err));
    }
}

void ExtensionManagerDialog::onNewExtension()
{
    NewExtensionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
    {
        onReloadAll(false);
        selectCategory("Extensions");
        m_statusLabel->setText(tr("Bibliothèque '%1' créée et chargée avec succès.").arg(dlg.createdExtensionId()));
    }
}

void ExtensionManagerDialog::onOpenExtensionsFolder()
{
    QString path = "e:/Book/Dev/TSA/Extensions";
    if (!QDir(path).exists())
    {
        path = QDir::currentPath() + "/Extensions";
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

} // namespace TSA::UI
