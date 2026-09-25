#pragma once

#include <QWidget>
#include "../../Model/Section.h"
#include "../../Model/Beam.h"

namespace TSA::UI
{

/**
 * @brief Widget graphique 2D pour le dessin technique paramétrique de sections structurales.
 * 
 * Affiche en temps réel :
 * - Le contour exact de la section (Rectangle, Circulaire, Tube, IPE, HEA, HEB, UPN, Angle, Boîte creuse)
 * - Les cotes paramétriques (b, h, ØD, t, tw, tf)
 * - Les axes locaux principaux y et z
 * - Le centre de gravité (CG)
 * - La rotation axiale gamma
 * - L'axe de référence de la barre et l'excentrement (ey, ez)
 */
class SectionPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SectionPreviewWidget(QWidget* parent = nullptr);
    ~SectionPreviewWidget() override = default;

    void setSection(const TSA::Model::Section& section);
    const TSA::Model::Section& section() const { return m_section; }

    void setRotation(double gammaDegrees);
    double rotation() const { return m_gamma; }

    void setEccentricity(TSA::Model::BarEccentricity ecc, double ey = 0.0, double ez = 0.0);
    TSA::Model::BarEccentricity eccentricity() const { return m_eccentricity; }
    double eccentricityY() const { return m_ey; }
    double eccentricityZ() const { return m_ez; }

    void setShowDimensions(bool show);
    bool showDimensions() const { return m_showDimensions; }

    void setShowAxes(bool show);
    bool showAxes() const { return m_showAxes; }

    void setShowCentroid(bool show);
    bool showCentroid() const { return m_showCentroid; }

    void setShowBarAxis(bool show);
    bool showBarAxis() const { return m_showBarAxis; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawDimensionH(QPainter& p, double x1, double x2, double y, const QString& text, bool above = true);
    void drawDimensionV(QPainter& p, double y1, double y2, double x, const QString& text, bool toLeft = true);
    void drawArrowHead(QPainter& p, const QPointF& tip, const QPointF& dir);

private:
    TSA::Model::Section m_section;
    double m_gamma = 0.0; // Angle de rotation en degrés
    TSA::Model::BarEccentricity m_eccentricity = TSA::Model::BarEccentricity::None;
    double m_ey = 0.0;
    double m_ez = 0.0;

    bool m_showDimensions = true;
    bool m_showAxes = true;
    bool m_showCentroid = true;
    bool m_showBarAxis = true;
};

} // namespace TSA::UI
