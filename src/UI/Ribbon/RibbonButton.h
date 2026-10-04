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

    // Mode compact : un petit bouton n'affiche plus que son icône (le libellé reste dans l'infobulle).
    void setIconOnly(bool iconOnly);
    bool isIconOnly() const { return m_iconOnly; }

public:
    void updateTheme(bool isDark);

protected:
    bool event(QEvent* e) override;

private:
    void initStyle();
    QString richToolTip() const;

private:
    RibbonButtonSize m_size = RibbonButtonSize::Large;
    bool m_iconOnly = false;
};

} // namespace TSA::UI
