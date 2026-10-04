#pragma once

#include <QWidget>
#include <QString>
#include <vector>

class QAction;

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
