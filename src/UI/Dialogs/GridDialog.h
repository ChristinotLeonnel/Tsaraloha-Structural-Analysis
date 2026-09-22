#pragma once

#include <QDialog>
#include <vector>
#include <string>
#include "../../Grid/GridDefinition.h"

class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QTabWidget;
class QPushButton;
class QTableWidget;
class QLabel;

namespace TSA::Grid
{
    class GridManager;
}

namespace TSA::Model
{
    class Model;
}

namespace TSA::UI
{

class GridDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GridDialog(QWidget* parent = nullptr);
    explicit GridDialog(TSA::Grid::GridManager* gridManager,
                        TSA::Model::Model* model,
                        QWidget* parent = nullptr);
    explicit GridDialog(const TSA::Grid::GridDefinition& existingDef,
                        QWidget* parent = nullptr);
    explicit GridDialog(const TSA::Grid::GridDefinition& existingDef,
                        TSA::Grid::GridManager* gridManager,
                        TSA::Model::Model* model,
                        QWidget* parent = nullptr);
    ~GridDialog() override = default;

    TSA::Grid::GridDefinition getDefinition() const;

signals:
    void gridDefinitionApplied(const TSA::Grid::GridDefinition& def);

private slots:
    void onModeCartesian();
    void onModeCylindrical();
    void onModeArbitrary();
    void onAddLines();
    void onRemoveLine();
    void onClearLines();
    void onLabelStyleChanged(int index);
    void onApply();
    void onNewGrid();
    void onTabChanged(int index);

private:
    void setupUi();
    void loadFromDefinition(const TSA::Grid::GridDefinition& def);
    void updateTableForCurrentTab();
    void applyLabels(int tabIdx);

    struct AxisState
    {
        std::vector<double> positions;
        std::vector<std::string> labels;
        int labelStyle = 0; // 0: 1 2 3..., 1: A B C..., 2: Niveau 1 2 3..., 3: Custom
        std::string customLabel;
        double currentPosition = 0.0;
        int repeatCount = 1;
        double spacing = 3.0;
    };

private:
    TSA::Grid::GridManager* m_gridManager = nullptr;
    TSA::Model::Model* m_model = nullptr;
    bool m_isEditMode = false;
    std::string m_gridId;

    // Header controls
    QComboBox* m_nameCombo = nullptr;
    QPushButton* m_btnCartesian = nullptr;
    QPushButton* m_btnCylindrical = nullptr;
    QPushButton* m_btnArbitrary = nullptr;
    QPushButton* m_btnAdvanced = nullptr;

    // Sub-tabs
    QTabWidget* m_axisTabs = nullptr;

    // Saisie par onglet
    QDoubleSpinBox* m_posSpin = nullptr;
    QSpinBox* m_repeatSpin = nullptr;
    QDoubleSpinBox* m_spacingSpin = nullptr;

    QTableWidget* m_table = nullptr;

    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnDelete = nullptr;
    QPushButton* m_btnClearAll = nullptr;
    QPushButton* m_btnBold = nullptr;

    QComboBox* m_labelStyleCombo = nullptr;
    QLineEdit* m_customLabelEdit = nullptr;

    // Boutons inférieurs
    QPushButton* m_btnNew = nullptr;
    QPushButton* m_btnManage = nullptr;
    QPushButton* m_btnApply = nullptr;
    QPushButton* m_btnClose = nullptr;
    QPushButton* m_btnHelp = nullptr;

    // Données par axe (X = 0, Y = 1, Z = 2)
    AxisState m_axes[3];
    int m_currentAxisIndex = 0;
    TSA::Grid::GridType m_currentType = TSA::Grid::GridType::Cartesian;

    bool m_isUpdating = false;
};

} // namespace TSA::UI
