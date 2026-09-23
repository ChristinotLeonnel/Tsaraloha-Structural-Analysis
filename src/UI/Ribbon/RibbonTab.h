#pragma once

#include <QWidget>
#include <vector>

class QHBoxLayout;
class QScrollArea;

namespace TSA::UI
{

class RibbonPanel;

class RibbonTab : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonTab(QWidget* parent = nullptr);

    void addPanel(RibbonPanel* panel);
    const std::vector<RibbonPanel*>& panels() const { return m_panels; }

    void updateTheme(bool isDark);

private:
    void setupUi();

private:
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_container = nullptr;
    QHBoxLayout* m_panelLayout = nullptr;
    std::vector<RibbonPanel*> m_panels;
};

} // namespace TSA::UI
