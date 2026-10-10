#pragma once

#include <QDialog>
#include <QPointer>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class OccView;

namespace TSA::Model
{
class Model;
}

namespace TSA::Viewer
{
class SelectionManager;
}

namespace TSA::UI
{

/**
 * @brief Dialogue de création et d'application de charges nodales (forces et moments)
 * sur les nœuds du modèle structural TSA.
 */
class NodalLoadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NodalLoadDialog(TSA::Model::Model* model,
                             TSA::Viewer::SelectionManager* selectionManager = nullptr,
                             OccView* occView = nullptr,
                             QWidget* parent = nullptr);
    /// Annule une sélection 3D encore en attente (sinon le clic suivant dans la vue appelait ce
    /// dialogue détruit : crash du 2026-10-09).
    ~NodalLoadDialog() override;

    void setTargetNodeId(int nodeId);

private slots:
    void onNodeSelectionChanged(int index);
    void onPick3DClicked();
    void onApplyClicked();

private:
    void setupUI();
    void populateNodes();
    void populateLoadCases();
    void updateNodeCoordinatesDisplay();

    TSA::Model::Model* m_model = nullptr;
    TSA::Viewer::SelectionManager* m_selectionManager = nullptr;
    QPointer<OccView> m_occView;   // la vue peut disparaître avant ce dialogue (fermeture)

    QComboBox* m_comboNode = nullptr;
    QPushButton* m_btnPick3D = nullptr;
    QLabel* m_lblNodeCoords = nullptr;

    QComboBox* m_comboLoadCase = nullptr;
    QComboBox* m_comboCoordSys = nullptr;

    QDoubleSpinBox* m_spinFx = nullptr;
    QDoubleSpinBox* m_spinFy = nullptr;
    QDoubleSpinBox* m_spinFz = nullptr;

    QDoubleSpinBox* m_spinMx = nullptr;
    QDoubleSpinBox* m_spinMy = nullptr;
    QDoubleSpinBox* m_spinMz = nullptr;

    QLineEdit* m_editName = nullptr;
    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnClose = nullptr;
};

} // namespace TSA::UI
