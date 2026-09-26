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

void GridLabelRenderer::updateLabels(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context)
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

    if (gridSystem.type() == GridType::Cartesian && gridSystem.cartesian())
    {
        const auto* cartesian = gridSystem.cartesian();
        const auto& anchors = cartesian->labelAnchors();

        double rad = gridSystem.definition().displaySettings().bubbleRadius > 0.0
            ? gridSystem.definition().displaySettings().bubbleRadius
            : 0.40;
        bool showBubbles = gridSystem.definition().displaySettings().showBubbles;

        for (const auto& anchor : anchors)
        {
            // 1. Étiquette textuelle 3D centrée
            Handle(AIS_TextLabel) aisText = new AIS_TextLabel();
            aisText->SetText(TCollection_ExtendedString(anchor.text.c_str()));
            aisText->SetPosition(anchor.position);
            aisText->SetColor(textColor);
            aisText->SetHJustification(Graphic3d_HTA_CENTER);
            aisText->SetVJustification(Graphic3d_VTA_CENTER);
            aisText->SetHeight(anchor.isBold ? 15.0 : 13.0);
            if (anchor.isBold)
            {
                aisText->SetFontAspect(Font_FA_Bold);
            }

            context->Display(aisText, false);
            perGrid.textLabels.push_back(aisText);

            // 2. Bulle circulaire entourant l'étiquette
            if (showBubbles)
            {
                gp_Circ circ(gp_Ax2(anchor.position, gp_Dir(0, 0, 1)), rad);
                TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ);
                if (!edge.IsNull())
                {
                    Handle(AIS_Shape) aisBubble = new AIS_Shape(edge);
                    aisBubble->SetColor(bubbleColor);
                    aisBubble->SetWidth(anchor.isBold ? 2.5 : 1.8);
                    context->Display(aisBubble, false);
                    perGrid.bubbleShapes.push_back(aisBubble);
                }
            }
        }

        // 3. Étiquettes d'élévations d'étages le long de la colonne verticale Z
        Quantity_Color levelTextColor = m_isDarkMode
            ? Quantity_Color(1.0, 0.85, 0.30, Quantity_TOC_RGB)  // Jaune d'or chaud lisible sur fond sombre
            : Quantity_Color(0.70, 0.40, 0.05, Quantity_TOC_RGB); // Ambre sombre lisible sur fond clair
        for (const auto& anchor : cartesian->levelLabelAnchors())
        {
            Handle(AIS_TextLabel) aisText = new AIS_TextLabel();
            aisText->SetText(TCollection_ExtendedString(anchor.text.c_str()));
            aisText->SetPosition(anchor.position);
            aisText->SetColor(levelTextColor);
            aisText->SetHJustification(Graphic3d_HTA_RIGHT);
            aisText->SetVJustification(Graphic3d_VTA_CENTER);
            aisText->SetHeight(anchor.isBold ? 14.0 : 12.0);
            if (anchor.isBold)
            {
                aisText->SetFontAspect(Font_FA_Bold);
            }

            context->Display(aisText, false);
            perGrid.textLabels.push_back(aisText);
        }
    }
    else if (gridSystem.type() == GridType::Cylindrical && gridSystem.cylindrical())
    {
        const auto& anchors = gridSystem.cylindrical()->labelAnchors();

        for (const auto& anchor : anchors)
        {
            Handle(AIS_TextLabel) aisText = new AIS_TextLabel();
            aisText->SetText(TCollection_ExtendedString(anchor.text.c_str()));
            aisText->SetPosition(anchor.position);
            aisText->SetColor(textColor);
            aisText->SetHJustification(Graphic3d_HTA_CENTER);
            aisText->SetVJustification(Graphic3d_VTA_CENTER);
            aisText->SetHeight(12.0);

            context->Display(aisText, false);
            perGrid.textLabels.push_back(aisText);

            // Bulle pour rayons et angles
            gp_Circ circ(gp_Ax2(anchor.position, gp_Dir(0, 0, 1)), 0.35);
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ);
            if (!edge.IsNull())
            {
                Handle(AIS_Shape) aisBubble = new AIS_Shape(edge);
                aisBubble->SetColor(m_isDarkMode ? Quantity_NOC_CYAN2 : Quantity_NOC_CYAN4);
                aisBubble->SetWidth(1.6);
                context->Display(aisBubble, false);
                perGrid.bubbleShapes.push_back(aisBubble);
            }
        }
    }
    else if (gridSystem.type() == GridType::Arbitrary && gridSystem.arbitrary())
    {
        const auto* arbitrary = gridSystem.arbitrary();
        const auto& anchors = arbitrary->labelAnchors();
        double rad = gridSystem.definition().displaySettings().bubbleRadius > 0.0
            ? gridSystem.definition().displaySettings().bubbleRadius
            : 0.40;
        bool showBubbles = gridSystem.definition().displaySettings().showBubbles;

        for (const auto& anchor : anchors)
        {
            Handle(AIS_TextLabel) aisText = new AIS_TextLabel();
            aisText->SetText(TCollection_ExtendedString(anchor.text.c_str()));
            aisText->SetPosition(anchor.position);
            aisText->SetColor(textColor);
            aisText->SetHJustification(Graphic3d_HTA_CENTER);
            aisText->SetVJustification(Graphic3d_VTA_CENTER);
            aisText->SetHeight(anchor.isBold ? 15.0 : 13.0);
            if (anchor.isBold)
            {
                aisText->SetFontAspect(Font_FA_Bold);
            }

            context->Display(aisText, false);
            perGrid.textLabels.push_back(aisText);

            if (showBubbles)
            {
                gp_Circ circ(gp_Ax2(anchor.position, gp_Dir(0, 0, 1)), rad);
                TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ);
                if (!edge.IsNull())
                {
                    Handle(AIS_Shape) aisBubble = new AIS_Shape(edge);
                    aisBubble->SetColor(bubbleColor);
                    aisBubble->SetWidth(anchor.isBold ? 2.5 : 1.8);
                    context->Display(aisBubble, false);
                    perGrid.bubbleShapes.push_back(aisBubble);
                }
            }
        }
    }

    m_gridLabelsMap[id] = std::move(perGrid);
}

} // namespace TSA::Grid
