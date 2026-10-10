#pragma once

#include "../Model/Material.h"
#include <Graphic3d_MaterialAspect.hxx>
#include <Graphic3d_PBRMaterial.hxx>
#include <Quantity_Color.hxx>
#include <Graphic3d_Texture2D.hxx>
#include <AIS_Shape.hxx>
#include <QString>
#include <string>
#include <unordered_map>
#include <memory>

namespace TSA::Viewer
{

enum class RenderDisplayMode
{
    Materials,  // Rendu visuel fidèle aux matériaux (béton mat, acier métallique, bois, sol, verre)
    Structure,  // Couleurs standards par type structurel
    Analysis,   // Rendu simplifié d'analyse filaire/surfacique
    Results,    // Présentation des résultats FEM
    Wireframe   // Filaire pur
};

/**
 * @brief Gestionnaire et convertisseur de rendu visuel réaliste pour OpenCASCADE.
 * Fait le pont entre TSA::Model::Material et les représentations graphiques OCCT (PBR & Phong).
 */
class MaterialVisual
{
public:
    static MaterialVisual& instance();

    // Récupération de l'aspect matériel OCCT avec mise en cache haute performance
    Graphic3d_MaterialAspect getOcctMaterial(const TSA::Model::Material& mat);

    // Conversion de couleur avec prise en compte éventuelle d'un override utilisateur
    Quantity_Color getOcctColor(const TSA::Model::Material& mat, const std::string& overrideHexColor = "") const;

    // Transparence effective
    double getTransparency(const TSA::Model::Material& mat, double defaultTransparency = 0.0) const;

    // Application directe de l'apparence matérielle à un objet interactif AIS_Shape
    void applyToShape(Handle(AIS_Shape) aisShape,
                      const TSA::Model::Material& mat,
                      const std::string& overrideHexColor = "",
                      RenderDisplayMode mode = RenderDisplayMode::Materials,
                      double defaultTransparency = 0.0);

    // Résolution et gestion des textures physiques externes
    QString resolveTexturePath(const TSA::Model::Material& mat) const;
    bool hasTexture(const TSA::Model::Material& mat) const;

    /// Texture d'un fichier, partagée par tous les objets qui l'utilisent (une seule ressource
    /// graphique par fichier, quel que soit le nombre de barres). neutral : version en niveaux de
    /// gris (grain du matériau seul), teintée par la couleur choisie par l'utilisateur.
    occ::handle<Graphic3d_Texture2D> sharedTexture(const QString& filePath, bool neutral = false);

    // Utilitaire de parsing hexadécimal vers Quantity_Color OCCT
    static bool parseHexColor(const std::string& hex, Quantity_Color& outColor);

    // Vidage du cache en cas de modification dynamique de matériau
    void clearCache();

private:
    MaterialVisual() = default;
    ~MaterialVisual() = default;
    MaterialVisual(const MaterialVisual&) = delete;
    MaterialVisual& operator=(const MaterialVisual&) = delete;

private:
    std::unordered_map<int, Graphic3d_MaterialAspect> m_aspectCache;
    std::unordered_map<std::string, occ::handle<Graphic3d_Texture2D>> m_textureCache;
};

} // namespace TSA::Viewer
