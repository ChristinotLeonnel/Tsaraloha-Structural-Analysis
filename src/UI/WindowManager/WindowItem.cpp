#include "WindowItem.h"

namespace TSA::UI
{

bool WindowInfo::isVisible() const
{
    if (dockWidget)
    {
        return !dockWidget->isHidden();
    }
    if (widget)
    {
        return !widget->isHidden();
    }
    return false;
}

bool WindowInfo::isFloating() const
{
    if (dockWidget)
    {
        return dockWidget->isFloating();
    }
    return false;
}

QPoint WindowInfo::pos() const
{
    if (dockWidget)
    {
        return dockWidget->pos();
    }
    if (widget)
    {
        return widget->pos();
    }
    return QPoint();
}

QSize WindowInfo::size() const
{
    if (dockWidget)
    {
        return dockWidget->size();
    }
    if (widget)
    {
        return widget->size();
    }
    return QSize();
}

} // namespace TSA::UI
