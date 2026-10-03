#include "NodePropertiesView.h"
#include "../../UndoRedo/UndoManager.h"
#include <QMessageBox>
#include "../../Model/Model.h"
#include "../../Model/Node.h"
#include "../../Analysis/ResultsModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>

namespace TSA::UI
{

NodePropertiesView::NodePropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void NodePropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void NodePropertiesView::setElementId(int id)
{
    m_nodeId = id;
    refreshView();
}

void NodePropertiesView::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_resultsModel = results;
    refreshView();
}

void NodePropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // --- 1. Groupe Géométrie & Identité ---
    auto* grpGeom = new QGroupBox(tr("Géométrie du Nœud"), this);
    auto* formGeom = new QFormLayout(grpGeom);
    formGeom->setContentsMargins(8, 8, 8, 8);
    formGeom->setSpacing(6);

    m_editName = new QLineEdit(grpGeom);
    connect(m_editName, &QLineEdit::editingFinished, this, &NodePropertiesView::onWidgetChanged);
    formGeom->addRow(tr("Nom :"), m_editName);

    m_spinX = new QDoubleSpinBox(grpGeom);
    m_spinX->setRange(-100000.0, 100000.0);
    m_spinX->setDecimals(3);
    m_spinX->setSuffix(" m");
    connect(m_spinX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    formGeom->addRow(tr("Coordonnée X :"), m_spinX);

    m_spinY = new QDoubleSpinBox(grpGeom);
    m_spinY->setRange(-100000.0, 100000.0);
    m_spinY->setDecimals(3);
    m_spinY->setSuffix(" m");
    connect(m_spinY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    formGeom->addRow(tr("Coordonnée Y :"), m_spinY);

    m_spinZ = new QDoubleSpinBox(grpGeom);
    m_spinZ->setRange(-100000.0, 100000.0);
    m_spinZ->setDecimals(3);
    m_spinZ->setSuffix(" m");
    connect(m_spinZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    formGeom->addRow(tr("Coordonnée Z :"), m_spinZ);

    m_btnColor = new QPushButton(grpGeom);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &NodePropertiesView::pickColor);
    formGeom->addRow(tr("Couleur :"), m_btnColor);

    mainLayout->addWidget(grpGeom);

    // --- 2. Groupe Conditions d'Appui & DDLs ---
    auto* grpSupp = new QGroupBox(tr("Conditions d'Appui (6 DDL)"), this);
    auto* suppLayout = new QVBoxLayout(grpSupp);
    suppLayout->setContentsMargins(8, 8, 8, 8);
    suppLayout->setSpacing(6);

    // Preset ComboBox
    auto* hPreset = new QHBoxLayout();
    hPreset->addWidget(new QLabel(tr("Type prédéfini :"), grpSupp));
    m_comboPreset = new QComboBox(grpSupp);
    m_comboPreset->addItem(tr("Libre (Aucun appui)"), static_cast<int>(TSA::Model::SupportType::Free));
    m_comboPreset->addItem(tr("Encastrement total (6 DDLs)"), static_cast<int>(TSA::Model::SupportType::Fixed));
    m_comboPreset->addItem(tr("Articulation (Rotule 3D)"), static_cast<int>(TSA::Model::SupportType::Pinned));
    m_comboPreset->addItem(tr("Appui simple (Rouleau Tz)"), static_cast<int>(TSA::Model::SupportType::Roller));
    m_comboPreset->addItem(tr("Appui glissant (Sliding)"), static_cast<int>(TSA::Model::SupportType::Sliding));
    m_comboPreset->addItem(tr("Appui linéaire guidé"), static_cast<int>(TSA::Model::SupportType::Linear));
    m_comboPreset->addItem(tr("Appui plan"), static_cast<int>(TSA::Model::SupportType::Planar));
    m_comboPreset->addItem(tr("Appui élastique (Ressorts)"), static_cast<int>(TSA::Model::SupportType::Elastic));
    m_comboPreset->addItem(tr("Personnalisé..."), static_cast<int>(TSA::Model::SupportType::Custom));
    connect(m_comboPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NodePropertiesView::onPresetChanged);
    hPreset->addWidget(m_comboPreset, 1);
    suppLayout->addLayout(hPreset);

    // Grille des 6 DDLs
    auto* gridDof = new QGridLayout();
    gridDof->setSpacing(4);

    gridDof->addWidget(new QLabel(tr("<b>DDL</b>"), grpSupp), 0, 0);
    gridDof->addWidget(new QLabel(tr("<b>État</b>"), grpSupp), 0, 1);
    gridDof->addWidget(new QLabel(tr("<b>Raideur K</b>"), grpSupp), 0, 2);

    auto setupDofRow = [&](int row, const QString& label, QComboBox*& combo, QDoubleSpinBox*& spin, const QString& unit) {
        gridDof->addWidget(new QLabel(label, grpSupp), row, 0);

        combo = new QComboBox(grpSupp);
        combo->addItem(tr("Libre"), static_cast<int>(TSA::Model::DOFState::Free));
        combo->addItem(tr("Bloqué"), static_cast<int>(TSA::Model::DOFState::Fixed));
        combo->addItem(tr("Élastique"), static_cast<int>(TSA::Model::DOFState::Spring));
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NodePropertiesView::onDofChanged);
        gridDof->addWidget(combo, row, 1);

        spin = new QDoubleSpinBox(grpSupp);
        spin->setRange(0.0, 1e9);
        spin->setDecimals(1);
        spin->setSuffix(unit);
        spin->setEnabled(false);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
        gridDof->addWidget(spin, row, 2);
    };

    setupDofRow(1, tr("UX (Tx)"), m_comboTx, m_spinKx, " kN/m");
    setupDofRow(2, tr("UY (Ty)"), m_comboTy, m_spinKy, " kN/m");
    setupDofRow(3, tr("UZ (Tz)"), m_comboTz, m_spinKz, " kN/m");
    setupDofRow(4, tr("RX (Rx)"), m_comboRx, m_spinKrx, " kNm/rad");
    setupDofRow(5, tr("RY (Ry)"), m_comboRy, m_spinKry, " kNm/rad");
    setupDofRow(6, tr("RZ (Rz)"), m_comboRz, m_spinKrz, " kNm/rad");

    suppLayout->addLayout(gridDof);
    mainLayout->addWidget(grpSupp);

    // --- 3. Groupe Orientation de l'Appui ---
    auto* grpOrient = new QGroupBox(tr("Orientation de l'Appui"), this);
    auto* formOrient = new QFormLayout(grpOrient);
    formOrient->setContentsMargins(8, 8, 8, 8);
    formOrient->setSpacing(6);

    m_comboOrientation = new QComboBox(grpOrient);
    m_comboOrientation->addItem(tr("Repère Global (X, Y, Z)"), static_cast<int>(TSA::Model::SupportOrientationType::Global));
    m_comboOrientation->addItem(tr("Barre Locale connectée"), static_cast<int>(TSA::Model::SupportOrientationType::LocalBar));
    m_comboOrientation->addItem(tr("Vecteur Normal Personnalisé"), static_cast<int>(TSA::Model::SupportOrientationType::CustomVector));
    connect(m_comboOrientation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NodePropertiesView::onOrientationChanged);
    formOrient->addRow(tr("Repère :"), m_comboOrientation);

    m_customDirWidget = new QWidget(grpOrient);
    auto* hDir = new QHBoxLayout(m_customDirWidget);
    hDir->setContentsMargins(0, 0, 0, 0);
    hDir->setSpacing(4);

    m_spinDirX = new QDoubleSpinBox(m_customDirWidget);
    m_spinDirX->setRange(-1.0, 1.0);
    m_spinDirX->setDecimals(2);
    m_spinDirX->setSingleStep(0.1);
    m_spinDirX->setPrefix("X: ");
    connect(m_spinDirX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    hDir->addWidget(m_spinDirX);

    m_spinDirY = new QDoubleSpinBox(m_customDirWidget);
    m_spinDirY->setRange(-1.0, 1.0);
    m_spinDirY->setDecimals(2);
    m_spinDirY->setSingleStep(0.1);
    m_spinDirY->setPrefix("Y: ");
    connect(m_spinDirY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    hDir->addWidget(m_spinDirY);

    m_spinDirZ = new QDoubleSpinBox(m_customDirWidget);
    m_spinDirZ->setRange(-1.0, 1.0);
    m_spinDirZ->setDecimals(2);
    m_spinDirZ->setSingleStep(0.1);
    m_spinDirZ->setPrefix("Z: ");
    connect(m_spinDirZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    hDir->addWidget(m_spinDirZ);

    formOrient->addRow(tr("Direction :"), m_customDirWidget);
    m_customDirWidget->setVisible(false);

    mainLayout->addWidget(grpOrient);

    // 4. Groupe : Résultats de Calcul OpenSees
    m_groupResults = new QGroupBox(tr("Résultats OpenSees"), this);
    auto* formRes = new QFormLayout(m_groupResults);
    formRes->setContentsMargins(8, 8, 8, 8);
    formRes->setSpacing(6);

    m_labelNodeStatus = new QLabel(m_groupResults);
    m_labelNodeStatus->setStyleSheet("font-weight: bold;");
    formRes->addRow(tr("Connectivité :"), m_labelNodeStatus);

    m_labelDispX = new QLabel(m_groupResults);
    m_labelDispY = new QLabel(m_groupResults);
    m_labelDispZ = new QLabel(m_groupResults);
    m_labelDispRes = new QLabel(m_groupResults);
    m_labelDispRes->setStyleSheet("font-weight: bold; color: #3b82f6;");

    formRes->addRow(tr("UX :"), m_labelDispX);
    formRes->addRow(tr("UY :"), m_labelDispY);
    formRes->addRow(tr("UZ :"), m_labelDispZ);
    formRes->addRow(tr("U_res :"), m_labelDispRes);

    m_labelRotX = new QLabel(m_groupResults);
    m_labelRotY = new QLabel(m_groupResults);
    m_labelRotZ = new QLabel(m_groupResults);
    formRes->addRow(tr("Rot RX :"), m_labelRotX);
    formRes->addRow(tr("Rot RY :"), m_labelRotY);
    formRes->addRow(tr("Rot RZ :"), m_labelRotZ);

    m_labelReactFx = new QLabel(m_groupResults);
    m_labelReactFy = new QLabel(m_groupResults);
    m_labelReactFz = new QLabel(m_groupResults);
    m_labelReactMx = new QLabel(m_groupResults);
    m_labelReactMy = new QLabel(m_groupResults);
    m_labelReactMz = new QLabel(m_groupResults);
    formRes->addRow(tr("Réaction Fx :"), m_labelReactFx);
    formRes->addRow(tr("Réaction Fy :"), m_labelReactFy);
    formRes->addRow(tr("Réaction Fz :"), m_labelReactFz);
    formRes->addRow(tr("Moment Mx :"), m_labelReactMx);
    formRes->addRow(tr("Moment My :"), m_labelReactMy);
    formRes->addRow(tr("Moment Mz :"), m_labelReactMz);

    m_groupResults->setVisible(false);
    mainLayout->addWidget(m_groupResults);

    mainLayout->addStretch();
}

void NodePropertiesView::refreshView()
{
    if (!m_model || m_nodeId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* node = m_model->getNode(m_nodeId);
    if (!node)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(node->name()));
    m_spinX->setValue(node->x());
    m_spinY->setValue(node->y());
    m_spinZ->setValue(node->z());

    m_colorHex = QString::fromStdString(node->color().empty() ? "#2563EB" : node->color());
    m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));

    syncUiFromSupport(node->support());

    // Connectivité
    bool isFree = m_model->isNodeFree(m_nodeId);
    if (isFree)
    {
        m_labelNodeStatus->setText(tr("⚠️ Nœud libre (non connecté)"));
        m_labelNodeStatus->setStyleSheet("font-weight: bold; color: #ef4444;");
    }
    else
    {
        m_labelNodeStatus->setText(tr("✓ Nœud connecté au modèle"));
        m_labelNodeStatus->setStyleSheet("font-weight: bold; color: #10b981;");
    }

    // Résultats OpenSees
    if (m_resultsModel && m_resultsModel->hasResults() && m_resultsModel->hasNodeDisplacement(m_nodeId))
    {
        m_groupResults->setVisible(true);
        const auto& d = m_resultsModel->nodeDisplacement(m_nodeId);
        double ures = std::sqrt(d.ux * d.ux + d.uy * d.uy + d.uz * d.uz);

        m_labelDispX->setText(tr("%1 mm").arg(d.ux * 1000.0, 0, 'f', 3));
        m_labelDispY->setText(tr("%1 mm").arg(d.uy * 1000.0, 0, 'f', 3));
        m_labelDispZ->setText(tr("%1 mm").arg(d.uz * 1000.0, 0, 'f', 3));
        m_labelDispRes->setText(tr("%1 mm").arg(ures * 1000.0, 0, 'f', 3));

        m_labelRotX->setText(tr("%1 mrad").arg(d.rx * 1000.0, 0, 'f', 3));
        m_labelRotY->setText(tr("%1 mrad").arg(d.ry * 1000.0, 0, 'f', 3));
        m_labelRotZ->setText(tr("%1 mrad").arg(d.rz * 1000.0, 0, 'f', 3));

        if (m_resultsModel->hasNodeReaction(m_nodeId))
        {
            const auto& r = m_resultsModel->nodeReaction(m_nodeId);
            m_labelReactFx->setText(tr("%1 kN").arg(r.rx, 0, 'f', 2));
            m_labelReactFy->setText(tr("%1 kN").arg(r.ry, 0, 'f', 2));
            m_labelReactFz->setText(tr("%1 kN").arg(r.rz, 0, 'f', 2));
            m_labelReactMx->setText(tr("%1 kNm").arg(r.mx, 0, 'f', 2));
            m_labelReactMy->setText(tr("%1 kNm").arg(r.my, 0, 'f', 2));
            m_labelReactMz->setText(tr("%1 kNm").arg(r.mz, 0, 'f', 2));
        }
        else
        {
            m_labelReactFx->setText(tr("—"));
            m_labelReactFy->setText(tr("—"));
            m_labelReactFz->setText(tr("—"));
            m_labelReactMx->setText(tr("—"));
            m_labelReactMy->setText(tr("—"));
            m_labelReactMz->setText(tr("—"));
        }
    }
    else
    {
        m_groupResults->setVisible(false);
    }

    m_isLoading = false;
}

void NodePropertiesView::syncUiFromSupport(const TSA::Model::SupportDefinition& supp)
{
    // Preset
    int presetIdx = m_comboPreset->findData(static_cast<int>(supp.supportType()));
    if (presetIdx >= 0)
    {
        m_comboPreset->setCurrentIndex(presetIdx);
    }
    else
    {
        int custIdx = m_comboPreset->findData(static_cast<int>(TSA::Model::SupportType::Custom));
        if (custIdx >= 0) m_comboPreset->setCurrentIndex(custIdx);
    }

    auto setDofCombo = [](QComboBox* combo, TSA::Model::DOFState state) {
        int idx = combo->findData(static_cast<int>(state));
        if (idx >= 0) combo->setCurrentIndex(idx);
    };

    setDofCombo(m_comboTx, supp.tx());
    setDofCombo(m_comboTy, supp.ty());
    setDofCombo(m_comboTz, supp.tz());
    setDofCombo(m_comboRx, supp.rx());
    setDofCombo(m_comboRy, supp.ry());
    setDofCombo(m_comboRz, supp.rz());

    m_spinKx->setValue(supp.kx());
    m_spinKy->setValue(supp.ky());
    m_spinKz->setValue(supp.kz());
    m_spinKrx->setValue(supp.krx());
    m_spinKry->setValue(supp.kry());
    m_spinKrz->setValue(supp.krz());

    int orientIdx = m_comboOrientation->findData(static_cast<int>(supp.orientationType()));
    if (orientIdx >= 0) m_comboOrientation->setCurrentIndex(orientIdx);

    m_spinDirX->setValue(supp.customDirX());
    m_spinDirY->setValue(supp.customDirY());
    m_spinDirZ->setValue(supp.customDirZ());

    updateDofUiState();
}

void NodePropertiesView::updateDofUiState()
{
    m_spinKx->setEnabled(m_comboTx->currentData().toInt() == static_cast<int>(TSA::Model::DOFState::Spring));
    m_spinKy->setEnabled(m_comboTy->currentData().toInt() == static_cast<int>(TSA::Model::DOFState::Spring));
    m_spinKz->setEnabled(m_comboTz->currentData().toInt() == static_cast<int>(TSA::Model::DOFState::Spring));

    m_spinKrx->setEnabled(m_comboRx->currentData().toInt() == static_cast<int>(TSA::Model::DOFState::Spring));
    m_spinKry->setEnabled(m_comboRy->currentData().toInt() == static_cast<int>(TSA::Model::DOFState::Spring));
    m_spinKrz->setEnabled(m_comboRz->currentData().toInt() == static_cast<int>(TSA::Model::DOFState::Spring));

    bool isCustomOrient = (m_comboOrientation->currentData().toInt() == static_cast<int>(TSA::Model::SupportOrientationType::CustomVector));
    m_customDirWidget->setVisible(isCustomOrient);
}

void NodePropertiesView::onPresetChanged(int index)
{
    if (m_isLoading) return;

    auto type = static_cast<TSA::Model::SupportType>(m_comboPreset->itemData(index).toInt());
    if (type == TSA::Model::SupportType::Custom)
    {
        return;
    }

    m_isLoading = true;
    TSA::Model::SupportDefinition def;
    switch (type)
    {
    case TSA::Model::SupportType::Free:
        def = TSA::Model::SupportDefinition::free();
        break;
    case TSA::Model::SupportType::Fixed:
        def = TSA::Model::SupportDefinition::fixed();
        break;
    case TSA::Model::SupportType::Pinned:
        def = TSA::Model::SupportDefinition::pinned();
        break;
    case TSA::Model::SupportType::Roller:
        def = TSA::Model::SupportDefinition::roller();
        break;
    case TSA::Model::SupportType::Sliding:
        def = TSA::Model::SupportDefinition::sliding();
        break;
    case TSA::Model::SupportType::Linear:
        def = TSA::Model::SupportDefinition::linear();
        break;
    case TSA::Model::SupportType::Planar:
        def = TSA::Model::SupportDefinition::planar();
        break;
    case TSA::Model::SupportType::Elastic:
        def = TSA::Model::SupportDefinition::elastic(10000.0, 10000.0, 10000.0);
        break;
    default:
        break;
    }

    syncUiFromSupport(def);
    m_isLoading = false;

    applyChanges();
}

void NodePropertiesView::onDofChanged()
{
    if (m_isLoading) return;
    updateDofUiState();

    // Vérifier si la combinaison actuelle correspond à un preset
    TSA::Model::SupportDefinition currentSupp = buildSupportFromUi();
    int presetIdx = m_comboPreset->findData(static_cast<int>(currentSupp.supportType()));
    m_isLoading = true;
    if (presetIdx >= 0)
    {
        m_comboPreset->setCurrentIndex(presetIdx);
    }
    else
    {
        int custIdx = m_comboPreset->findData(static_cast<int>(TSA::Model::SupportType::Custom));
        if (custIdx >= 0) m_comboPreset->setCurrentIndex(custIdx);
    }
    m_isLoading = false;

    applyChanges();
}

void NodePropertiesView::onOrientationChanged(int index)
{
    Q_UNUSED(index);
    if (m_isLoading) return;
    updateDofUiState();
    applyChanges();
}

void NodePropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(QColor(m_colorHex), this, tr("Couleur du Nœud"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));
        applyChanges();
    }
}

void NodePropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

TSA::Model::SupportDefinition NodePropertiesView::buildSupportFromUi() const
{
    auto getDof = [](QComboBox* combo) -> TSA::Model::DOFState {
        return static_cast<TSA::Model::DOFState>(combo->currentData().toInt());
    };

    TSA::Model::SupportDefinition supp(
        getDof(m_comboTx), getDof(m_comboTy), getDof(m_comboTz),
        getDof(m_comboRx), getDof(m_comboRy), getDof(m_comboRz),
        m_spinKx->value(), m_spinKy->value(), m_spinKz->value(),
        m_spinKrx->value(), m_spinKry->value(), m_spinKrz->value(),
        static_cast<TSA::Model::SupportOrientationType>(m_comboOrientation->currentData().toInt()),
        m_spinDirX->value(), m_spinDirY->value(), m_spinDirZ->value()
    );

    return supp;
}

void NodePropertiesView::applyChanges()
{
    if (!m_model || m_nodeId <= 0) return;
    auto* node = m_model->getNode(m_nodeId);
    if (!node) return;

    // Validation géométrique AVANT toute modification : refuser un déplacement qui rendrait un
    // élément connecté de longueur nulle (géométrie OCCT dégénérée, matrice de rigidité singulière).
    if (m_model->wouldCollapseConnectedElement(m_nodeId, m_spinX->value(), m_spinY->value(), m_spinZ->value()))
    {
        QMessageBox::warning(this, tr("Coordonnées refusées"),
                             tr("Ces coordonnées confondraient le nœud %1 avec l'autre extrémité d'un élément connecté "
                                "(longueur nulle). La modification n'a pas été appliquée.").arg(m_nodeId));
        refreshView();
        return;
    }

    const double oldX = node->x(), oldY = node->y(), oldZ = node->z();
    const std::string undoName = tr("Modification Nœud %1").arg(m_nodeId).toStdString();
    // Clé de coalescence = nom : crans successifs sur le même objet -> une seule entrée Undo
    m_model->pushUndoState(undoName, undoName);

    node->setName(m_editName->text().toStdString());
    node->setCoordinates(m_spinX->value(), m_spinY->value(), m_spinZ->value());
    node->setColor(m_colorHex.toStdString());

    TSA::Model::SupportDefinition supp = buildSupportFromUi();
    node->setSupport(supp);

    if (auto* um = m_model->undoManager();
        um && (oldX != node->x() || oldY != node->y() || oldZ != node->z()))
    {
        auto fmt = [](double x, double y, double z) {
            return QString("(%1, %2, %3) m").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(z, 0, 'f', 3).toStdString();
        };
        um->addRecord({ "modify_property", "Node", m_nodeId, "coordinates", fmt(oldX, oldY, oldZ),
                        fmt(node->x(), node->y(), node->z()), { "geometry", "connected_elements", "results_invalidated" } });
    }

    m_model->notifyNodeModified(m_nodeId);
    emit elementModified();
}

} // namespace TSA::UI
