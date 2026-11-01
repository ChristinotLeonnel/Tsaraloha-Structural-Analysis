#include "WheelGuard.h"

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QScrollBar>
#include <QWheelEvent>

#include <cstdlib>

namespace TSA::UI
{

WheelGuard::WheelGuard(QObject* parent) : QObject(parent) {}

void WheelGuard::install()
{
    if (!qApp || qApp->findChild<WheelGuard*>(QString(), Qt::FindDirectChildrenOnly))
        return;
    qApp->installEventFilter(new WheelGuard(qApp));
}

QWidget* WheelGuard::protectedField(QWidget* widget)
{
    if (!widget) return nullptr;
    // Liste d'une liste déroulante ouverte (fenêtre surgissante) : défilement normal de la liste.
    if (widget->window() && widget->window()->windowType() == Qt::Popup) return nullptr;
    for (QWidget* p = widget; p && p->window() == widget->window(); p = p->parentWidget())
        if (qobject_cast<QAbstractSpinBox*>(p) || qobject_cast<QComboBox*>(p)) return p;
    return nullptr;
}

bool WheelGuard::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Polish)
    {
        // Un champ à la politique WheelFocus prenait le focus à la molette (avant tout filtre d'événements) : il ne le
        // prend plus qu'au clic ou au clavier.
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && (qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QComboBox*>(w)) && w->focusPolicy() == Qt::WheelFocus)
            w->setFocusPolicy(Qt::StrongFocus);
        return false;
    }
    if (event->type() != QEvent::Wheel) return false;
    QWidget* field = protectedField(qobject_cast<QWidget*>(watched));
    if (!field) return false;

    // Défilement transmis à la première zone défilante qui contient le champ (panneau, boîte de dialogue, tableau).
    for (QWidget* p = field->parentWidget(); p; p = p->parentWidget())
    {
        if (auto* area = qobject_cast<QAbstractScrollArea*>(p))
        {
            auto* wheel = static_cast<QWheelEvent*>(event);
            const bool horizontal = std::abs(wheel->angleDelta().x()) > std::abs(wheel->angleDelta().y());
            QScrollBar* bar = horizontal ? area->horizontalScrollBar() : area->verticalScrollBar();
            if (bar && bar->isVisible()) QApplication::sendEvent(bar, event);
            return true;
        }
    }
    return true;   // aucune zone défilante : molette simplement ignorée par le champ
}

} // namespace TSA::UI
