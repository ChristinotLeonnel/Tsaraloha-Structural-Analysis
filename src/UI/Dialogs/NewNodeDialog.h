#pragma once

#include <QDialog>
#include <gp_Pnt.hxx>

class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

namespace TSA::Model
{
    class Model;
}

namespace TSA::Viewer
{
    class SelectionManager;
}

class OccView;

namespace TSA::UI
{

class PointSelector;

/**
 * @brief Dialogue moderne et centralisé de création de nœuds dans TSA.
 * 
 * Remplace l'ancien système de 3 boîtes de dialogue séquentielles par un dialogue
 * unique intégrant le PointSelector (saisie manuelle, sélection 3D avec accrochage
 * et sélection de nœuds existants).
 */
class NewNodeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewNodeDialog(TSA::Model::Model* model,
                           OccView* occView = nullptr,
                           TSA::Viewer::SelectionManager* selectionManager = nullptr,
                           QWidget* parent = nullptr);
    ~NewNodeDialog() override = default;

    int createdNodeId() const noexcept { return m_createdNodeId; }
    void setInitialCoordinates(double x, double y, double z);

private slots:
    void onCreateClicked();

private:
    void setupUi();
    void populateLevels();

private:
    TSA::Model::Model* m_model = nullptr;
    OccView* m_occView = nullptr;
    TSA::Viewer::SelectionManager* m_selectionManager = nullptr;

    int m_createdNodeId = -1;

    PointSelector* m_pointSelector = nullptr;
    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboSupport = nullptr;
    QComboBox* m_comboLevel = nullptr;
    QPushButton* m_btnCreate = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // namespace TSA::UI
