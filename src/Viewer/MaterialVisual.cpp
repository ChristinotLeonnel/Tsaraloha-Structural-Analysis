#include "MaterialVisual.h"
#include "TextureManager.h"
#include <QColor>
#include <algorithm>
#include <QString>
#include <TCollection_AsciiString.hxx>
#include <AIS_DisplayMode.hxx>
#include <AIS_TexturedShape.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Graphic3d_TextureParams.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <gp_Pnt2d.hxx>
#include <Image_PixMap.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <QImage>

namespace TSA::Viewer
{

namespace
{
/// Texture dont l'identifiant de ressource dépend du fichier : OpenGL la charge une seule fois et la
/// partage entre toutes les barres du même matériau (l'identifiant par défaut est unique par objet).
class SharedTexture2D : public Graphic3d_Texture2D
{
public:
    explicit SharedTexture2D(const TCollection_AsciiString& filePath)
        : Graphic3d_Texture2D(filePath)
    {
        setup(TCollection_AsciiString("TSA_Texture_") + filePath);
    }
    SharedTexture2D(const occ::handle<Image_PixMap>& pixmap, const TCollection_AsciiString& id)
        : Graphic3d_Texture2D(pixmap)
    {
        setup(id);
    }

private:
    void setup(const TCollection_AsciiString& id)
    {
        myTexId = id;
        GetParams()->SetModulate(true); // multipliée par la couleur de l'objet (blanc : couleurs de la texture)
        GetParams()->SetRepeat(true);
        GetParams()->SetFilter(Graphic3d_TOTF_TRILINEAR);
    }
};

/// Niveaux de gris normalisés (luminance moyenne ≈ 0,9) : la couleur choisie par l'utilisateur
/// apparaît presque telle quelle, avec le grain du matériau. Lignes rangées du bas vers le haut
/// (convention des textures OpenGL).
occ::handle<Image_PixMap> neutralPixmap(const QString& filePath)
{
    QImage img(filePath);
    if (img.isNull()) return {};
    img = img.convertToFormat(QImage::Format_Grayscale8);
    const int w = img.width(), h = img.height();
    double sum = 0.0;
    for (int y = 0; y < h; ++y)
    {
        const uchar* row = img.constScanLine(y);
        for (int x = 0; x < w; ++x) sum += row[x];
    }
    const double mean = sum / std::max(1, w * h);
    const double gain = mean > 1.0 ? (0.9 * 255.0) / mean : 1.0;

    occ::handle<Image_PixMap> pm = new Image_PixMap();
    if (!pm->InitTrash(Image_Format_RGB, w, h)) return {};
    for (int y = 0; y < h; ++y)
    {
        const uchar* src = img.constScanLine(h - 1 - y);
        uint8_t* dst = pm->ChangeRow(y);
        for (int x = 0; x < w; ++x)
        {
            const auto v = static_cast<uint8_t>(std::min(255.0, src[x] * gain));
            dst[3 * x] = dst[3 * x + 1] = dst[3 * x + 2] = v;
        }
    }
    return pm;
}
} // namespace

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

    // Texture du matériau (mode Matériaux) : mapping natif d'AIS_Shape, appliqué par l'aspect
    // d'ombrage de l'objet. La couleur effective (matériau, ou couleur choisie par l'utilisateur dans
    // les propriétés) module la texture. Les coordonnées de texture sont générées au calcul de la
    // présentation : appeler avant l'affichage, ou réafficher l'objet ensuite.
    const QString texPath = mode == RenderDisplayMode::Materials ? resolveTexturePath(mat) : QString();
    aisShape->Attributes()->SetupOwnShadingAspect();
    const occ::handle<Graphic3d_AspectFillArea3d>& fill = aisShape->Attributes()->ShadingAspect()->Aspect();
    if (!texPath.isEmpty())
    {
        // Sans couleur propre : la texture garde ses couleurs (objet blanc, aspect du matériau
        // conservé). Avec une couleur choisie : grain du matériau en gris, teinté par cette couleur.
        const bool userColored = !overrideHexColor.empty();
        if (!userColored)
        {
            aspect.SetColor(Quantity_Color(Quantity_NOC_WHITE));
            aisShape->SetMaterial(aspect);
            aisShape->SetColor(Quantity_Color(Quantity_NOC_WHITE));
        }
        fill->SetTextureMap(sharedTexture(texPath, userColored));
        fill->SetTextureMapOn();
        // Coordonnées de texture normalisées par face : sans répétition, le motif serait étiré sur
        // toute la longueur d'une barre. Environ un motif par mètre : V suit la plus grande dimension
        // (longueur d'une barre), U la dimension intermédiaire (section, ou largeur d'une dalle).
        double mid = 1.0, longest = 1.0;
        if (aisShape->Shape().IsNull() == false)
        {
            Bnd_Box box;
            BRepBndLib::Add(aisShape->Shape(), box);
            if (!box.IsVoid())
            {
                double x0, y0, z0, x1, y1, z1;
                box.Get(x0, y0, z0, x1, y1, z1);
                double d[3] = { x1 - x0, y1 - y0, z1 - z0 };
                std::sort(d, d + 3);
                mid = d[1];
                longest = d[2];
            }
        }
        constexpr double kTileMetres = 1.0;
        const double u = (mat.visual.textureScaleU > 0.0 ? mat.visual.textureScaleU : 1.0) * std::max(1.0, mid / kTileMetres);
        const double v = (mat.visual.textureScaleV > 0.0 ? mat.visual.textureScaleV : 1.0) * std::max(1.0, longest / kTileMetres);
        aisShape->SetTextureRepeatUV(gp_Pnt2d(u, v));
    }
    else
    {
        fill->SetTextureMapOff();
        fill->SetTextureMap(occ::handle<Graphic3d_TextureMap>());
    }

    // Objet AIS_TexturedShape (extensions) : même texture par son propre mécanisme.
    Handle(AIS_TexturedShape) texShape = Handle(AIS_TexturedShape)::DownCast(aisShape);
    if (!texShape.IsNull())
    {
        if (!texPath.isEmpty())
        {
            texShape->SetTextureFileName(TCollection_AsciiString(texPath.toUtf8().constData()));
            texShape->SetTextureMapOn();
            texShape->SetTextureRepeat(true, mat.visual.textureScaleU, mat.visual.textureScaleV);
            texShape->EnableTextureModulate();
            texShape->UpdateAttributes();
        }
        else
        {
            texShape->SetTextureMapOff();
        }
    }
}

QString MaterialVisual::resolveTexturePath(const TSA::Model::Material& mat) const
{
    if (!mat.visual.texturePath.empty())
    {
        QString res = TextureManager::instance().resolveTexturePath(mat.visual.texturePath);
        if (!res.isEmpty()) return res;
    }
    if (!mat.visual.textureName.empty())
    {
        QString res = TextureManager::instance().resolveTexturePath(mat.visual.textureName);
        if (!res.isEmpty()) return res;
    }
    return QString();
}

bool MaterialVisual::hasTexture(const TSA::Model::Material& mat) const
{
    return !resolveTexturePath(mat).isEmpty();
}

occ::handle<Graphic3d_Texture2D> MaterialVisual::sharedTexture(const QString& filePath, bool neutral)
{
    const std::string key = (neutral ? "neutral:" : "") + filePath.toStdString();
    auto it = m_textureCache.find(key);
    if (it != m_textureCache.end()) return it->second;
    const TCollection_AsciiString path(filePath.toUtf8().constData());
    occ::handle<Graphic3d_Texture2D> texture;
    if (neutral)
    {
        if (occ::handle<Image_PixMap> pm = neutralPixmap(filePath); !pm.IsNull())
            texture = new SharedTexture2D(pm, TCollection_AsciiString("TSA_TextureNeutral_") + path);
    }
    if (texture.IsNull()) texture = new SharedTexture2D(path);
    m_textureCache.emplace(key, texture);
    return texture;
}

void MaterialVisual::clearCache()
{
    m_aspectCache.clear();
    m_textureCache.clear();
    TextureManager::instance().clearCache();
}

} // namespace TSA::Viewer
