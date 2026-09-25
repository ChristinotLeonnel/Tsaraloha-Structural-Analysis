#pragma once

#include "GridSystem.h"
#include <AIS_InteractiveContext.hxx>
#include <AIS_TextLabel.hxx>
#include <AIS_Shape.hxx>
#include <vector>

namespace TSA::Grid
{

class GridLabelRenderer
{
public:
    GridLabelRenderer();
    ~GridLabelRenderer() = default;

    void updateLabels(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context);
    void removeLabels(const Handle(AIS_InteractiveContext)& context);
    void setVisible(bool visible, const Handle(AIS_InteractiveContext)& context);

    bool isVisible() const { return m_isVisible; }

    void setDarkMode(bool dark) { m_isDarkMode = dark; }
    bool isDarkMode() const { return m_isDarkMode; }

private:
    bool m_isVisible = true;
    bool m_isDarkMode = true;
    std::vector<Handle(AIS_TextLabel)> m_textLabels;
    std::vector<Handle(AIS_Shape)> m_bubbleShapes;
};

} // namespace TSA::Grid
