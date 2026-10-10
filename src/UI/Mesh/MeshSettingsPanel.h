#pragma once

// Panneau « Paramètres du maillage » (docs/MESH.md §4) : paramètres globaux, surfaces et volumes,
// raffinement (automatique et zones), paramètres locaux, critères de qualité, profils ; commandes
// Estimer, Aperçu, Générer, Régénérer, Vérifier la qualité, Supprimer, Importer, Exporter. Les options
// proposées sont celles que le moteur (Gmsh) prend réellement en charge.

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;

namespace TSA::UI
{

class MeshController;

class MeshSettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MeshSettingsPanel(MeshController* controller, QWidget* parent = nullptr);

    /// Relit les paramètres du contrôleur (projet ouvert, profil appliqué…).
    void refresh();

signals:
    void zoneFromSelectionRequested();
    void localForSelectionRequested();
    void qualityRequested();
    void exportRequested();
    void importRequested();

private:
    void buildGeneral(QTabWidget* tabs);
    void buildSurfaces(QTabWidget* tabs);
    void buildRefinement(QTabWidget* tabs);
    void buildLocals(QTabWidget* tabs);
    void buildQuality(QTabWidget* tabs);
    void readWidgets();       ///< widgets → paramètres du contrôleur (enregistrés dans le projet)
    void refreshTables();
    void refreshStatus();
    void refreshProfiles();

    MeshController* m_controller;
    bool m_loading = false;
    QLabel* m_engine = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;
    QComboBox* m_profile = nullptr;
    // Général
    QCheckBox *m_d1 = nullptr, *m_d2 = nullptr, *m_d3 = nullptr, *m_keepPoints = nullptr;
    QDoubleSpinBox *m_size = nullptr, *m_min = nullptr, *m_max = nullptr, *m_factor = nullptr, *m_rate = nullptr;
    QComboBox *m_fineness = nullptr, *m_order = nullptr;
    QSpinBox* m_maxElements = nullptr;
    // Surfaces et volumes
    QComboBox *m_algo2D = nullptr, *m_recombAlgo = nullptr, *m_volume = nullptr, *m_algo3D = nullptr;
    QCheckBox *m_structured = nullptr, *m_recombine = nullptr, *m_optimize = nullptr;
    QSpinBox* m_smoothing = nullptr;
    // Raffinement automatique
    struct AutoRow
    {
        QCheckBox* on = nullptr;
        QDoubleSpinBox *size = nullptr, *radius = nullptr;
    };
    AutoRow m_auto[5];
    QTableWidget* m_zones = nullptr;
    QTableWidget* m_locals = nullptr;
    // Qualité
    QDoubleSpinBox *m_qAspect = nullptr, *m_qMinAngle = nullptr, *m_qMaxAngle = nullptr, *m_qJac = nullptr, *m_qGrowth = nullptr, *m_qSicn = nullptr;
    QCheckBox *m_boundary = nullptr, *m_groups = nullptr;
    QPushButton *m_btnPreview = nullptr, *m_btnGenerate = nullptr, *m_btnRegenerate = nullptr, *m_btnCancel = nullptr;
};

} // namespace TSA::UI
