#pragma once

// Champ de recherche globale de la barre de titre (docs/UI_SEARCH.md). Saisie → résultats classés (CommandSearchIndex)
// dans une liste déroulante : icône, libellé, raccourci, nature, emplacement dans le ruban, description. Clavier :
// flèches, Pg.Préc / Pg.Suiv, Entrée (exécuter), Échap (fermer la liste, puis effacer). La recherche est différée de
// quelques millisecondes après la dernière frappe et ne bloque pas la saisie.
//
// Le widget n'exécute rien : il émet activated(identifiant) ; le shell déclenche la vraie commande (ShortcutManager),
// ouvre le panneau ou explique pourquoi la commande n'est pas disponible (showFeedback).

#include "../../Commands/CommandSearchIndex.h"

#include <QIcon>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QFrame;
class QTimer;

namespace TSA::UI
{

class GlobalSearchWidget : public QWidget
{
    Q_OBJECT

public:
    using EntriesProvider = std::function<QList<TSA::Commands::SearchEntry>()>;
    using IconProvider = std::function<QIcon(const TSA::Commands::SearchEntry&)>;

    explicit GlobalSearchWidget(QWidget* parent = nullptr);

    /// Source des entrées, relue à chaque ouverture de la recherche (document actif, commandes des extensions).
    void setEntriesProvider(EntriesProvider provider);
    void setIconProvider(IconProvider provider);
    /// Raccourci affiché dans le texte indicatif (« Ctrl+F »).
    void setShortcutText(const QString& text);

    /// Donne le focus au champ et sélectionne son texte (raccourci de la recherche).
    void focusSearch();
    /// Message sous le champ (commande indisponible, panneau ouvert…), gardé pour les essais.
    void showFeedback(const QString& message);
    QString lastFeedback() const { return m_lastFeedback; }

    // Accès pour les essais et l'accessibilité
    QLineEdit* lineEdit() const { return m_edit; }
    QListWidget* resultsList() const { return m_list; }
    bool isPopupVisible() const;
    const TSA::Commands::CommandSearchIndex& index() const { return m_index; }
    /// Relit les entrées auprès du fournisseur.
    void refreshIndex();
    /// Recherche immédiate (sans attendre la temporisation).
    void runSearchNow();
    /// Identifiants affichés, dans l'ordre (vide si aucun résultat).
    QStringList resultIds() const;

signals:
    void activated(const QString& id);
    /// Le champ a perdu le focus (recherche terminée ou abandonnée).
    void searchClosed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateResults();
    void activateRow(int row);
    void moveSelection(int delta);
    void showPopup();
    void hidePopup();
    void positionPopup();
    void applyTheme(bool dark);
    QString kindLabel(TSA::Commands::SearchKind kind) const;

    QLineEdit* m_edit = nullptr;
    QFrame* m_popup = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_hint = nullptr;
    QTimer* m_debounce = nullptr;
    TSA::Commands::CommandSearchIndex m_index;
    EntriesProvider m_provider;
    IconProvider m_iconProvider;
    QString m_shortcutText;
    QString m_lastFeedback;
    bool m_indexStale = true;
    bool m_followsWindow = false;
};

} // namespace TSA::UI
