#include "MemberLoadDialog.h"
#include "../../Model/Model.h"
#include "../../Model/Load/LoadManager.h"
#include "../../Model/Load/MemberLoadCommands.h"
#include "../../Viewer/SelectionManager.h"
#include "../../Viewer/OccView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

namespace TSA::UI
{

MemberLoadDialog::MemberLoadDialog(TSA::Model::Model* model,
                                   TSA::Viewer::SelectionManager* selectionManager,
                                   OccView* occView,
                                   QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_selectionManager(selectionManager)
    , m_occView(occView)
{
    setWindowTitle(tr("Charge sur barre"));
    resize(480, 520);
    setupUI();
    populateElements();
    populateLoadCases();

    // Si une poutre ou poteau est sélectionné dans le viewport 3D, présélectionner
    if (m_selectionManager)
    {
        if (!m_selectionManager->selectedBeams().empty())
        {
            setTargetElementId(*m_selectionManager->selectedBeams().begin());
        }
        else if (!m_selectionManager->selectedColumns().empty())
        {
            setTargetElementId(*m_selectionManager->selectedColumns().begin(), TSA::Model::MemberTargetType::Column);
        }
        const std::size_t count = m_selectionManager->selectedBeams().size() + m_selectionManager->selectedColumns().size();
        if (count > 1)
        {
            m_chkAllSelected->setText(tr("Appliquer aux %1 barres sélectionnées").arg(count));
            m_chkAllSelected->setChecked(true);
            m_chkAllSelected->setVisible(true);
        }
    }
}

void MemberLoadDialog::setLoadType(TSA::Model::LoadType type)
{
    const int idx = m_comboType->findData(static_cast<int>(type));
    if (idx >= 0) m_comboType->setCurrentIndex(idx);
}

double MemberLoadDialog::currentLength() const
{
    return m_model ? TSA::Model::memberLoadTargetLength(*m_model, m_comboElement->currentData().toInt(), currentTarget()) : -1.0;
}

std::vector<TSA::Model::MemberLoadTarget> MemberLoadDialog::chosenTargets() const
{
    std::vector<TSA::Model::MemberLoadTarget> targets;
    const int id = m_comboElement->currentData().toInt();
    if (id > 0) targets.push_back({ id, currentTarget() });
    if (m_selectionManager && m_chkAllSelected->isVisible() && m_chkAllSelected->isChecked())
    {
        auto add = [&](int elemId, TSA::Model::MemberTargetType type) {
            for (const auto& t : targets)
                if (t.elementId == elemId && t.type == type) return;
            targets.push_back({ elemId, type });
        };
        for (int b : m_selectionManager->selectedBeams()) add(b, TSA::Model::MemberTargetType::Beam);
        for (int c : m_selectionManager->selectedColumns()) add(c, TSA::Model::MemberTargetType::Column);
    }
    return targets;
}

void MemberLoadDialog::onDirectionChanged(int /*index*/)
{
    const auto dir = static_cast<TSA::Model::LoadDirection>(m_comboDirection->currentData().toInt());
    const bool local = TSA::Model::MemberLoad::coordSystemFor(dir) == TSA::Model::LoadCoordSystem::Local;
    if (dir == TSA::Model::LoadDirection::Gravity)
        m_lblDirectionHint->setText(tr("Repère global. Gravité : charge toujours dirigée vers le bas (−Z), le signe saisi est ignoré."));
    else
        m_lblDirectionHint->setText(tr("Repère %1. Valeur positive : sens de l'axe ; négative : sens opposé.")
                                        .arg(local ? tr("local de la barre") : tr("global")));
}

void MemberLoadDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 1. Barre cible
    auto* groupElem = new QGroupBox(tr("Barre / Élément structural"), this);
    auto* elemLayout = new QVBoxLayout(groupElem);

    m_comboElement = new QComboBox(this);
    elemLayout->addWidget(m_comboElement);

    m_lblElementInfo = new QLabel(tr("Longueur : 0.00 m | Section : -"), this);
    m_lblElementInfo->setStyleSheet("color: #00adb5; font-weight: bold; padding: 4px;");
    elemLayout->addWidget(m_lblElementInfo);

    m_chkAllSelected = new QCheckBox(this);
    m_chkAllSelected->setVisible(false);
    m_chkAllSelected->setToolTip(tr("Applique la même charge à chaque barre sélectionnée dans la vue (une seule entrée Annuler). "
                                    "Les positions sont en mètres depuis le nœud de début de chaque barre."));
    elemLayout->addWidget(m_chkAllSelected);

    mainLayout->addWidget(groupElem);

    // 2. Cas de charge & Type de chargement
    auto* groupParam = new QGroupBox(tr("Paramètres du chargement"), this);
    auto* paramForm = new QFormLayout(groupParam);

    m_comboLoadCase = new QComboBox(this);
    paramForm->addRow(tr("Cas de charge :"), m_comboLoadCase);

    m_comboType = new QComboBox(this);
    m_comboType->addItem(tr("Uniforme (kN/m)"), static_cast<int>(TSA::Model::LoadType::MemberUniform));
    m_comboType->addItem(tr("Linéaire : triangulaire ou trapézoïdale (q1 → q2, kN/m)"), static_cast<int>(TSA::Model::LoadType::MemberLinear));
    m_comboType->addItem(tr("Ponctuelle sur barre (kN)"), static_cast<int>(TSA::Model::LoadType::MemberPoint));
    m_comboType->setToolTip(tr("Triangulaire : une des deux intensités vaut 0. Moments répartis et charges surfaciques : "
                               "non disponibles (non transmis aux moteurs de calcul)."));
    paramForm->addRow(tr("Type de charge :"), m_comboType);

    m_comboDirection = new QComboBox(this);
    m_comboDirection->addItem(tr("Gravité (-Z)"), static_cast<int>(TSA::Model::LoadDirection::Gravity));
    m_comboDirection->addItem(tr("Global X"), static_cast<int>(TSA::Model::LoadDirection::GlobalX));
    m_comboDirection->addItem(tr("Global Y"), static_cast<int>(TSA::Model::LoadDirection::GlobalY));
    m_comboDirection->addItem(tr("Global Z"), static_cast<int>(TSA::Model::LoadDirection::GlobalZ));
    m_comboDirection->addItem(tr("Local x (axial)"), static_cast<int>(TSA::Model::LoadDirection::LocalX));
    m_comboDirection->addItem(tr("Local y (transversal)"), static_cast<int>(TSA::Model::LoadDirection::LocalY));
    m_comboDirection->addItem(tr("Local z (transversal)"), static_cast<int>(TSA::Model::LoadDirection::LocalZ));
    paramForm->addRow(tr("Direction :"), m_comboDirection);

    m_lblDirectionHint = new QLabel(this);
    m_lblDirectionHint->setWordWrap(true);
    m_lblDirectionHint->setStyleSheet("color: #94a3b8;");
    paramForm->addRow(QString(), m_lblDirectionHint);

    mainLayout->addWidget(groupParam);

    // 3. Intensités et Positions
    auto* groupVal = new QGroupBox(tr("Intensités & Position"), this);
    auto* valGrid = new QGridLayout(groupVal);

    m_lblQ1 = new QLabel(tr("Intensité q :"), this);
    m_spinQ1 = new QDoubleSpinBox(this);
    m_spinQ1->setRange(-10000.0, 10000.0);
    m_spinQ1->setDecimals(2);
    m_spinQ1->setSuffix(" kN/m");

    m_lblQ2 = new QLabel(tr("Intensité q2 (fin) :"), this);
    m_spinQ2 = new QDoubleSpinBox(this);
    m_spinQ2->setRange(-10000.0, 10000.0);
    m_spinQ2->setDecimals(2);
    m_spinQ2->setSuffix(" kN/m");
    m_lblQ2->setVisible(false);
    m_spinQ2->setVisible(false);

    valGrid->addWidget(m_lblQ1, 0, 0);
    valGrid->addWidget(m_spinQ1, 0, 1);
    valGrid->addWidget(m_lblQ2, 0, 2);
    valGrid->addWidget(m_spinQ2, 0, 3);

    m_lblX1 = new QLabel(tr("Début x1 :"), this);
    m_spinX1 = new QDoubleSpinBox(this);
    m_spinX1->setRange(0.0, 1000.0);
    m_spinX1->setDecimals(2);
    m_spinX1->setSuffix(" m");

    m_lblX2 = new QLabel(tr("Fin x2 :"), this);
    m_spinX2 = new QDoubleSpinBox(this);
    m_spinX2->setRange(0.0, 1000.0);
    m_spinX2->setDecimals(2);
    m_spinX2->setSuffix(" m");

    valGrid->addWidget(m_lblX1, 1, 0);
    valGrid->addWidget(m_spinX1, 1, 1);
    valGrid->addWidget(m_lblX2, 1, 2);
    valGrid->addWidget(m_spinX2, 1, 3);

    // Aucune valeur ni préréglage chiffré : l'intensité part de zéro.

    mainLayout->addWidget(groupVal);

    // 4. Libellé optionnel
    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("Libellé :"), this));
    m_editName = new QLineEdit(this);
    m_editName->setPlaceholderText(tr("Automatique (ex: ML1)"));
    nameLayout->addWidget(m_editName);
    mainLayout->addLayout(nameLayout);

    m_lblStatus = new QLabel(this);
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setStyleSheet("color: #34d399;");
    mainLayout->addWidget(m_lblStatus);

    // 5. Boutons d'action
    auto* btnLayout = new QHBoxLayout();
    m_btnApply = new QPushButton(tr("Appliquer la Charge"), this);
    m_btnApply->setStyleSheet("background-color: #00adb5; color: white; font-weight: bold; padding: 6px;");
    m_btnClose = new QPushButton(tr("Fermer"), this);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnApply);
    btnLayout->addWidget(m_btnClose);
    mainLayout->addLayout(btnLayout);

    // Connexions
    connect(m_comboElement, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MemberLoadDialog::onElementSelectionChanged);
    connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MemberLoadDialog::onLoadTypeChanged);
    connect(m_comboDirection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MemberLoadDialog::onDirectionChanged);
    onLoadTypeChanged(m_comboType->currentIndex());
    onDirectionChanged(m_comboDirection->currentIndex());
    connect(m_btnApply, &QPushButton::clicked, this, &MemberLoadDialog::onApplyClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void MemberLoadDialog::populateElements()
{
    m_comboElement->blockSignals(true);
    m_comboElement->clear();

    if (m_model)
    {
        for (const auto& [id, b] : m_model->beams())
        {
            double len = b.length(*m_model);
            QString txt = QString("Poutre B%1 - L=%2 m [%3]").arg(id).arg(QString::number(len, 'f', 2)).arg(QString::fromStdString(b.section().name));
            m_comboElement->addItem(txt, id);
            m_comboElement->setItemData(m_comboElement->count() - 1, static_cast<int>(TSA::Model::MemberTargetType::Beam), Qt::UserRole + 1);
        }
        for (const auto& [id, col] : m_model->columns())
        {
            double len = col.length(*m_model);
            QString txt = QString("Poteau C%1 - L=%2 m [%3]").arg(id).arg(QString::number(len, 'f', 2)).arg(QString::fromStdString(col.section().name));
            m_comboElement->addItem(txt, id);
            m_comboElement->setItemData(m_comboElement->count() - 1, static_cast<int>(TSA::Model::MemberTargetType::Column), Qt::UserRole + 1);
        }
        for (const auto& [id, tr] : m_model->trussMembers())
        {
            double len = tr.length(*m_model);
            QString txt = QString("Treillis T%1 - L=%2 m").arg(id).arg(QString::number(len, 'f', 2));
            m_comboElement->addItem(txt, id);
            m_comboElement->setItemData(m_comboElement->count() - 1, static_cast<int>(TSA::Model::MemberTargetType::Truss), Qt::UserRole + 1);
        }
    }
    m_comboElement->blockSignals(false);
    updateElementInfoDisplay();
}

void MemberLoadDialog::populateLoadCases()
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

void MemberLoadDialog::setTargetElementId(int elemId, TSA::Model::MemberTargetType target)
{
    for (int i = 0; i < m_comboElement->count(); ++i)
    {
        if (m_comboElement->itemData(i).toInt() == elemId
            && m_comboElement->itemData(i, Qt::UserRole + 1).toInt() == static_cast<int>(target))
        {
            m_comboElement->setCurrentIndex(i);
            break;
        }
    }
    updateElementInfoDisplay();
}

TSA::Model::MemberTargetType MemberLoadDialog::currentTarget() const
{
    return static_cast<TSA::Model::MemberTargetType>(m_comboElement->currentData(Qt::UserRole + 1).toInt());
}

void MemberLoadDialog::onElementSelectionChanged(int /*index*/)
{
    updateElementInfoDisplay();
}

void MemberLoadDialog::updateElementInfoDisplay()
{
    int elemId = m_comboElement->currentData().toInt();
    const auto target = currentTarget();
    if (m_model)
    {
        const auto* b = target == TSA::Model::MemberTargetType::Beam ? m_model->getBeam(elemId) : nullptr;
        if (b)
        {
            m_lblElementInfo->setText(tr("Poutre | Longueur : %1 m | Section : %2 | Matériau : %3")
                .arg(QString::number(b->length(*m_model), 'f', 2))
                .arg(QString::fromStdString(b->section().name))
                .arg(QString::fromStdString(b->material().name)));
            m_spinX1->setMaximum(b->length(*m_model));
            m_spinX2->setMaximum(b->length(*m_model));
            m_spinX2->setValue(b->length(*m_model));
            return;
        }

        const auto* col = target == TSA::Model::MemberTargetType::Column ? m_model->getColumn(elemId) : nullptr;
        if (col)
        {
            m_lblElementInfo->setText(tr("Poteau | Longueur : %1 m | Section : %2 | Matériau : %3")
                .arg(QString::number(col->length(*m_model), 'f', 2))
                .arg(QString::fromStdString(col->section().name))
                .arg(QString::fromStdString(col->material().name)));
            m_spinX1->setMaximum(col->length(*m_model));
            m_spinX2->setMaximum(col->length(*m_model));
            m_spinX2->setValue(col->length(*m_model));
            return;
        }

        const auto* truss = target == TSA::Model::MemberTargetType::Truss ? m_model->getTrussMember(elemId) : nullptr;
        if (truss)
        {
            m_lblElementInfo->setText(tr("Treillis | Longueur : %1 m").arg(QString::number(truss->length(*m_model), 'f', 2)));
            m_spinX1->setMaximum(truss->length(*m_model));
            m_spinX2->setMaximum(truss->length(*m_model));
            m_spinX2->setValue(truss->length(*m_model));
            return;
        }
    }
    m_lblElementInfo->setText(tr("Aucun élément sélectionné"));
}

void MemberLoadDialog::onLoadTypeChanged(int index)
{
    auto type = static_cast<TSA::Model::LoadType>(m_comboType->itemData(index).toInt());
    bool isLinear = (type == TSA::Model::LoadType::MemberLinear);
    bool isPoint = (type == TSA::Model::LoadType::MemberPoint);

    m_lblQ2->setVisible(isLinear);
    m_spinQ2->setVisible(isLinear);

    // Intervalle [x1, x2] pour toute charge répartie (uniforme comprise), position seule pour une force.
    m_lblX2->setVisible(!isPoint);
    m_spinX2->setVisible(!isPoint);

    if (isPoint)
    {
        m_lblQ1->setText(tr("Force P :"));
        m_spinQ1->setSuffix(" kN");
        m_lblX1->setText(tr("Position x :"));
    }
    else
    {
        m_lblQ1->setText(isLinear ? tr("Intensité q1 (début) :") : tr("Intensité q :"));
        m_spinQ1->setSuffix(" kN/m");
        m_lblX1->setText(tr("Début x1 :"));
    }
}

void MemberLoadDialog::onApplyClicked()
{
    if (!m_model) return;
    const std::vector<TSA::Model::MemberLoadTarget> targets = chosenTargets();
    if (targets.empty())
    {
        QMessageBox::warning(this, tr("Charge sur barre"), tr("Veuillez sélectionner un élément structural valide."));
        return;
    }

    const auto type = static_cast<TSA::Model::LoadType>(m_comboType->currentData().toInt());
    const auto dir = static_cast<TSA::Model::LoadDirection>(m_comboDirection->currentData().toInt());
    const bool isPoint = type == TSA::Model::LoadType::MemberPoint;
    const double q1 = m_spinQ1->value();
    const double q2 = type == TSA::Model::LoadType::MemberLinear ? m_spinQ2->value() : q1;
    const double x1 = m_spinX1->value();
    const double x2 = isPoint ? x1 : m_spinX2->value();

    TSA::Model::MemberLoad prototype(0, targets.front().elementId, m_comboLoadCase->currentData().toInt(), type, q1, q2, dir,
                                     TSA::Model::MemberLoad::coordSystemFor(dir), x1, x2, false,
                                     m_editName->text().trimmed().toStdString(), targets.front().type);

    // Double application involontaire (double clic sur « Appliquer ») : confirmation.
    int duplicates = 0;
    for (const auto& t : targets)
    {
        TSA::Model::MemberLoad probe = prototype;
        probe.setElementId(t.elementId);
        probe.setTargetType(t.type);
        if (TSA::Model::hasEquivalentMemberLoad(*m_model, probe)) ++duplicates;
    }
    if (duplicates > 0
        && QMessageBox::question(this, tr("Charge déjà présente"),
                                 tr("Une charge identique existe déjà sur %1 barre(s). L'ajouter quand même (elle s'additionnera) ?")
                                     .arg(duplicates),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    std::string error;
    const std::vector<int> ids = TSA::Model::applyMemberLoad(
        *m_model, prototype, targets,
        tr("Charge sur %1 barre(s)").arg(targets.size()).toStdString(), &error);
    if (ids.empty())
    {
        QMessageBox::warning(this, tr("Charge sur barre"), QString::fromStdString(error));
        return;
    }
    m_lblStatus->setText(ids.size() == 1 ? tr("Charge ML%1 appliquée.").arg(ids.front())
                                         : tr("%1 charges appliquées (ML%2 à ML%3).").arg(ids.size()).arg(ids.front()).arg(ids.back()));
}

} // namespace TSA::UI
