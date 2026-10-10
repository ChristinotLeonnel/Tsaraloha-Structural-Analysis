#pragma once

#include <QDialog>
#include "../../Model/Load/LoadEnums.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

#include <vector>

class OccView;

namespace TSA::Model
{
class Model;
struct MemberLoadTarget;
}

namespace TSA::Viewer
{
class SelectionManager;
}

namespace TSA::UI
{

/**
 * @brief Dialogue d'application de charges réparties (uniformes, trapézoïdales)
 * ou ponctuelles sur les barres (poutres et poteaux).
 */
class MemberLoadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MemberLoadDialog(TSA::Model::Model* model,
                             TSA::Viewer::SelectionManager* selectionManager = nullptr,
                             OccView* occView = nullptr,
                             QWidget* parent = nullptr);
    ~MemberLoadDialog() override = default;

    /// Préselection : les ids ne sont uniques que par famille (poutre 1 ≠ poteau 1).
    void setTargetElementId(int elemId, TSA::Model::MemberTargetType target = TSA::Model::MemberTargetType::Beam);
    /// Type proposé à l'ouverture (boutons Uniforme / Trapézoïdale / Ponctuelle du ruban).
    void setLoadType(TSA::Model::LoadType type);

private slots:
    void onElementSelectionChanged(int index);
    void onLoadTypeChanged(int index);
    void onDirectionChanged(int index);
    void onApplyClicked();

private:
    void setupUI();
    void populateElements();
    void populateLoadCases();
    void updateElementInfoDisplay();
    TSA::Model::MemberTargetType currentTarget() const;
    double currentLength() const;
    /// Barre de la liste, plus les autres barres sélectionnées si la case est cochée.
    std::vector<TSA::Model::MemberLoadTarget> chosenTargets() const;

    TSA::Model::Model* m_model = nullptr;
    TSA::Viewer::SelectionManager* m_selectionManager = nullptr;
    OccView* m_occView = nullptr;

    QComboBox* m_comboElement = nullptr;
    QLabel* m_lblElementInfo = nullptr;

    QComboBox* m_comboLoadCase = nullptr;
    QComboBox* m_comboType = nullptr;
    QComboBox* m_comboDirection = nullptr;

    QLabel* m_lblQ1 = nullptr;
    QDoubleSpinBox* m_spinQ1 = nullptr;

    QLabel* m_lblQ2 = nullptr;
    QDoubleSpinBox* m_spinQ2 = nullptr;

    QLabel* m_lblX1 = nullptr;
    QDoubleSpinBox* m_spinX1 = nullptr;

    QLabel* m_lblX2 = nullptr;
    QDoubleSpinBox* m_spinX2 = nullptr;

    QLabel* m_lblDirectionHint = nullptr;
    QCheckBox* m_chkAllSelected = nullptr;
    QLabel* m_lblStatus = nullptr;
    QLineEdit* m_editName = nullptr;
    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnClose = nullptr;
};

} // namespace TSA::UI
