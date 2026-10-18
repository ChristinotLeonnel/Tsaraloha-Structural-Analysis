#pragma once

// Choix de la langue de l'interface (English, Français, Malagasy ; docs/I18N.md). La langue s'applique au démarrage :
// la boîte propose de redémarrer TSA tout de suite (projets enregistrés rouverts) ou au prochain lancement. Ses
// explications s'affichent dans la langue sélectionnée, pour qui ne lit pas la langue actuelle.

#include "../../Core/I18n/LanguageManager.h"

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;

namespace TSA::UI
{

class LanguageDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Choice
    {
        Cancelled,
        Later,       ///< préférence enregistrée, appliquée au prochain démarrage
        RestartNow   ///< préférence enregistrée, redémarrage demandé
    };

    explicit LanguageDialog(TSA::Core::I18n::Language current, QWidget* parent = nullptr);

    TSA::Core::I18n::Language selectedLanguage() const;
    Choice choice() const { return m_choice; }

private:
    void refreshTexts();

    TSA::Core::I18n::Language m_current;
    QListWidget* m_list = nullptr;
    QLabel* m_note = nullptr;
    QPushButton* m_restart = nullptr;
    QPushButton* m_later = nullptr;
    QPushButton* m_cancel = nullptr;
    Choice m_choice = Choice::Cancelled;
};

} // namespace TSA::UI
