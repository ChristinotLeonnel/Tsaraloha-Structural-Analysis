#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "../../Library/LibraryManager.h"
#include "../../Model/Model.h"

namespace TSA::UI
{

class LibraryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LibraryDialog(TSA::Model::Model* model = nullptr, QWidget* parent = nullptr);
    ~LibraryDialog() override = default;

    void selectTab(int index);

signals:
    void sectionLibraryUpdated();
    void materialLibraryUpdated();
    void colorLibraryUpdated();

private slots:
    // Onglet Sections
    void onAddCustomSection();
    void onDeleteCustomSection();
    void onSectionShapeChanged(int index);

    // Onglet Matériaux
    void onAddCustomMaterial();
    void onDeleteCustomMaterial();

    // Onglet Couleurs
    void onAddCustomColor();
    void onDeleteCustomColor();
    void onPickColor();

    // Onglet Textures
    void onAddCustomTexture();
    void onDeleteCustomTexture();
    void onBrowseTextureFile();

    // Onglet Structures
    void onSaveCurrentModelAsTemplate();
    void onInsertTemplateIntoModel();
    void onDeleteTemplate();

private:
    void setupUi();
    QWidget* createSectionsTab();
    QWidget* createMaterialsTab();
    QWidget* createColorsTab();
    QWidget* createTexturesTab();
    QWidget* createStructuresTab();

    void refreshSectionsTable();
    void refreshMaterialsTable();
    void refreshColorsTable();
    void refreshTexturesTable();
    void refreshStructuresTable();

private:
    TSA::Model::Model* m_model = nullptr;

    QTabWidget* m_tabWidget = nullptr;

    // Sections
    QTableWidget* m_sectionsTable = nullptr;
    QLineEdit* m_secNameEdit = nullptr;
    QComboBox* m_secShapeCombo = nullptr;
    QDoubleSpinBox* m_secWidthSpin = nullptr;
    QDoubleSpinBox* m_secHeightSpin = nullptr;
    QDoubleSpinBox* m_secTwSpin = nullptr;
    QDoubleSpinBox* m_secTfSpin = nullptr;
    QDoubleSpinBox* m_secDiameterSpin = nullptr;

    // Matériaux
    QTableWidget* m_materialsTable = nullptr;
    QLineEdit* m_matNameEdit = nullptr;
    QComboBox* m_matTypeCombo = nullptr;
    QDoubleSpinBox* m_matESpin = nullptr;
    QDoubleSpinBox* m_matNuSpin = nullptr;
    QDoubleSpinBox* m_matRhoSpin = nullptr;
    QDoubleSpinBox* m_matFkSpin = nullptr;

    // Couleurs
    QTableWidget* m_colorsTable = nullptr;
    QLineEdit* m_colorNameEdit = nullptr;
    QPushButton* m_colorPickBtn = nullptr;
    QComboBox* m_colorCategoryCombo = nullptr;
    QString m_currentColorHex;

    // Textures
    QTableWidget* m_texturesTable = nullptr;
    QLineEdit* m_texNameEdit = nullptr;
    QLineEdit* m_texPathEdit = nullptr;
    QComboBox* m_texCategoryCombo = nullptr;
    QDoubleSpinBox* m_texRepeatUSpin = nullptr;
    QDoubleSpinBox* m_texRepeatVSpin = nullptr;

    // Structures
    QTableWidget* m_templatesTable = nullptr;
    QLineEdit* m_tplNameEdit = nullptr;
    QLineEdit* m_tplDescEdit = nullptr;
    QComboBox* m_tplCategoryCombo = nullptr;
};

} // namespace TSA::UI
