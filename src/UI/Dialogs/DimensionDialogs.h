#pragma once

// Fenêtres des cotations 3D : modification d'une cotation (position, texte, couleur, réassociation des
// références invalides) et style du projet (unités, précision, arrondi, tailles, couleurs, texte).

#include "../../Annotation/Dimension.h"

#include <QDialog>

#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

class DimensionEditDialog : public QDialog
{
    Q_OBJECT
public:
    DimensionEditDialog(TSA::Model::Model* model, int dimensionId, QWidget* parent = nullptr);
    /// Applique les modifications (une entrée Annuler) ; false et message si refusé.
    bool applyChanges(QString* error = nullptr);

private:
    TSA::Model::Model* m_model;
    int m_id;
    QLabel* m_lblValue = nullptr;
    QDoubleSpinBox* m_pos[3] = { nullptr, nullptr, nullptr };
    QLineEdit* m_editText = nullptr;
    QLineEdit* m_editColor = nullptr;
    std::vector<QSpinBox*> m_anchorNodes; ///< nœud associé par ancrage (0 : point fixe)
};

class DimensionStyleDialog : public QDialog
{
    Q_OBJECT
public:
    DimensionStyleDialog(const TSA::Annotation::DimensionStyle& style, QWidget* parent = nullptr);
    TSA::Annotation::DimensionStyle style() const;

private:
    QComboBox* m_unit = nullptr;
    QSpinBox* m_decimals = nullptr;
    QDoubleSpinBox* m_rounding = nullptr;
    QCheckBox* m_showUnit = nullptr;
    QSpinBox* m_angleDecimals = nullptr;
    QDoubleSpinBox* m_textHeight = nullptr;
    QDoubleSpinBox* m_arrowSize = nullptr;
    QDoubleSpinBox* m_gap = nullptr;
    QDoubleSpinBox* m_overshoot = nullptr;
    QDoubleSpinBox* m_levelRef = nullptr;
    QLineEdit* m_color = nullptr;
    QCheckBox* m_textInPlane = nullptr;
    QCheckBox* m_visible = nullptr;
    TSA::Annotation::DimensionStyle m_base;
};

} // namespace TSA::UI
