#include "SectionPreviewWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <cmath>
#include <algorithm>

namespace TSA::UI
{

SectionPreviewWidget::SectionPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void SectionPreviewWidget::setSection(const TSA::Model::Section& section)
{
    m_section = section;
    update();
}

void SectionPreviewWidget::setRotation(double gammaDegrees)
{
    m_gamma = gammaDegrees;
    update();
}

void SectionPreviewWidget::setEccentricity(TSA::Model::BarEccentricity ecc, double ey, double ez)
{
    m_eccentricity = ecc;
    m_ey = ey;
    m_ez = ez;
    update();
}

void SectionPreviewWidget::setShowDimensions(bool show)
{
    m_showDimensions = show;
    update();
}

void SectionPreviewWidget::setShowAxes(bool show)
{
    m_showAxes = show;
    update();
}

void SectionPreviewWidget::setShowCentroid(bool show)
{
    m_showCentroid = show;
    update();
}

void SectionPreviewWidget::setShowBarAxis(bool show)
{
    m_showBarAxis = show;
    update();
}

QSize SectionPreviewWidget::sizeHint() const
{
    return QSize(260, 200);
}

QSize SectionPreviewWidget::minimumSizeHint() const
{
    return QSize(200, 160);
}

void SectionPreviewWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();

    // 1. Fond technique style CAO pro
    p.fillRect(rect(), QColor(28, 32, 38));
    p.setPen(QColor(60, 68, 80));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    // Titre discret d'en-tête
    p.setPen(QColor(160, 175, 195));
    QFont fontHdr = font();
    fontHdr.setPointSize(8);
    fontHdr.setBold(true);
    p.setFont(fontHdr);
    QString secTitle = QString::fromStdString(m_section.name);
    if (secTitle.isEmpty()) secTitle = tr("Section");
    p.drawText(8, 16, secTitle.toUpper());

    // Affichage angle de rotation gamma
    if (std::abs(m_gamma) > 1e-2)
    {
        QString rotStr = QString("γ = %1°").arg(m_gamma, 0, 'f', 1);
        p.setPen(QColor(255, 180, 50));
        p.drawText(w - p.fontMetrics().horizontalAdvance(rotStr) - 10, 16, rotStr);
    }

    // 2. Détermination des dimensions géométriques enveloppes réelles (en mètres)
    double realW = 0.30;
    double realH = 0.50;

    switch (m_section.shape)
    {
    case TSA::Model::SectionShape::Circular:
    case TSA::Model::SectionShape::Pipe:
    {
        double d = (m_section.diameter > 0.0) ? m_section.diameter : m_section.width;
        realW = d;
        realH = d;
        break;
    }
    default:
        realW = std::max(0.01, m_section.width);
        realH = std::max(0.01, m_section.height);
        break;
    }

    // Marges pour le dessin et les cotes
    const double margin = 42.0;
    const double drawAreaW = std::max(40.0, w - 2.0 * margin);
    const double drawAreaH = std::max(40.0, h - 2.0 * margin - 15.0);

    // Échelle : pixels par mètre
    double scale = std::min(drawAreaW / realW, drawAreaH / realH);
    if (scale <= 0.0) scale = 100.0;

    // Dimensions en pixels
    double pixW = realW * scale;
    double pixH = realH * scale;

    const QPointF center(w / 2.0, h / 2.0 + 8.0);

    // Sauvegarder l'état avant transformation
    p.save();
    p.translate(center);
    p.rotate(-m_gamma); // Rotation visuelle de la section et de ses axes locaux

    // 3. Dessin du profilé selon sa forme
    QColor fillColor(45, 125, 210, 140);
    QColor strokeColor(100, 185, 255);
    p.setBrush(fillColor);
    p.setPen(QPen(strokeColor, 1.8));

    QPainterPath path;

    switch (m_section.shape)
    {
    case TSA::Model::SectionShape::Circular:
    {
        double r = (pixW / 2.0);
        path.addEllipse(QPointF(0, 0), r, r);
        p.drawPath(path);
        break;
    }
    case TSA::Model::SectionShape::Pipe:
    {
        double ro = pixW / 2.0;
        double tw = (m_section.tw > 0.0 && m_section.tw < m_section.diameter / 2.0) ? m_section.tw : 0.01;
        double ri = std::max(2.0, (m_section.diameter / 2.0 - tw) * scale);
        path.addEllipse(QPointF(0, 0), ro, ro);
        QPainterPath innerPath;
        innerPath.addEllipse(QPointF(0, 0), ri, ri);
        path = path.subtracted(innerPath);
        p.drawPath(path);
        break;
    }
    case TSA::Model::SectionShape::IShape:
    {
        double b2 = pixW / 2.0;
        double h2 = pixH / 2.0;
        double twPix = std::max(3.0, (m_section.tw > 0.0 ? m_section.tw : 0.007) * scale);
        double tfPix = std::max(4.0, (m_section.tf > 0.0 ? m_section.tf : 0.011) * scale);
        double w2 = twPix / 2.0;
        double hin = h2 - tfPix;

        path.moveTo( b2,  h2);
        path.lineTo(-b2,  h2);
        path.lineTo(-b2,  hin);
        path.lineTo(-w2,  hin);
        path.lineTo(-w2, -hin);
        path.lineTo(-b2, -hin);
        path.lineTo(-b2, -h2);
        path.lineTo( b2, -h2);
        path.lineTo( b2, -hin);
        path.lineTo( w2, -hin);
        path.lineTo( w2,  hin);
        path.lineTo( b2,  hin);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case TSA::Model::SectionShape::BoxHollow:
    {
        double b2 = pixW / 2.0;
        double h2 = pixH / 2.0;
        double twPix = std::max(2.0, (m_section.tw > 0.0 ? m_section.tw : 0.006) * scale);
        double tfPix = std::max(2.0, (m_section.tf > 0.0 ? m_section.tf : 0.006) * scale);

        path.addRect(-b2, -h2, pixW, pixH);
        QPainterPath inner;
        inner.addRect(-b2 + twPix, -h2 + tfPix, pixW - 2 * twPix, pixH - 2 * tfPix);
        path = path.subtracted(inner);
        p.drawPath(path);
        break;
    }
    case TSA::Model::SectionShape::UPN:
    {
        double b2 = pixW / 2.0;
        double h2 = pixH / 2.0;
        double twPix = std::max(3.0, (m_section.tw > 0.0 ? m_section.tw : 0.007) * scale);
        double tfPix = std::max(4.0, (m_section.tf > 0.0 ? m_section.tf : 0.010) * scale);

        path.moveTo(-b2, -h2);
        path.lineTo( b2, -h2);
        path.lineTo( b2, -h2 + tfPix);
        path.lineTo(-b2 + twPix, -h2 + tfPix);
        path.lineTo(-b2 + twPix,  h2 - tfPix);
        path.lineTo( b2,  h2 - tfPix);
        path.lineTo( b2,  h2);
        path.lineTo(-b2,  h2);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case TSA::Model::SectionShape::Angle:
    {
        double b2 = pixW / 2.0;
        double h2 = pixH / 2.0;
        double tPix = std::max(3.0, (m_section.tw > 0.0 ? m_section.tw : 0.008) * scale);

        path.moveTo(-b2, -h2);
        path.lineTo( b2, -h2);
        path.lineTo( b2, -h2 + tPix);
        path.lineTo(-b2 + tPix, -h2 + tPix);
        path.lineTo(-b2 + tPix,  h2);
        path.lineTo(-b2,  h2);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case TSA::Model::SectionShape::Rectangular:
    default:
    {
        path.addRect(-pixW / 2.0, -pixH / 2.0, pixW, pixH);
        p.drawPath(path);
        break;
    }
    }

    // 4. Tracé des axes locaux y et z au centre de gravité
    if (m_showAxes)
    {
        double axisLenX = pixW / 2.0 + 20.0;
        double axisLenY = pixH / 2.0 + 20.0;

        // Axe z horizontal (transversal) en rouge/orange vif
        p.setPen(QPen(QColor(255, 90, 80), 1.4, Qt::DashLine));
        p.drawLine(QPointF(-axisLenX * 0.8, 0), QPointF(axisLenX, 0));
        drawArrowHead(p, QPointF(axisLenX, 0), QPointF(1, 0));

        // Axe y vertical (flexion forte) en vert/cyan vif
        p.setPen(QPen(QColor(80, 220, 120), 1.4, Qt::DashLine));
        p.drawLine(QPointF(0, axisLenY * 0.8), QPointF(0, -axisLenY));
        drawArrowHead(p, QPointF(0, -axisLenY), QPointF(0, -1));

        // Libellés des axes
        QFont fontAxis = font();
        fontAxis.setPointSize(8);
        fontAxis.setBold(true);
        p.setFont(fontAxis);

        p.setPen(QColor(255, 90, 80));
        p.drawText(QPointF(axisLenX + 4.0, 4.0), "z");

        p.setPen(QColor(80, 220, 120));
        p.drawText(QPointF(-10.0, -axisLenY - 4.0), "y");
    }

    // 5. Centre de gravité CG
    if (m_showCentroid)
    {
        p.setBrush(QColor(255, 235, 80));
        p.setPen(QPen(QColor(20, 20, 20), 1.5));
        p.drawEllipse(QPointF(0, 0), 3.5, 3.5);

        QFont fontCg = font();
        fontCg.setPointSize(7);
        fontCg.setBold(true);
        p.setFont(fontCg);
        p.setPen(QColor(255, 235, 80));
        p.drawText(QPointF(5.0, 12.0), "CG");
    }

    // 6. Axe de la barre et Excentrement
    if (m_showBarAxis)
    {
        double offX = 0.0;
        double offY = 0.0;

        if (m_eccentricity == TSA::Model::BarEccentricity::TopFlange)
        {
            offY = pixH / 2.0; // Dans le repère Qt Y vers le bas
        }
        else if (m_eccentricity == TSA::Model::BarEccentricity::BottomFlange)
        {
            offY = -pixH / 2.0;
        }
        else if (m_eccentricity == TSA::Model::BarEccentricity::LeftFlange)
        {
            offX = -pixW / 2.0;
        }
        else if (m_eccentricity == TSA::Model::BarEccentricity::RightFlange)
        {
            offX = pixW / 2.0;
        }
        else
        {
            offX = m_ez * scale;
            offY = -m_ey * scale;
        }

        if (std::abs(offX) > 1.0 || std::abs(offY) > 1.0)
        {
            // Ligne de décalage entre CG et axe barre
            p.setPen(QPen(QColor(255, 120, 220), 1.2, Qt::DotLine));
            p.drawLine(QPointF(0, 0), QPointF(offX, offY));

            // Réticule pour l'axe de la barre
            p.setPen(QPen(QColor(255, 120, 220), 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(offX, offY), 4.5, 4.5);
            p.drawLine(QPointF(offX - 6, offY), QPointF(offX + 6, offY));
            p.drawLine(QPointF(offX, offY - 6), QPointF(offX, offY + 6));

            QFont fontEcc = font();
            fontEcc.setPointSize(7);
            p.setFont(fontEcc);
            p.drawText(QPointF(offX + 6, offY - 4), tr("Axe Barre"));
        }
    }

    // 7. Cotes dimensionnelles paramétriques
    if (m_showDimensions)
    {
        QFont fontDim = font();
        fontDim.setPointSize(8);
        p.setFont(fontDim);

        switch (m_section.shape)
        {
        case TSA::Model::SectionShape::Circular:
        {
            double diamMm = (m_section.diameter > 0.0 ? m_section.diameter : m_section.width) * 1000.0;
            QString dStr = QString("Ø%1 mm").arg(diamMm, 0, 'f', 0);
            drawDimensionH(p, -pixW / 2.0, pixW / 2.0, pixH / 2.0 + 16.0, dStr, false);
            break;
        }
        case TSA::Model::SectionShape::Pipe:
        {
            double dExtMm = m_section.diameter * 1000.0;
            double twMm = m_section.tw * 1000.0;
            QString dStr = QString("Ø%1 mm").arg(dExtMm, 0, 'f', 0);
            QString tStr = QString("t=%1 mm").arg(twMm, 0, 'f', 1);
            drawDimensionH(p, -pixW / 2.0, pixW / 2.0, pixH / 2.0 + 16.0, dStr, false);
            drawDimensionV(p, -pixH / 2.0, -pixH / 2.0 + std::max(4.0, m_section.tw * scale), -pixW / 2.0 - 16.0, tStr, true);
            break;
        }
        case TSA::Model::SectionShape::IShape:
        {
            double bMm = m_section.width * 1000.0;
            double hMm = m_section.height * 1000.0;
            QString bStr = QString("b=%1").arg(bMm, 0, 'f', 0);
            QString hStr = QString("h=%1").arg(hMm, 0, 'f', 0);
            drawDimensionH(p, -pixW / 2.0, pixW / 2.0, pixH / 2.0 + 16.0, bStr, false);
            drawDimensionV(p, -pixH / 2.0, pixH / 2.0, -pixW / 2.0 - 16.0, hStr, true);

            if (m_section.tf > 0.0)
            {
                double tfMm = m_section.tf * 1000.0;
                QString tfStr = QString("tf=%1").arg(tfMm, 0, 'f', 1);
                drawDimensionV(p, pixH / 2.0 - std::max(4.0, m_section.tf * scale), pixH / 2.0, pixW / 2.0 + 14.0, tfStr, false);
            }
            break;
        }
        default:
        {
            double bMm = m_section.width * 1000.0;
            double hMm = m_section.height * 1000.0;
            QString bStr = QString("b=%1 mm").arg(bMm, 0, 'f', 0);
            QString hStr = QString("h=%1 mm").arg(hMm, 0, 'f', 0);
            drawDimensionH(p, -pixW / 2.0, pixW / 2.0, pixH / 2.0 + 16.0, bStr, false);
            drawDimensionV(p, -pixH / 2.0, pixH / 2.0, -pixW / 2.0 - 16.0, hStr, true);
            break;
        }
        }
    }

    p.restore();
}

void SectionPreviewWidget::drawDimensionH(QPainter& p, double x1, double x2, double y, const QString& text, bool above)
{
    p.setPen(QPen(QColor(220, 225, 235), 1.0));
    // Lignes de rappel
    p.drawLine(QPointF(x1, y - (above ? -4 : 4)), QPointF(x1, y + (above ? -4 : 4)));
    p.drawLine(QPointF(x2, y - (above ? -4 : 4)), QPointF(x2, y + (above ? -4 : 4)));
    // Ligne de cote
    p.drawLine(QPointF(x1, y), QPointF(x2, y));

    // Flèches
    drawArrowHead(p, QPointF(x1, y), QPointF(1, 0));
    drawArrowHead(p, QPointF(x2, y), QPointF(-1, 0));

    // Texte centré
    QFontMetrics fm = p.fontMetrics();
    int tw = fm.horizontalAdvance(text);
    double midX = (x1 + x2) / 2.0;
    double textY = above ? (y - 4.0) : (y + fm.ascent() + 2.0);
    p.drawText(QPointF(midX - tw / 2.0, textY), text);
}

void SectionPreviewWidget::drawDimensionV(QPainter& p, double y1, double y2, double x, const QString& text, bool toLeft)
{
    p.setPen(QPen(QColor(220, 225, 235), 1.0));
    // Lignes de rappel
    p.drawLine(QPointF(x - (toLeft ? 4 : -4), y1), QPointF(x + (toLeft ? 4 : -4), y1));
    p.drawLine(QPointF(x - (toLeft ? 4 : -4), y2), QPointF(x + (toLeft ? 4 : -4), y2));
    // Ligne de cote
    p.drawLine(QPointF(x, y1), QPointF(x, y2));

    // Flèches
    drawArrowHead(p, QPointF(x, y1), QPointF(0, 1));
    drawArrowHead(p, QPointF(x, y2), QPointF(0, -1));

    // Texte centré
    QFontMetrics fm = p.fontMetrics();
    int tw = fm.horizontalAdvance(text);
    double midY = (y1 + y2) / 2.0;
    double textX = toLeft ? (x - tw - 4.0) : (x + 6.0);
    p.drawText(QPointF(textX, midY + fm.ascent() / 2.0 - 2.0), text);
}

void SectionPreviewWidget::drawArrowHead(QPainter& p, const QPointF& tip, const QPointF& dir)
{
    const double arrowSize = 4.0;
    QPointF norm(-dir.y(), dir.x());
    QPointF p1 = tip + dir * arrowSize + norm * (arrowSize * 0.5);
    QPointF p2 = tip + dir * arrowSize - norm * (arrowSize * 0.5);

    QPolygonF arrow;
    arrow << tip << p1 << p2;
    p.setBrush(p.pen().color());
    p.drawPolygon(arrow);
}

} // namespace TSA::UI
