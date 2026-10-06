#include "IfcMapper.h"

#include <cstdio>

namespace TSA::BIM::Ifc
{

using W = IfcStepWriter;
using TSA::Model::MaterialType;
using TSA::Model::SectionShape;

std::string IfcMapper::stepEntity(BimCategory c)
{
    switch (c)
    {
    case BimCategory::Beam: return "IFCBEAM";
    case BimCategory::Column: return "IFCCOLUMN";
    case BimCategory::Member: return "IFCMEMBER";
    case BimCategory::Slab: return "IFCSLAB";
    case BimCategory::Wall: return "IFCWALL";
    case BimCategory::Footing: return "IFCFOOTING";
    case BimCategory::Pile: return "IFCPILE";
    case BimCategory::Plate: return "IFCPLATE";
    default: return {};
    }
}

std::string IfcMapper::predefinedType(BimCategory c, const std::string& requested)
{
    // Valeurs des énumérations IFC 4.3 utilisées par TSA (les autres → NOTDEFINED / USERDEFINED)
    static const std::map<BimCategory, std::vector<std::string>> allowed = {
        { BimCategory::Beam, { "BEAM", "JOIST", "HOLLOWCORE", "LINTEL", "SPANDREL", "T_BEAM", "GIRDER_SEGMENT", "PIERCAP", "CORNICE", "DIAPHRAGM", "EDGEBEAM" } },
        { BimCategory::Column, { "COLUMN", "PILASTER", "PIERSTEM", "PIERSTEM_SEGMENT", "STANDCOLUMN" } },
        { BimCategory::Member, { "BRACE", "CHORD", "COLLAR", "MEMBER", "MULLION", "PLATE", "POST", "PURLIN", "RAFTER", "STRINGER", "STRUT", "STUD", "STIFFENING_RIB", "ARCH_SEGMENT", "SUSPENSION_CABLE", "SUSPENDER", "STAY_CABLE", "STRUCTURALCABLE", "TIEBAR" } },
        { BimCategory::Slab, { "FLOOR", "ROOF", "LANDING", "BASESLAB", "APPROACH_SLAB", "PAVING", "WEARING", "SIDEWALK", "TRACKSLAB" } },
        { BimCategory::Wall, { "MOVABLE", "PARAPET", "PARTITIONING", "PLUMBINGWALL", "SHEAR", "SOLIDWALL", "STANDARD", "POLYGONAL", "ELEMENTEDWALL", "RETAININGWALL", "WAVEWALL" } },
        { BimCategory::Footing, { "CAISSON_FOUNDATION", "FOOTING_BEAM", "PAD_FOOTING", "PILE_CAP", "STRIP_FOOTING" } },
        { BimCategory::Pile, { "BORED", "DRIVEN", "JETGROUTING", "COHESION", "FRICTION", "SUPPORT" } },
        { BimCategory::Plate, { "CURTAIN_PANEL", "SHEET", "FLANGE_PLATE", "WEB_PLATE", "STIFFENER_PLATE", "GUSSET_PLATE", "COVER_PLATE", "SPLICE_PLATE", "BASE_PLATE" } },
    };
    auto it = allowed.find(c);
    if (it != allowed.end())
        for (const auto& v : it->second)
            if (v == requested) return v;
    return "NOTDEFINED";
}

std::string IfcMapper::profileKey(const TSA::Model::Section& s)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%d|%.6g|%.6g|%.6g|%.6g|%.6g|", static_cast<int>(s.shape), s.width, s.height,
                  s.diameter, s.tw, s.tf);
    return buf + s.name;
}

int IfcMapper::profile(const TSA::Model::Section& s)
{
    const std::string key = profileKey(s);
    if (auto it = m_profiles.find(key); it != m_profiles.end()) return it->second;

    auto& w = m_ctx.w;
    const std::string head = ".AREA.," + W::optStr(s.name) + ",$,";
    int id = 0;
    switch (s.shape)
    {
    case SectionShape::Circular:
        id = w.add("IFCCIRCLEPROFILEDEF", head + W::real(s.diameter / 2.0));
        break;
    case SectionShape::Pipe:
        id = w.add("IFCCIRCLEHOLLOWPROFILEDEF", head + W::real(s.diameter / 2.0) + "," + W::real(s.tw));
        break;
    case SectionShape::IShape:
        id = w.add("IFCISHAPEPROFILEDEF", head + W::real(s.width) + "," + W::real(s.height) + "," + W::real(s.tw) + ","
                                              + W::real(s.tf) + ",$,$,$");
        break;
    case SectionShape::BoxHollow:
        id = w.add("IFCRECTANGLEHOLLOWPROFILEDEF", head + W::real(s.width) + "," + W::real(s.height) + "," + W::real(s.tw) + ",$,$");
        break;
    case SectionShape::UPN:
        id = w.add("IFCUSHAPEPROFILEDEF", head + W::real(s.height) + "," + W::real(s.width) + "," + W::real(s.tw) + ","
                                              + W::real(s.tf) + ",$,$,$");
        break;
    case SectionShape::Angle:
        id = w.add("IFCLSHAPEPROFILEDEF", head + W::real(s.height) + "," + W::real(s.width) + "," + W::real(s.tw) + ",$,$,$");
        break;
    case SectionShape::TSection:
        id = w.add("IFCTSHAPEPROFILEDEF", head + W::real(s.height) + "," + W::real(s.width) + "," + W::real(s.tw) + ","
                                              + W::real(s.tf) + ",$,$,$,$,$");
        break;
    case SectionShape::Rectangular:
    default:
        id = w.add("IFCRECTANGLEPROFILEDEF", head + W::real(s.width) + "," + W::real(s.height));
        break;
    }
    m_profiles[key] = id;
    return id;
}

namespace
{
const char* materialCategory(MaterialType t)
{
    switch (t)
    {
    case MaterialType::Concrete:
    case MaterialType::ReinforcedConcrete: return "concrete";
    case MaterialType::Steel:
    case MaterialType::RebarSteel:
    case MaterialType::GalvanizedSteel: return "steel";
    case MaterialType::Aluminum: return "aluminium";
    case MaterialType::Timber: return "wood";
    case MaterialType::Masonry: return "block";
    default: return "";
    }
}
} // namespace

int IfcMapper::material(const TSA::Model::Material& m)
{
    const std::string key = m.name;
    if (auto it = m_materials.find(key); it != m_materials.end()) return it->second;

    auto& w = m_ctx.w;
    const std::string cat = materialCategory(m.type);
    const int mat = w.add("IFCMATERIAL", W::str(m.name.empty() ? "Matériau" : m.name) + ",$," + W::optStr(cat));

    auto prop = [&w](const char* name, const std::string& value) {
        return w.add("IFCPROPERTYSINGLEVALUE", std::string("'") + name + "',$," + value + ",$");
    };
    const int e = prop("YoungModulus", "IFCMODULUSOFELASTICITYMEASURE(" + W::real(m.E) + ")");
    const int nu = prop("PoissonRatio", "IFCPOSITIVERATIOMEASURE(" + W::real(m.nu) + ")");
    const int th = prop("ThermalExpansionCoefficient", "IFCTHERMALEXPANSIONCOEFFICIENTMEASURE(" + W::real(m.thermalCoeff) + ")");
    w.add("IFCMATERIALPROPERTIES", "'Pset_MaterialMechanical',$," + W::refs({ e, nu, th }) + "," + W::ref(mat));
    const int rho = prop("MassDensity", "IFCMASSDENSITYMEASURE(" + W::real(m.density) + ")");
    w.add("IFCMATERIALPROPERTIES", "'Pset_MaterialCommon',$," + W::refs({ rho }) + "," + W::ref(mat));
    // Résistance caractéristique (fck / fy) : pas de propriété standard commune → Pset propre à TSA
    const int fk = prop("CharacteristicStrength", "IFCPRESSUREMEASURE(" + W::real(m.fk) + ")");
    w.add("IFCMATERIALPROPERTIES", "'Pset_TSA_Material',$," + W::refs({ fk }) + "," + W::ref(mat));

    m_materials[key] = mat;
    return mat;
}

int IfcMapper::materialProfileSet(const TSA::Model::Material& m, const TSA::Model::Section& s)
{
    const std::string key = m.name + "#" + profileKey(s);
    if (auto it = m_profileSets.find(key); it != m_profileSets.end()) return it->second;
    const int mat = material(m);
    const int prof = profile(s);
    const int mp = m_ctx.w.add("IFCMATERIALPROFILE", W::optStr(s.name) + ",$," + W::ref(mat) + "," + W::ref(prof) + ",$,$");
    const int set = m_ctx.w.add("IFCMATERIALPROFILESET", W::optStr(m.name + " / " + s.name) + ",$," + W::refs({ mp }) + ",$");
    m_profileSets[key] = set;
    return set;
}

} // namespace TSA::BIM::Ifc
