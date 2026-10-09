#include "GridLabelRenderer.h"

#include <TopoDS_Edge.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <Quantity_Color.hxx>
#include <Prs3d_Drawer.hxx>
#include <Font_FontAspect.hxx>

namespace TSA::Grid
{

GridLabelRenderer::GridLabelRenderer()
    : m_isVisible(true)
{
}

void GridLabelRenderer::removeLabels(const std::string& gridId, const Handle(AIS_InteractiveContext)& context)
{
    auto it = m_gridLabelsMap.find(gridId);
    if (it == m_gridLabelsMap.end())
        return;

    if (!context.IsNull())
    {
        for (auto& label : it->second.textLabels)
        {
            if (!label.IsNull())
            {
                context->Remove(label, false);
            }
        }
        for (auto& bubble : it->second.bubbleShapes)
        {
            if (!bubble.IsNull())
            {
                context->Remove(bubble, false);
            }
        }
    }

    m_gridLabelsMap.erase(it);
}

void GridLabelRenderer::removeAllLabels(const Handle(AIS_InteractiveContext)& context)
{
    if (!context.IsNull())
    {
        for (auto& [id, perGrid] : m_gridLabelsMap)
        {
            for (auto& label : perGrid.textLabels)
            {
                if (!label.IsNull()) context->Remove(label, false);
            }
            for (auto& bubble : perGrid.bubbleShapes)
            {
                if (!bubble.IsNull()) context->Remove(bubble, false);
            }
        }
    }
    m_gridLabelsMap.clear();
}

void GridLabelRenderer::setGridLabelsVisible(const std::string& gridId, bool visible, const Handle(AIS_InteractiveContext)& context)
{
    auto it = m_gridLabelsMap.find(gridId);
    if (it == m_gridLabelsMap.end() || context.IsNull())
        return;

    bool show = visible && m_isVisible;
    for (auto& label : it->second.textLabels)
    {
        if (!label.IsNull())
        {
            if (show) context->Display(label, false);
            else context->Erase(label, false);
        }
    }
    for (auto& bubble : it->second.bubbleShapes)
    {
        if (!bubble.IsNull())
        {
            if (show) context->Display(bubble, false);
            else context->Erase(bubble, false);
        }
    }
}

void GridLabelRenderer::setVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    if (m_isVisible == visible)
        return;

    m_isVisible = visible;
    if (context.IsNull())
        return;

    for (auto& [id, perGrid] : m_gridLabelsMap)
    {
        for (auto& label : perGrid.textLabels)
        {
            if (!label.IsNull())
            {
                if (m_isVisible) context->Display(label, false);
                else context->Erase(label, false);
            }
        }
        for (auto& bubble : perGrid.bubbleShapes)
        {
            if (!bubble.IsNull())
            {
                if (m_isVisible) context->Display(bubble, false);
                else context->Erase(bubble, false);
            }
        }
    }
}

void GridLabelRenderer::updateLabels(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context,
                                     const GridLabelView& view)
{
    std::string id = gridSystem.id();
    removeLabels(id, context);

    if (context.IsNull() || !gridSystem.isVisible() || !gridSystem.showLabels() || !m_isVisible)
    {
        return;
    }

    PerGridLabels perGrid;

    Quantity_Color textColor = m_isDarkMode
        ? Quantity_Color(0.90, 0.93, 0.98, Quantity_TOC_RGB)
        : Quantity_Color(0.12, 0.16, 0.24, Quantity_TOC_RGB);
    Quantity_Color bubbleColor = m_isDarkMode
        ? Quantity_Color(0.40, 0.65, 0.90, Quantity_TOC_RGB)
        : Quantity_Color(0.20, 0.45, 0.70, Quantity_TOC_RGB);
    Quantity_Color levelTextColor = m_isDarkMode
        ? Quantity_Color(1.0, 0.85, 0.30, Quantity_TOC_RGB)  // Jaune d'or chaud lisible sur fond sombre
        : Quantity_Color(0.70, 0.40, 0.05, Quantity_TOC_RGB); // Ambre sombre lisible sur fond clair

    const auto& ds = gridSystem.definition().displaySettings();
    const double defaultRadius = ds.bubbleRadius > 0.0 ? ds.bubbleRadius : 0.40;

    // Rendu d'une étiquette placée : texte en pixels (non déformé par la vue) + bulle dont le
    // cercle fait face à la vue (normale fournie par GridLabelLayout).
    auto draw = [&](const PlacedGridLabel& label, double radius, bool withBubble, const Quantity_Color& bubble) {
        Handle(AIS_TextLabel) aisText = new AIS_TextLabel();
        aisText->SetText(TCollection_ExtendedString(label.text.c_str(), true));
        aisText->SetPosition(label.position);
        aisText->SetColor(label.isLevelLabel ? levelTextColor : textColor);
        aisText->SetHJustification(label.isLevelLabel ? Graphic3d_HTA_RIGHT : Graphic3d_HTA_CENTER);
        aisText->SetVJustification(Graphic3d_VTA_CENTER);
        aisText->SetHeight(label.isLevelLabel ? (label.isBold ? 14.0 : 12.0) : (label.isBold ? 15.0 : 13.0));
        if (label.isBold)
        {
            aisText->SetFontAspect(Font_FA_Bold);
        }
        context->Display(aisText, false);
        perGrid.textLabels.push_back(aisText);

        if (!withBubble || label.isLevelLabel)
            return;
        gp_Circ circ(gp_Ax2(label.position, label.bubbleNormal), radius);
        TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ);
        if (!edge.IsNull())
        {
            Handle(AIS_Shape) aisBubble = new AIS_Shape(edge);
            aisBubble->SetColor(bubble);
            aisBubble->SetWidth(label.isBold ? 2.5 : 1.8);
            context->Display(aisBubble, false);
            perGrid.bubbleShapes.push_back(aisBubble);
        }
    };

    if (gridSystem.type() == GridType::Cartesian && gridSystem.cartesian())
    {
        for (const auto& label : layoutCartesianLabels(*gridSystem.cartesian(), view))
            draw(label, defaultRadius, ds.showBubbles, bubbleColor);
    }
    else if (gridSystem.type() == GridType::Cylindrical && gridSystem.cylindrical())
    {
        const Quantity_Color cyan = m_isDarkMode ? Quantity_NOC_CYAN2 : Quantity_NOC_CYAN4;
        for (const auto& label : layoutCylindricalLabels(*gridSystem.cylindrical(), view))
            draw(label, 0.35, true, cyan);
    }
    else if (gridSystem.type() == GridType::Arbitrary && gridSystem.arbitrary())
    {
        // Grille arbitraire : une ancre par ligne (aucune répétition par niveau).
        int index = 0;
        for (const auto& anchor : gridSystem.arbitrary()->labelAnchors())
        {
            PlacedGridLabel label;
            label.position = anchor.position;
            label.text = anchor.text;
            label.isBold = anchor.isBold;
            label.index = index++;
            draw(label, defaultRadius, ds.showBubbles, bubbleColor);
        }
    }

    m_gridLabelsMap[id] = std::move(perGrid);
}

} // namespace TSA::Grid
