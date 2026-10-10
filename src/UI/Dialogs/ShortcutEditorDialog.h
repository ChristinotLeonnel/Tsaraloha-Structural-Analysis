#pragma once

// Éditeur des raccourcis clavier (Aide > Personnaliser les raccourcis, Ctrl+F1). Travaille sur une copie
// des réglages de ShortcutManager ; « Appliquer » valide (conflits compris), écrit shortcut.txt de façon
// atomique et applique : le rechargement à chaud reconnaît sa propre écriture (pas de boucle).

#include "../Shortcuts/ShortcutConfig.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace TSA::UI::Shortcuts
{
class ShortcutManager;
}

namespace TSA::UI
{

class ShortcutEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ShortcutEditorDialog(TSA::UI::Shortcuts::ShortcutManager& manager, QWidget* parent = nullptr);

    // Accès pour les tests
    void setSearchText(const QString& text);
    int visibleRowCount() const;
    bool selectCommand(const QString& id);
    /// Remplace les raccourcis de la commande sélectionnée (texte « Ctrl+S ; F7 »). false : texte invalide.
    bool setSelectedShortcutText(const QString& text);
    void setSelectedEnabled(bool enabled);
    void resetSelected();
    void resetAll();
    /// Conflits de la copie de travail (vide : applicable).
    QStringList conflicts() const;
    bool apply(QStringList* errors = nullptr);
    TSA::UI::Shortcuts::ShortcutSettings workingSettings() const { return m_work; }

private:
    void populate();
    void refreshRow(int row);
    void refreshAllRows();
    void refreshFilter();
    void refreshEditor();
    void refreshConflicts();
    QString selectedId() const;
    int rowOf(const QString& id) const;
    TSA::UI::Shortcuts::ShortcutSetting workingSetting(const QString& id) const;

    TSA::UI::Shortcuts::ShortcutManager& m_manager;
    QList<TSA::UI::Shortcuts::CommandDefinition> m_defs;
    TSA::UI::Shortcuts::ShortcutSettings m_work;
    QLineEdit* m_search = nullptr;
    QComboBox* m_category = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_selectedLabel = nullptr;
    QKeySequenceEdit* m_recorder = nullptr;
    QLineEdit* m_sequenceText = nullptr;
    QCheckBox* m_enabled = nullptr;
    QLabel* m_conflictLabel = nullptr;
    QLabel* m_status = nullptr;
    bool m_updating = false;
};

} // namespace TSA::UI
