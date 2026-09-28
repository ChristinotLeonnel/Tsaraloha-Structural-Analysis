#pragma once

#include <Quantity_Color.hxx>
#include <QColor>

namespace TSA::Coordinate
{

/**
 * @brief Configuration centralisée des couleurs des axes du repère global et local (Règle 2).
 * Permet d'éviter le codage en dur des couleurs dans tout TSA (X rouge, Y vert, Z bleu)
 * et de personnaliser dynamiquement la palette visuelle.
 */
class AxisColorConfig
{
public:
    static AxisColorConfig& instance();

    // Couleurs repère Global (X, Y, Z)
    Quantity_Color axisXColor() const noexcept { return m_colorX; }
    Quantity_Color axisYColor() const noexcept { return m_colorY; }
    Quantity_Color axisZColor() const noexcept { return m_colorZ; }

    QColor axisXQColor() const noexcept { return m_qcolorX; }
    QColor axisYQColor() const noexcept { return m_qcolorY; }
    QColor axisZQColor() const noexcept { return m_qcolorZ; }

    void setAxisColors(const Quantity_Color& xCol, const Quantity_Color& yCol, const Quantity_Color& zCol);

    // Couleurs repère Local WorkPlane (Xwp, Ywp, Zwp)
    Quantity_Color workPlaneAxisXColor() const noexcept { return m_colorWpX; }
    Quantity_Color workPlaneAxisYColor() const noexcept { return m_colorWpY; }
    Quantity_Color workPlaneAxisZColor() const noexcept { return m_colorWpZ; }

    QColor workPlaneAxisXQColor() const noexcept { return m_qcolorWpX; }
    QColor workPlaneAxisYQColor() const noexcept { return m_qcolorWpY; }
    QColor workPlaneAxisZQColor() const noexcept { return m_qcolorWpZ; }

    void setWorkPlaneAxisColors(const Quantity_Color& xCol, const Quantity_Color& yCol, const Quantity_Color& zCol);

    // Rétablissement des couleurs par défaut
    void resetToDefaults();

private:
    AxisColorConfig();
    ~AxisColorConfig() = default;

    Quantity_Color m_colorX;
    Quantity_Color m_colorY;
    Quantity_Color m_colorZ;
    QColor m_qcolorX;
    QColor m_qcolorY;
    QColor m_qcolorZ;

    Quantity_Color m_colorWpX;
    Quantity_Color m_colorWpY;
    Quantity_Color m_colorWpZ;
    QColor m_qcolorWpX;
    QColor m_qcolorWpY;
    QColor m_qcolorWpZ;
};

} // namespace TSA::Coordinate
