#include "AxisColorConfig.h"

namespace TSA::Coordinate
{

AxisColorConfig& AxisColorConfig::instance()
{
    static AxisColorConfig s_instance;
    return s_instance;
}

AxisColorConfig::AxisColorConfig()
{
    resetToDefaults();
}

void AxisColorConfig::resetToDefaults()
{
    // Global standard : X = Rouge (0.92, 0.20, 0.20), Y = Vert (0.20, 0.82, 0.20), Z = Bleu (0.20, 0.45, 0.95)
    m_colorX = Quantity_Color(0.92, 0.20, 0.20, Quantity_TOC_RGB);
    m_colorY = Quantity_Color(0.20, 0.82, 0.20, Quantity_TOC_RGB);
    m_colorZ = Quantity_Color(0.20, 0.45, 0.95, Quantity_TOC_RGB);

    m_qcolorX = QColor(235, 51, 51);
    m_qcolorY = QColor(51, 209, 51);
    m_qcolorZ = QColor(51, 115, 242);

    // WorkPlane local axes : tonalité éclatante claire pour distinction immédiate
    m_colorWpX = Quantity_Color(1.00, 0.35, 0.35, Quantity_TOC_RGB);
    m_colorWpY = Quantity_Color(0.35, 0.95, 0.35, Quantity_TOC_RGB);
    m_colorWpZ = Quantity_Color(0.35, 0.65, 1.00, Quantity_TOC_RGB);

    m_qcolorWpX = QColor(255, 89, 89);
    m_qcolorWpY = QColor(89, 242, 89);
    m_qcolorWpZ = QColor(89, 166, 255);
}

void AxisColorConfig::setAxisColors(const Quantity_Color& xCol, const Quantity_Color& yCol, const Quantity_Color& zCol)
{
    m_colorX = xCol;
    m_colorY = yCol;
    m_colorZ = zCol;

    m_qcolorX = QColor::fromRgbF(xCol.Red(), xCol.Green(), xCol.Blue());
    m_qcolorY = QColor::fromRgbF(yCol.Red(), yCol.Green(), yCol.Blue());
    m_qcolorZ = QColor::fromRgbF(zCol.Red(), zCol.Green(), zCol.Blue());
}

void AxisColorConfig::setWorkPlaneAxisColors(const Quantity_Color& xCol, const Quantity_Color& yCol, const Quantity_Color& zCol)
{
    m_colorWpX = xCol;
    m_colorWpY = yCol;
    m_colorWpZ = zCol;

    m_qcolorWpX = QColor::fromRgbF(xCol.Red(), xCol.Green(), xCol.Blue());
    m_qcolorWpY = QColor::fromRgbF(yCol.Red(), yCol.Green(), yCol.Blue());
    m_qcolorWpZ = QColor::fromRgbF(zCol.Red(), zCol.Green(), zCol.Blue());
}

} // namespace TSA::Coordinate
