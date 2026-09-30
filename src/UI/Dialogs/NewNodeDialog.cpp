#include "NewNodeDialog.h"
#include "../Widgets/PointSelector.h"
#include "../../Model/Model.h"
#include "../../Model/Node.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Viewer/OccView.h"
#include "../../Viewer/SelectionManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

namespace TSA::UI
{

NewNodeDialog::NewNodeDialog(TSA::Model::Model* model,
                             OccView* occView,
                             TSA::Viewer::SelectionManager* selectionManager,
                             QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_occView(occView)
    , m_selectionManager(selectionManager)
{
    setWindowTitle(tr("Nouveau Nœud"));
    setMinimumWidth(440);
    setupUi();
    populateLevels();
}

void NewNodeDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 1. Sélecteur de point centralisé
    m_pointSelector = new PointSelector(m_model, m_occView, this);
    m_pointSelector->setPlaceholderText(tr("Ex: (12.5, 0.0, 3.0) ou N<id>"));
    formLayout->addRow(tr("Position 3D :"), m_pointSelector);

    // 2. Nom personnalisé optionnel
    m_editName = new QLineEdit(this);
    m_editName->setPlaceholderText(tr("Laisser vide pour nom auto (N001, N002...)"));
    m_editName->setClearButtonEnabled(true);
    formLayout->addRow(tr("Nom du nœud :"), m_editName);

    // 3. Condition d'appui
    m_comboSupport = new QComboBox(this);
    m_comboSupport->addItem(tr("Libre (aucun blocage)"), static_cast<int>(TSA::Model::SupportType::Free));
    m_comboSupport->addItem(tr("Encastrement (Tx, Ty, Tz, Rx, Ry, Rz bloqués)"), static_cast<int>(TSA::Model::SupportType::Fixed));
    m_comboSupport->addItem(tr("Articulation / Rotule (Tx, Ty, Tz bloqués)"), static_cast<int>(TSA::Model::SupportType::Pinned));
    m_comboSupport->addItem(tr("Appui simple / Rouleau (Tz bloqué)"), static_cast<int>(TSA::Model::SupportType::Roller));
    formLayout->addRow(tr("Condition d'appui :"), m_comboSupport);

    // 4. Étage associé
    m_comboLevel = new QComboBox(this);
    formLayout->addRow(tr("Étage / Niveau :"), m_comboLevel);

    mainLayout->addLayout(formLayout);

    // Statut
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #888888; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    // Boutons d'action
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_btnCreate = new QPushButton(tr("Créer le Nœud"), this);
    m_btnCreate->setDefault(true);
    m_btnCreate->setStyleSheet("font-weight: bold; padding: 6px 16px;");
    connect(m_btnCreate, &QPushButton::clicked, this, &NewNodeDialog::onCreateClicked);

    m_btnCancel = new QPushButton(tr("Annuler"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(m_btnCreate);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(m_pointSelector, &PointSelector::pointChanged, this, [this](const gp_Pnt& /*pt*/, int nodeId) {
        if (nodeId > 0)
        {
            m_statusLabel->setText(tr("⚠️ Le nœud N%1 existe déjà à cette position.").arg(nodeId));
            m_statusLabel->setStyleSheet("color: #d97706; font-size: 11px;");
        }
        else
        {
            m_statusLabel->setText(tr("Position 3D prête pour la création."));
            m_statusLabel->setStyleSheet("color: #888888; font-size: 11px;");
        }
    });
}

void NewNodeDialog::populateLevels()
{
    m_comboLevel->clear();
    m_comboLevel->addItem(tr("(Automatique selon l'altitude Z)"), QString());

    if (m_model && m_model->levelManager())
    {
        for (const auto& lvl : m_model->levelManager()->levels())
        {
            QString txt = QString("%1 (Z = %2 m)")
                .arg(QString::fromStdString(lvl.name))
                .arg(lvl.elevation, 0, 'f', 2);
            m_comboLevel->addItem(txt, QString::fromStdString(lvl.id));
        }
    }
}

void NewNodeDialog::setInitialCoordinates(double x, double y, double z)
{
    if (m_pointSelector)
    {
        m_pointSelector->setCoordinates(x, y, z);
    }
}

void NewNodeDialog::onCreateClicked()
{
    if (!m_model)
    {
        QMessageBox::critical(this, tr("Erreur"), tr("Modèle structurel non initialisé."));
        return;
    }

    if (!m_pointSelector->isValid())
    {
        QMessageBox::warning(this, tr("Saisie invalide"), tr("Veuillez renseigner des coordonnées valides (X, Y, Z)."));
        return;
    }

    if (m_pointSelector->isNode())
    {
        int existingId = m_pointSelector->nodeId();
        auto ans = QMessageBox::question(
            this,
            tr("Nœud existant"),
            tr("Le nœud N%1 existe déjà exactement à cette position.\nVoulez-vous le sélectionner plutôt que de créer un doublon ?")
                .arg(existingId),
            QMessageBox::Yes | QMessageBox::No
        );

        if (ans == QMessageBox::Yes)
        {
            m_createdNodeId = existingId;
            if (m_selectionManager)
            {
                m_selectionManager->selectNode(existingId);
            }
            if (m_occView)
            {
                m_occView->highlightNode(existingId);
            }
            accept();
            return;
        }
    }

    gp_Pnt pt = m_pointSelector->point();
    std::string name = m_editName->text().trimmed().toStdString();
    std::string levelId = m_comboLevel->currentData().toString().toStdString();
    auto support = static_cast<TSA::Model::SupportType>(m_comboSupport->currentData().toInt());

    m_model->pushUndoState(tr("Création Nœud").toStdString());
    int newId = m_model->addNode(pt.X(), pt.Y(), pt.Z(), levelId, name);

    if (newId > 0)
    {
        auto* node = m_model->getNode(newId);
        if (node)
        {
            node->setSupportType(support);
        }

        m_createdNodeId = newId;

        if (m_occView)
        {
            m_occView->updateNodeShape(newId);
            m_occView->highlightNode(newId);
        }
        if (m_selectionManager)
        {
            m_selectionManager->selectNode(newId);
        }

        accept();
    }
    else
    {
        QMessageBox::critical(this, tr("Erreur"), tr("Échec de la création du nœud dans le modèle."));
    }
}

} // namespace TSA::UI
