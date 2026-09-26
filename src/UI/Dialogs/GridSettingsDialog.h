#pragma once

#include <QDialog>
#include "../../Grid/GridManager.h"
#include "../../Grid/GridSnapManager.h"

class QListWidget;
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;

class OccView;

namespace TSA::UI
{

class GridSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GridSettingsDialog(TSA::Grid::GridManager* gridManager,
                                TSA::Grid::GridSnapManager* snapManager,
                                OccView* occView,
                                QWidget* parent = nullptr);
    ~GridSettingsDialog() override = default;

private slots:
    void onAddGrid();
    void onEditGrid();
    void onDuplicateGrid();
    void onDeleteGrid();
    void onSetActiveGrid();
    void onToggleVisibility(bool checked);
    void onToggleSnap(bool checked);
    void onToggleLabels(bool checked);
    void onToggleIntersections(bool checked);
    void onSnapToleranceChanged(double val);
    void refreshGridList();
    void onSelectedGridChanged();
    void onApply();

private:
    void setupUi();

private:
    TSA::Grid::GridManager* m_gridManager = nullptr;
    TSA::Grid::GridSnapManager* m_snapManager = nullptr;
    OccView* m_occView = nullptr;

    QListWidget* m_gridList = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_duplicateBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_setActiveBtn = nullptr;

    QCheckBox* m_visibleCheck = nullptr;
    QCheckBox* m_snapCheck = nullptr;
    QCheckBox* m_labelsCheck = nullptr;
    QCheckBox* m_intersectionsCheck = nullptr;
    QDoubleSpinBox* m_snapToleranceSpin = nullptr;

    QCheckBox* m_chkLiveSync = nullptr;
    QPushButton* m_btnApply = nullptr;

    QLabel* m_infoLabel = nullptr;
};

} // namespace TSA::UI
