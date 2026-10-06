#pragma once

// Fenêtre « Nettoyer le modèle » : choix des opérations et de la tolérance, bilan calculé sur une
// copie (rien n'est modifié tant que l'utilisateur n'a pas cliqué « Nettoyer »).

#include "../../Model/ModelCleanup.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace TSA::UI
{

class ModelCleanupDialog : public QDialog
{
    Q_OBJECT

public:
    ModelCleanupDialog(const TSA::Model::Model& model, QWidget* parent = nullptr);

    TSA::Model::CleanupOptions options() const;
    void setOptions(const TSA::Model::CleanupOptions& options);
    /// Recalcule le bilan (copie du modèle) et l'affiche ; retourne ce bilan.
    TSA::Model::CleanupReport refreshPreview();

private:
    const TSA::Model::Model& m_model;
    QCheckBox* m_merge = nullptr;
    QCheckBox* m_duplicates = nullptr;
    QCheckBox* m_connect = nullptr;
    QCheckBox* m_crossings = nullptr;
    QCheckBox* m_orphans = nullptr;
    QDoubleSpinBox* m_tolerance = nullptr;
    QLabel* m_summary = nullptr;
    QPlainTextEdit* m_details = nullptr;
    QPushButton* m_apply = nullptr;
};

} // namespace TSA::UI
