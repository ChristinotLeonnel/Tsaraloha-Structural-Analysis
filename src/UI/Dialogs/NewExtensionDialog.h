#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QString>

#include "../../ExtensionSystem/ExtensionScaffolder.h"

namespace TSA::UI
{

/**
 * @brief Dialogue interactif moderne pour la création et l'échafaudage de bibliothèques TSALib.
 * Guide l'utilisateur pas à pas pour initialiser les dossiers, fichiers manifest, fiches types
 * de matériaux, sections et profilés, et générer le package .tsalib en 1 clic.
 */
class NewExtensionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewExtensionDialog(QWidget* parent = nullptr);
    ~NewExtensionDialog() override = default;

    /**
     * @brief Retourne l'identifiant de l'extension créée.
     */
    QString createdExtensionId() const { return m_createdId; }

    /**
     * @brief Retourne le répertoire de l'extension créée.
     */
    QString createdExtensionDirectory() const { return m_createdDirectory; }

signals:
    void extensionCreated(const QString& extensionId, const QString& directory);

private slots:
    void onIdTextChanged(const QString& text);
    void onBrowseCustomFolder();
    void onLocationTypeChanged();
    void onCreateClicked();

private:
    void setupUi();
    void updateTargetPathPreview();
    bool validateInputs(QString* outMessage = nullptr);

private:
    // Identité
    QLineEdit* m_editId = nullptr;
    QLabel* m_lblIdValidation = nullptr;
    QLineEdit* m_editName = nullptr;
    QLineEdit* m_editVersion = nullptr;
    QLineEdit* m_editAuthor = nullptr;
    QComboBox* m_comboLicense = nullptr;
    QTextEdit* m_editDescription = nullptr;

    // Catégories
    QCheckBox* m_chkMaterials = nullptr;
    QCheckBox* m_chkSections = nullptr;
    QCheckBox* m_chkProfiles = nullptr;
    QCheckBox* m_chkCables = nullptr;
    QCheckBox* m_chkTextures = nullptr;
    QCheckBox* m_chkStandards = nullptr;

    // Emplacement
    QRadioButton* m_radioProjectExtensions = nullptr;
    QRadioButton* m_radioUserExtensions = nullptr;
    QRadioButton* m_radioCustomLocation = nullptr;
    QLineEdit* m_editCustomPath = nullptr;
    QPushButton* m_btnBrowseCustom = nullptr;
    QLabel* m_lblPathPreview = nullptr;

    // Options post-création
    QCheckBox* m_chkCreatePackage = nullptr;
    QCheckBox* m_chkLoadImmediately = nullptr;

    // Actions
    QPushButton* m_btnCreate = nullptr;
    QPushButton* m_btnCancel = nullptr;

    // Résultats
    QString m_createdId;
    QString m_createdDirectory;
};

} // namespace TSA::UI
