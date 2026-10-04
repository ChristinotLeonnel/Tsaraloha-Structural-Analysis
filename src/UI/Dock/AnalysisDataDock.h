#pragma once

#include <QDockWidget>
#include <memory>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;
class QTableView;
class QTableWidget;

namespace TSA::Model { class Model; }
namespace TSA::Analysis { class ResultsModel; }

namespace TSA::UI
{

class SparseMatrixTableModel;

/**
 * @brief Données numériques du calcul : déplacements, réactions, efforts (repère RDM, local,
 * global, basique), mapping des DDL, K_global, rigidités élémentaires, contexte IA et export.
 * Lecture seule ; les matrices ne sont affichées qu'en mode ADVANCED et toujours avec leurs
 * métadonnées (source, type, repère, exactitude). La K_global est servie par un modèle de table
 * paresseux (seules les cellules visibles sont lues) : aucune copie dense n'est créée.
 */
class AnalysisDataDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit AnalysisDataDock(QWidget* parent = nullptr);

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

private:
    void refresh();
    void fillDisplacements();
    void fillReactions();
    void fillForces();
    void fillDofMap();
    void fillGlobalStiffness();
    void fillElementList();
    void showElement(int index);
    void showNodeContext();
    void exportData();

    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;

    QLabel* m_status = nullptr;
    QTabWidget* m_tabs = nullptr;
    QTableWidget* m_dispTable = nullptr;
    QTableWidget* m_reactTable = nullptr;
    QComboBox* m_forceSystem = nullptr;
    QTableWidget* m_forceTable = nullptr;
    QTableWidget* m_dofTable = nullptr;
    QLabel* m_kMeta = nullptr;
    QTableView* m_kView = nullptr;
    SparseMatrixTableModel* m_kModel = nullptr;
    QComboBox* m_elementCombo = nullptr;
    QPlainTextEdit* m_elementText = nullptr;
    QSpinBox* m_nodeSpin = nullptr;
    QPlainTextEdit* m_contextText = nullptr;
};

} // namespace TSA::UI
