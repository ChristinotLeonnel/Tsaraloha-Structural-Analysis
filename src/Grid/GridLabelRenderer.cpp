#include "GridLabelRenderer.h"

#include <TopoDS_Edge.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <Quantity_Color.hxx>
#include <Prs3d_Drawer.hxx>

namespace TSA::Grid
{

GridLabelRenderer::GridLabelRenderer()
    : m_isVisible(true)
{
}

void GridLabelRenderer::removeLabels(const Handle(AIS_InteractiveContext)& context)
{
    if (context.IsNull())
    {
        m_textLabels.clear();
        m_bubbleShapes.clear();
        return;
    }

    for (auto& label : m_textLabels)
    {
        if (!label.IsNull())
        {
            context->Remove(label, false);
        }
    }
    m_textLabels.clear();

    for (auto& bubble : m_bubbleShapes)
    {
        if (!bubble.IsNull())
        {
            context->Remove(bubble, false);
        }
    }
    m_bubbleShapes.clear();
}

void GridLabelRenderer::setVisible(bool visible, const Handle(AIS_InteractiveContext)& context)
{
    if (m_isVisible == visible)
        return;

    m_isVisible = visible;
    if (context.IsNull())
        return;

    for (auto& label : m_textLabels)
    {
        if (!label.IsNull())
        {
            if (m_isVisible)
                context->Display(label, false);
            else
                context->Erase(label, false);
        }
    }

    for (auto& bubble : m_bubbleShapes)
    {
        if (!bubble.IsNull())
        {
            if (m_isVisible)
                context->Display(bubble, false);
            else
                context->Erase(bubble, false);
        }
    }
}

void GridLabelRenderer::updateLabels(const GridSystem& gridSystem, const Handle(AIS_InteractiveContext)& context)
{
    removeLabels(context);

    if (context.IsNull() || !gridSystem.isVisible() || !gridSystem.showLabels() || !m_isVisible)
    {
        return;
    }

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

        for (const auto& anchor : anchors)
        {
            // 1. Étiquette textuelle 3D centrée
            Handle(AIS_TextLabel) aisText = new AIS_TextLabel();
            aisText->SetText(TCollection_ExtendedString(anchor.text.c_str()));
            aisText->SetPosition(anchor.position);
            aisText->SetColor(textColor);
            aisText->SetHJustification(Graphic3d_HTA_CENTER);
            aisText->SetVJustification(Graphic3d_VTA_CENTER);
            aisText->SetHeight(13.0);

            context->Display(aisText, false);
            m_textLabels.push_back(aisText);

            // 2. Bulle circulaire entourant l'étiquette (R = 0.40m)
            gp_Circ circ(gp_Ax2(anchor.position, gp_Dir(0, 0, 1)), 0.40);
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ);
            if (!edge.IsNull())
            {
                Handle(AIS_Shape) aisBubble = new AIS_Shape(edge);
                aisBubble->SetColor(bubbleColor);
                aisBubble->SetWidth(1.8);
                context->Display(aisBubble, false);
                m_bubbleShapes.push_back(aisBubble);
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
            aisText->SetHeight(12.0);

            context->Display(aisText, false);
            m_textLabels.push_back(aisText);
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
            m_textLabels.push_back(aisText);

            // Bulle pour rayons et angles
            gp_Circ circ(gp_Ax2(anchor.position, gp_Dir(0, 0, 1)), 0.35);
            TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(circ);
            if (!edge.IsNull())
            {
                Handle(AIS_Shape) aisBubble = new AIS_Shape(edge);
                aisBubble->SetColor(m_isDarkMode ? Quantity_NOC_CYAN2 : Quantity_NOC_CYAN4);
                aisBubble->SetWidth(1.6);
                context->Display(aisBubble, false);
                m_bubbleShapes.push_back(aisBubble);
            }
        }
    }
}

} // namespace TSA::Grid
