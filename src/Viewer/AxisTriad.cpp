#include "AxisTriad.h"

#include <Font_FontAspect.hxx>
#include <Geom_Axis2Placement.hxx>
#include <Graphic3d_AspectText3d.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <Graphic3d_TypeOfShadingModel.hxx>
#include <Prs3d_DatumAspect.hxx>
#include <Prs3d_DatumParts.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <Prs3d_TextAspect.hxx>
#include <TCollection_ExtendedString.hxx>
#include <gp_Ax2.hxx>

namespace TSA::Viewer
{

namespace
{
constexpr Prs3d_DatumParts kAxisParts[3] = { Prs3d_DatumParts_XAxis, Prs3d_DatumParts_YAxis, Prs3d_DatumParts_ZAxis };

/// Lettre de la couleur de son axe, cernée d'un contour : disponible sur tout contexte OpenGL (BUG-122), sans rectangle de fond.
void styleAxisLetter(const Handle(Prs3d_TextAspect)& text, const Quantity_Color& outline)
{
    text->Aspect()->SetDisplayType(Aspect_TODT_DEKALE);
    text->Aspect()->SetColorSubTitle(outline);
    text->Aspect()->SetTextFontAspect(Font_FA_Bold);
}
} // namespace

Quantity_Color axisScreenColor(const Quantity_Color& configured)
{
    return Quantity_Color(configured.Red(), configured.Green(), configured.Blue(), Quantity_TOC_sRGB);
}

Handle(AIS_Trihedron) createAxisTriad(const AxisTriadAppearance& appearance)
{
    Handle(AIS_Trihedron) triad = new AIS_Trihedron(new Geom_Axis2Placement(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0))));
    triad->SetDatumDisplayMode(Prs3d_DM_Shaded);
    triad->SetZLayer(Graphic3d_ZLayerId_Topmost);
    styleAxisTriad(triad, appearance);
    return triad;
}

void styleAxisTriad(const Handle(AIS_Trihedron)& triad, const AxisTriadAppearance& appearance)
{
    triad->SetSize(appearance.lengthPixels);   // crée aussi l'aspect propre du trièdre
    const Handle(Prs3d_DatumAspect)& datum = triad->Attributes()->DatumAspect();
    datum->SetDrawDatumAxes(appearance.transverseAxes ? Prs3d_DatumAxes_XYZAxes : Prs3d_DatumAxes_XAxis);
    datum->SetDrawLabels(appearance.letters);
    // Flèches plus marquées que les valeurs d'OCCT, lisibles sur la barre qu'elles longent.
    datum->SetAttribute(Prs3d_DatumAttribute_ShadingTubeRadiusPercent, 0.03);
    datum->SetAttribute(Prs3d_DatumAttribute_ShadingConeRadiusPercent, 0.085);
    datum->SetAttribute(Prs3d_DatumAttribute_ShadingConeLengthPercent, 0.24);
    datum->SetAttribute(Prs3d_DatumAttribute_ShadingOriginRadiusPercent, 0.06);
    triad->SetDatumPartColor(Prs3d_DatumParts_Origin, appearance.outline);
    for (int k = 0; k < 3; ++k)
    {
        const Quantity_Color& color = appearance.colors[static_cast<std::size_t>(k)];
        triad->SetDatumPartColor(kAxisParts[k], color);
        triad->SetArrowColor(kAxisParts[k], color);
        triad->SetTextColor(kAxisParts[k], color);
        triad->SetLabel(kAxisParts[k], TCollection_ExtendedString(appearance.labels[static_cast<std::size_t>(k)].c_str(), true));
        // Sans éclairage : la couleur reste franche quelle que soit l'orientation de l'axe.
        datum->ShadingAspect(kAxisParts[k])->Aspect()->SetShadingModel(Graphic3d_TypeOfShadingModel_Unlit);
        const Handle(Prs3d_TextAspect)& text = datum->TextAspect(kAxisParts[k]);
        text->SetHeight(appearance.labelPixels);
        styleAxisLetter(text, appearance.outline);
    }
}

void styleViewCubeAxes(const Handle(AIS_ViewCube)& cube, const AxisTriadAppearance& appearance)
{
    const Handle(Prs3d_DatumAspect)& datum = cube->Attributes()->DatumAspect();
    for (int k = 0; k < 3; ++k)
    {
        const Quantity_Color& color = appearance.colors[static_cast<std::size_t>(k)];
        datum->ShadingAspect(kAxisParts[k])->SetColor(color);
        datum->ShadingAspect(kAxisParts[k])->Aspect()->SetShadingModel(Graphic3d_TypeOfShadingModel_Unlit);
        datum->TextAspect(kAxisParts[k])->SetColor(color);
        styleAxisLetter(datum->TextAspect(kAxisParts[k]), appearance.outline);
    }
}

void placeAxisTriad(const Handle(AIS_Trihedron)& triad, const gp_Trsf& frame)
{
    const gp_Pnt anchor(frame.TranslationPart());
    gp_Trsf rotation = frame;
    rotation.SetTranslationPart(gp_Vec(0.0, 0.0, 0.0));
    triad->SetLocalTransformation(rotation);
    const Handle(Graphic3d_TransformPers)& pers = triad->TransformPersistence();
    if (pers.IsNull() || pers->Mode() != Graphic3d_TMF_ZoomPers || !pers->AnchorPoint().IsEqual(anchor, 0.0))
        triad->SetTransformPersistence(new Graphic3d_TransformPers(Graphic3d_TMF_ZoomPers, anchor));
}

void placeAxisTriad(const Handle(AIS_Trihedron)& triad, const gp_Pnt& origin, const gp_Dir& x, const gp_Dir& y, const gp_Dir& z)
{
    gp_Trsf frame;
    frame.SetValues(x.X(), y.X(), z.X(), origin.X(),
                    x.Y(), y.Y(), z.Y(), origin.Y(),
                    x.Z(), y.Z(), z.Z(), origin.Z());
    placeAxisTriad(triad, frame);
}

} // namespace TSA::Viewer
