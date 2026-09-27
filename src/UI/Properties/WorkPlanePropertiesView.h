#pragma once

#include "IElementPropertyView.h"
#include "../../Coordinate/WorkPlane.h"

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;
class QGroupBox;

namespace TSA::UI
{

/**
 * @brief Panneau d'inspection et modification numérique bidirectionnelle des Plans de Travail (WorkPlane).
 * Permet de régler en temps réel l'origine (X, Y, Z), l'orientation (Rx, Ry, Rz / Normale),
 * les dimensions (Largeur, Hauteur), la grille locale (espacements, subdivisions)
 * et les états (Actif, Visible, Verrouillé, Isolé).
 */
class WorkPlanePropertiesView : public IElementPropertyView
{
    Q_OBJECT

public:
    explicit WorkPlanePropertiesView(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~WorkPlanePropertiesView() override = default;

    void setModel(TSA::Model::Model* model) override;
    void setElementId(int id) override;
    int elementId() const override { return m_workPlane.id(); }

    void refreshView() override;
    void refreshLibraries() override {}
    void applyChanges() override;

    // Synchronisation directe avec l'objet WorkPlane actif
    void setWorkPlane(const TSA::Coordinate::WorkPlane& wp);
    const TSA::Coordinate::WorkPlane& workPlane() const noexcept { return m_workPlane; }

signals:
    void workPlaneModified(const TSA::Coordinate::WorkPlane& wp);
    void viewNormalRequested();
    void resetOriginRequested();

private slots:
    void onWidgetChanged();
    void onTypeChanged(int index);
    void onViewNormalClicked();
    void onResetOriginClicked();

private:
    void setupUi();

private:
    TSA::Model::Model* m_model = nullptr;
    TSA::Coordinate::WorkPlane m_workPlane;
    bool m_isLoading = false;

    // Identification
    QLineEdit* m_editName = nullptr;
    QComboBox* m_comboType = nullptr;

    // Position 3D (Origine)
    QDoubleSpinBox* m_spinX = nullptr;
    QDoubleSpinBox* m_spinY = nullptr;
    QDoubleSpinBox* m_spinZ = nullptr;

    // Orientation (Degrés & Normale)
    QDoubleSpinBox* m_spinRotX = nullptr;
    QDoubleSpinBox* m_spinRotY = nullptr;
    QDoubleSpinBox* m_spinRotZ = nullptr;
    QDoubleSpinBox* m_spinNormX = nullptr;
    QDoubleSpinBox* m_spinNormY = nullptr;
    QDoubleSpinBox* m_spinNormZ = nullptr;

    // Dimensions visibles
    QDoubleSpinBox* m_spinWidth = nullptr;
    QDoubleSpinBox* m_spinHeight = nullptr;

    // Grille locale
    QDoubleSpinBox* m_spinSpacingX = nullptr;
    QDoubleSpinBox* m_spinSpacingY = nullptr;
    QSpinBox* m_spinSubdivisions = nullptr;
    QCheckBox* m_chkGridVisible = nullptr;

    // États
    QCheckBox* m_chkVisible = nullptr;
    QCheckBox* m_chkActive = nullptr;
    QCheckBox* m_chkLocked = nullptr;
    QCheckBox* m_chkIsolated = nullptr;
    QDoubleSpinBox* m_spinIsolationDist = nullptr;

    // Boutons d'action
    QPushButton* m_btnViewNormal = nullptr;
    QPushButton* m_btnResetOrigin = nullptr;
};

} // namespace TSA::UI
