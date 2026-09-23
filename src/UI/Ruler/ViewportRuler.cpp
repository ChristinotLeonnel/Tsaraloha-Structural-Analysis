#include "ViewportRuler.h"
#include "../../Viewer/OccView.h"
#include "../Theme/ThemeManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>
#include <algorithm>

namespace TSA::UI
{

// =============================================================================
// CornerWidget
// =============================================================================
CornerWidget::CornerWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(34, 22);
}

void CornerWidget::paintEvent(QPaintEvent* /*event*/)
{
    const auto& tm = ThemeManager::instance();
    QPainter p(this);
    p.fillRect(rect(), tm.rulerBackground());

    p.setPen(tm.rulerBorder());
    p.drawLine(rect().right(), 0, rect().right(), rect().bottom());
    p.drawLine(0, rect().bottom(), rect().right(), rect().bottom());

    QFont font = p.font();
    font.setPointSize(8);
    font.setBold(true);
    p.setFont(font);
    p.setPen(tm.rulerText());
    p.drawText(rect(), Qt::AlignCenter, "m");
}

// =============================================================================
// HorizontalRulerWidget
// =============================================================================
HorizontalRulerWidget::HorizontalRulerWidget(OccView* occView, QWidget* parent)
    : QWidget(parent)
    , m_occView(occView)
{
    setFixedHeight(22);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void HorizontalRulerWidget::setCursorPos(int px)
{
    if (m_cursorPixelX != px)
    {
        m_cursorPixelX = px;
        update();
    }
}

void HorizontalRulerWidget::updateRuler()
{
    update();
}

static double calculateNiceStep(double rawStep)
{
    if (rawStep <= 0.0) return 1.0;
    double mag = std::pow(10.0, std::floor(std::log10(rawStep)));
    double ratio = rawStep / mag;
    if (ratio >= 5.0) return 5.0 * mag;
    if (ratio >= 2.0) return 2.0 * mag;
    return mag;
}

void HorizontalRulerWidget::paintEvent(QPaintEvent* /*event*/)
{
    const auto& tm = ThemeManager::instance();
    QPainter p(this);
    p.fillRect(rect(), tm.rulerBackground());

    // Ligne de bordure inférieure
    p.setPen(tm.rulerBorder());
    p.drawLine(0, height() - 1, width(), height() - 1);

    if (!m_occView)
        return;

    // Déterminer la plage monde visible
    double wx0 = 0.0, wy0 = 0.0, wz0 = 0.0;
    double wx1 = 0.0, wy1 = 0.0, wz1 = 0.0;
    bool ok0 = m_occView->pixelToWorldPlane(0, height(), wx0, wy0, wz0);
    bool ok1 = m_occView->pixelToWorldPlane(width(), height(), wx1, wy1, wz1);

    double minVal = 0.0, maxVal = 0.0;
    if (ok0 && ok1)
    {
        minVal = std::min(wx0, wx1);
        maxVal = std::max(wx0, wx1);
    }
    else
    {
        minVal = -25.0;
        maxVal = 25.0;
    }

    double span = maxVal - minVal;
    if (span <= 1e-4) span = 10.0;

    double rawStep = span / std::max(2.0, width() / 85.0);
    double step = calculateNiceStep(rawStep);
    if (step <= 0.0) step = 1.0;

    double startVal = std::floor(minVal / step) * step;
    double endVal = std::ceil(maxVal / step) * step;

    QFont font = p.font();
    font.setPointSize(7);
    p.setFont(font);

    // Dessin des graduations principales et secondaires
    for (double v = startVal; v <= endVal + 0.5 * step; v += step)
    {
        int px = 0, py = 0;
        m_occView->worldToPixel(v, wy0, wz0, px, py);

        if (px >= -20 && px <= width() + 20)
        {
            // Trait majeur
            p.setPen(tm.rulerMajorTick());
            p.drawLine(px, height() - 9, px, height() - 1);

            // Trait mineur médian
            int pxMid = 0, pyMid = 0;
            m_occView->worldToPixel(v + 0.5 * step, wy0, wz0, pxMid, pyMid);
            if (pxMid >= 0 && pxMid <= width())
            {
                p.setPen(tm.rulerMinorTick());
                p.drawLine(pxMid, height() - 5, pxMid, height() - 1);
            }

            // Libellé numérique (format avec virgule ex: -25,0 / 0,0 / 15,0)
            QString txt;
            if (step >= 1.0)
            {
                txt = QString("%1,0").arg(static_cast<int>(std::round(v)));
            }
            else
            {
                txt = QString::number(v, 'f', 1).replace('.', ',');
            }

            p.setPen(tm.rulerText());
            QRect textRect(px - 35, 1, 70, height() - 10);
            p.drawText(textRect, Qt::AlignCenter, txt);
        }
    }

    // Repère suiveur de souris (Hairline curseur)
    if (m_cursorPixelX >= 0 && m_cursorPixelX <= width())
    {
        p.setPen(QColor(0xD3, 0x2F, 0x2F)); // Rouge alerte suiveur
        p.drawLine(m_cursorPixelX, 0, m_cursorPixelX, height() - 1);

        // Petit triangle indicateur en haut de la règle
        QPolygon triangle;
        triangle << QPoint(m_cursorPixelX - 3, 0)
                 << QPoint(m_cursorPixelX + 3, 0)
                 << QPoint(m_cursorPixelX, 4);
        p.setBrush(QColor(0xD3, 0x2F, 0x2F));
        p.drawPolygon(triangle);
    }
}

// =============================================================================
// VerticalRulerWidget
// =============================================================================
VerticalRulerWidget::VerticalRulerWidget(OccView* occView, Position pos, QWidget* parent)
    : QWidget(parent)
    , m_occView(occView)
    , m_position(pos)
{
    setFixedWidth(34);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void VerticalRulerWidget::setCursorPos(int py)
{
    if (m_cursorPixelY != py)
    {
        m_cursorPixelY = py;
        update();
    }
}

void VerticalRulerWidget::updateRuler()
{
    update();
}

void VerticalRulerWidget::paintEvent(QPaintEvent* /*event*/)
{
    const auto& tm = ThemeManager::instance();
    QPainter p(this);
    p.fillRect(rect(), tm.rulerBackground());

    // Ligne de bordure latérale
    p.setPen(tm.rulerBorder());
    if (m_position == Position::Left)
    {
        p.drawLine(width() - 1, 0, width() - 1, height());
    }
    else
    {
        p.drawLine(0, 0, 0, height());
    }

    if (!m_occView)
        return;

    // Échantillonner les coordonnées Z (ou Y) monde entre haut et bas du viewport
    double wx0 = 0.0, wy0 = 0.0, wz0 = 0.0;
    double wx1 = 0.0, wy1 = 0.0, wz1 = 0.0;
    bool ok0 = m_occView->pixelToWorldPlane(width() / 2, 0, wx0, wy0, wz0);
    bool ok1 = m_occView->pixelToWorldPlane(width() / 2, height(), wx1, wy1, wz1);
    if (!ok0 || !ok1)
    {
        wz0 = 10.0;
        wz1 = -10.0;
    }

    // En vue 3D / Face, l'axe vertical d'élévation est Z (ou Y si vue de dessus)
    double valTop = wz0;
    double valBot = wz1;
    if (std::abs(valTop - valBot) < 1e-4)
    {
        valTop = wy0;
        valBot = wy1;
    }

    double minVal = std::min(valTop, valBot);
    double maxVal = std::max(valTop, valBot);

    double span = maxVal - minVal;
    if (span <= 1e-4) span = 10.0;

    double rawStep = span / std::max(2.0, height() / 70.0);
    double step = calculateNiceStep(rawStep);
    if (step <= 0.0) step = 1.0;

    double startVal = std::floor(minVal / step) * step;
    double endVal = std::ceil(maxVal / step) * step;

    QFont font = p.font();
    font.setPointSize(7);
    p.setFont(font);

    for (double v = startVal; v <= endVal + 0.5 * step; v += step)
    {
        int px = 0, py = 0;
        // On projette sur Z
        m_occView->worldToPixel(wx0, wy0, v, px, py);

        if (py >= -20 && py <= height() + 20)
        {
            // Trait majeur
            p.setPen(tm.rulerMajorTick());
            if (m_position == Position::Left)
            {
                p.drawLine(width() - 9, py, width() - 1, py);
            }
            else
            {
                p.drawLine(0, py, 8, py);
            }

            // Trait mineur médian
            int pxMid = 0, pyMid = 0;
            m_occView->worldToPixel(wx0, wy0, v + 0.5 * step, pxMid, pyMid);
            if (pyMid >= 0 && pyMid <= height())
            {
                p.setPen(tm.rulerMinorTick());
                if (m_position == Position::Left)
                {
                    p.drawLine(width() - 5, pyMid, width() - 1, pyMid);
                }
                else
                {
                    p.drawLine(0, pyMid, 4, pyMid);
                }
            }

            // Libellé numérique (texte tourné ou lisible verticalement)
            QString txt;
            if (step >= 1.0)
            {
                txt = QString("%1,0").arg(static_cast<int>(std::round(v)));
            }
            else
            {
                txt = QString::number(v, 'f', 1).replace('.', ',');
            }

            p.save();
            p.setPen(tm.rulerText());

            // Pour une lisibilité optimale, on pivote le texte de 90°
            if (m_position == Position::Left)
            {
                p.translate(14, py);
                p.rotate(-90);
                p.drawText(QRect(-25, -10, 50, 20), Qt::AlignCenter, txt);
            }
            else
            {
                p.translate(width() - 14, py);
                p.rotate(90);
                p.drawText(QRect(-25, -10, 50, 20), Qt::AlignCenter, txt);
            }
            p.restore();
        }
    }

    // Repère suiveur de souris
    if (m_cursorPixelY >= 0 && m_cursorPixelY <= height())
    {
        p.setPen(QColor(0xD3, 0x2F, 0x2F));
        p.drawLine(0, m_cursorPixelY, width() - 1, m_cursorPixelY);

        QPolygon triangle;
        if (m_position == Position::Left)
        {
            triangle << QPoint(0, m_cursorPixelY - 3)
                     << QPoint(0, m_cursorPixelY + 3)
                     << QPoint(4, m_cursorPixelY);
        }
        else
        {
            triangle << QPoint(width() - 1, m_cursorPixelY - 3)
                     << QPoint(width() - 1, m_cursorPixelY + 3)
                     << QPoint(width() - 5, m_cursorPixelY);
        }
        p.setBrush(QColor(0xD3, 0x2F, 0x2F));
        p.drawPolygon(triangle);
    }
}

} // namespace TSA::UI
