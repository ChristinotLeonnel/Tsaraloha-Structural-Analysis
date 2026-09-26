#include "BarCreationDialog.h"
#include "SectionCustomizationDialog.h"
#include "../../Library/LibraryManager.h"
#include "../../Model/Model.h"
#include "../../Viewer/OccView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QIcon>
#include <QKeyEvent>
#include <cmath>
#include <sstream>

namespace TSA::UI
{

BarCreationDialog::BarCreationDialog(TSA::Model::Model* model, OccView* occView, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_occView(occView)
{
    setWindowTitle(tr("Éléments Filaires (Barres, Poutres, Poteaux)"));
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_QuitOnClose, false);
    setFixedWidth(360);

    setupUi();
    populateSections();
    updateHighlight(false);

    if (m_model)
    {
        m_spinBarId->setValue(m_model->nextBeamId());
        m_editName->setText(QString("Barre_%1").arg(m_model->nextBeamId()));
    }
}

void BarCreationDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 1. Barre n° et Pas
    auto* numLayout = new QHBoxLayout();
    numLayout->setSpacing(6);

    auto* lblBarId = new QLabel(tr("Barre n° :"), this);
    m_spinBarId = new QSpinBox(this);
    m_spinBarId->setRange(1, 999999);
    m_spinBarId->setValue(1);

    auto* lblStep = new QLabel(tr("Pas :"), this);
    lblStep->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_spinStep = new QSpinBox(this);
    m_spinStep->setRange(1, 100);
    m_spinStep->setValue(1);

    numLayout->addWidget(lblBarId);
    numLayout->addWidget(m_spinBarId, 1);
    numLayout->addWidget(lblStep);
    numLayout->addWidget(m_spinStep, 1);
    mainLayout->addLayout(numLayout);

    // 2. Nom
    auto* nameLayout = new QHBoxLayout();
    nameLayout->setSpacing(6);
    auto* lblName = new QLabel(tr("Nom :"), this);
    lblName->setFixedWidth(50);
    m_editName = new QLineEdit(this);
    m_editName->setText("Barre_1");
    m_btnNameMore = new QToolButton(this);
    m_btnNameMore->setText("...");
    nameLayout->addWidget(lblName);
    nameLayout->addWidget(m_editName, 1);
    nameLayout->addWidget(m_btnNameMore);
    mainLayout->addLayout(nameLayout);

    // 3. Groupe "Caractéristiques"
    auto* grpProps = new QGroupBox(tr("Caractéristiques"), this);
    auto* propsLayout = new QGridLayout(grpProps);
    propsLayout->setContentsMargins(8, 12, 8, 8);
    propsLayout->setSpacing(6);

    // Type (Role)
    propsLayout->addWidget(new QLabel(tr("Type :"), grpProps), 0, 0);
    m_comboRole = new QComboBox(grpProps);
    m_comboRole->addItem(tr("Barre"), static_cast<int>(TSA::Model::BarRole::Generic));
    m_comboRole->addItem(tr("Poutre"), static_cast<int>(TSA::Model::BarRole::Beam));
    m_comboRole->addItem(tr("Poteau"), static_cast<int>(TSA::Model::BarRole::Column));
    m_comboRole->addItem(tr("Diagonale / Contreventement"), static_cast<int>(TSA::Model::BarRole::Brace));
    m_comboRole->addItem(tr("Tirant"), static_cast<int>(TSA::Model::BarRole::Tie));
    m_comboRole->addItem(tr("Barre acier"), static_cast<int>(TSA::Model::BarRole::SteelMember));
    m_comboRole->addItem(tr("Treillis"), static_cast<int>(TSA::Model::BarRole::Truss));
    m_comboRole->setCurrentIndex(1); // Poutre par défaut
    propsLayout->addWidget(m_comboRole, 0, 1);

    m_btnRoleMore = new QToolButton(grpProps);
    m_btnRoleMore->setIcon(QIcon(":/icons/settings.svg"));
    m_btnRoleMore->setToolTip(tr("Options du rôle..."));
    propsLayout->addWidget(m_btnRoleMore, 0, 2);

    // Section
    propsLayout->addWidget(new QLabel(tr("Section :"), grpProps), 1, 0);
    m_comboSection = new QComboBox(grpProps);
    propsLayout->addWidget(m_comboSection, 1, 1);

    m_btnSectionMore = new QToolButton(grpProps);
    m_btnSectionMore->setIcon(QIcon(":/icons/section_i.svg"));
    m_btnSectionMore->setToolTip(tr("Modifier / Personnaliser la section..."));
    propsLayout->addWidget(m_btnSectionMore, 1, 2);

    m_btnCustomizeSection = new QPushButton(tr("Modifier / Personnaliser la section..."), grpProps);
    m_btnCustomizeSection->setIcon(QIcon(":/icons/edit.svg"));
    m_btnCustomizeSection->setStyleSheet("font-size: 8.5pt; font-weight: bold; background-color: #1E70BF; color: white; padding: 4px 8px; border-radius: 3px;");
    propsLayout->addWidget(m_btnCustomizeSection, 2, 1, 1, 2);

    // Matériau par défaut
    propsLayout->addWidget(new QLabel(tr("Matériau par défaut :"), grpProps), 3, 0);
    m_editMaterial = new QLineEdit(grpProps);
    m_editMaterial->setReadOnly(true);
    m_editMaterial->setText("ACIER S235");
    m_editMaterial->setStyleSheet("background-color: #F0F0F0; color: #404040;");
    propsLayout->addWidget(m_editMaterial, 3, 1, 1, 2);

    mainLayout->addWidget(grpProps);

    // 4. Groupe "Coordonnées des noeuds (m)"
    auto* grpCoords = new QGroupBox(tr("Coordonnées des noeuds (m)"), this);
    auto* coordsLayout = new QGridLayout(grpCoords);
    coordsLayout->setContentsMargins(8, 12, 8, 8);
    coordsLayout->setSpacing(6);

    coordsLayout->addWidget(new QLabel(tr("Origine :"), grpCoords), 0, 0);
    m_editOrigin = new QLineEdit(grpCoords);
    m_editOrigin->setPlaceholderText("X; Y; Z");
    coordsLayout->addWidget(m_editOrigin, 0, 1);

    coordsLayout->addWidget(new QLabel(tr("Extrémité :"), grpCoords), 1, 0);
    m_editEnd = new QLineEdit(grpCoords);
    m_editEnd->setPlaceholderText("X; Y; Z");
    coordsLayout->addWidget(m_editEnd, 1, 1);

    m_chkChain = new QCheckBox(tr("Étirer"), grpCoords);
    m_chkChain->setChecked(true); // Comportement Robot: tracé continu
    coordsLayout->addWidget(m_chkChain, 2, 1);

    mainLayout->addWidget(grpCoords);

    // 5. Groupe "Position de l'axe"
    auto* grpAxis = new QGroupBox(tr("Position de l'axe"), this);
    auto* axisLayout = new QGridLayout(grpAxis);
    axisLayout->setContentsMargins(8, 12, 8, 8);
    axisLayout->setSpacing(6);

    axisLayout->addWidget(new QLabel(tr("Excentrement :"), grpAxis), 0, 0);
    m_comboEccentricity = new QComboBox(grpAxis);
    m_comboEccentricity->addItem(tr("inexistant"), static_cast<int>(TSA::Model::BarEccentricity::None));
    m_comboEccentricity->addItem(tr("Fibre supérieure"), static_cast<int>(TSA::Model::BarEccentricity::TopFlange));
    m_comboEccentricity->addItem(tr("Fibre inférieure"), static_cast<int>(TSA::Model::BarEccentricity::BottomFlange));
    m_comboEccentricity->addItem(tr("Fibre gauche"), static_cast<int>(TSA::Model::BarEccentricity::LeftFlange));
    m_comboEccentricity->addItem(tr("Fibre droite"), static_cast<int>(TSA::Model::BarEccentricity::RightFlange));
    axisLayout->addWidget(m_comboEccentricity, 0, 1);

    m_btnEccMore = new QToolButton(grpAxis);
    m_btnEccMore->setIcon(QIcon(":/icons/settings.svg"));
    m_btnEccMore->setToolTip(tr("Options d'excentrement..."));
    axisLayout->addWidget(m_btnEccMore, 0, 2);

    axisLayout->addWidget(new QLabel(tr("Rotation γ :"), grpAxis), 1, 0);
    m_spinRotation = new QDoubleSpinBox(grpAxis);
    m_spinRotation->setRange(0.0, 360.0);
    m_spinRotation->setSingleStep(15.0);
    m_spinRotation->setSuffix(" °");
    axisLayout->addWidget(m_spinRotation, 1, 1, 1, 2);

    mainLayout->addWidget(grpAxis);

    // 6. Boutons inférieurs
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnAdd = new QPushButton(tr("Ajouter"), this);
    m_btnAdd->setIcon(QIcon(":/icons/beam_add.svg"));
    m_btnAdd->setStyleSheet("font-weight: bold; background-color: #2E7D32; color: white; padding: 5px 12px; border-radius: 3px;");
    m_btnClose = new QPushButton(tr("Fermer"), this);
    m_btnClose->setIcon(QIcon(":/icons/cancel.svg"));
    m_btnHelp = new QPushButton(tr("Aide"), this);
    m_btnHelp->setIcon(QIcon(":/icons/TSA.svg"));

    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnClose);
    btnLayout->addWidget(m_btnHelp);
    mainLayout->addLayout(btnLayout);

    // Connexions
    connect(m_comboRole, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BarCreationDialog::onRoleChanged);
    connect(m_comboSection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BarCreationDialog::onSectionChanged);
    connect(m_spinRotation, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BarCreationDialog::onRotationChanged);
    connect(m_comboEccentricity, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BarCreationDialog::onEccentricityChanged);
    connect(m_editOrigin, &QLineEdit::returnPressed, this, &BarCreationDialog::onOriginReturnPressed);
    connect(m_editEnd, &QLineEdit::returnPressed, this, &BarCreationDialog::onEndReturnPressed);
    connect(m_btnAdd, &QPushButton::clicked, this, &BarCreationDialog::onAddClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::close);
    connect(m_btnHelp, &QPushButton::clicked, this, &BarCreationDialog::onHelpClicked);
    connect(m_btnSectionMore, &QPushButton::clicked, this, &BarCreationDialog::onCustomSectionRequested);
    if (m_btnCustomizeSection)
    {
        connect(m_btnCustomizeSection, &QPushButton::clicked, this, &BarCreationDialog::onCustomSectionRequested);
    }
}

void BarCreationDialog::populateSections()
{
    m_sectionLibrary = TSA::Model::Section::defaultLibrary();
    m_comboSection->clear();
    for (size_t i = 0; i < m_sectionLibrary.size(); ++i)
    {
        m_comboSection->addItem(QString::fromStdString(m_sectionLibrary[i].name), static_cast<int>(i));
    }

    // Sélectionner IPE 100 par défaut (ou Rect 300x500)
    int idx = m_comboSection->findText("IPE 100");
    if (idx >= 0) m_comboSection->setCurrentIndex(idx);
    else if (!m_sectionLibrary.empty()) m_comboSection->setCurrentIndex(0);
}

void BarCreationDialog::updateHighlight(bool waitingForSecondPoint)
{
    if (!waitingForSecondPoint)
    {
        // Origine en vert vif (#00FF00), Extrémité en blanc
        m_editOrigin->setStyleSheet("background-color: #00FF00; color: #000000; font-weight: bold; border: 1px solid #009900;");
        m_editEnd->setStyleSheet("background-color: #FFFFFF; color: #000000; border: 1px solid #CCCCCC;");
        m_editOrigin->setFocus();
    }
    else
    {
        // Origine validée (vert clair / pastel), Extrémité en vert vif (#00FF00)
        m_editOrigin->setStyleSheet("background-color: #E8F5E9; color: #000000; border: 1px solid #81C784;");
        m_editEnd->setStyleSheet("background-color: #00FF00; color: #000000; font-weight: bold; border: 1px solid #009900;");
        m_editEnd->setFocus();
    }
}

TSA::Model::BarProperties BarCreationDialog::currentProperties() const
{
    TSA::Model::BarProperties p;
    p.id = m_spinBarId->value();
    p.name = m_editName->text().toStdString();
    p.role = static_cast<TSA::Model::BarRole>(m_comboRole->currentData().toInt());

    int secIdx = m_comboSection->currentData().toInt();
    if (secIdx >= 0 && secIdx < static_cast<int>(m_sectionLibrary.size()))
    {
        p.section = m_sectionLibrary[secIdx];
    }
    else
    {
        p.section = TSA::Model::Section::ipe(100);
    }

    // Matériau selon section ou type
    if (p.section.shape == TSA::Model::SectionShape::IShape ||
        p.section.shape == TSA::Model::SectionShape::UPN ||
        p.section.shape == TSA::Model::SectionShape::Angle ||
        p.section.shape == TSA::Model::SectionShape::BoxHollow ||
        p.role == TSA::Model::BarRole::SteelMember)
    {
        p.material = TSA::Model::Material::steelS235();
    }
    else
    {
        p.material = TSA::Model::Material::concreteC25_30();
    }

    p.rotation = m_spinRotation->value();
    p.eccentricity = static_cast<TSA::Model::BarEccentricity>(m_comboEccentricity->currentData().toInt());
    return p;
}

void BarCreationDialog::setRole(TSA::Model::BarRole role)
{
    int idx = m_comboRole->findData(static_cast<int>(role));
    if (idx >= 0) m_comboRole->setCurrentIndex(idx);
}

void BarCreationDialog::setSection(const TSA::Model::Section& section)
{
    // Chercher par nom
    int idx = m_comboSection->findText(QString::fromStdString(section.name));
    if (idx >= 0)
    {
        m_comboSection->setCurrentIndex(idx);
    }
    else
    {
        m_sectionLibrary.push_back(section);
        m_comboSection->addItem(QString::fromStdString(section.name), static_cast<int>(m_sectionLibrary.size() - 1));
        m_comboSection->setCurrentIndex(m_comboSection->count() - 1);
    }
    emitPropertiesChanged();
}

void BarCreationDialog::setRotation(double gammaDegrees)
{
    m_spinRotation->setValue(gammaDegrees);
    emitPropertiesChanged();
}

bool BarCreationDialog::isChainMode() const
{
    return m_chkChain->isChecked();
}

void BarCreationDialog::loadFromBar(const TSA::Model::Bar& bar)
{
    m_spinBarId->setValue(bar.id());
    m_editName->setText(QString::fromStdString(bar.formattedName()));
    setRole(bar.role());
    setSection(bar.section());
    m_spinRotation->setValue(bar.rotation());
    int eccIdx = m_comboEccentricity->findData(static_cast<int>(bar.eccentricity()));
    if (eccIdx >= 0) m_comboEccentricity->setCurrentIndex(eccIdx);

    if (m_model)
    {
        const auto* nA = m_model->getNode(bar.startNodeId());
        const auto* nB = m_model->getNode(bar.endNodeId());
        if (nA) m_editOrigin->setText(formatPoint(gp_Pnt(nA->x(), nA->y(), nA->z())));
        if (nB) m_editEnd->setText(formatPoint(gp_Pnt(nB->x(), nB->y(), nB->z())));
    }
}

void BarCreationDialog::onFirstPointPicked(const gp_Pnt& pt, int nodeId)
{
    m_originPt = pt;
    m_originNodeId = nodeId;
    m_hasOrigin = true;
    m_editOrigin->setText(formatPoint(pt));
    m_editEnd->clear();
    updateHighlight(true); // Passer l'extrémité en vert vif
}

void BarCreationDialog::onSecondPointPicked(const gp_Pnt& pt, int nodeId)
{
    if (!m_hasOrigin) return;

    m_endPt = pt;
    m_endNodeId = nodeId;
    m_editEnd->setText(formatPoint(pt));

    if (m_originPt.Distance(m_endPt) < 1e-4)
    {
        return; // Éviter barre de longueur nulle
    }

    if (!m_model) return;

    // Créer ou récupérer les nœuds dans le modèle structural
    int nStartId = m_originNodeId;
    if (nStartId <= 0)
    {
        nStartId = m_model->addNode(m_originPt.X(), m_originPt.Y(), m_originPt.Z());
    }

    int nEndId = m_endNodeId;
    if (nEndId <= 0)
    {
        nEndId = m_model->addNode(m_endPt.X(), m_endPt.Y(), m_endPt.Z());
    }

    if (nStartId == nEndId) return;

    auto props = currentProperties();
    m_model->pushUndoState(tr("Création %1").arg(QString::fromStdString(props.name)).toStdString());

    int barId = m_model->addBar(props, nStartId, nEndId);
    emit barCreated(barId);

    // Incrémenter la numérotation
    int nextId = m_spinBarId->value() + m_spinStep->value();
    m_spinBarId->setValue(nextId);
    m_editName->setText(QString("Barre_%1").arg(nextId));

    if (m_chkChain->isChecked())
    {
        // Enchaînement continu ("Étirer") : le nœud d'arrivée devient le nouveau départ
        m_originPt = m_endPt;
        m_originNodeId = nEndId;
        m_hasOrigin = true;
        m_editOrigin->setText(formatPoint(m_originPt));
        m_editEnd->clear();
        updateHighlight(true);
        if (m_occView)
        {
            m_occView->startChainedBarDrawing(m_originPt, m_originNodeId);
        }
    }
    else
    {
        // Tracé discontinu : réinitialiser pour la prochaine barre
        m_hasOrigin = false;
        m_originNodeId = -1;
        m_editOrigin->clear();
        m_editEnd->clear();
        updateHighlight(false);
        if (m_occView)
        {
            m_occView->cancelCurrentDrawing();
        }
    }
}

void BarCreationDialog::onDrawingCancelled()
{
    m_hasOrigin = false;
    m_originNodeId = -1;
    m_editOrigin->clear();
    m_editEnd->clear();
    updateHighlight(false);
}

void BarCreationDialog::onRoleChanged(int)
{
    auto role = static_cast<TSA::Model::BarRole>(m_comboRole->currentData().toInt());
    if (role == TSA::Model::BarRole::SteelMember)
    {
        m_editMaterial->setText("ACIER S235");
        int idx = m_comboSection->findText("IPE 100");
        if (idx >= 0) m_comboSection->setCurrentIndex(idx);
    }
    else if (role == TSA::Model::BarRole::Column || role == TSA::Model::BarRole::Beam)
    {
        auto props = currentProperties();
        if (props.section.shape == TSA::Model::SectionShape::IShape ||
            props.section.shape == TSA::Model::SectionShape::UPN ||
            props.section.shape == TSA::Model::SectionShape::Angle)
        {
            m_editMaterial->setText("ACIER S235");
        }
        else
        {
            m_editMaterial->setText("BÉTON C25/30");
        }
    }
    emitPropertiesChanged();
}

void BarCreationDialog::onSectionChanged(int)
{
    auto props = currentProperties();
    if (props.section.shape == TSA::Model::SectionShape::IShape ||
        props.section.shape == TSA::Model::SectionShape::UPN ||
        props.section.shape == TSA::Model::SectionShape::Angle ||
        props.section.shape == TSA::Model::SectionShape::BoxHollow ||
        props.section.shape == TSA::Model::SectionShape::Pipe)
    {
        m_editMaterial->setText("ACIER S235");
    }
    else
    {
        m_editMaterial->setText("BÉTON C25/30");
    }
    emitPropertiesChanged();
}

void BarCreationDialog::onRotationChanged(double)
{
    emitPropertiesChanged();
}

void BarCreationDialog::onEccentricityChanged(int)
{
    emitPropertiesChanged();
}

void BarCreationDialog::onOriginReturnPressed()
{
    double x, y, z;
    if (parseCoordinates(m_editOrigin->text(), x, y, z))
    {
        m_originPt = gp_Pnt(x, y, z);
        m_originNodeId = -1;
        m_hasOrigin = true;
        m_editOrigin->setText(formatPoint(m_originPt));
        updateHighlight(true);
        emit manualPointEntered(m_originPt, true);
    }
    else
    {
        QMessageBox::warning(this, tr("Format de coordonnées"),
            tr("Veuillez saisir les coordonnées sous la forme : X; Y; Z\nExemple : -10.00; 0.00; 0.00"));
    }
}

void BarCreationDialog::onEndReturnPressed()
{
    onAddClicked();
}

void BarCreationDialog::onAddClicked()
{
    double x1, y1, z1, x2, y2, z2;
    if (!m_hasOrigin)
    {
        if (!parseCoordinates(m_editOrigin->text(), x1, y1, z1))
        {
            QMessageBox::warning(this, tr("Coordonnées"), tr("Veuillez renseigner les coordonnées d'Origine."));
            m_editOrigin->setFocus();
            return;
        }
        m_originPt = gp_Pnt(x1, y1, z1);
        m_hasOrigin = true;
    }

    if (!parseCoordinates(m_editEnd->text(), x2, y2, z2))
    {
        QMessageBox::warning(this, tr("Coordonnées"), tr("Veuillez renseigner les coordonnées d'Extrémité."));
        m_editEnd->setFocus();
        return;
    }

    m_endPt = gp_Pnt(x2, y2, z2);
    onSecondPointPicked(m_endPt, -1);
}

void BarCreationDialog::onCustomSectionRequested()
{
    TSA::Model::Section initialSec;
    int secIdx = m_comboSection ? m_comboSection->currentData().toInt() : -1;
    if (secIdx >= 0 && secIdx < static_cast<int>(m_sectionLibrary.size()))
    {
        initialSec = m_sectionLibrary[secIdx];
    }
    else
    {
        initialSec = TSA::Model::Section::rectangular(0.40, 0.40);
    }

    SectionCustomizationDialog dlg(initialSec, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        auto newSec = dlg.customizedSection();
        setSection(newSec);
        TSA::Library::LibraryManager::instance().addCustomSection(newSec);
    }
}

void BarCreationDialog::onHelpClicked()
{
    QMessageBox::information(this, tr("Aide - Outil Barres"),
        tr("<b>Outil de création de Barres (type Robot Structural Analysis) :</b><br><br>"
           "1. Choisissez le <b>Type</b> (Poutre, Poteau, Diagonale, etc.), la <b>Section</b> et l'<b>Angle γ</b>.<br>"
           "2. Cliquez sur le premier point dans la vue 3D (ou tapez ses coordonnées dans <i>Origine</i>). Le champ <i>Extrémité</i> s'illumine en vert.<br>"
           "3. Déplacez la souris : la barre 3D solide s'affiche en prévisualisation dynamique.<br>"
           "4. Cliquez sur le second point : la barre est créée avec sa section réelle extrudée.<br>"
           "5. Si <b>Étirer</b> est coché, le tracé s'enchaîne automatiquement pour la barre suivante.<br>"
           "Appuyez sur <b>Échap</b> ou <b>Fermer</b> pour quitter."));
}

bool BarCreationDialog::parseCoordinates(const QString& text, double& x, double& y, double& z) const
{
    QString s = text.trimmed();
    if (s.isEmpty()) return false;

    // Remplacer virgules séparatrices ou point-virgules par des espaces
    // Attention : si la virgule est utilisée comme séparateur décimal (ex: -10,00; 0,00; 0,00)
    if (s.contains(';'))
    {
        QStringList parts = s.split(';', Qt::SkipEmptyParts);
        if (parts.size() >= 3)
        {
            bool ok1 = false, ok2 = false, ok3 = false;
            QString s1 = parts[0].trimmed().replace(',', '.');
            QString s2 = parts[1].trimmed().replace(',', '.');
            QString s3 = parts[2].trimmed().replace(',', '.');
            x = s1.toDouble(&ok1);
            y = s2.toDouble(&ok2);
            z = s3.toDouble(&ok3);
            return ok1 && ok2 && ok3;
        }
    }

    // Format avec espaces
    std::string str = s.toStdString();
    for (char& c : str) if (c == ',' || c == ';') c = ' ';
    std::istringstream iss(str);
    if (iss >> x >> y >> z) return true;

    return false;
}

QString BarCreationDialog::formatPoint(const gp_Pnt& pt) const
{
    return QString("%1; %2; %3")
        .arg(pt.X(), 0, 'f', 2)
        .arg(pt.Y(), 0, 'f', 2)
        .arg(pt.Z(), 0, 'f', 2);
}

void BarCreationDialog::emitPropertiesChanged()
{
    emit barPropertiesChanged(currentProperties());
}

void BarCreationDialog::closeEvent(QCloseEvent* event)
{
    if (m_occView)
    {
        m_occView->cancelCurrentDrawing();
        m_occView->setInteractionMode(OccView::InteractionMode::Select);
    }
    QDialog::closeEvent(event);
}

void BarCreationDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        close();
        return;
    }
    QDialog::keyPressEvent(event);
}

} // namespace TSA::UI
