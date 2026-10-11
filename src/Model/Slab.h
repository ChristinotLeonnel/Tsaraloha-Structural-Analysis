#pragma once

#include "Material.h"
#include "SurfaceFormulation.h"
#include <vector>
#include <string>
#include <utility>

namespace TSA::Model
{

class Model;

/// Ouverture (trémie) d'une dalle : polygone dans le repère local de la dalle (origine au premier nœud,
/// u vers le deuxième nœud, n normale de Newell, v = n × u ; m). Suit la dalle quand ses nœuds bougent.
/// Ouverture d'une surface (trémie de dalle, porte ou fenêtre de voile), dans le repère local de la surface.
struct SlabOpening
{
    std::string name;
    std::string shape = "polygon";                      ///< rectangle, circle, polygon (information)
    std::vector<std::pair<double, double>> polygon;     ///< (u, v), au moins 3 sommets
};

enum class SlabType
{
    TwoWay,     // Portance bidirectionnelle
    OneWay,     // Portance unidirectionnelle
    FlatSlab    // Dalle champignon / plancher-dalle
};

class Slab
{
public:
    Slab() = default;
    Slab(int id, const std::vector<int>& nodeIds, double thickness = 0.20, const std::string& name = "", SlabType type = SlabType::TwoWay);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;

    const std::vector<int>& nodeIds() const { return m_nodeIds; }
    void setNodeIds(const std::vector<int>& nodeIds) { m_nodeIds = nodeIds; }

    double thickness() const { return m_thickness; }
    void setThickness(double thickness) { m_thickness = thickness; }

    const Material& material() const { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& material) { m_material = material; }
    int materialId() const { return m_material.id; }
    void setMaterialId(int id);

    SlabType slabType() const { return m_slabType; }
    void setSlabType(SlabType type) { m_slabType = type; }

    /// Aire nette (ouvertures déduites).
    double area(const Model& model) const;
    double grossArea(const Model& model) const;

    SurfaceFormulation formulation() const { return m_formulation; }
    void setFormulation(SurfaceFormulation f) { m_formulation = f; }

    const std::string& color() const { return m_color; }
    void setColor(const std::string& color) { m_color = color; }

    /// Ouvertures (maillage, affichage) ; persistées dans le chunk OPNG du .tsa (format ≥ 1.8).
    const std::vector<SlabOpening>& openings() const { return m_openings; }
    void setOpenings(const std::vector<SlabOpening>& openings) { m_openings = openings; }

private:
    int m_id = 0;
    std::string m_name;
    std::vector<int> m_nodeIds;
    double m_thickness = 0.20; // Épaisseur en mètres
    Material m_material = Material::concreteC25_30();
    SlabType m_slabType = SlabType::TwoWay;
    std::string m_color;
    std::vector<SlabOpening> m_openings;
    SurfaceFormulation m_formulation = SurfaceFormulation::Shell;
};

using SurfaceOpening = SlabOpening;

/// Aire algébrique d'un polygone 2D (u, v).
double polygonArea(const std::vector<std::pair<double, double>>& poly);

} // namespace TSA::Model
