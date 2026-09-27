#pragma once

#include <QDialog>
#include <gp_Pnt.hxx>
#include "../../Model/Cable/Cable.h"
#include "../../Model/Material.h"

class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QToolButton;
class QLabel;

class OccView;

namespace TSA::Model
{
    class Model;
}

namespace TSA::UI
{

/**
 * @brief Boîte de dialogue et fenêtre de propriétés dédiée exclusivement aux Câbles & Systèmes de Tension.
 *
 * Établit une séparation stricte entre les éléments linéaires (Poutres, Poteaux, Barres)
 * et les câbles. Affiche uniquement les paramètres physiques, mécaniques et normatifs
 * d'un câble (diamètre Ø, section circulaire, aire nette, matériau haute résistance,
 * tension initiale / précontrainte, flèche, ancrages) et gère le tracé 3D interactif
 * avec accrochage et continuité en chaîne ("Étirer").
 */
class CableCreationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CableCreationDialog(TSA::Model::Model* model, OccView* occView, QWidget* parent = nullptr);
    ~CableCreationDialog() override = default;

    TSA::Model::CableType cableType() const;
    double diameter() const; // En mètres
    double area() const;     // En m²
    double elasticModulus() const; // En Pa
    double characteristicStrength() const; // En Pa
    double minimumBreakingForce() const; // En N
    double initialTension() const; // En N
    TSA::Model::CableGeometryMode geometryMode() const;
    double sag() const; // En mètres
    TSA::Model::AnchorType startAnchorType() const;
    TSA::Model::AnchorType endAnchorType() const;
    bool isTensionOnly() const;
    bool isChainMode() const;

    void loadFromCable(const TSA::Model::Cable& cable);

public slots:
    void onFirstPointPicked(const gp_Pnt& pt, int nodeId);
    void onSecondPointPicked(const gp_Pnt& pt, int nodeId);
    void onDrawingCancelled();

signals:
    void cableCreated(int cableId);
    void cableModified(int cableId);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onCableTypeChanged(int index);
    void onPresetSectionChanged(int index);
    void onDiameterChanged(double valMm);
    void onAreaChanged(double valMm2);
    void onGeometryModeChanged(int index);
    void onOriginReturnPressed();
    void onEndReturnPressed();
    void onAddClicked();
    void onHelpClicked();

private:
    void setupUi();
    void populatePresets();
    void populateMaterials();
    void updateCalculatedArea();
    void updateHighlight(bool waitingForSecondPoint);
    bool parseCoordinates(const QString& text, double& x, double& y, double& z) const;
    QString formatPoint(const gp_Pnt& pt) const;

private:
    TSA::Model::Model* m_model = nullptr;
    OccView* m_occView = nullptr;

    // Numérotation & Identification
    QSpinBox* m_spinCableId = nullptr;
    QSpinBox* m_spinStep = nullptr;
    QLineEdit* m_editName = nullptr;

    // Type de câble
    QComboBox* m_comboType = nullptr;

    // Section du câble
    QComboBox* m_comboPreset = nullptr;
    QDoubleSpinBox* m_spinDiameter = nullptr; // mm
    QDoubleSpinBox* m_spinArea = nullptr;     // mm²

    // Matériau & Caractéristiques mécaniques
    QComboBox* m_comboMaterial = nullptr;
    QDoubleSpinBox* m_spinModulus = nullptr;      // GPa
    QDoubleSpinBox* m_spinStrength = nullptr;     // MPa
    QDoubleSpinBox* m_spinBreakingForce = nullptr; // kN

    // Paramètres spécifiques câble
    QDoubleSpinBox* m_spinInitialTension = nullptr; // kN
    QComboBox* m_comboGeomMode = nullptr;
    QLabel* m_lblSag = nullptr;
    QDoubleSpinBox* m_spinSag = nullptr; // m
    QComboBox* m_comboStartAnchor = nullptr;
    QComboBox* m_comboEndAnchor = nullptr;
    QCheckBox* m_chkTensionOnly = nullptr;

    // Coordonnées & Tracé
    QLineEdit* m_editOrigin = nullptr;
    QLineEdit* m_editEnd = nullptr;
    QCheckBox* m_chkChain = nullptr; // "Étirer"

    // Boutons
    QPushButton* m_btnAdd = nullptr;
    QPushButton* m_btnClose = nullptr;
    QPushButton* m_btnHelp = nullptr;

    // État interne de saisie
    int m_originNodeId = -1;
    gp_Pnt m_originPt{0, 0, 0};
    int m_endNodeId = -1;
    gp_Pnt m_endPt{0, 0, 0};
    bool m_hasOrigin = false;
    bool m_hasEnd = false;
    bool m_isInternalUpdate = false;
};

} // namespace TSA::UI
