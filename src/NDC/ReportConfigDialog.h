#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTabWidget>

#include "ReportConfiguration.h"
#include "ReportTemplate.h"

namespace TSA::NDC
{

/**
 * @brief Boîte de dialogue modale de personnalisation complète de la Note de Calcul (NDC).
 * Permet d'éditer les métadonnées de projet, la charte graphique, les marges,
 * la sélection granulaire des chapitres/sections et la gestion des templates (.tsareport).
 */
class ReportConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportConfigDialog(const ReportConfiguration& config, QWidget* parent = nullptr);
    ~ReportConfigDialog() override = default;

    ReportConfiguration configuration() const;

private slots:
    void onApplyTemplateClicked();
    void onLoadFromFileClicked();
    void onSaveToFileClicked();

private:
    void setupUi();
    void loadFromConfig(const ReportConfiguration& cfg);

private:
    ReportConfiguration m_config;

    // Onglet 1 : Informations
    QLineEdit* m_editTitle = nullptr;
    QLineEdit* m_editDesc = nullptr;
    QLineEdit* m_editProjNum = nullptr;
    QLineEdit* m_editDocNum = nullptr;
    QLineEdit* m_editRev = nullptr;
    QLineEdit* m_editStatus = nullptr;
    QLineEdit* m_editEngineer = nullptr;
    QLineEdit* m_editOrg = nullptr;
    QLineEdit* m_editClient = nullptr;
    QLineEdit* m_editDate = nullptr;

    // Onglet 2 : Mise en page
    QComboBox* m_comboPageFormat = nullptr;
    QComboBox* m_comboOrientation = nullptr;
    QDoubleSpinBox* m_spinMarginLeft = nullptr;
    QDoubleSpinBox* m_spinMarginRight = nullptr;
    QDoubleSpinBox* m_spinMarginTop = nullptr;
    QDoubleSpinBox* m_spinMarginBottom = nullptr;
    QCheckBox* m_chkShowLogo = nullptr;
    QLineEdit* m_editPrimaryColor = nullptr;
    QSpinBox* m_spinFontSize = nullptr;

    // Onglet 3 : Sections & Résultats
    QCheckBox* m_chkCover = nullptr;
    QCheckBox* m_chkToc = nullptr;
    QCheckBox* m_chkLof = nullptr;
    QCheckBox* m_chkLot = nullptr;
    QCheckBox* m_chkIntro = nullptr;
    QCheckBox* m_chkStandards = nullptr;
    QCheckBox* m_chkGeom = nullptr;
    QCheckBox* m_chkMaterials = nullptr;
    QCheckBox* m_chkSections = nullptr;
    QCheckBox* m_chkBoundary = nullptr;
    QCheckBox* m_chkLoads = nullptr;
    QCheckBox* m_chkMethod = nullptr;
    QCheckBox* m_chkQuality = nullptr;
    QCheckBox* m_chkSnapshots = nullptr;
    QCheckBox* m_chkDisp = nullptr;
    QCheckBox* m_chkDefl = nullptr;
    QCheckBox* m_chkMoment = nullptr;
    QCheckBox* m_chkShear = nullptr;
    QCheckBox* m_chkAxial = nullptr;
    QCheckBox* m_chkTorsion = nullptr;
    QCheckBox* m_chkExtrema = nullptr;
    QCheckBox* m_chkDetailedTables = nullptr;
    QCheckBox* m_chkEurocode = nullptr;
    QCheckBox* m_chkWarn = nullptr;
    QCheckBox* m_chkConclusion = nullptr;
    QCheckBox* m_chkBiblio = nullptr;

    // Onglet 4 : Modèles (Templates)
    QComboBox* m_comboTemplate = nullptr;
    QPushButton* m_btnApplyTemplate = nullptr;
    QPushButton* m_btnLoadFile = nullptr;
    QPushButton* m_btnSaveFile = nullptr;
};

} // namespace TSA::NDC
