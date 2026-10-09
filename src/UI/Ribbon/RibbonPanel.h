#pragma once

#include <QWidget>
#include <vector>
#include "RibbonTypes.h"

class QAction;
class QMenu;
class QHBoxLayout;
class QVBoxLayout;
class QLabel;

namespace TSA::UI
{

class RibbonButton;

// Niveaux de densité d'un panneau, du plus riche au plus compact.
enum class RibbonPanelMode
{
    Full,       // libellés complets
    IconOnly,   // petits boutons réduits à leur icône (infobulle conservée)
    Collapsed   // un seul bouton déroulant portant le titre du panneau
};

class RibbonPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonPanel(const QString& title, QWidget* parent = nullptr);

    const QString& title() const { return m_title; }

    // Ajoute un grand bouton principal (32x32 avec texte en-dessous)
    RibbonButton* addLargeAction(QAction* action, QMenu* menu = nullptr);

    // Ajoute une colonne verticale contenant 2 ou 3 petits boutons compacts (façon AutoCAD)
    std::vector<RibbonButton*> addSmallColumn(const std::vector<QAction*>& actions);

    // Ajoute un bouton de menu déroulant (dropdown / split button)
    RibbonButton* addMenuButton(const QString& text, const QIcon& icon, QMenu* menu, RibbonButtonSize size = RibbonButtonSize::Large);

    // Ajoute un séparateur interne discret
    void addInternalSeparator();

    // Ajoute un widget personnalisé dans le panneau
    void addCustomWidget(QWidget* widget);

    void updateTheme(bool isDark);

    // Responsive : le RibbonTab choisit le mode selon la largeur disponible.
    void setMode(RibbonPanelMode mode);
    RibbonPanelMode mode() const { return m_mode; }
    /// Largeur du panneau dans ce mode, mesurée une fois puis mise en cache : la disposition du
    /// ruban ne dépend ainsi que de la largeur disponible (même largeur → même disposition).
    int widthForMode(RibbonPanelMode mode);
    /// Oublie les largeurs mesurées (thème, police, style changés). Vrai si un cache existait.
    bool invalidateWidths();
    bool hasCompactableButtons() const { return !m_smallButtons.empty(); }

    // Menu reprenant toutes les commandes du panneau (bouton replié, menu « Plus »).
    // Le menu est créé à la demande, parenté à  parent.
    QMenu* buildOverflowMenu(QWidget* parent) const;
    bool isEmptyPanel() const { return m_entries.empty(); }

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void trackAction(QAction* action);
    void trackSeparator();
    void measureWidths();

private:
    struct Entry { QAction* action = nullptr; }; // action == nullptr : séparateur de groupe

    QString m_title;
    RibbonPanelMode m_mode = RibbonPanelMode::Full;
    QWidget* m_contentHost = nullptr;
    RibbonButton* m_collapsedButton = nullptr;
    std::vector<Entry> m_entries;
    std::vector<RibbonButton*> m_smallButtons;
    QHBoxLayout* m_contentLayout = nullptr;
    QLabel* m_lblTitle = nullptr;
    std::vector<QWidget*> m_separators;
    int m_widths[3] = { 0, 0, 0 };   // indexé par RibbonPanelMode
    bool m_widthsValid = false;
    bool m_measuring = false;
};

} // namespace TSA::UI
