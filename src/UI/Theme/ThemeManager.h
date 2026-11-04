#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariant>

#include <vector>

class QWidget;

namespace TSA::UI
{

/// Jetons sémantiques d'un thème (docs/UI/TSA_UI_DESIGN_STANDARD.md § 2) : SEULE source des couleurs de
/// l'interface. Un jeton nomme une fonction (« fond d'un champ », « texte d'erreur »), jamais une teinte.
/// Les couleurs métier du viewport (axes, éléments, charges, résultats) et du Blueprint n'en font pas partie.
struct ThemeTokens
{
    // Surfaces, du plus bas au plus haut
    QColor canvas;          ///< fond de la fenêtre principale, entre les docks
    QColor panel;           ///< fond des docks, dialogues, menus
    QColor surface;         ///< fond des champs, listes, tableaux (zones qui reçoivent du contenu)
    QColor surfaceAlt;      ///< lignes alternées, champs en lecture seule ou désactivés
    QColor raised;          ///< boutons, onglet actif, fond du ruban
    QColor chrome;          ///< barres de menus, d'état, d'outils, titres de dock, en-têtes de tableau
    QColor hover;           ///< survol d'un élément
    QColor pressed;         ///< élément enfoncé
    QColor selection;       ///< fond d'un élément sélectionné
    QColor selectionText;   ///< texte d'un élément sélectionné
    // Bordures
    QColor border;          ///< bordure standard des cadres et champs
    QColor borderSubtle;    ///< séparation discrète (grille de tableau, onglets inactifs)
    QColor borderStrong;    ///< bordure accentuée (survol neutre, poignées)
    QColor focus;           ///< contour du focus clavier
    // Texte
    QColor text;            ///< texte principal
    QColor textSecondary;   ///< texte de second niveau (libellés du ruban, valeurs secondaires)
    QColor textMuted;       ///< métadonnées, unités, aides
    QColor textDisabled;    ///< texte désactivé (non soumis au contraste minimal)
    QColor textOnAccent;    ///< texte posé sur une couleur pleine (bouton principal)
    QColor hoverText;       ///< texte d'un élément du ruban ou d'un menu survolé
    // Accent (une seule teinte)
    QColor accent;          ///< bouton principal, onglet actif
    QColor accentHover;
    QColor accentPressed;
    QColor accentBorder;    ///< contour d'un élément actif ou coché
    QColor accentText;      ///< accent lisible comme texte (liens, titres de groupe, info)
    QColor accentSubtle;    ///< fond léger d'un élément actif, d'un badge, d'une saisie mise en avant
    // États sémantiques : couleur pleine, survol, enfoncé, texte lisible, fond léger
    QColor success, successHover, successPressed, successText, successSubtle;
    QColor warning, warningText, warningSubtle;
    QColor danger, dangerHover, dangerPressed, dangerText, dangerSubtle;
    // Console et journaux
    QColor console, consoleText;
    QColor logTime, logInfo, logCommand, logSystem, logWarning, logError, logCritical, logDebug;
    // Règles graduées du viewport
    QColor rulerMajorTick, rulerMinorTick;
};

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager& instance();

    bool isDarkMode() const { return m_isDark; }
    void setDarkMode(bool dark, bool force = false);
    void toggleTheme();

    // Méthodes statiques pratiques
    static bool isDark() { return instance().isDarkMode(); }
    static void setDark(bool dark, bool force = false) { instance().setDarkMode(dark, force); }

    /// Jetons du thème actif, du thème sombre, du thème clair.
    const ThemeTokens& tokens() const;
    static const ThemeTokens& darkTokens();
    static const ThemeTokens& lightTokens();
    /// Noms des jetons, dans l'ordre de ThemeTokens (« panel », « textMuted »…).
    static std::vector<QString> tokenNames();
    /// Couleur d'un jeton par son nom ; invalide si le nom est inconnu.
    static QColor tokenColor(const ThemeTokens& tokens, const QString& name);

    /// Feuille de style modèle → feuille du thème actif : chaque « @nom » est remplacé par la couleur du jeton
    /// (« @textMuted » → « #59636e »). Seul moyen d'écrire une couleur dans une feuille de style locale.
    QString resolve(const QString& styleSheetTemplate) const;
    static QString resolve(const QString& styleSheetTemplate, const ThemeTokens& tokens);
    /// Feuille de style de l'application pour le thème actif (appliquée par setDarkMode).
    QString applicationStyleSheet() const;

    /// Couleur d'un niveau de journal (CMD, SYS, INFO, WARN, ERR / ERROR, CRIT, DEBUG ; autre : INFO).
    QColor logLevelColor(const QString& level) const;

    // Couleurs des règles graduées
    QColor rulerBackground() const;
    QColor rulerBorder() const;
    QColor rulerMajorTick() const;
    QColor rulerMinorTick() const;
    QColor rulerText() const;

    // Couleurs sémantiques et de thème (alias des jetons, conservés pour les appelants existants)
    QColor primaryColor() const;
    QColor windowColor() const;
    QColor surfaceColor() const;
    QColor borderColor() const;
    QColor textColor() const;
    QColor textMutedColor() const;
    QColor successColor() const;
    QColor warningColor() const;
    QColor destructiveColor() const;
    QColor infoColor() const;

    // Stylesheets spécifiques
    QString ribbonTabWidgetStyleSheet() const;
    QString ribbonScrollStyleSheet() const;
    QString ribbonContainerStyleSheet() const;
    QString ribbonPanelStyleSheet() const;
    QString ribbonPanelTitleStyleSheet() const;
    QString ribbonSeparatorStyleSheet() const;
    QString ribbonButtonLargeStyleSheet() const;
    QString ribbonButtonSmallStyleSheet() const;
    QString topBarStyleSheet() const;
    QString topBarButtonStyleSheet() const;
    QString topBarComboStyleSheet() const;
    QString primaryButtonStyleSheet() const;
    QString destructiveButtonStyleSheet() const;
    QString successButtonStyleSheet() const;

signals:
    void themeChanged(bool isDark);

private:
    ThemeManager();
    void applyApplicationTheme();

    bool m_isDark = true;
};

/// Donne une valeur à une propriété dynamique de style (« active », « invalid »…) et réapplique la feuille de style
/// au widget : Qt ne réévalue pas seul les sélecteurs [propriété="valeur"] quand la propriété change.
void setStyleProperty(QWidget* widget, const char* name, const QVariant& value);

} // namespace TSA::UI
