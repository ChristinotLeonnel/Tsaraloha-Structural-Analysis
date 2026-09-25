#include "SurfaceCreationDialog.h"
#include "../../Model/Model.h"
#include "../../Viewer/OccView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QStackedWidget>
#include <QColorDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <sstream>

namespace TSA::UI
{

SurfaceCreationDialog::SurfaceCreationDialog(TSA::Model::Model* model, OccView* occView, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_occView(occView)
    , m_slabColor(180, 200, 220)
    , m_wallColor(210, 190, 180)
{
    setWindowTitle(tr("Éléments Surfaciques (Dalles, Voiles)"));
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setFixedWidth(380);

    setupUi();
    updateNextId();
    syncPresetsToOccView();
}

void SurfaceCreationDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 1. Sélecteur de type d'élément surfacique
    auto* typeLayout = new QHBoxLayout();
    typeLayout->setSpacing(6);
    auto* lblType = new QLabel(tr("Type de surface :"), this);
    lblType->setStyleSheet("font-weight: bold;");
    m_comboType = new QComboBox(this);
    m_comboType->addItem(tr("Dalle / Plancher"), static_cast<int>(SurfaceType::Slab));
    m_comboType->addItem(tr("Voile / Mur porteur"), static_cast<int>(SurfaceType::Wall));
    typeLayout->addWidget(lblType);
    typeLayout->addWidget(m_comboType, 1);
    mainLayout->addLayout(typeLayout);

    // 2. Numérotation et Nom
    auto* numLayout = new QHBoxLayout();
    numLayout->setSpacing(6);

    auto* lblId = new QLabel(tr("N° :"), this);
    m_spinId = new QSpinBox(this);
    m_spinId->setRange(1, 999999);
    m_spinId->setValue(1);

    auto* lblStep = new QLabel(tr("Pas :"), this);
    lblStep->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_spinStep = new QSpinBox(this);
    m_spinStep->setRange(1, 100);
    m_spinStep->setValue(1);

    numLayout->addWidget(lblId);
    numLayout->addWidget(m_spinId, 1);
    numLayout->addWidget(lblStep);
    numLayout->addWidget(m_spinStep, 1);
    mainLayout->addLayout(numLayout);

    auto* nameLayout = new QHBoxLayout();
    nameLayout->setSpacing(6);
    auto* lblName = new QLabel(tr("Nom :"), this);
    lblName->setFixedWidth(40);
    m_editName = new QLineEdit(this);
    m_editName->setText("Dalle_1");
    nameLayout->addWidget(lblName);
    nameLayout->addWidget(m_editName, 1);
    mainLayout->addLayout(nameLayout);

    // 3. Stacked Widget pour les paramètres spécifiques Dalle vs Voile
    m_paramStack = new QStackedWidget(this);

    // --- Page 0 : DALLE ---
    m_slabPage = new QWidget(this);
    auto* slabLayout = new QVBoxLayout(m_slabPage);
    slabLayout->setContentsMargins(0, 0, 0, 0);
    slabLayout->setSpacing(6);

    auto* grpSlab = new QGroupBox(tr("Caractéristiques Dalle / Plancher"), m_slabPage);
    auto* slabForm = new QGridLayout(grpSlab);
    slabForm->setContentsMargins(8, 12, 8, 8);
    slabForm->setSpacing(6);

    slabForm->addWidget(new QLabel(tr("Épaisseur e :"), grpSlab), 0, 0);
    m_spinSlabThickness = new QDoubleSpinBox(grpSlab);
    m_spinSlabThickness->setRange(0.01, 5.0);
    m_spinSlabThickness->setValue(0.20);
    m_spinSlabThickness->setSingleStep(0.01);
    m_spinSlabThickness->setSuffix(" m");
    slabForm->addWidget(m_spinSlabThickness, 0, 1);

    slabForm->addWidget(new QLabel(tr("Comportement :"), grpSlab), 1, 0);
    m_comboSlabType = new QComboBox(grpSlab);
    m_comboSlabType->addItem(tr("Bidirectionnelle (2 sens)"));
    m_comboSlabType->addItem(tr("Unidirectionnelle (1 sens)"));
    slabForm->addWidget(m_comboSlabType, 1, 1);

    slabForm->addWidget(new QLabel(tr("Matériau :"), grpSlab), 2, 0);
    m_comboSlabMaterial = new QComboBox(grpSlab);
    m_comboSlabMaterial->addItem("Béton C25/30");
    m_comboSlabMaterial->addItem("Béton C30/37");
    m_comboSlabMaterial->addItem("Béton C20/25");
    m_comboSlabMaterial->addItem("Béton C35/45");
    m_comboSlabMaterial->addItem("Béton Armé");
    slabForm->addWidget(m_comboSlabMaterial, 2, 1);

    slabForm->addWidget(new QLabel(tr("Couleur :"), grpSlab), 3, 0);
    m_btnSlabColor = new QPushButton(tr("Choisir..."), grpSlab);
    updateColorButton(m_btnSlabColor, m_slabColor);
    slabForm->addWidget(m_btnSlabColor, 3, 1);

    slabLayout->addWidget(grpSlab);

    auto* grpSlabContour = new QGroupBox(tr("Contour Polygonal (Nœuds)"), m_slabPage);
    auto* contourLayout = new QVBoxLayout(grpSlabContour);
    contourLayout->setContentsMargins(8, 10, 8, 8);
    contourLayout->setSpacing(6);

    m_editSlabNodes = new QLineEdit(grpSlabContour);
    m_editSlabNodes->setPlaceholderText(tr("Ex: 1, 2, 4, 3 ou cliquez en 3D"));
    contourLayout->addWidget(m_editSlabNodes);

    m_lblSlabStatus = new QLabel(tr("Cliquez les nœuds en 3D pour définir le contour"), grpSlabContour);
    m_lblSlabStatus->setStyleSheet("color: #606060; font-size: 11px; font-style: italic;");
    contourLayout->addWidget(m_lblSlabStatus);

    auto* subContourBtns = new QHBoxLayout();
    m_btnCloseContour = new QPushButton(tr("Fermer le contour"), grpSlabContour);
    m_btnCloseContour->setIcon(QIcon(":/icons/draw_slab.svg"));
    m_btnClearContour = new QPushButton(tr("Effacer"), grpSlabContour);
    subContourBtns->addWidget(m_btnCloseContour);
    subContourBtns->addWidget(m_btnClearContour);
    contourLayout->addLayout(subContourBtns);

    slabLayout->addWidget(grpSlabContour);
    m_paramStack->addWidget(m_slabPage);

    // --- Page 1 : VOILE ---
    m_wallPage = new QWidget(this);
    auto* wallLayout = new QVBoxLayout(m_wallPage);
    wallLayout->setContentsMargins(0, 0, 0, 0);
    wallLayout->setSpacing(6);

    auto* grpWall = new QGroupBox(tr("Caractéristiques Voile / Mur"), m_wallPage);
    auto* wallForm = new QGridLayout(grpWall);
    wallForm->setContentsMargins(8, 12, 8, 8);
    wallForm->setSpacing(6);

    wallForm->addWidget(new QLabel(tr("Épaisseur e :"), grpWall), 0, 0);
    m_spinWallThickness = new QDoubleSpinBox(grpWall);
    m_spinWallThickness->setRange(0.01, 2.0);
    m_spinWallThickness->setValue(0.20);
    m_spinWallThickness->setSingleStep(0.01);
    m_spinWallThickness->setSuffix(" m");
    wallForm->addWidget(m_spinWallThickness, 0, 1);

    wallForm->addWidget(new QLabel(tr("Hauteur H :"), grpWall), 1, 0);
    m_spinWallHeight = new QDoubleSpinBox(grpWall);
    m_spinWallHeight->setRange(0.10, 50.0);
    m_spinWallHeight->setValue(3.00);
    m_spinWallHeight->setSingleStep(0.50);
    m_spinWallHeight->setSuffix(" m");
    wallForm->addWidget(m_spinWallHeight, 1, 1);

    wallForm->addWidget(new QLabel(tr("Excentrement :"), grpWall), 2, 0);
    m_spinWallOffset = new QDoubleSpinBox(grpWall);
    m_spinWallOffset->setRange(-5.0, 5.0);
    m_spinWallOffset->setValue(0.0);
    m_spinWallOffset->setSingleStep(0.05);
    m_spinWallOffset->setSuffix(" m");
    wallForm->addWidget(m_spinWallOffset, 2, 1);

    wallForm->addWidget(new QLabel(tr("Matériau :"), grpWall), 3, 0);
    m_comboWallMaterial = new QComboBox(grpWall);
    m_comboWallMaterial->addItem("Béton C25/30");
    m_comboWallMaterial->addItem("Béton C30/37");
    m_comboWallMaterial->addItem("Béton C20/25");
    m_comboWallMaterial->addItem("Béton C35/45");
    m_comboWallMaterial->addItem("Béton Armé");
    wallForm->addWidget(m_comboWallMaterial, 3, 1);

    wallForm->addWidget(new QLabel(tr("Couleur :"), grpWall), 4, 0);
    m_btnWallColor = new QPushButton(tr("Choisir..."), grpWall);
    updateColorButton(m_btnWallColor, m_wallColor);
    wallForm->addWidget(m_btnWallColor, 4, 1);

    wallLayout->addWidget(grpWall);

    auto* grpWallNodes = new QGroupBox(tr("Position de base (Nœuds ou Coordonnées)"), m_wallPage);
    auto* wallNodesForm = new QGridLayout(grpWallNodes);
    wallNodesForm->setContentsMargins(8, 12, 8, 8);
    wallNodesForm->setSpacing(6);

    wallNodesForm->addWidget(new QLabel(tr("Origine :"), grpWallNodes), 0, 0);
    m_editWallOrigin = new QLineEdit(grpWallNodes);
    m_editWallOrigin->setPlaceholderText(tr("Nœud ou X; Y; Z"));
    wallNodesForm->addWidget(m_editWallOrigin, 0, 1);

    wallNodesForm->addWidget(new QLabel(tr("Extrémité :"), grpWallNodes), 1, 0);
    m_editWallEnd = new QLineEdit(grpWallNodes);
    m_editWallEnd->setPlaceholderText(tr("Nœud ou X; Y; Z"));
    wallNodesForm->addWidget(m_editWallEnd, 1, 1);

    m_chkWallChain = new QCheckBox(tr("Étirer (Tracé continu de voiles)"), grpWallNodes);
    m_chkWallChain->setChecked(true);
    wallNodesForm->addWidget(m_chkWallChain, 2, 0, 1, 2);

    wallLayout->addWidget(grpWallNodes);
    m_paramStack->addWidget(m_wallPage);

    mainLayout->addWidget(m_paramStack);

    // 4. Boutons inférieurs
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnAdd = new QPushButton(tr("Ajouter"), this);
    m_btnAdd->setIcon(QIcon(":/icons/slab_add.svg"));
    m_btnAdd->setStyleSheet("font-weight: bold; background-color: #2E7D32; color: white; padding: 6px 14px; border-radius: 4px;");

    m_btnClose = new QPushButton(tr("Fermer"), this);
    m_btnClose->setIcon(QIcon(":/icons/cancel.svg"));

    m_btnHelp = new QPushButton(tr("Aide"), this);
    m_btnHelp->setIcon(QIcon(":/icons/TSA.svg"));

    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnClose);
    btnLayout->addWidget(m_btnHelp);
    mainLayout->addLayout(btnLayout);

    // Connexions
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SurfaceCreationDialog::onTypeChanged);
    connect(m_spinSlabThickness, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { syncPresetsToOccView(); });
    connect(m_comboSlabMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { syncPresetsToOccView(); });
    connect(m_spinWallThickness, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { syncPresetsToOccView(); });
    connect(m_spinWallHeight, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { syncPresetsToOccView(); });
    connect(m_spinWallOffset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { syncPresetsToOccView(); });
    connect(m_comboWallMaterial, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { syncPresetsToOccView(); });

    connect(m_btnSlabColor, &QPushButton::clicked, this, &SurfaceCreationDialog::onColorPickClicked);
    connect(m_btnWallColor, &QPushButton::clicked, this, &SurfaceCreationDialog::onColorPickClicked);

    connect(m_btnCloseContour, &QPushButton::clicked, this, &SurfaceCreationDialog::onCloseContourClicked);
    connect(m_btnClearContour, &QPushButton::clicked, this, &SurfaceCreationDialog::onClearContourClicked);

    connect(m_editWallOrigin, &QLineEdit::returnPressed, this, &SurfaceCreationDialog::onWallOriginReturnPressed);
    connect(m_editWallEnd, &QLineEdit::returnPressed, this, &SurfaceCreationDialog::onWallEndReturnPressed);

    connect(m_btnAdd, &QPushButton::clicked, this, &SurfaceCreationDialog::onAddClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::close);
    connect(m_btnHelp, &QPushButton::clicked, this, &SurfaceCreationDialog::onHelpClicked);
}

SurfaceCreationDialog::SurfaceType SurfaceCreationDialog::currentSurfaceType() const
{
    return static_cast<SurfaceType>(m_comboType->currentIndex());
}

void SurfaceCreationDialog::setSurfaceType(SurfaceType type)
{
    m_comboType->setCurrentIndex(static_cast<int>(type));
}

void SurfaceCreationDialog::setSlabThickness(double th)
{
    if (m_spinSlabThickness) m_spinSlabThickness->setValue(th);
}

void SurfaceCreationDialog::setWallParameters(double th, double height, double offset)
{
    if (m_spinWallThickness) m_spinWallThickness->setValue(th);
    if (m_spinWallHeight) m_spinWallHeight->setValue(height);
    if (m_spinWallOffset) m_spinWallOffset->setValue(offset);
}

void SurfaceCreationDialog::onTypeChanged(int index)
{
    m_paramStack->setCurrentIndex(index);
    updateNextId();
    syncPresetsToOccView();

    if (m_occView)
    {
        if (index == 0) // Slab
        {
            m_occView->setInteractionMode(OccView::InteractionMode::DrawSlab);
            emit surfaceTypeChanged(SurfaceType::Slab);
        }
        else // Wall
        {
            m_occView->setInteractionMode(OccView::InteractionMode::DrawWall);
            emit surfaceTypeChanged(SurfaceType::Wall);
        }
    }
}

void SurfaceCreationDialog::updateNextId()
{
    if (!m_model) return;

    if (currentSurfaceType() == SurfaceType::Slab)
    {
        int nextId = m_model->nextSlabId();
        m_spinId->setValue(nextId);
        m_editName->setText(QString("Dalle_%1").arg(nextId));
    }
    else
    {
        int nextId = m_model->nextWallId();
        m_spinId->setValue(nextId);
        m_editName->setText(QString("Voile_%1").arg(nextId));
    }
}

void SurfaceCreationDialog::syncPresetsToOccView()
{
    if (!m_occView) return;

    auto& presets = m_occView->creationPresets();
    presets.slab.thickness = m_spinSlabThickness->value();
    presets.slab.material = TSA::Model::Material::findByName(m_comboSlabMaterial->currentText().toStdString());
    if (m_slabColor.isValid()) presets.slab.color = m_slabColor.name().toStdString();

    presets.wall.thickness = m_spinWallThickness->value();
    presets.wall.height = m_spinWallHeight->value();
    presets.wall.offset = m_spinWallOffset->value();
    presets.wall.material = TSA::Model::Material::findByName(m_comboWallMaterial->currentText().toStdString());
    if (m_wallColor.isValid()) presets.wall.color = m_wallColor.name().toStdString();
}

void SurfaceCreationDialog::onSlabNodePicked(int nodeId, const gp_Pnt& pt, int totalCount)
{
    Q_UNUSED(pt);
    m_currentSlabNodes.push_back(nodeId);

    QStringList ids;
    for (int nid : m_currentSlabNodes)
    {
        ids << QString::number(nid);
    }
    m_editSlabNodes->setText(ids.join(", "));
    m_lblSlabStatus->setText(tr("%1 nœud(s) cliqué(s). Fermez sur N%2 ou cliquez 'Fermer le contour'")
        .arg(totalCount).arg(m_currentSlabNodes.front()));
}

void SurfaceCreationDialog::onSlabCreated(int slabId)
{
    m_currentSlabNodes.clear();
    m_editSlabNodes->clear();
    m_lblSlabStatus->setText(tr("Dalle S%1 créée avec succès").arg(slabId));
    updateNextId();
    emit surfaceCreated(slabId, SurfaceType::Slab);
}

void SurfaceCreationDialog::onSlabDrawingCancelled()
{
    m_currentSlabNodes.clear();
    m_editSlabNodes->clear();
    m_lblSlabStatus->setText(tr("Tracé polygonal réinitialisé"));
}

void SurfaceCreationDialog::onWallFirstPointPicked(const gp_Pnt& pt, int nodeId)
{
    m_editWallOrigin->setText(nodeId > 0 ? QString::number(nodeId) : formatPoint(pt));
    m_editWallEnd->clear();
}

void SurfaceCreationDialog::onWallSecondPointPicked(const gp_Pnt& pt, int nodeId)
{
    m_editWallEnd->setText(nodeId > 0 ? QString::number(nodeId) : formatPoint(pt));
}

void SurfaceCreationDialog::onWallDrawingCancelled()
{
    m_editWallOrigin->clear();
    m_editWallEnd->clear();
}

void SurfaceCreationDialog::onWallCreated(int wallId)
{
    updateNextId();
    if (!m_chkWallChain->isChecked())
    {
        m_editWallOrigin->clear();
        m_editWallEnd->clear();
    }
    else
    {
        m_editWallOrigin->setText(m_editWallEnd->text());
        m_editWallEnd->clear();
    }
    emit surfaceCreated(wallId, SurfaceType::Wall);
}

void SurfaceCreationDialog::onCloseContourClicked()
{
    if (m_occView)
    {
        m_occView->finishCurrentSlab();
    }
}

void SurfaceCreationDialog::onClearContourClicked()
{
    if (m_occView)
    {
        m_occView->resetCurrentSlabContour();
    }
    m_currentSlabNodes.clear();
    m_editSlabNodes->clear();
    m_lblSlabStatus->setText(tr("Contour effacé"));
}

void SurfaceCreationDialog::onAddClicked()
{
    if (!m_model) return;

    if (currentSurfaceType() == SurfaceType::Slab)
    {
        std::vector<int> nodeIds;
        QString text = m_editSlabNodes->text().trimmed();

        if (text.isEmpty() && m_currentSlabNodes.size() >= 3)
        {
            nodeIds = m_currentSlabNodes;
        }
        else
        {
            std::string s = text.toStdString();
            for (char& c : s)
            {
                if (c == ',' || c == ';') c = ' ';
            }
            std::istringstream iss(s);
            int nid = 0;
            while (iss >> nid)
            {
                if (!m_model->getNode(nid))
                {
                    QMessageBox::warning(this, tr("Nœud introuvable"), tr("Le nœud N%1 n'existe pas dans le modèle.").arg(nid));
                    return;
                }
                nodeIds.push_back(nid);
            }
        }

        if (nodeIds.size() < 3)
        {
            QMessageBox::warning(this, tr("Contour incomplet"), tr("Une dalle surfacique nécessite au moins 3 nœuds pour former un polygone fermé."));
            return;
        }

        m_model->pushUndoState(tr("Création Dalle").toStdString());
        TSA::Model::SlabType st = (m_comboSlabType->currentIndex() == 0) ? TSA::Model::SlabType::TwoWay : TSA::Model::SlabType::OneWay;
        int slabId = m_model->addSlab(nodeIds, m_spinSlabThickness->value(), m_editName->text().toStdString(), st);

        if (auto* s = m_model->getSlab(slabId))
        {
            s->setMaterial(TSA::Model::Material::findByName(m_comboSlabMaterial->currentText().toStdString()));
            if (m_slabColor.isValid()) s->setColor(m_slabColor.name().toStdString());
            if (m_occView) m_occView->updateSlabShape(slabId);
        }

        onSlabCreated(slabId);
    }
    else // Wall
    {
        QString origText = m_editWallOrigin->text().trimmed();
        QString endText = m_editWallEnd->text().trimmed();

        if (origText.isEmpty() || endText.isEmpty())
        {
            QMessageBox::warning(this, tr("Saisie incomplète"), tr("Veuillez renseigner le nœud/point d'origine et d'extrémité du voile."));
            return;
        }

        int startId = resolveOrCreateNode(origText);
        int endId = resolveOrCreateNode(endText);

        if (startId <= 0 || endId <= 0 || startId == endId)
        {
            QMessageBox::warning(this, tr("Nœuds invalides"), tr("L'origine et l'extrémité doivent être deux nœuds distincts valides."));
            return;
        }

        m_model->pushUndoState(tr("Création Voile").toStdString());
        int wallId = m_model->addWall(startId, endId, m_spinWallHeight->value(), m_spinWallThickness->value(), m_editName->text().toStdString());

        if (auto* w = m_model->getWall(wallId))
        {
            w->setOffset(m_spinWallOffset->value());
            w->setMaterial(TSA::Model::Material::findByName(m_comboWallMaterial->currentText().toStdString()));
            if (m_wallColor.isValid()) w->setColor(m_wallColor.name().toStdString());
            if (m_occView) m_occView->updateWallShape(wallId);
        }

        onWallCreated(wallId);
    }
}

int SurfaceCreationDialog::resolveOrCreateNode(const QString& text)
{
    if (!m_model) return -1;

    bool isInt = false;
    int id = text.toInt(&isInt);
    if (isInt)
    {
        if (m_model->getNode(id)) return id;
    }

    double x = 0.0, y = 0.0, z = 0.0;
    if (parseCoordinates(text, x, y, z))
    {
        return m_model->addNode(x, y, z);
    }

    return -1;
}

bool SurfaceCreationDialog::parseCoordinates(const QString& text, double& x, double& y, double& z) const
{
    QString clean = text;
    clean.replace(',', ' ').replace(';', ' ');
    QStringList parts = clean.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 3)
    {
        bool okX = false, okY = false, okZ = false;
        x = parts[0].toDouble(&okX);
        y = parts[1].toDouble(&okY);
        z = parts[2].toDouble(&okZ);
        return okX && okY && okZ;
    }
    return false;
}

QString SurfaceCreationDialog::formatPoint(const gp_Pnt& pt) const
{
    return QString("%1; %2; %3")
        .arg(pt.X(), 0, 'f', 2)
        .arg(pt.Y(), 0, 'f', 2)
        .arg(pt.Z(), 0, 'f', 2);
}

void SurfaceCreationDialog::updateColorButton(QPushButton* btn, const QColor& col)
{
    if (!btn || !col.isValid()) return;
    btn->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold; border: 1px solid #A0A0A0; border-radius: 3px;")
        .arg(col.name())
        .arg((col.lightness() > 130) ? "#000000" : "#FFFFFF"));
}

void SurfaceCreationDialog::onColorPickClicked()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    QColor& targetColor = (btn == m_btnWallColor) ? m_wallColor : m_slabColor;

    QColor chosen = QColorDialog::getColor(targetColor, this, tr("Sélectionner la couleur de l'élément"));
    if (chosen.isValid())
    {
        targetColor = chosen;
        updateColorButton(btn, chosen);
        syncPresetsToOccView();
    }
}

void SurfaceCreationDialog::onWallOriginReturnPressed()
{
    if (m_editWallEnd) m_editWallEnd->setFocus();
}

void SurfaceCreationDialog::onWallEndReturnPressed()
{
    onAddClicked();
}

void SurfaceCreationDialog::onHelpClicked()
{
    QMessageBox::information(this, tr("Aide - Outil Éléments Surfaciques"),
        tr("<b>Outil de création d'Éléments Surfaciques (Dalles & Voiles) :</b><br><br>"
           "Cette interface unifiée regroupe l'ensemble des éléments 2D surfaciques.<br><br>"
           "<b>1. Tracé d'une Dalle / Plancher :</b><br>"
           "• Sélectionnez <i>Dalle / Plancher</i> dans la liste supérieure.<br>"
           "• Définissez l'épaisseur, le type et le matériau.<br>"
           "• Cliquez dans la vue 3D sur les nœuds ou intersections du contour polygonal.<br>"
           "• Pour fermer et créer la dalle : recliquez sur le 1er nœud, faites un <b>clic droit</b>, ou appuyez sur <b>Fermer le contour</b>.<br>"
           "• Vous pouvez également saisir directement la liste des nœuds (ex: <i>1, 2, 4, 3</i>) puis cliquer sur <b>Ajouter</b>.<br><br>"
           "<b>2. Tracé d'un Voile / Mur porteur :</b><br>"
           "• Sélectionnez <i>Voile / Mur porteur</i>.<br>"
           "• Définissez l'épaisseur, la hauteur H et l'excentrement.<br>"
           "• Cliquez sur le 1er point de base, puis sur le second dans la vue 3D.<br>"
           "• Si <b>Étirer</b> est coché, le tracé s'enchaîne pour le voile suivant.<br>"
           "• Ou entrez les coordonnées / n° de nœuds d'origine et d'extrémité puis cliquez <b>Ajouter</b>."));
}

void SurfaceCreationDialog::closeEvent(QCloseEvent* event)
{
    if (m_occView && (m_occView->interactionMode() == OccView::InteractionMode::DrawSlab ||
                      m_occView->interactionMode() == OccView::InteractionMode::DrawWall))
    {
        m_occView->setInteractionMode(OccView::InteractionMode::Select);
    }
    QDialog::closeEvent(event);
}

void SurfaceCreationDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        if (m_occView)
        {
            m_occView->cancelCurrentDrawing();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (currentSurfaceType() == SurfaceType::Slab && m_currentSlabNodes.size() >= 3)
        {
            onCloseContourClicked();
            event->accept();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

} // namespace TSA::UI
