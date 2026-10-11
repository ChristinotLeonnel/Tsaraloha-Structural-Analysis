#pragma once

// Appuis linéiques des bords des dalles et voiles sélectionnés (docs/SURFACES.md) : liste, ajout (appliqué à chaque surface
// sélectionnée), suppression. Copie de travail : rien n'est modifié dans le modèle avant la validation, que l'appelant
// encadre par Model::pushUndoState.

#include "../../Model/Load/SurfaceLoad.h"
#include "../../Model/SurfaceSupport.h"

#include <QDialog>
#include <array>
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

class SurfaceSupportsDialog : public QDialog
{
    Q_OBJECT

public:
    struct Entry
    {
        TSA::Model::SurfaceTargetKind kind = TSA::Model::SurfaceTargetKind::Slab;
        int surfaceId = 0;
        TSA::Model::SurfaceEdgeSupport support;
    };

    SurfaceSupportsDialog(const TSA::Model::Model& model, const std::vector<std::pair<TSA::Model::SurfaceTargetKind, int>>& surfaces, QWidget* parent = nullptr);

    /// Appuis des surfaces visées après édition.
    const std::vector<Entry>& entries() const { return m_entries; }
    /// Appuis d'une surface (pour Slab/Wall::setEdgeSupports).
    std::vector<TSA::Model::SurfaceEdgeSupport> supportsOf(TSA::Model::SurfaceTargetKind kind, int id) const;

private:
    void fill();
    void addFromForm();
    void applyPreset(int preset);
    const TSA::Model::Model& m_model;
    std::vector<std::pair<TSA::Model::SurfaceTargetKind, int>> m_surfaces;
    std::vector<Entry> m_entries;
    QTableWidget* m_table = nullptr;
    QComboBox *m_edge = nullptr, *m_preset = nullptr;
    std::array<QCheckBox*, 6> m_fixed {};
    std::array<QDoubleSpinBox*, 6> m_k {};
    QLineEdit* m_name = nullptr;
};

/// Libellé d'un appui linéique (bord, DDL bloqués, raideurs) pour les listes et la note de calcul.
QString describeEdgeSupport(TSA::Model::SurfaceTargetKind kind, const TSA::Model::SurfaceEdgeSupport& s);

} // namespace TSA::UI
