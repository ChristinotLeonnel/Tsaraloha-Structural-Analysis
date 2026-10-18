#include "IfcOpenShellImport.h"

#include "../Core/BimModel.h"
#include "../../Coordinate/LevelManager.h"
#include "../../Model/Model.h"

#include <QCoreApplication>

#ifdef TSA_HAVE_IFCOPENSHELL
#include <ifcgeom/filter.h>
#include <ifcgeom/iterator.h>
#include <ifcgeom/kernel_registry.h>
#include <ifcparse/file.h>
#endif

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace TSA::BIM::Ifc::detail
{

#ifndef TSA_HAVE_IFCOPENSHELL

bool fullEngineAvailable() { return false; }
std::string fullEngineName() { return {}; }
bool importPhysicalProducts(const std::string&, TSA::Model::Model&, const IfcImportOptions&, IfcImportReport& report)
{
    report.error = QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "noyau IFC complet (IfcOpenShell) absent de cette version de TSA").toStdString();
    return false;
}

bool verifyFile(const std::string&, std::string&, int&, std::string& error)
{
    error = QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "relecture indépendante indisponible (IfcOpenShell absent de cette version de TSA)")
                .toStdString();
    return false;
}

#else

namespace
{
using Ent = express::entity;
using ifcopenshell::attribute_value;

// ------------------------------------------------------------------------------------------- accès aux attributs
// Par nom, quel que soit le schéma : un attribut absent (IFC2X3 ≠ IFC4) ou nul donne une valeur vide, jamais une
// exception (entity::get lève ifcopenshell::exception pour un nom inconnu du schéma).

std::optional<attribute_value> attr(const Ent& e, const char* name)
{
    if (!e) return std::nullopt;
    try
    {
        auto v = e.get(name);
        if (v.isNull()) return std::nullopt;
        return v;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool isA(const express::base& e, const char* type)
{
    return e && e.declaration().is(type);
}

/// Valeur simple : chaîne, énumération, nombre, booléen, ou valeur typée (IfcLabel('…'), IfcLengthMeasure(…)).
std::string scalarText(const attribute_value& v)
{
    try
    {
        switch (v.type())
        {
        case ifcopenshell::Argument_STRING: return static_cast<std::string>(v);
        case ifcopenshell::Argument_ENUMERATION: return static_cast<ifcopenshell::enumeration_reference>(v).value();
        case ifcopenshell::Argument_INT: return std::to_string(static_cast<int64_t>(v));
        case ifcopenshell::Argument_BOOL: return static_cast<bool>(v) ? "true" : "false";
        case ifcopenshell::Argument_DOUBLE:
        {
            std::ostringstream s;
            s << static_cast<double>(v);
            return s.str();
        }
        case ifcopenshell::Argument_ENTITY_INSTANCE:
        {
            const express::base b = v;
            if (b && b.declaration().as_entity() == nullptr) return scalarText(b.get_attribute_value(0));   // type déclaré
            return b ? b.declaration().name() : std::string();
        }
        default: return {};
        }
    }
    catch (...)
    {
        return {};
    }
}

std::string text(const Ent& e, const char* name)
{
    const auto v = attr(e, name);
    return v ? scalarText(*v) : std::string();
}

std::optional<double> number(const attribute_value& v)
{
    try
    {
        switch (v.type())
        {
        case ifcopenshell::Argument_DOUBLE: return static_cast<double>(v);
        case ifcopenshell::Argument_INT: return static_cast<double>(static_cast<int64_t>(v));
        case ifcopenshell::Argument_ENTITY_INSTANCE:
        {
            const express::base b = v;
            if (b && b.declaration().as_entity() == nullptr) return number(b.get_attribute_value(0));
            return std::nullopt;
        }
        default: return std::nullopt;
        }
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<double> number(const Ent& e, const char* name)
{
    const auto v = attr(e, name);
    return v ? number(*v) : std::nullopt;
}

Ent entity(const Ent& e, const char* name)
{
    const auto v = attr(e, name);
    if (!v || v->type() != ifcopenshell::Argument_ENTITY_INSTANCE) return {};
    try
    {
        const express::base b = *v;
        return b.as<Ent>();
    }
    catch (...)
    {
        return {};
    }
}

std::vector<Ent> entities(const Ent& e, const char* name)
{
    std::vector<Ent> out;
    const auto v = attr(e, name);
    if (!v) return out;
    try
    {
        if (v->type() == ifcopenshell::Argument_ENTITY_INSTANCE)
        {
            const express::base b = *v;
            if (auto x = b.as<Ent>()) out.push_back(x);
        }
        else if (v->type() == ifcopenshell::Argument_AGGREGATE_OF_ENTITY_INSTANCE)
            for (const express::base& b : static_cast<std::vector<express::base>>(*v))
                if (auto x = b.as<Ent>()) out.push_back(x);
    }
    catch (...)
    {
    }
    return out;
}

std::vector<Ent> inverse(const Ent& e, const char* name)
{
    try
    {
        return e ? e.get_inverse(name) : std::vector<Ent> {};
    }
    catch (...)
    {
        return {};
    }
}

// -------------------------------------------------------------------------------------------------- unités
struct Units
{
    double length = 1.0, area = 1.0, volume = 1.0, force = 1.0, pressure = 1.0;
};

double prefixFactor(const std::string& p)
{
    static const std::map<std::string, double> f = { { "EXA", 1e18 }, { "PETA", 1e15 }, { "TERA", 1e12 }, { "GIGA", 1e9 },
                                                      { "MEGA", 1e6 }, { "KILO", 1e3 }, { "HECTO", 1e2 }, { "DECA", 1e1 },
                                                      { "DECI", 1e-1 }, { "CENTI", 1e-2 }, { "MILLI", 1e-3 }, { "MICRO", 1e-6 },
                                                      { "NANO", 1e-9 }, { "PICO", 1e-12 } };
    auto it = f.find(p);
    return it == f.end() ? 1.0 : it->second;
}

/// Facteur vers l'unité SI d'une unité nommée (IfcSIUnit préfixée, IfcConversionBasedUnit : pied, pouce, kip…).
double unitFactor(const Ent& u, int power, int depth = 0)
{
    if (!u || depth > 4) return 1.0;
    if (isA(u, "IfcSIUnit")) return std::pow(prefixFactor(text(u, "Prefix")), power);
    if (isA(u, "IfcConversionBasedUnit"))
    {
        const Ent m = entity(u, "ConversionFactor");
        const double value = number(m, "ValueComponent").value_or(1.0);
        return value * unitFactor(entity(m, "UnitComponent"), power, depth + 1);
    }
    return 1.0;
}

Units projectUnits(ifcopenshell::file& file)
{
    Units u;
    for (const express::base& b : file.instances_by_type("IfcProject"))
    {
        const Ent project = b.as<Ent>();
        for (const Ent& unit : entities(entity(project, "UnitsInContext"), "Units"))
        {
            const std::string t = text(unit, "UnitType");
            if (t == "LENGTHUNIT") u.length = unitFactor(unit, 1);
            else if (t == "AREAUNIT") u.area = unitFactor(unit, 2);
            else if (t == "VOLUMEUNIT") u.volume = unitFactor(unit, 3);
            else if (t == "FORCEUNIT") u.force = unitFactor(unit, 1);
            else if (t == "PRESSUREUNIT") u.pressure = unitFactor(unit, 1);
        }
        break;   // un seul IfcProject par fichier
    }
    return u;
}

// --------------------------------------------------------------------------------------------- propriétés
using PType = PropertyValue::Type;

/// Valeur typée IFC → PropertyValue, convertie en unités SI (longueurs en m, surfaces en m², volumes en m³…).
PropertyValue typedValue(const attribute_value& v, const Units& u)
{
    const express::base b = v.type() == ifcopenshell::Argument_ENTITY_INSTANCE ? static_cast<express::base>(v) : express::base();
    const std::string type = b ? b.declaration().name() : std::string();
    const attribute_value w = b && b.declaration().as_entity() == nullptr ? b.get_attribute_value(0) : v;
    try
    {
        switch (w.type())
        {
        case ifcopenshell::Argument_BOOL: return PropertyValue::ofBool(static_cast<bool>(w));
        case ifcopenshell::Argument_LOGICAL:
        {
            const boost::logic::tribool t = w;
            if (boost::logic::indeterminate(t)) return PropertyValue::ofText("UNKNOWN", PType::Label);
            return PropertyValue::ofBool(static_cast<bool>(t));
        }
        case ifcopenshell::Argument_INT: return PropertyValue::ofReal(static_cast<double>(static_cast<int64_t>(w)), PType::Integer);
        case ifcopenshell::Argument_DOUBLE:
        {
            const double x = static_cast<double>(w);
            if (type == "IfcLengthMeasure" || type == "IfcPositiveLengthMeasure" || type == "IfcNonNegativeLengthMeasure")
                return PropertyValue::ofReal(x * u.length, PType::Length);
            if (type == "IfcAreaMeasure") return PropertyValue::ofReal(x * u.area);
            if (type == "IfcVolumeMeasure") return PropertyValue::ofReal(x * u.volume);
            if (type == "IfcForceMeasure") return PropertyValue::ofReal(x * u.force, PType::Force);
            if (type == "IfcPressureMeasure" || type == "IfcModulusOfElasticityMeasure") return PropertyValue::ofReal(x * u.pressure, PType::Pressure);
            if (type == "IfcRatioMeasure" || type == "IfcPositiveRatioMeasure" || type == "IfcNormalisedRatioMeasure")
                return PropertyValue::ofReal(x, PType::Ratio);
            if (type == "IfcCountMeasure" || type == "IfcInteger") return PropertyValue::ofReal(x, PType::Integer);
            return PropertyValue::ofReal(x);
        }
        case ifcopenshell::Argument_STRING:
        case ifcopenshell::Argument_ENUMERATION:
            return PropertyValue::ofText(scalarText(w), type == "IfcText" ? PType::Text : PType::Label);
        default: return PropertyValue::ofText(scalarText(w));
        }
    }
    catch (...)
    {
        return PropertyValue::ofText(scalarText(v));
    }
}

std::string joinValues(const Ent& p, const char* attribute, const Units& u)
{
    std::string out;
    const auto v = attr(p, attribute);
    if (!v) return out;
    try
    {
        if (v->type() == ifcopenshell::Argument_AGGREGATE_OF_ENTITY_INSTANCE)
            for (const express::base& b : static_cast<std::vector<express::base>>(*v))
                out += (out.empty() ? "" : ", ") + typedValue(attribute_value(b.get_attribute_value(0)), u).toString();
        else
            out = scalarText(*v);
    }
    catch (...)
    {
    }
    return out;
}

/// Propriétés d'un jeu (IfcPropertySet / IfcElementQuantity) ; toutes les formes de propriété sont conservées, les
/// formes non scalaires sous forme de texte (énumérée, liste, bornée, table, complexe, référence).
void readProperties(const Ent& set, const Units& u, PropertySet& out, const std::string& prefix = {})
{
    if (isA(set, "IfcElementQuantity"))
    {
        for (const Ent& q : entities(set, "Quantities"))
        {
            const std::string name = text(q, "Name");
            if (isA(q, "IfcQuantityLength")) out.properties[name] = PropertyValue::ofReal(number(q, "LengthValue").value_or(0) * u.length, PType::Length);
            else if (isA(q, "IfcQuantityArea")) out.properties[name] = PropertyValue::ofReal(number(q, "AreaValue").value_or(0) * u.area);
            else if (isA(q, "IfcQuantityVolume")) out.properties[name] = PropertyValue::ofReal(number(q, "VolumeValue").value_or(0) * u.volume);
            else if (isA(q, "IfcQuantityCount")) out.properties[name] = PropertyValue::ofReal(number(q, "CountValue").value_or(0), PType::Integer);
            else if (isA(q, "IfcQuantityWeight")) out.properties[name] = PropertyValue::ofReal(number(q, "WeightValue").value_or(0));
            else if (isA(q, "IfcQuantityTime")) out.properties[name] = PropertyValue::ofReal(number(q, "TimeValue").value_or(0));
        }
        return;
    }
    for (const Ent& p : entities(set, "HasProperties"))
    {
        const std::string name = prefix + text(p, "Name");
        if (isA(p, "IfcPropertySingleValue"))
        {
            if (const auto v = attr(p, "NominalValue")) out.properties[name] = typedValue(*v, u);
        }
        else if (isA(p, "IfcPropertyEnumeratedValue"))
            out.properties[name] = PropertyValue::ofText(joinValues(p, "EnumerationValues", u), PType::Label);
        else if (isA(p, "IfcPropertyListValue"))
            out.properties[name] = PropertyValue::ofText(joinValues(p, "ListValues", u));
        else if (isA(p, "IfcPropertyBoundedValue"))
        {
            const auto lo = attr(p, "LowerBoundValue"), hi = attr(p, "UpperBoundValue");
            out.properties[name] = PropertyValue::ofText((lo ? typedValue(*lo, u).toString() : "") + " … " + (hi ? typedValue(*hi, u).toString() : ""));
        }
        else if (isA(p, "IfcPropertyTableValue"))
            out.properties[name] = PropertyValue::ofText("table : " + joinValues(p, "DefiningValues", u) + " → " + joinValues(p, "DefinedValues", u));
        else if (isA(p, "IfcPropertyReferenceValue"))
        {
            const Ent ref = entity(p, "PropertyReference");
            out.properties[name] = PropertyValue::ofText(ref ? ref.declaration().name() + " " + text(ref, "Name") : "");
        }
        else if (isA(p, "IfcComplexProperty"))
            readProperties(p, u, out, name + ".");
    }
}

// ------------------------------------------------------------------------------------- classes et statut
struct Classification
{
    BimCategory category = BimCategory::Proxy;
    AnalyticalStatus status = AnalyticalStatus::NotStructural;
    const char* note = nullptr;
};

Classification classify(const Ent& product)
{
    static const std::pair<const char*, BimCategory> structural[] = {
        { "IfcBeam", BimCategory::Beam }, { "IfcColumn", BimCategory::Column }, { "IfcMember", BimCategory::Member },
        { "IfcSlab", BimCategory::Slab }, { "IfcWall", BimCategory::Wall }, { "IfcFooting", BimCategory::Footing },
        { "IfcPile", BimCategory::Pile }, { "IfcPlate", BimCategory::Plate },
    };
    for (const auto& [type, cat] : structural)
        if (isA(product, type)) return { cat, AnalyticalStatus::NeedsPreparation, nullptr };   // sous-types compris (…StandardCase)
    if (isA(product, "IfcBearing")) return { BimCategory::Proxy, AnalyticalStatus::NeedsPreparation, "appareil d'appui : condition d'appui à définir" };
    if (isA(product, "IfcBuildingElementProxy"))
        return { BimCategory::Proxy, AnalyticalStatus::NotStructural, "objet générique : rôle porteur à confirmer (propriété LoadBearing)" };
    if (isA(product, "IfcReinforcingElement") || isA(product, "IfcTendon"))
        return { BimCategory::Proxy, AnalyticalStatus::NotStructural, "armature : conservée, non représentée dans le calcul" };
    return {};
}

/// LoadBearing des Psets « …Common » : surcharge le statut quand le fichier le précise.
std::optional<bool> loadBearing(const std::vector<PropertySet>& psets)
{
    for (const auto& ps : psets)
        if (ps.name.rfind("Pset_", 0) == 0 && ps.name.size() > 6 && ps.name.compare(ps.name.size() - 6, 6, "Common") == 0)
            if (auto it = ps.properties.find("LoadBearing"); it != ps.properties.end() && it->second.type == PType::Boolean)
                return it->second.flag;
    return std::nullopt;
}

// ------------------------------------------------------------------------------------- matériaux, profils
void collectMaterials(const Ent& m, std::set<std::string>& names, std::set<std::string>& profiles, int depth = 0)
{
    if (!m || depth > 4) return;
    if (isA(m, "IfcMaterial"))
    {
        names.insert(text(m, "Name"));
        return;
    }
    if (isA(m, "IfcMaterialLayerSetUsage")) return collectMaterials(entity(m, "ForLayerSet"), names, profiles, depth + 1);
    if (isA(m, "IfcMaterialProfileSetUsage")) return collectMaterials(entity(m, "ForProfileSet"), names, profiles, depth + 1);
    for (const Ent& l : entities(m, "MaterialLayers")) collectMaterials(entity(l, "Material"), names, profiles, depth + 1);
    for (const Ent& p : entities(m, "MaterialProfiles"))
    {
        collectMaterials(entity(p, "Material"), names, profiles, depth + 1);
        const Ent profile = entity(p, "Profile");
        if (profile) profiles.insert(text(profile, "ProfileName").empty() ? profile.declaration().name() : text(profile, "ProfileName"));
    }
    for (const Ent& c : entities(m, "MaterialConstituents")) collectMaterials(entity(c, "Material"), names, profiles, depth + 1);
    for (const Ent& x : entities(m, "Materials")) collectMaterials(x, names, profiles, depth + 1);   // IfcMaterialList
}

std::string joinSet(const std::set<std::string>& s)
{
    std::string out;
    for (const auto& x : s)
        if (!x.empty()) out += (out.empty() ? "" : ", ") + x;
    return out;
}

/// Type de représentation « Body » du produit (SweptSolid, Brep, Tessellation, MappedRepresentation, Clipping…).
std::string bodyRepresentationType(const Ent& product)
{
    for (const Ent& r : entities(entity(product, "Representation"), "Representations"))
        if (text(r, "RepresentationIdentifier") == "Body") return text(r, "RepresentationType");
    return {};
}

// ---------------------------------------------------------------------------------------------- import
class PhysicalImport
{
public:
    PhysicalImport(ifcopenshell::file& file, TSA::Model::Model& model, const IfcImportOptions& opt, IfcImportReport& report)
        : m_file(file), m_model(model), m_opt(opt), m_report(report)
    {
    }

    bool run()
    {
        m_units = projectUnits(m_file);
        m_report.lengthUnit = m_units.length;
        ensureSpatial();
        if (!progress(15, QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Propriétés et relations").toStdString())) return false;
        if (m_opt.importGeometry && !geometry()) return false;
        if (!progress(92, QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Produits sans géométrie").toStdString())) return false;
        productsWithoutGeometry();
        finalizeReport();
        return true;
    }

private:
    bool progress(int percent, const std::string& stage)
    {
        if (m_opt.progress && !m_opt.progress(percent, stage))
        {
            m_report.canceled = true;
            return false;
        }
        return true;
    }

    /// Structure spatiale et étages : repris de la passe analytique, complétés ici s'ils manquent (fichier non
    /// lisible par le lecteur interne, ou sans modèle analytique).
    void ensureSpatial()
    {
        auto& bim = m_model.bimForEdit();
        auto spatial = [this](const char* type, SpatialElement& out) {
            for (const express::base& b : m_file.instances_by_type(type))
            {
                const Ent e = b.as<Ent>();
                if (out.globalId.empty()) out.globalId = text(e, "GlobalId");
                if (!text(e, "Name").empty()) out.name = text(e, "Name");
                if (out.description.empty()) out.description = text(e, "Description");
                break;
            }
        };
        spatial("IfcProject", bim.spatial().project);
        spatial("IfcSite", bim.spatial().site);
        spatial("IfcBuilding", bim.spatial().building);

        for (const auto& [levelId, guid] : bim.spatial().storeyGlobalIds) m_levelByStorey[guid] = levelId;
        auto* lm = m_model.levelManager();
        if (!lm) return;
        for (const express::base& b : m_file.instances_by_type("IfcBuildingStorey"))
        {
            const Ent st = b.as<Ent>();
            const std::string guid = text(st, "GlobalId");
            if (m_levelByStorey.count(guid)) continue;
            // Cote : attribut Elevation (unités du fichier). Sans elle, 0 et signalé (le placement de l'étage n'est
            // pas recalculé ici).
            const auto elevation = number(st, "Elevation");
            const double z = elevation.value_or(0.0) * m_units.length;
            if (!elevation)
                m_report.warnings.push_back(QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Étage « %1 » sans cote (Elevation) : placé à 0 m.")
                                                .arg(QString::fromStdString(text(st, "Name")))
                                                .toStdString());
            const TSA::Coordinate::Level* lvl = nullptr;
            for (const auto& l : lm->levels())
                if (std::abs(l.elevation - z) < 1e-6) lvl = &l;
            const std::string name = text(st, "Name").empty() ? "Étage" : text(st, "Name");
            const std::string id = lvl ? lvl->id : lm->addLevel(name, z)->id;
            m_levelByStorey[guid] = id;
            bim.spatial().storeyGlobalIds[id] = guid;
            ++m_report.storeys;
        }
    }

    /// Données non géométriques d'un produit (identité, propriétés, matériaux, classification, étage, relations).
    PhysicalElement describe(const Ent& product)
    {
        PhysicalElement e;
        const Classification c = classify(product);
        e.category = c.category;
        e.status = c.status;
        e.ifcClass = product.declaration().name();
        e.globalId = text(product, "GlobalId");
        e.name = text(product, "Name");
        e.description = text(product, "Description");
        e.objectType = text(product, "ObjectType");
        e.tag = text(product, "Tag");
        e.predefinedType = text(product, "PredefinedType");
        if (c.note) e.importNotes.push_back(c.note);

        // Propriétés et quantités : propres à l'objet, puis celles de son type (non déjà présentes)
        std::set<std::string> seen;
        auto addSet = [&](const Ent& def) {
            if (!(isA(def, "IfcPropertySet") || isA(def, "IfcElementQuantity"))) return;
            PropertySet ps;
            ps.name = text(def, "Name");
            if (ps.name.empty() || !seen.insert(ps.name).second) return;
            readProperties(def, m_units, ps);
            if (!ps.properties.empty()) e.propertySets.push_back(std::move(ps));
        };
        std::vector<Ent> types;
        for (const Ent& rel : inverse(product, "IsDefinedBy"))
        {
            if (isA(rel, "IfcRelDefinesByProperties"))
                for (const Ent& def : entities(rel, "RelatingPropertyDefinition")) addSet(def);
            else if (isA(rel, "IfcRelDefinesByType"))   // IFC2X3
                types.push_back(entity(rel, "RelatingType"));
        }
        for (const Ent& rel : inverse(product, "IsTypedBy")) types.push_back(entity(rel, "RelatingType"));   // IFC4
        std::string typeName;
        for (const Ent& t : types)
        {
            if (!t) continue;
            typeName = text(t, "Name");
            for (const Ent& def : entities(t, "HasPropertySets")) addSet(def);
        }
        if (const auto lb = loadBearing(e.propertySets); lb && e.status != AnalyticalStatus::Analytical)
        {
            e.status = *lb ? AnalyticalStatus::NeedsPreparation : AnalyticalStatus::NotStructural;
            e.importNotes.push_back(std::string("LoadBearing = ") + (*lb ? "TRUE" : "FALSE") + " dans le fichier");
        }

        // Matériaux, profils, classification
        std::set<std::string> materials, profiles;
        for (const Ent& rel : inverse(product, "HasAssociations"))
        {
            if (isA(rel, "IfcRelAssociatesMaterial")) collectMaterials(entity(rel, "RelatingMaterial"), materials, profiles);
            else if (isA(rel, "IfcRelAssociatesClassification"))
            {
                const Ent ref = entity(rel, "RelatingClassification");
                if (!isA(ref, "IfcClassificationReference")) continue;
                std::string id = text(ref, "Identification");
                if (id.empty()) id = text(ref, "ItemReference");   // IFC2X3
                e.classifications.push_back({ text(entity(ref, "ReferencedSource"), "Name"), id, text(ref, "Name") });
            }
        }

        // Étage (contenance spatiale), décomposition, ouvertures
        for (const Ent& rel : inverse(product, "ContainedInStructure"))
            if (auto it = m_levelByStorey.find(text(entity(rel, "RelatingStructure"), "GlobalId")); it != m_levelByStorey.end())
                e.storeyLevelId = it->second;
        std::string parent;
        for (const Ent& rel : inverse(product, "Decomposes"))
            if (const Ent whole = entity(rel, "RelatingObject")) parent = whole.declaration().name() + " " + text(whole, "GlobalId");
        const auto openings = inverse(product, "HasOpenings").size();

        // Données d'origine sans équivalent dans le modèle TSA, réexportées telles quelles (jeu « TSA_Import », complété
        // s'il vient déjà du fichier : réimport d'un IFC écrit par TSA).
        PropertySet* origin = nullptr;
        for (auto& ps : e.propertySets)
            if (ps.name == "TSA_Import") origin = &ps;
        const bool created = origin == nullptr;
        if (created)
        {
            e.propertySets.push_back({ "TSA_Import", {} });
            origin = &e.propertySets.back();
        }
        // Jeu venu du fichier (IFC écrit par TSA) : ses valeurs décrivent la source d'origine et priment sur l'état de ce
        // fichier, qui est la conversion faite par TSA (maillage « Tessellation », ouvertures déjà soustraites…).
        auto keep = [origin, created](const char* key) { return created || !origin->properties.count(key); };
        auto put = [&](const char* key, const std::string& v) {
            if (!v.empty() && keep(key)) origin->properties[key] = PropertyValue::ofText(v, PType::Label);
        };
        put("Materials", joinSet(materials));
        put("Profiles", joinSet(profiles));
        put("TypeName", typeName);
        put("Parent", parent);
        put("Representation", bodyRepresentationType(product));
        if (openings && keep("Openings")) origin->properties["Openings"] = PropertyValue::ofReal(static_cast<double>(openings), PType::Integer);
        if (isA(product, "IfcDoor") || isA(product, "IfcWindow"))
            for (const char* key : { "OverallHeight", "OverallWidth" })
                if (const auto v = number(product, key); v && keep(key)) origin->properties[key] = PropertyValue::ofReal(*v * m_units.length, PType::Length);
        // Classe écrite autrement par TSA (proxy pour une classe qu'il ne sait pas exporter, …StandardCase absente
        // d'IFC4X3) : la classe d'origine est rétablie.
        if (auto it = origin->properties.find("OriginalIfcClass"); it != origin->properties.end())
        {
            const std::string written = product.declaration().name();
            if (isA(product, "IfcBuildingElementProxy") || it->second.text.rfind(written, 0) == 0) e.ifcClass = it->second.text;
        }
        if (created && origin->properties.empty()) e.propertySets.pop_back();
        return e;
    }

    /// Insère le produit, ou complète celui de même GlobalId créé par la passe analytique (éléments de calcul gardés).
    void store(PhysicalElement e, IfcImportItem::Outcome outcome, const std::string& note)
    {
        auto& bim = m_model.bimForEdit();
        if (const auto* existing = bim.elementByGlobalId(e.globalId))
        {
            auto* p = bim.element(existing->id);
            p->ifcClass = e.ifcClass;
            if (e.geometry) p->geometry = e.geometry;
            for (auto& ps : e.propertySets)
                if (!p->propertySet(ps.name) && ps.name != "Pset_TSA_Structural") p->propertySets.push_back(std::move(ps));
            if (p->classifications.empty()) p->classifications = std::move(e.classifications);
            for (auto& n : e.importNotes) p->importNotes.push_back(std::move(n));
            if (!p->analytical.empty())
            {
                p->status = AnalyticalStatus::Analytical;
                outcome = IfcImportItem::Outcome::Analytical;
            }
            m_report.items.push_back({ p->globalId, p->ifcClass, p->name, outcome, note });
            return;
        }
        m_report.items.push_back({ e.globalId, e.ifcClass, e.name, outcome, note });
        if (e.predefinedType.empty()) e.predefinedType = "NOTDEFINED";
        bim.insert(std::move(e));
    }

    bool geometry()
    {
        ifcopenshell::geom::settings settings;
        settings.get<ifcopenshell::geom::settings::UseWorldCoords>().value = true;
        settings.get<ifcopenshell::geom::settings::WeldVertices>().value = true;
        settings.get<ifcopenshell::geom::settings::ApplyDefaultMaterials>().value = true;
        // Ouvertures (déjà soustraites des murs et dalles), espaces, structure spatiale, annotations, grilles : pas
        // des objets physiques.
        std::vector<ifcopenshell::geom::filter_function> filters;
        filters.push_back(ifcopenshell::geom::entity_filter(false, false,
            { "IfcOpeningElement", "IfcSpace", "IfcSpatialStructureElement", "IfcAnnotation", "IfcGrid", "IfcVirtualElement" }));
        const int threads = static_cast<int>(std::clamp(std::thread::hardware_concurrency(), 1u, 8u));

        std::unique_ptr<ifcopenshell::geom::iterator> it;
        try
        {
            it = std::make_unique<ifcopenshell::geom::iterator>(ifcopenshell::geom::kernels::construct(&m_file, "opencascade", settings),
                                                                settings, &m_file, filters, threads);
        }
        catch (const std::exception& ex)
        {
            m_report.warnings.push_back(QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Noyau géométrique indisponible : %1. Produits importés sans géométrie.")
                                            .arg(QString::fromStdString(ex.what()))
                                            .toStdString());
            return true;
        }
        if (!it->initialize())
        {
            m_report.warnings.push_back(
                QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Aucune représentation géométrique interprétable dans le fichier.").toStdString());
            return true;
        }
        // Unité retenue par le noyau, contrôlée contre celle du projet : un fichier avec un second contexte
        // (IfcProjectLibrary sans unités) peut l'égarer. La géométrie est alors remise à l'échelle du projet.
        const double scale = it->unit_magnitude() > 0 ? m_units.length / it->unit_magnitude() : 1.0;
        if (std::abs(scale - 1.0) > 1e-9)
            m_report.warnings.push_back(QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport",
                                                                    "Unité de longueur du noyau géométrique (%1) différente de celle du projet : géométrie remise à "
                                                                    "l'échelle du projet (facteur %2).")
                                            .arg(QString::fromStdString(it->unit_name()), QString::fromStdString(std::to_string(scale)))
                                            .toStdString());
        do
        {
            auto el = it->get();
            const auto* t = static_cast<const ifcopenshell::geom::triangulation_element*>(el.get());
            if (!t) continue;
            const Ent product = t->product();
            m_seen.insert(product.id());
            PhysicalElement e = describe(product);
            e.geometry = mesh(*t, scale);
            const IfcImportItem::Outcome outcome = e.status == AnalyticalStatus::NotStructural ? IfcImportItem::Outcome::NotStructural
                                                                                                : IfcImportItem::Outcome::ToPrepare;
            store(std::move(e), outcome, {});
            if (!progress(15 + it->progress() * 3 / 4, QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Géométrie des produits").toStdString()))
                return false;
        } while (it->next());
        return true;
    }

    std::shared_ptr<const TriangleMesh> mesh(const ifcopenshell::geom::triangulation_element& t, double scale)
    {
        const auto& g = t.geometry();
        const auto& v = g.verts();
        const auto& f = g.faces();
        if (v.size() < 9 || f.size() < 3) return nullptr;
        auto m = std::make_shared<TriangleMesh>();
        m->origin = { v[0] * scale, v[1] * scale, v[2] * scale };
        m->positions.reserve(v.size());
        for (std::size_t i = 0; i < v.size(); ++i) m->positions.push_back(static_cast<float>(v[i] * scale - m->origin[i % 3]));
        m->indices.reserve(f.size());
        for (int i : f) m->indices.push_back(static_cast<std::uint32_t>(i));
        // Couleur : style de surface le plus représenté
        std::map<int, int> votes;
        for (int id : g.material_ids()) ++votes[id];
        int best = -1, count = 0;
        for (const auto& [id, n] : votes)
            if (n > count && id >= 0 && id < static_cast<int>(g.materials().size())) best = id, count = n;
        if (best >= 0 && g.materials()[best])
        {
            const auto& style = *g.materials()[best];
            if (const auto& c = style.get_color())
                m->color = { static_cast<float>(c.r()), static_cast<float>(c.g()), static_cast<float>(c.b()),
                             std::isnan(style.transparency) ? 1.0f : static_cast<float>(1.0 - style.transparency) };
        }
        m->representation = bodyRepresentationType(t.product());
        return m->valid() ? std::shared_ptr<const TriangleMesh>(std::move(m)) : nullptr;
    }

    /// Éléments sans géométrie interprétable (pas de représentation, ou représentation non convertie) : conservés,
    /// signalés. Les ouvertures et éléments virtuels n'en sont pas.
    void productsWithoutGeometry()
    {
        for (const express::base& b : m_file.instances_by_type("IfcElement"))
        {
            const Ent product = b.as<Ent>();
            if (!product || m_seen.count(product.id()) || isA(product, "IfcFeatureElementSubtraction") || isA(product, "IfcVirtualElement")) continue;
            PhysicalElement e = describe(product);
            const std::string rep = bodyRepresentationType(product);
            const std::string note = rep.empty() ? "aucune représentation géométrique dans le fichier"
                                                 : "représentation « " + rep + " » non convertie par le noyau géométrique";
            e.importNotes.push_back("Sans géométrie : " + note + ".");   // enregistrée dans le projet : texte inchangé
            // Note du rapport d'import (affichée) : traduite
            const std::string shownNote =
                rep.empty() ? QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "aucune représentation géométrique dans le fichier").toStdString()
                            : QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "représentation « %1 » non convertie par le noyau géométrique")
                                  .arg(QString::fromStdString(rep))
                                  .toStdString();
            store(std::move(e), IfcImportItem::Outcome::Partial, shownNote);
        }
    }

    void finalizeReport()
    {
        const auto& bim = m_model.bim();
        m_report.products = m_report.productsWithGeometry = m_report.toPrepare = m_report.notStructural = 0;
        m_report.byClass.clear();
        for (const auto& [id, e] : bim.elements())
        {
            ++m_report.products;
            if (e.geometry) ++m_report.productsWithGeometry;
            if (e.status == AnalyticalStatus::NeedsPreparation) ++m_report.toPrepare;
            if (e.status == AnalyticalStatus::NotStructural) ++m_report.notStructural;
            ++m_report.byClass[e.ifcClass.empty() ? std::string(ifcEntity(e.category)) : e.ifcClass];
        }
        m_report.ignored = static_cast<int>(std::count_if(m_report.items.begin(), m_report.items.end(),
                                                          [](const IfcImportItem& i) { return i.outcome == IfcImportItem::Outcome::Ignored; }));
    }

    ifcopenshell::file& m_file;
    TSA::Model::Model& m_model;
    const IfcImportOptions& m_opt;
    IfcImportReport& m_report;
    Units m_units;
    std::map<std::string, std::string> m_levelByStorey;   ///< GlobalId d'étage → id de niveau TSA
    std::set<uint32_t> m_seen;                              ///< produits traités par le noyau géométrique
};

/// Messages d'IfcOpenShell pendant une opération : avertissements et erreurs recueillis pour le rapport (pas de console,
/// pas d'avis de progression) ; journal global rétabli à la sortie, exceptions comprises.
class LogCapture
{
public:
    LogCapture() : m_previous(ifcopenshell::logger::root().verbosity())
    {
        ifcopenshell::logger::root().verbosity(ifcopenshell::logger::LOG_WARNING);
        ifcopenshell::logger::root().set_output(static_cast<std::ostream*>(nullptr), &m_log);
    }
    ~LogCapture()
    {
        ifcopenshell::logger::root().set_output(static_cast<std::ostream*>(nullptr), static_cast<std::ostream*>(nullptr));
        ifcopenshell::logger::root().verbosity(m_previous);
    }
    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;

    /// Lignes distinctes, horodatage retiré, dans l'ordre d'apparition.
    std::vector<std::string> lines() const
    {
        static const std::regex stamp(R"(\[\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\] )");
        std::vector<std::string> out;
        std::set<std::string> seen;
        std::istringstream in(m_log.str());
        std::string line;
        while (std::getline(in, line))
        {
            line = std::regex_replace(line, stamp, "");
            if (!line.empty() && seen.insert(line).second) out.push_back(line);
        }
        return out;
    }

private:
    std::ostringstream m_log;
    ifcopenshell::logger::severity m_previous;
};
} // namespace

bool verifyFile(const std::string& path, std::string& schema, int& elements, std::string& error)
{
    LogCapture capture;
    bool ok = false;
    try
    {
        ifcopenshell::file file(path);
        if (!file.good())
            error = QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "fichier illisible par IfcOpenShell").toStdString();
        else
        {
            schema = file.schema() ? file.schema()->name() : std::string();
            elements = static_cast<int>(file.instances_by_type("IfcElement").size());
            ok = true;
        }
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
    }
    // Le lecteur signale les écarts au schéma (attributs, types) : premier message rapporté.
    if (const auto lines = capture.lines(); ok && !lines.empty()) error = QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "relu avec remarques : %1").arg(QString::fromStdString(lines.front())).toStdString();
    return ok;
}

bool fullEngineAvailable() { return true; }

std::string fullEngineName() { return QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "IfcOpenShell 0.9.0post1 (noyau OpenCASCADE)").toStdString(); }

bool importPhysicalProducts(const std::string& path, TSA::Model::Model& model, const IfcImportOptions& options, IfcImportReport& report)
{
    // Messages du noyau (géométries non convertibles…) recueillis pour le rapport, pas sur la console.
    LogCapture capture;
    bool ok = false;
    try
    {
        ifcopenshell::file file(path);
        if (!file.good())
        {
            report.error = QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "fichier IFC illisible par IfcOpenShell (syntaxe STEP ou schéma non reconnu)")
                               .toStdString();
        }
        else
        {
            report.schema = file.schema() ? file.schema()->name() : report.schema;
            PhysicalImport imp(file, model, options, report);
            ok = imp.run();
        }
    }
    catch (const std::exception& ex)
    {
        report.error = std::string("IfcOpenShell : ") + ex.what();
    }
    const auto lines = capture.lines();
    for (std::size_t i = 0; i < lines.size() && i < 50; ++i)
        report.warnings.push_back(
            QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Noyau géométrique : %1").arg(QString::fromStdString(lines[i])).toStdString());
    if (lines.size() > 50)
        report.warnings.push_back(QCoreApplication::translate("TSA::BIM::Ifc::IfcOpenShellImport", "Noyau géométrique : %1 autre(s) message(s).")
                                      .arg(QString::number(lines.size() - 50))
                                      .toStdString());
    return ok;
}

#endif

} // namespace TSA::BIM::Ifc::detail
