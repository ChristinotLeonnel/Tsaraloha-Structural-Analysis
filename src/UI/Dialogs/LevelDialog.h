#pragma once

#include <QDialog>
#include <string>
#include <vector>
#include "../../Coordinate/LevelManager.h"

class QTableWidget;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;

namespace TSA::UI
{

class LevelDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LevelDialog(TSA::Coordinate::LevelManager* levelManager, QWidget* parent = nullptr);
    ~LevelDialog() override = default;

private slots:
    void onAddLevel();
    void onRemoveSelectedLevel();
    void onGenerateStories();
    void onTableCellChanged(int row, int column);
    void refreshTable();

private:
    void setupUi();

private:
    TSA::Coordinate::LevelManager* m_levelManager = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnGenerate = nullptr;

    bool m_isUpdatingTable = false;
};

} // namespace TSA::UI
