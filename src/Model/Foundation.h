#pragma once

#include "Material.h"
#include <string>

namespace TSA::Model
{

class Model;

enum class FoundationType
{
    IsolatedFooting, // Semelle isolée sous poteau
    StripFooting,    // Semelle filante sous voile
    Raft,            // Radier général
    Pile             // Pieu
};

class Foundation
{
public:
    Foundation() = default;
    Foundation(int id, int nodeId, double widthA = 1.50, double lengthB = 1.50, double heightH = 0.50, const std::string& name = "", FoundationType type = FoundationType::IsolatedFooting);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;

    int nodeId() const { return m_nodeId; }
    void setNodeId(int id) { m_nodeId = id; }

    FoundationType foundationType() const { return m_type; }
    void setFoundationType(FoundationType t) { m_type = t; }

    double widthA() const { return m_widthA; }
    void setWidthA(double a) { m_widthA = a; }

    double lengthB() const { return m_lengthB; }
    void setLengthB(double b) { m_lengthB = b; }

    double heightH() const { return m_heightH; }
    void setHeightH(double h) { m_heightH = h; }

    const Material& material() const { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& m) { m_material = m; }

    double soilBearingCapacity() const { return m_soilBearingCapacity; } // En kPa (ex: 250 kPa = 0.25 MPa)
    void setSoilBearingCapacity(double q) { m_soilBearingCapacity = q; }

    double volume() const { return m_widthA * m_lengthB * m_heightH; }
    double baseArea() const { return m_widthA * m_lengthB; }

private:
    int m_id = 0;
    std::string m_name;
    int m_nodeId = 0;
    FoundationType m_type = FoundationType::IsolatedFooting;
    double m_widthA = 1.50;  // Largeur A en mètres
    double m_lengthB = 1.50; // Longueur B en mètres
    double m_heightH = 0.50; // Hauteur H en mètres
    Material m_material = Material::concreteC25_30();
    double m_soilBearingCapacity = 250.0; // Contrainte admissible du sol en kPa
};

} // namespace TSA::Model
