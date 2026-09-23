#pragma once

#include "Section.h"
#include "Material.h"
#include <string>

namespace TSA::Model
{

class Model;

enum class TrussMemberRole
{
    TopChord,
    BottomChord,
    Vertical,
    Diagonal,
    Brace
};

class TrussMember
{
public:
    TrussMember() = default;
    TrussMember(int id, int startNodeId, int endNodeId, double diameterOrWidth = 0.10, const std::string& name = "", TrussMemberRole role = TrussMemberRole::Diagonal);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    std::string formattedName() const;

    int startNodeId() const { return m_startNodeId; }
    void setStartNodeId(int nodeId) { m_startNodeId = nodeId; }

    int endNodeId() const { return m_endNodeId; }
    void setEndNodeId(int nodeId) { m_endNodeId = nodeId; }

    TrussMemberRole role() const { return m_role; }
    void setRole(TrussMemberRole r) { m_role = r; }

    const Section& section() const { return m_section; }
    Section& section() { return m_section; }
    void setSection(const Section& s) { m_section = s; }

    const Material& material() const { return m_material; }
    Material& material() { return m_material; }
    void setMaterial(const Material& m) { m_material = m; }

    double length(const Model& model) const;

private:
    int m_id = 0;
    std::string m_name;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
    TrussMemberRole m_role = TrussMemberRole::Diagonal;
    Section m_section = Section::circular(0.10);
    Material m_material = Material::steelS235();
};

} // namespace TSA::Model
