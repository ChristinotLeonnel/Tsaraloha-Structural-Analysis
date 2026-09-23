#pragma once

#include <QWidget>
#include <QString>
#include <vector>

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

    void updateTheme(bool isDark);

private:
    void setupUi();

private:
    QTabWidget* m_tabWidget = nullptr;
    std::vector<RibbonTab*> m_tabs;
};

} // namespace TSA::UI
