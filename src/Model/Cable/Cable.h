#pragma once

#include "CableTypes.h"
#include "CableStandards.h"
#include "CableAnchor.h"
#include "CablePrestress.h"
#include "CableAnalysisProperties.h"
#include "CableDefinition.h"
#include "CableGeometry.h"
#include "../Section.h"
#include "../Material.h"
#include "../Element.h"

#include <string>
#include <vector>

namespace TSA::Model
{

class Model;

/**
 * @brief Élément structural câble haute performance pour le génie civil et les ponts.
 * Intègre la géométrie 3D, le système de tension / précontrainte, les ancrages,
 * la section transversale, le matériau normé et les paramètres d'analyse non linéaire.
 * Hérite conceptuellement de LinearElement (ElementLineaire).
 */
class Cable : public LinearElement, public ICableElement
{
public:
    Cable();
    Cable(int id, int startNodeId, int endNodeId, const std::string& name = "", CableType type = CableType::Generic);
    Cable(int id, int startNodeId, int endNodeId, const CableDefinition& definition, const std::string& name = "");

    int id() const override { return m_id; }
    void setId(int id) { m_id = id; }

    std::string name() const override { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;
    std::string typeName() const override { return "Cable"; }

    int startNodeId() const override { return m_startNodeId; }
    void setStartNodeId(int nodeId) { m_startNodeId = nodeId; }

    int endNodeId() const override { return m_endNodeId; }
    void setEndNodeId(int nodeId) { m_endNodeId = nodeId; }

    CableType type() const { return m_type; }
    void setType(CableType type) { m_type = type; }

    // Définition catalogue associée
    const CableDefinition& definition() const { return m_definition; }
    CableDefinition& definition() { return m_definition; }
    void setDefinition(const CableDefinition& def);

    // Géométrie longitudinale du câble
    CableGeometryMode geometryMode() const { return m_geometry.mode(); }
    void setGeometryMode(CableGeometryMode mode) { m_geometry.setMode(mode); }

    const CableGeometry& geometry() const { return m_geometry; }
    CableGeometry& geometry() { return m_geometry; }
    void setGeometry(const CableGeometry& geom) { m_geometry = geom; }

    double sag() const { return m_geometry.sag(); }
    void setSag(double s) { m_geometry.setSag(s); }

    const std::vector<gp_Pnt>& intermediatePoints() const { return m_geometry.intermediatePoints(); }
    void setIntermediatePoints(const std::vector<gp_Pnt>& pts) { m_geometry.setIntermediatePoints(pts); }

    // Section et Matériau
    const Section& section() const override { return m_section; }
    Section& section() { return m_section; }
    void setSection(const Section& sec) { m_section = sec; }

    double diameter() const { return m_definition.nominalDiameter() > 0.0 ? m_definition.nominalDiameter() : m_section.diameter; }
    void setDiameter(double d);

    const Material& material() const override { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& mat) { m_material = mat; }

    // Précontrainte / Tension initiale
    const CablePrestress& prestress() const { return m_prestress; }
    CablePrestress& prestress() { return m_prestress; }
    void setPrestress(const CablePrestress& p) { m_prestress = p; }

    double initialTension() const override { return m_prestress.initialTension; }
    void setInitialTension(double forceN) { m_prestress.initialTension = forceN; }

    // Propriétés d'analyse non linéaire
    const CableAnalysisProperties& analysisProperties() const { return m_analysis; }
    CableAnalysisProperties& analysisProperties() { return m_analysis; }
    void setAnalysisProperties(const CableAnalysisProperties& a) { m_analysis = a; }

    bool tensionOnly() const override { return m_analysis.tensionOnly; }
    void setTensionOnly(bool to) { m_analysis.tensionOnly = to; }

    // Ancrages d'extrémité
    const CableAnchor& startAnchor() const { return m_startAnchor; }
    CableAnchor& startAnchor() { return m_startAnchor; }
    void setStartAnchor(const CableAnchor& a) { m_startAnchor = a; }

    const CableAnchor& endAnchor() const { return m_endAnchor; }
    CableAnchor& endAnchor() { return m_endAnchor; }
    void setEndAnchor(const CableAnchor& a) { m_endAnchor = a; }

    // Couleur d'affichage dans le viewport 3D
    const std::string& color() const { return m_color; }
    void setColor(const std::string& col) { m_color = col; }

    // ICableElement overrides & calculs mécaniques
    double length() const override; // Longueur chord par défaut
    double length(const Model& model) const override;
    double arcLength(const Model& model) const { return length(model); }
    double chordLength(const Model& model) const;
    double metallicArea() const override;
    double elasticModulus() const override;
    double equivalentErnstModulus(const Model& model) const;
    double equivalentElasticModulus(const Model& model) const { return equivalentErnstModulus(model); }
    double weight(const Model& model) const override;

    // Discrétisation synchronisée avec les coordonnées réelles des nœuds du modèle
    std::vector<gp_Pnt> sampleWorldPoints(const Model& model, int numSamples = 30) const;

private:
    int m_id = 0;
    std::string m_name;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
    CableType m_type = CableType::Generic;

    CableDefinition m_definition;
    CableGeometry m_geometry;
    Section m_section;
    Material m_material;
    CablePrestress m_prestress;
    CableAnalysisProperties m_analysis;

    CableAnchor m_startAnchor;
    CableAnchor m_endAnchor;

    std::string m_color = "#E06622"; // Couleur cuivrée / câble tendu caractéristique
};

} // namespace TSA::Model
