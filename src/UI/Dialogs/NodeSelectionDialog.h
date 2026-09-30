#pragma once

#include <QDialog>
#include <gp_Pnt.hxx>

class QTableWidget;
class QLineEdit;
class QPushButton;
class QLabel;

namespace TSA::Model
{
    class Model;
}

class OccView;

namespace TSA::UI
{

/**
 * @brief Dialogue centralisé de sélection d'un nœud existant dans le modèle.
 * 
 * Permet de rechercher par numéro (ex: 25 ou N25) ou par nom,
 * de filtrer en temps réel, de prévisualiser par surbrillance dans la vue 3D,
 * et de valider par double-clic ou bouton "Sélectionner".
 */
class NodeSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NodeSelectionDialog(TSA::Model::Model* model, OccView* occView = nullptr, QWidget* parent = nullptr);
    ~NodeSelectionDialog() override = default;

    int selectedNodeId() const noexcept { return m_selectedNodeId; }
    gp_Pnt selectedPoint() const noexcept { return m_selectedPoint; }
    void setSelectedNodeId(int nodeId);

private slots:
    void onFilterTextChanged(const QString& filter);
    void onTableRowDoubleClicked(int row, int col);
    void onTableSelectionChanged();
    void onAcceptClicked();

private:
    void setupUi();
    void populateTable();

private:
    TSA::Model::Model* m_model = nullptr;
    OccView* m_occView = nullptr;

    int m_selectedNodeId = -1;
    gp_Pnt m_selectedPoint = gp_Pnt(0.0, 0.0, 0.0);

    QLineEdit* m_filterEdit = nullptr;
    QTableWidget* m_tableWidget = nullptr;
    QPushButton* m_btnSelect = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QLabel* m_statusLabel = nullptr;
};

} // namespace TSA::UI
