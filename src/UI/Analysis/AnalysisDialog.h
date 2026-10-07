#pragma once

// Fenêtre Analysis commune à tous les moteurs. Elle est construite UNIQUEMENT à partir du
// registre des moteurs (liste, informations, capacités, disponibilité) et du registre des panneaux
// d'options : ajouter un moteur ne demande aucune modification de cette classe.

#include "../../Analysis/Engine/AnalysisManager.h"
#include "../../Model/SelectionQuery.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QVBoxLayout;

namespace TSA::Model
{
class Model;
}
namespace TSA::Grid
{
class GridManager;
}

namespace TSA::UI
{

class AnalysisEngineOptionsRegistry;
class AnalysisEngineOptionsWidget;

class AnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    AnalysisDialog(TSA::Analysis::AnalysisManager& manager,
                   const AnalysisEngineOptionsRegistry& options,
                   const TSA::Model::Model* model,
                   const TSA::Grid::GridManager* grids,
                   const TSA::Model::ElementSet& selection,
                   QWidget* parent = nullptr);

    void setContext(const TSA::Analysis::AnalysisContext& context);
    /// Contexte édité (réglages du panneau d'options courant inclus).
    TSA::Analysis::AnalysisContext context() const;

    /// true si la fenêtre a été fermée par « Lancer le calcul ».
    bool runRequested() const { return m_runRequested; }

    // --- Introspection (tests, IA) ---
    TSA::Analysis::EngineId currentEngineId() const;
    bool setEngine(const TSA::Analysis::EngineId& id);
    bool hasEngineOptionsPanel() const { return m_optionsWidget != nullptr; }
    std::vector<TSA::Analysis::AnalysisType> offeredAnalysisTypes() const;
    std::vector<TSA::Analysis::AnalysisDimension> offeredDimensions() const;
    QStringList scopeLabels() const;
    bool selectScope(const QString& label);
    bool isRunEnabled() const;
    /// Prépare (portée → modèle d'analyse) et valide sans calculer ; affiche le bilan.
    TSA::Analysis::ValidationResult validateNow();

private:
    void buildUi();
    void populateEngines();
    void populateScopes();
    void populateLoads();
    void onEngineChanged();
    void refreshEngineDependentWidgets();
    void syncCommonToOptions();
    void storeOptions();
    void showValidation(const TSA::Analysis::ValidationResult& v);

    TSA::Analysis::AnalysisManager& m_manager;
    const AnalysisEngineOptionsRegistry& m_options;
    const TSA::Model::Model* m_model = nullptr;
    const TSA::Grid::GridManager* m_grids = nullptr;
    TSA::Model::ElementSet m_selection;

    TSA::Analysis::AnalysisContext m_context;
    TSA::Analysis::EngineId m_shownEngine;
    std::vector<TSA::Analysis::AnalysisScope> m_scopes;
    bool m_runRequested = false;
    bool m_updating = false;

    QComboBox* m_engineCombo = nullptr;
    QLabel* m_engineInfo = nullptr;
    QLabel* m_capabilities = nullptr;
    QComboBox* m_dimensionCombo = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QComboBox* m_scopeCombo = nullptr;
    QComboBox* m_levelCombo = nullptr;
    QComboBox* m_loadCombo = nullptr;
    QCheckBox* m_selfWeight = nullptr;
    QGroupBox* m_optionsGroup = nullptr;
    QVBoxLayout* m_optionsLayout = nullptr;
    QLabel* m_noOptionsLabel = nullptr;
    AnalysisEngineOptionsWidget* m_optionsWidget = nullptr;
    QListWidget* m_validationList = nullptr;
    QPushButton* m_runButton = nullptr;
};

} // namespace TSA::UI
