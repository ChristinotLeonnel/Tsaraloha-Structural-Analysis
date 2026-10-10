#pragma once

// Paramètres du projet → Topologie et numérotation.
// Configure la numérotation des nœuds et des éléments, prévisualise le résultat (sans modifier le
// modèle), applique la renumérotation (étiquettes seulement, une entrée Annuler) et enregistre les
// paramètres dans le projet (chunk TOPO du .tsa).

#include "../../Topology/TopologyNumberingService.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class TopologyDialog : public QDialog
{
    Q_OBJECT

public:
    /// inputProvider : grille active, niveaux et sélection courante (lus à chaque prévisualisation).
    TopologyDialog(TSA::Model::Model* model, std::function<TSA::Topology::NumberingInput()> inputProvider,
                   QWidget* parent = nullptr);

    TSA::Topology::TopologySettings settingsFromUi() const;
    void loadSettings(const TSA::Topology::TopologySettings& settings);
    const TSA::Topology::NumberingPreview& lastPreview() const { return m_preview; }

public slots:
    bool preview();       ///< calcule et affiche l'aperçu ; true si applicable
    bool apply();         ///< prévisualise, applique et enregistre ; true si appliqué
    void resetToDefaults();
    void restorePrevious();

signals:
    /// Paramètres appliqués et enregistrés dans le projet (affichage des étiquettes…).
    void settingsApplied(const TSA::Topology::TopologySettings& settings);

private:
    void buildUi();
    QWidget* buildGeneralTab();
    QWidget* buildNodesTab();
    QWidget* buildElementsTab();
    QWidget* buildMeshTab();
    QWidget* buildPreviewTab();
    QWidget* buildMappingTab();
    void refreshAvailability();
    void updateStrategyControls();
    void showPreview();
    void refreshMapping();
    void exportMapping();

    TSA::Model::Model* m_model = nullptr;
    std::function<TSA::Topology::NumberingInput()> m_inputProvider;
    TSA::Topology::NumberingPreview m_preview;
    TSA::Topology::TopologySettings m_previousSettings; ///< configuration enregistrée à l'ouverture

    QTabWidget* m_tabs = nullptr;
    // Général
    QLabel* m_lblDimension = nullptr;
    QComboBox* m_comboScope = nullptr;
    QCheckBox* m_chkShowLabels = nullptr;
    // Nœuds
    QComboBox* m_comboNodeStrategy = nullptr;
    QLabel* m_lblNodeDescription = nullptr;
    QComboBox* m_comboNodeAxes = nullptr;
    QCheckBox* m_chkNodeDesc[3] = { nullptr, nullptr, nullptr };
    QDoubleSpinBox* m_spinNodeTol = nullptr;
    QSpinBox* m_spinStartNode = nullptr;
    QLineEdit* m_editNodePrefix = nullptr;
    QLineEdit* m_editNodePattern = nullptr;
    QSpinBox* m_spinNodeWidth = nullptr;
    QSpinBox* m_spinNodeStart = nullptr;
    QSpinBox* m_spinNodeIncrement = nullptr;
    QCheckBox* m_chkRestartPerLayer = nullptr;
    QCheckBox* m_chkPreserveNodes = nullptr;
    // Éléments
    QComboBox* m_comboElemStrategy = nullptr;
    QLabel* m_lblElemDescription = nullptr;
    QCheckBox* m_chkSharedSequence = nullptr;
    QComboBox* m_comboElemAxes = nullptr;
    QDoubleSpinBox* m_spinElemTol = nullptr;
    QLineEdit* m_editElemPattern = nullptr;
    QSpinBox* m_spinElemWidth = nullptr;
    QSpinBox* m_spinElemStart = nullptr;
    QSpinBox* m_spinElemIncrement = nullptr;
    QCheckBox* m_chkPreserveElems = nullptr;
    std::map<TSA::Topology::EntityFamily, QLineEdit*> m_prefixEdits;
    // Aperçu
    QLabel* m_lblSummary = nullptr;
    QCheckBox* m_chkChangedOnly = nullptr;
    QTableWidget* m_table = nullptr;
    QListWidget* m_messages = nullptr;
    QTableWidget* m_mappingTable = nullptr;
    QPushButton* m_btnApply = nullptr;
};

} // namespace TSA::UI
