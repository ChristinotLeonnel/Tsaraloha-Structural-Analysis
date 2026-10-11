#pragma once

// Charges surfaciques des dalles et voiles sélectionnés (docs/SURFACES.md) : liste, ajout (appliqué à chaque surface
// sélectionnée), duplication, suppression. Copie de travail : rien n'est modifié dans le modèle avant la validation, que
// l'appelant encadre par Model::pushUndoState.

#include "../../Model/Load/SurfaceLoad.h"

#include <QDialog>
#include <utility>
#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QTableWidget;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class SurfaceLoadsDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param surfaces surfaces visées (type, identifiant)
    SurfaceLoadsDialog(const TSA::Model::Model& model, const std::vector<std::pair<TSA::Model::SurfaceTargetKind, int>>& surfaces, QWidget* parent = nullptr);

    /// Charges des surfaces visées après édition (id > 0 : charge existante conservée, 0 : nouvelle).
    const std::vector<TSA::Model::SurfaceLoad>& loads() const { return m_loads; }

private:
    void fill();
    void addFromForm();
    const TSA::Model::Model& m_model;
    std::vector<std::pair<TSA::Model::SurfaceTargetKind, int>> m_surfaces;
    std::vector<TSA::Model::SurfaceLoad> m_loads;
    QTableWidget* m_table = nullptr;
    QComboBox *m_case = nullptr, *m_distribution = nullptr, *m_direction = nullptr;
    QDoubleSpinBox *m_value = nullptr, *m_u = nullptr, *m_v = nullptr;
    QCheckBox* m_projected = nullptr;
    QLineEdit* m_name = nullptr;
};

/// Libellé d'une charge surfacique (valeur, unité, direction, cas) pour les listes et les notes.
QString describeSurfaceLoad(const TSA::Model::Model& model, const TSA::Model::SurfaceLoad& l);

} // namespace TSA::UI
