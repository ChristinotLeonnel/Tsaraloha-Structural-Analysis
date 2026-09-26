#pragma once

#include "GridSystem.h"
#include <AIS_InteractiveContext.hxx>
#include <AIS_TextLabel.hxx>
#include <AIS_Shape.hxx>
#include <vector>
#include <unordered_map>
#include <string>

namespace TSA::Grid
{

struct PerGridLabels
{
    std::vector<Handle(AIS_TextLabel)> textLabels;
    std::vector<Handle(AIS_Shape)> bubbleShapes;
};

class GridLabelRenderer
{
public:
    GridLabelRenderer();
    ~GridLabelRenderer() = default;

    void updateLabels(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context);
    void removeLabels(const std::string& gridId, const Handle(AIS_InteractiveContext)& context);
    void removeAllLabels(const Handle(AIS_InteractiveContext)& context);
    void setGridLabelsVisible(const std::string& gridId, bool visible, const Handle(AIS_InteractiveContext)& context);
    void setVisible(bool visible, const Handle(AIS_InteractiveContext)& context);

    bool isVisible() const { return m_isVisible; }

    void setDarkMode(bool dark) { m_isDarkMode = dark; }
    bool isDarkMode() const { return m_isDarkMode; }

private:
    bool m_isVisible = true;
    bool m_isDarkMode = true;
    std::unordered_map<std::string, PerGridLabels> m_gridLabelsMap;
};

} // namespace TSA::Grid
