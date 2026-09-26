#include "MaterialVisual.h"
#include <QColor>
#include <QString>
#include <TCollection_AsciiString.hxx>
#include <AIS_DisplayMode.hxx>

namespace TSA::Viewer
{

MaterialVisual& MaterialVisual::instance()
{
    static MaterialVisual inst;
    return inst;
}

bool MaterialVisual::parseHexColor(const std::string& hex, Quantity_Color& outColor)
{
    if (hex.empty())
        return false;
    QColor qc(QString::fromStdString(hex));
    if (!qc.isValid())
        return false;
    outColor = Quantity_Color(qc.redF(), qc.greenF(), qc.blueF(), Quantity_TOC_sRGB);
    return true;
}

Quantity_Color MaterialVisual::getOcctColor(const TSA::Model::Material& mat, const std::string& overrideHexColor) const
{
    Quantity_Color qc;
    if (!overrideHexColor.empty() && parseHexColor(overrideHexColor, qc))
    {
        return qc;
    }
    if (parseHexColor(mat.visual.baseColor, qc))
    {
        return qc;
    }

    // Couleurs de secours par type si la chaîne hexadécimale est invalide
    switch (mat.type)
    {
    case TSA::Model::MaterialType::Steel:
        return Quantity_Color(Quantity_NOC_STEELBLUE);
    case TSA::Model::MaterialType::RebarSteel:
        return Quantity_Color(0.18, 0.20, 0.22, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::GalvanizedSteel:
        return Quantity_Color(0.70, 0.73, 0.75, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Aluminum:
        return Quantity_Color(0.85, 0.87, 0.88, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Timber:
        return Quantity_Color(0.73, 0.55, 0.33, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Brick:
        return Quantity_Color(0.65, 0.29, 0.21, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Masonry:
        return Quantity_Color(0.56, 0.53, 0.49, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Glass:
        return Quantity_Color(0.76, 0.89, 0.91, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Soil:
        return Quantity_Color(0.45, 0.32, 0.22, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Sand:
        return Quantity_Color(0.82, 0.71, 0.48, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Gravel:
        return Quantity_Color(0.43, 0.41, 0.38, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Rock:
        return Quantity_Color(0.27, 0.27, 0.29, Quantity_TOC_sRGB);
    case TSA::Model::MaterialType::Concrete:
    case TSA::Model::MaterialType::ReinforcedConcrete:
    default:
        return Quantity_Color(0.62, 0.63, 0.64, Quantity_TOC_sRGB);
    }
}

double MaterialVisual::getTransparency(const TSA::Model::Material& mat, double defaultTransparency) const
{
    if (mat.visual.transparency > 0.001)
    {
        return mat.visual.transparency;
    }
    return defaultTransparency;
}

Graphic3d_MaterialAspect MaterialVisual::getOcctMaterial(const TSA::Model::Material& mat)
{
    // Recherche dans le cache par identifiant de matériau
    if (mat.id > 0)
    {
        auto it = m_aspectCache.find(mat.id);
        if (it != m_aspectCache.end())
        {
            return it->second;
        }
    }

    Graphic3d_NameOfMaterial basePreset = Graphic3d_NameOfMaterial_Stone;
    switch (mat.type)
    {
    case TSA::Model::MaterialType::Steel:
    case TSA::Model::MaterialType::RebarSteel:
    case TSA::Model::MaterialType::GalvanizedSteel:
        basePreset = Graphic3d_NameOfMaterial_Steel;
        break;
    case TSA::Model::MaterialType::Aluminum:
        basePreset = Graphic3d_NameOfMaterial_Aluminum;
        break;
    case TSA::Model::MaterialType::Timber:
        basePreset = Graphic3d_NameOfMaterial_Satin;
        break;
    case TSA::Model::MaterialType::Glass:
        basePreset = Graphic3d_NameOfMaterial_Glass;
        break;
    case TSA::Model::MaterialType::Concrete:
    case TSA::Model::MaterialType::ReinforcedConcrete:
    case TSA::Model::MaterialType::Brick:
    case TSA::Model::MaterialType::Masonry:
    case TSA::Model::MaterialType::Soil:
    case TSA::Model::MaterialType::Sand:
    case TSA::Model::MaterialType::Gravel:
    case TSA::Model::MaterialType::Rock:
    default:
        basePreset = Graphic3d_NameOfMaterial_Stone;
        break;
    }

    Graphic3d_MaterialAspect aspect(basePreset);
    aspect.SetMaterialName(TCollection_AsciiString(mat.name.c_str()));

    Quantity_Color baseCol = getOcctColor(mat);
    aspect.SetColor(baseCol);
    aspect.SetShininess(static_cast<float>(mat.visual.shininess));

    if (mat.visual.transparency > 0.0)
    {
        aspect.SetTransparency(static_cast<float>(mat.visual.transparency));
    }

    // Configuration PBR (Metallic-Roughness)
    Graphic3d_PBRMaterial pbr;
    pbr.SetColor(baseCol);
    pbr.SetMetallic(static_cast<float>(mat.visual.metallic));
    pbr.SetRoughness(static_cast<float>(mat.visual.roughness));
    if (mat.type == TSA::Model::MaterialType::Glass)
    {
        pbr.SetIOR(1.52f);
    }
    aspect.SetPBRMaterial(pbr);

    if (mat.id > 0)
    {
        m_aspectCache[mat.id] = aspect;
    }
    return aspect;
}

void MaterialVisual::applyToShape(Handle(AIS_Shape) aisShape,
                                  const TSA::Model::Material& mat,
                                  const std::string& overrideHexColor,
                                  RenderDisplayMode mode,
                                  double defaultTransparency)
{
    if (aisShape.IsNull())
        return;

    Quantity_Color color = getOcctColor(mat, overrideHexColor);

    if (mode == RenderDisplayMode::Wireframe)
    {
        aisShape->SetDisplayMode(AIS_WireFrame);
        aisShape->SetColor(color);
        aisShape->UnsetTransparency();
        return;
    }

    // Mode Shaded standard ou Matériaux réaliste
    aisShape->SetDisplayMode(AIS_Shaded);

    Graphic3d_MaterialAspect aspect = getOcctMaterial(mat);
    if (!overrideHexColor.empty())
    {
        aspect.SetColor(color);
    }
    aisShape->SetMaterial(aspect);
    aisShape->SetColor(color);

    double trans = getTransparency(mat, defaultTransparency);
    if (trans > 0.01)
    {
        aisShape->SetTransparency(static_cast<float>(trans));
    }
    else
    {
        aisShape->UnsetTransparency();
    }
}

void MaterialVisual::clearCache()
{
    m_aspectCache.clear();
}

} // namespace TSA::Viewer
