#pragma once

#include "RibbonTypes.h"
#include <QToolButton>

class QAction;
class QMenu;

namespace TSA::UI
{

class RibbonButton : public QToolButton
{
    Q_OBJECT

public:
    explicit RibbonButton(QAction* action, RibbonButtonSize size = RibbonButtonSize::Large, QWidget* parent = nullptr);
    explicit RibbonButton(const QString& text, const QIcon& icon, RibbonButtonSize size = RibbonButtonSize::Large, QWidget* parent = nullptr);

    RibbonButtonSize ribbonSize() const { return m_size; }
    void setRibbonSize(RibbonButtonSize size);

public:
    void updateTheme(bool isDark);

private:
    void initStyle();

private:
    RibbonButtonSize m_size = RibbonButtonSize::Large;
};

} // namespace TSA::UI
