#pragma once

#include <QWidget>
#include "../../Model/Load/LoadEnums.h"

class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QGroupBox;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

/**
 * @brief Vue d'inspection et d'édition des propriétés d'une charge nodale ou sur barre sélectionnée.
 * Permet d'afficher et modifier :
 * - ID, libellé
 * - Nœud ou élément structural associé
 * - Cas de charge (G, Q, W, S, etc.)
 * - Système de coordonnées (Global / Local)
 * - Composantes de force (Fx, Fy, Fz en kN)
 * - Composantes de moment (Mx, My, Mz en kNm)
 * - Composantes réparties (q1, q2 en kN/m, position x1, x2 en m)
 */
class LoadPropertiesView : public QWidget
{
    Q_OBJECT

public:
    enum class DisplayMode
    {
        None,
        Nodal,
        Member
    };

    explicit LoadPropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~LoadPropertiesView() override = default;

    void setModel(TSA::Model::Model* model);
    void setNodalLoadId(int loadId);
    void setMemberLoadId(int loadId);

    int currentLoadId() const noexcept { return m_loadId; }
    DisplayMode displayMode() const noexcept { return m_mode; }

    void refreshView();
    void applyChanges();

signals:
    void loadModified();

private slots:
    void onFieldChanged();

private:
    void setupUi();
    void populateLoadCases();

private:
    TSA::Model::Model* m_model = nullptr;
    int m_loadId = -1;
    DisplayMode m_mode = DisplayMode::None;
    bool m_isLoading = false;

    // Identification
    QLabel* m_lblHeader = nullptr;
    QLineEdit* m_editName = nullptr;
    QLabel* m_lblTarget = nullptr;
    QComboBox* m_comboLoadCase = nullptr;
    QComboBox* m_comboCoordSys = nullptr;

    // Forces nodales (kN)
    QGroupBox* m_groupForces = nullptr;
    QDoubleSpinBox* m_spinFx = nullptr;
    QDoubleSpinBox* m_spinFy = nullptr;
    QDoubleSpinBox* m_spinFz = nullptr;

    // Moments nodaux (kNm)
    QGroupBox* m_groupMoments = nullptr;
    QDoubleSpinBox* m_spinMx = nullptr;
    QDoubleSpinBox* m_spinMy = nullptr;
    QDoubleSpinBox* m_spinMz = nullptr;

    // Charges sur barre (kN/m)
    QGroupBox* m_groupMember = nullptr;
    QComboBox* m_comboDirection = nullptr;
    QDoubleSpinBox* m_spinQ1 = nullptr;
    QDoubleSpinBox* m_spinQ2 = nullptr;
    QDoubleSpinBox* m_spinX1 = nullptr;
    QDoubleSpinBox* m_spinX2 = nullptr;
    QLabel* m_lblQ1 = nullptr;
    QLabel* m_lblQ2 = nullptr;
    QLabel* m_lblX1 = nullptr;
    QLabel* m_lblX2 = nullptr;
};

} // namespace TSA::UI
