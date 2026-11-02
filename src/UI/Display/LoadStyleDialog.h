#pragma once
// Boîte « Style des charges » (à la manière d'un style de cote AutoCAD) : cas affichés, code couleur, texte des valeurs
// (hauteur liée au modèle ou constante à l'écran, décimales, symbole, unité, nom du cas, contour), épaisseur des flèches.
// Aperçu immédiat dans la vue ; Annuler rétablit le style d'origine. Composant partagé TSA / TSALab (couche widgets).
#include "../../Viewer/LoadDisplayStyle.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QRadioButton;
class QSpinBox;

namespace TSA::UI
{

class LoadStyleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoadStyleDialog(QWidget* parent = nullptr);

    /// Réglages affichés par la boîte (aussi appliqués en direct au style des charges).
    TSA::Viewer::LoadDisplaySettings settingsFromUi() const;

protected:
    void reject() override;

private:
    void loadIntoUi(const TSA::Viewer::LoadDisplaySettings& s);
    void applyPreview();
    void updateEnabledStates();

    TSA::Viewer::LoadDisplaySettings m_original;
    QColor m_singleColor;
    std::array<QColor, TSA::Viewer::kLoadCategoryCount> m_categoryColors;
    std::array<QPushButton*, TSA::Viewer::kLoadCategoryCount> m_categoryButtons {};
    QWidget* m_categoryPanel = nullptr;
    void refreshCategoryButtons();
    bool m_loading = false;

    QRadioButton* m_activeCase = nullptr;
    QRadioButton* m_allCases = nullptr;
    QComboBox* m_colorMode = nullptr;
    QPushButton* m_colorButton = nullptr;
    QRadioButton* m_textModel = nullptr;
    QRadioButton* m_textScreen = nullptr;
    QCheckBox* m_autoHeight = nullptr;
    QDoubleSpinBox* m_textHeight = nullptr;
    QSpinBox* m_textPixels = nullptr;
    QSpinBox* m_textMinPixels = nullptr;
    QSpinBox* m_textMaxPixels = nullptr;
    QCheckBox* m_declutter = nullptr;
    QSpinBox* m_decimals = nullptr;
    QCheckBox* m_symbol = nullptr;
    QCheckBox* m_units = nullptr;
    QCheckBox* m_caseName = nullptr;
    QCheckBox* m_background = nullptr;
    QDoubleSpinBox* m_arrowWidth = nullptr;
};

} // namespace TSA::UI
