#include "NodalLoadDialog.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"
#include "../../Viewer/SelectionManager.h"
#include "../../Viewer/OccView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

namespace TSA::UI
{

NodalLoadDialog::NodalLoadDialog(TSA::Model::Model* model,
                                 TSA::Viewer::SelectionManager* selectionManager,
                                 OccView* occView,
                                 QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_selectionManager(selectionManager)
    , m_occView(occView)
{
    setWindowTitle(tr("Charge nodale : force et couple"));
    resize(460, 480);
    setupUI();
    populateNodes();
    populateLoadCases();

    // Si des nœuds sont déjà sélectionnés dans le viewport 3D, sélectionner le premier
    if (m_selectionManager && !m_selectionManager->selectedNodes().empty())
    {
        int firstNodeId = *m_selectionManager->selectedNodes().begin();
        setTargetNodeId(firstNodeId);
    }
}

void NodalLoadDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 1. Groupe Cible (Nœud)
    auto* groupNode = new QGroupBox(tr("Nœud cible"), this);
    auto* nodeLayout = new QVBoxLayout(groupNode);

    auto* nodePickLayout = new QHBoxLayout();
    m_comboNode = new QComboBox(this);
    m_btnPick3D = new QPushButton(tr("📍 Sélectionner 3D"), this);
    m_btnPick3D->setToolTip(tr("Cliquer sur un nœud dans le viewport 3D"));
    nodePickLayout->addWidget(m_comboNode, 1);
    nodePickLayout->addWidget(m_btnPick3D);
    nodeLayout->addLayout(nodePickLayout);

    m_lblNodeCoords = new QLabel(tr("Coordonnées : X=0.00, Y=0.00, Z=0.00 m"), this);
    m_lblNodeCoords->setStyleSheet("color: #00adb5; font-weight: bold; padding: 4px;");
    nodeLayout->addWidget(m_lblNodeCoords);

    mainLayout->addWidget(groupNode);

    // 2. Groupe Cas de charge & Repère
    auto* groupCase = new QGroupBox(tr("Cas de charge & Système de coordonnées"), this);
    auto* caseForm = new QFormLayout(groupCase);

    m_comboLoadCase = new QComboBox(this);
    caseForm->addRow(tr("Cas de charge :"), m_comboLoadCase);

    m_comboCoordSys = new QComboBox(this);
    m_comboCoordSys->addItem(tr("Global (WCS)"), static_cast<int>(TSA::Model::LoadCoordSystem::Global));
    m_comboCoordSys->addItem(tr("Local (LCS)"), static_cast<int>(TSA::Model::LoadCoordSystem::Local));
    caseForm->addRow(tr("Système de coordonnées :"), m_comboCoordSys);

    mainLayout->addWidget(groupCase);

    // 3. Groupe Forces (Fx, Fy, Fz en kN)
    auto* groupForces = new QGroupBox(tr("Forces nodales (kN)"), this);
    auto* forcesLayout = new QGridLayout(groupForces);

    m_spinFx = new QDoubleSpinBox(this);
    m_spinFx->setRange(-100000.0, 100000.0);
    m_spinFx->setDecimals(2);
    m_spinFx->setSuffix(" kN");

    m_spinFy = new QDoubleSpinBox(this);
    m_spinFy->setRange(-100000.0, 100000.0);
    m_spinFy->setDecimals(2);
    m_spinFy->setSuffix(" kN");

    m_spinFz = new QDoubleSpinBox(this);
    m_spinFz->setRange(-100000.0, 100000.0);
    m_spinFz->setDecimals(2);
    m_spinFz->setSuffix(" kN");

    forcesLayout->addWidget(new QLabel(tr("Fx :"), this), 0, 0);
    forcesLayout->addWidget(m_spinFx, 0, 1);
    forcesLayout->addWidget(new QLabel(tr("Fy :"), this), 0, 2);
    forcesLayout->addWidget(m_spinFy, 0, 3);
    forcesLayout->addWidget(new QLabel(tr("Fz (↓) :"), this), 1, 0);
    forcesLayout->addWidget(m_spinFz, 1, 1);

    // Aucune valeur ni préréglage chiffré : toutes les composantes partent de zéro.

    mainLayout->addWidget(groupForces);

    // 4. Groupe Moments (Mx, My, Mz en kNm)
    auto* groupMoments = new QGroupBox(tr("Couple nodal (kNm)"), this);
    auto* momentsLayout = new QGridLayout(groupMoments);

    m_spinMx = new QDoubleSpinBox(this);
    m_spinMx->setRange(-100000.0, 100000.0);
    m_spinMx->setDecimals(2);
    m_spinMx->setSuffix(" kNm");

    m_spinMy = new QDoubleSpinBox(this);
    m_spinMy->setRange(-100000.0, 100000.0);
    m_spinMy->setDecimals(2);
    m_spinMy->setSuffix(" kNm");

    m_spinMz = new QDoubleSpinBox(this);
    m_spinMz->setRange(-100000.0, 100000.0);
    m_spinMz->setDecimals(2);
    m_spinMz->setSuffix(" kNm");

    momentsLayout->addWidget(new QLabel(tr("Mx :"), this), 0, 0);
    momentsLayout->addWidget(m_spinMx, 0, 1);
    momentsLayout->addWidget(new QLabel(tr("My :"), this), 0, 2);
    momentsLayout->addWidget(m_spinMy, 0, 3);
    momentsLayout->addWidget(new QLabel(tr("Mz :"), this), 1, 0);
    momentsLayout->addWidget(m_spinMz, 1, 1);

    mainLayout->addWidget(groupMoments);

    // 5. Nom optionnel
    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("Libellé :"), this));
    m_editName = new QLineEdit(this);
    m_editName->setPlaceholderText(tr("Automatique (ex: NL1)"));
    nameLayout->addWidget(m_editName);
    mainLayout->addLayout(nameLayout);

    // 6. Boutons d'action
    auto* btnLayout = new QHBoxLayout();
    m_btnApply = new QPushButton(tr("Appliquer la Charge"), this);
    m_btnApply->setStyleSheet("background-color: #00adb5; color: white; font-weight: bold; padding: 6px;");
    m_btnClose = new QPushButton(tr("Fermer"), this);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnApply);
    btnLayout->addWidget(m_btnClose);
    mainLayout->addLayout(btnLayout);

    // Connexions
    connect(m_comboNode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NodalLoadDialog::onNodeSelectionChanged);
    connect(m_btnPick3D, &QPushButton::clicked, this, &NodalLoadDialog::onPick3DClicked);
    connect(m_btnApply, &QPushButton::clicked, this, &NodalLoadDialog::onApplyClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void NodalLoadDialog::populateNodes()
{
    m_comboNode->blockSignals(true);
    m_comboNode->clear();

    if (m_model)
    {
        for (const auto& [id, n] : m_model->nodes())
        {
            QString txt = QString("N%1 - (%2, %3, %4) m")
                .arg(id)
                .arg(QString::number(n.x(), 'f', 2))
                .arg(QString::number(n.y(), 'f', 2))
                .arg(QString::number(n.z(), 'f', 2));
            if (!n.name().empty())
            {
                txt = QString("N%1 [%2] - (%3, %4, %5) m")
                    .arg(id)
                    .arg(QString::fromStdString(n.name()))
                    .arg(QString::number(n.x(), 'f', 2))
                    .arg(QString::number(n.y(), 'f', 2))
                    .arg(QString::number(n.z(), 'f', 2));
            }
            m_comboNode->addItem(txt, id);
        }
    }
    m_comboNode->blockSignals(false);
    updateNodeCoordinatesDisplay();
}

void NodalLoadDialog::populateLoadCases()
{
    m_comboLoadCase->clear();
    if (m_model)
    {
        for (const auto& [id, lc] : m_model->loadManager().loadCases())
        {
            QString txt = QString("%1 - %2")
                .arg(QString::fromStdString(lc.name()))
                .arg(QString::fromStdString(lc.description()));
            m_comboLoadCase->addItem(txt, id);
        }
    }
}

void NodalLoadDialog::setTargetNodeId(int nodeId)
{
    int idx = m_comboNode->findData(nodeId);
    if (idx >= 0)
    {
        m_comboNode->setCurrentIndex(idx);
    }
    updateNodeCoordinatesDisplay();
}

void NodalLoadDialog::onNodeSelectionChanged(int /*index*/)
{
    updateNodeCoordinatesDisplay();
}

void NodalLoadDialog::updateNodeCoordinatesDisplay()
{
    int nodeId = m_comboNode->currentData().toInt();
    if (m_model)
    {
        const auto* n = m_model->getNode(nodeId);
        if (n)
        {
            m_lblNodeCoords->setText(tr("Coordonnées : X=%1 m, Y=%2 m, Z=%3 m")
                .arg(QString::number(n->x(), 'f', 3))
                .arg(QString::number(n->y(), 'f', 3))
                .arg(QString::number(n->z(), 'f', 3)));
            return;
        }
    }
    m_lblNodeCoords->setText(tr("Aucun nœud sélectionné"));
}

void NodalLoadDialog::onPick3DClicked()
{
    if (!m_occView)
    {
        QMessageBox::information(this, tr("Sélection 3D"), tr("Le viewport 3D n'est pas disponible."));
        return;
    }

    m_occView->pickPoint3D([this](const gp_Pnt& /*pt*/, int nodeId) {
        if (nodeId > 0)
        {
            setTargetNodeId(nodeId);
        }
        else
        {
            QMessageBox::warning(this, tr("Sélection"), tr("Le point sélectionné n'est pas un nœud existant."));
        }
    });
}

void NodalLoadDialog::onApplyClicked()
{
    if (!m_model) return;

    int nodeId = m_comboNode->currentData().toInt();
    if (!m_model->getNode(nodeId))
    {
        QMessageBox::warning(this, tr("Erreur"), tr("Veuillez sélectionner un nœud valide."));
        return;
    }

    int loadCaseId = m_comboLoadCase->currentData().toInt();
    auto coordSys = static_cast<TSA::Model::LoadCoordSystem>(m_comboCoordSys->currentData().toInt());

    double fx = m_spinFx->value();
    double fy = m_spinFy->value();
    double fz = m_spinFz->value();
    double mx = m_spinMx->value();
    double my = m_spinMy->value();
    double mz = m_spinMz->value();

    if (std::abs(fx) < 1e-9 && std::abs(fy) < 1e-9 && std::abs(fz) < 1e-9 &&
        std::abs(mx) < 1e-9 && std::abs(my) < 1e-9 && std::abs(mz) < 1e-9)
    {
        QMessageBox::warning(this, tr("Erreur"), tr("Toutes les composantes de force et moment sont nulles."));
        return;
    }

    std::string name = m_editName->text().trimmed().toStdString();

    m_model->pushUndoState(tr("Appliquer Charge Nodale sur N%1").arg(nodeId).toStdString());

    TSA::Model::NodalLoad nl(0, nodeId, loadCaseId, fx, fy, fz, mx, my, mz, coordSys, name);
    int newId = m_model->loadManager().addNodalLoad(nl);

    m_model->notifyNodalLoadAdded(newId);

    QMessageBox::information(this, tr("Succès"),
                             tr("Charge nodale NL#%1 appliquée avec succès sur le nœud N%2 !").arg(newId).arg(nodeId));
}

} // namespace TSA::UI
