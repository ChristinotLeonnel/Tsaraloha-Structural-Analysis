#pragma once

#include <QHash>
#include <QWidget>
#include <QString>
#include <vector>

class QAction;

class QTabBar;
class QTabWidget;
class QVBoxLayout;

namespace TSA::UI
{

class RibbonTab;

class RibbonBar : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonBar(QWidget* parent = nullptr);

    RibbonTab* addTab(const QString& title);
    int currentTabIndex() const;
    void setCurrentTabIndex(int index);
    int tabCount() const;
    QString tabTitle(int index) const;
    RibbonTab* tab(int index) const;
    /// Rangée des onglets (largeur des libellés dans chaque langue).
    QTabBar* tabBar() const;

    /// Emplacement de chaque action du ruban : « Onglet › Groupe » (premier emplacement si l'action est répétée).
    QHash<const QAction*, QString> actionLocations() const;
    /// Affiche l'onglet qui contient l'action ; faux si elle n'est pas dans le ruban.
    bool revealAction(const QAction* action);

    // Barre d'accès rapide (coin gauche de la rangée d'onglets) : icônes seules, toujours visibles.
    // Une action nulle est ignorée ; une action nulle précédée d'un groupe n'ajoute pas de séparateur.
    void setQuickAccess(const std::vector<QAction*>& actions);

    void updateTheme(bool isDark);

private:
    void setupUi();

private:
    QTabWidget* m_tabWidget = nullptr;
    std::vector<RibbonTab*> m_tabs;
};

} // namespace TSA::UI
