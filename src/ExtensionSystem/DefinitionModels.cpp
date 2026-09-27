#include "DefinitionModels.h"
#include "../Model/Material.h"
#include <algorithm>
#include <cctype>

namespace TSA::ExtensionSystem
{

StandardReference StandardReference::fromJson(const QJsonObject& json)
{
    StandardReference stdRef;
    if (json.contains("name")) stdRef.name = json["name"].toString().toStdString();
    if (json.contains("edition")) stdRef.edition = json["edition"].toString().toStdString();
    if (json.contains("clause")) stdRef.clause = json["clause"].toString().toStdString();
    if (json.contains("source")) stdRef.source = json["source"].toString().toStdString();
    return stdRef;
}

QJsonObject StandardReference::toJson() const
{
    QJsonObject obj;
    obj["name"] = QString::fromStdString(name);
    obj["edition"] = QString::fromStdString(edition);
    obj["clause"] = QString::fromStdString(clause);
    obj["source"] = QString::fromStdString(source);
    return obj;
}

VisualDefinition VisualDefinition::fromJson(const QJsonObject& json)
{
    VisualDefinition v;
    if (json.contains("base_color")) v.baseColor = json["base_color"].toString().toStdString();
    if (json.contains("roughness")) v.roughness = json["roughness"].toDouble();
    if (json.contains("metallic")) v.metallic = json["metallic"].toDouble();
    if (json.contains("transparency")) v.transparency = json["transparency"].toDouble();
    if (json.contains("shininess")) v.shininess = json["shininess"].toDouble();
    if (json.contains("texture_scale_u")) v.textureScaleU = json["texture_scale_u"].toDouble();
    if (json.contains("texture_scale_v")) v.textureScaleV = json["texture_scale_v"].toDouble();

    if (json.contains("textures") && json["textures"].isObject())
    {
        QJsonObject texObj = json["textures"].toObject();
        for (auto it = texObj.begin(); it != texObj.end(); ++it)
        {
            v.textures[it.key().toStdString()] = it.value().toString().toStdString();
        }
    }
    return v;
}

QJsonObject VisualDefinition::toJson() const
{
    QJsonObject obj;
    obj["base_color"] = QString::fromStdString(baseColor);
    obj["roughness"] = roughness;
    obj["metallic"] = metallic;
    obj["transparency"] = transparency;
    obj["shininess"] = shininess;
    obj["texture_scale_u"] = textureScaleU;
    obj["texture_scale_v"] = textureScaleV;

    QJsonObject texObj;
    for (const auto& [k, val] : textures)
    {
        texObj[QString::fromStdString(k)] = QString::fromStdString(val);
    }
    obj["textures"] = texObj;
    return obj;
}

MechanicalSnapshot MaterialDefinition::createSnapshot() const
{
    MechanicalSnapshot snap;
    snap.youngModulus = youngModulus.toBaseSI();
    snap.poissonRatio = poissonRatio;
    snap.density = density.toBaseSI();
    snap.characteristicStrength = fck.toBaseSI() > 0.0 ? fck.toBaseSI() : ft.toBaseSI();
    snap.yieldStrength = fy.toBaseSI();
    snap.thermalCoeff = thermalCoeff.toBaseSI();
    return snap;
}

TSA::Model::Material MaterialDefinition::toModelMaterial(int fallbackId) const
{
    TSA::Model::Material m;
    m.id = fallbackId;
    m.name = name;

    std::string catLower = category;
    std::transform(catLower.begin(), catLower.end(), catLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string idLower = id;
    std::transform(idLower.begin(), idLower.end(), idLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (catLower == "concrete")
    {
        if (idLower.find("reinforced") != std::string::npos)
            m.type = TSA::Model::MaterialType::ReinforcedConcrete;
        else
            m.type = TSA::Model::MaterialType::Concrete;
    }
    else if (catLower == "steel")
    {
        if (idLower.find("rebar") != std::string::npos)
            m.type = TSA::Model::MaterialType::RebarSteel;
        else if (idLower.find("galvanized") != std::string::npos)
            m.type = TSA::Model::MaterialType::GalvanizedSteel;
        else
            m.type = TSA::Model::MaterialType::Steel;
    }
    else if (catLower == "timber")
    {
        m.type = TSA::Model::MaterialType::Timber;
    }
    else if (catLower == "masonry")
    {
        if (idLower.find("brick") != std::string::npos)
            m.type = TSA::Model::MaterialType::Brick;
        else
            m.type = TSA::Model::MaterialType::Masonry;
    }
    else if (catLower == "aluminum")
    {
        m.type = TSA::Model::MaterialType::Aluminum;
    }
    else if (catLower == "glass")
    {
        m.type = TSA::Model::MaterialType::Glass;
    }
    else if (catLower == "soil")
    {
        if (idLower.find("sand") != std::string::npos)
            m.type = TSA::Model::MaterialType::Sand;
        else if (idLower.find("gravel") != std::string::npos)
            m.type = TSA::Model::MaterialType::Gravel;
        else if (idLower.find("rock") != std::string::npos)
            m.type = TSA::Model::MaterialType::Rock;
        else
            m.type = TSA::Model::MaterialType::Soil;
    }
    else
    {
        m.type = TSA::Model::MaterialType::Custom;
    }

    m.E = youngModulus.toBaseSI();
    m.nu = poissonRatio;
    m.density = density.toBaseSI();
    if (fy.value > 0.0)
        m.fk = fy.toBaseSI();
    else if (fck.value > 0.0)
        m.fk = fck.toBaseSI();
    else if (ft.value > 0.0)
        m.fk = ft.toBaseSI();
    else
        m.fk = 0.0;
    m.thermalCoeff = thermalCoeff.toBaseSI();

    m.syncMechanical();

    m.visual.baseColor = visual.baseColor;
    m.visual.roughness = visual.roughness;
    m.visual.metallic = visual.metallic;
    m.visual.transparency = visual.transparency;
    m.visual.shininess = visual.shininess;
    m.visual.textureScaleU = visual.textureScaleU;
    m.visual.textureScaleV = visual.textureScaleV;

    auto itTex = visual.textures.find("albedo");
    if (itTex != visual.textures.end())
    {
        m.visual.texturePath = itTex->second;
    }

    return m;
}

MaterialDefinition MaterialDefinition::fromModelMaterial(const TSA::Model::Material& mat, const std::string& libraryId)
{
    MaterialDefinition def;
    def.ref.libraryId = libraryId;
    def.ref.definitionVersion = SemanticVersion{ 1, 0, 0 };
    def.version = SemanticVersion{ 1, 0, 0 };

    def.name = mat.name;

    std::string cleanId = mat.name;
    std::transform(cleanId.begin(), cleanId.end(), cleanId.begin(), [](char c) {
        if (isalnum(static_cast<unsigned char>(c))) return static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return '_';
    });

    std::string finalId;
    for (size_t i = 0; i < cleanId.size(); ++i)
    {
        if (cleanId[i] == '_' && !finalId.empty() && finalId.back() == '_') continue;
        finalId.push_back(cleanId[i]);
    }
    if (!finalId.empty() && finalId.back() == '_') finalId.pop_back();

    switch (mat.type)
    {
    case TSA::Model::MaterialType::Concrete:
    case TSA::Model::MaterialType::ReinforcedConcrete:
        def.category = "Concrete";
        def.id = "concrete." + finalId;
        def.fck = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    case TSA::Model::MaterialType::Steel:
    case TSA::Model::MaterialType::RebarSteel:
    case TSA::Model::MaterialType::GalvanizedSteel:
        def.category = "Steel";
        def.id = "steel." + finalId;
        def.fy = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    case TSA::Model::MaterialType::Timber:
        def.category = "Timber";
        def.id = "timber." + finalId;
        def.ft = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    case TSA::Model::MaterialType::Masonry:
    case TSA::Model::MaterialType::Brick:
        def.category = "Masonry";
        def.id = "masonry." + finalId;
        def.fck = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    case TSA::Model::MaterialType::Aluminum:
        def.category = "Aluminum";
        def.id = "aluminum." + finalId;
        def.fy = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    case TSA::Model::MaterialType::Glass:
        def.category = "Glass";
        def.id = "glass." + finalId;
        def.ft = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    case TSA::Model::MaterialType::Soil:
    case TSA::Model::MaterialType::Sand:
    case TSA::Model::MaterialType::Gravel:
    case TSA::Model::MaterialType::Rock:
        def.category = "Soil";
        def.id = "soil." + finalId;
        def.ft = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    default:
        def.category = "Custom";
        def.id = "custom." + finalId;
        def.fck = PhysicalValue{ mat.fk / 1.0e6, "MPa" };
        break;
    }

    def.ref.definitionId = def.id;

    def.youngModulus = PhysicalValue{ mat.E / 1.0e6, "MPa" };
    def.poissonRatio = mat.nu;
    def.density = PhysicalValue{ mat.density, "kg/m3" };
    def.thermalCoeff = PhysicalValue{ mat.thermalCoeff, "1/K" };

    def.visual.baseColor = mat.visual.baseColor;
    def.visual.roughness = mat.visual.roughness;
    def.visual.metallic = mat.visual.metallic;
    def.visual.transparency = mat.visual.transparency;
    def.visual.shininess = mat.visual.shininess;
    def.visual.textureScaleU = mat.visual.textureScaleU;
    def.visual.textureScaleV = mat.visual.textureScaleV;
    if (!mat.visual.texturePath.empty())
    {
        def.visual.textures["albedo"] = mat.visual.texturePath;
    }

    return def;
}

std::optional<MaterialDefinition> MaterialDefinition::fromJson(const QJsonObject& json, std::string* outError)
{
    MaterialDefinition m;

    if (!json.contains("id") || !json["id"].isString())
    {
        if (outError) *outError = "Champ requis manquant ou invalide : 'id'.";
        return std::nullopt;
    }
    m.id = json["id"].toString().toStdString();
    m.ref.definitionId = m.id;

    if (!json.contains("name") || !json["name"].isString())
    {
        if (outError) *outError = "Champ requis manquant ou invalide : 'name'.";
        return std::nullopt;
    }
    m.name = json["name"].toString().toStdString();

    if (json.contains("category"))
    {
        m.category = json["category"].toString().toStdString();
    }
    else
    {
        m.category = "Concrete";
    }

    if (json.contains("version"))
    {
        auto v = SemanticVersion::fromString(json["version"].toString().toStdString());
        if (v)
        {
            m.version = *v;
            m.ref.definitionVersion = *v;
        }
    }

    if (json.contains("standard") && json["standard"].isObject())
    {
        m.standard = StandardReference::fromJson(json["standard"].toObject());
    }

    if (json.contains("mechanical") && json["mechanical"].isObject())
    {
        QJsonObject mech = json["mechanical"].toObject();
        if (mech.contains("density") && mech["density"].isObject())
            m.density = PhysicalValue::fromJson(mech["density"].toObject());
        if (mech.contains("young_modulus") && mech["young_modulus"].isObject())
            m.youngModulus = PhysicalValue::fromJson(mech["young_modulus"].toObject());
        if (mech.contains("poisson_ratio"))
            m.poissonRatio = mech["poisson_ratio"].toDouble();
        if (mech.contains("thermal_coeff") && mech["thermal_coeff"].isObject())
            m.thermalCoeff = PhysicalValue::fromJson(mech["thermal_coeff"].toObject());
    }

    if (json.contains("strength") && json["strength"].isObject())
    {
        QJsonObject str = json["strength"].toObject();
        if (str.contains("fck") && str["fck"].isObject())
            m.fck = PhysicalValue::fromJson(str["fck"].toObject());
        if (str.contains("fy") && str["fy"].isObject())
            m.fy = PhysicalValue::fromJson(str["fy"].toObject());
        if (str.contains("ft") && str["ft"].isObject())
            m.ft = PhysicalValue::fromJson(str["ft"].toObject());
    }

    if (json.contains("visual") && json["visual"].isObject())
    {
        m.visual = VisualDefinition::fromJson(json["visual"].toObject());
    }

    return m;
}

QJsonObject MaterialDefinition::toJson() const
{
    QJsonObject json;
    json["id"] = QString::fromStdString(id);
    json["name"] = QString::fromStdString(name);
    json["category"] = QString::fromStdString(category);
    json["version"] = QString::fromStdString(version.toString());
    json["standard"] = standard.toJson();

    QJsonObject mech;
    mech["density"] = density.toJson();
    mech["young_modulus"] = youngModulus.toJson();
    mech["poisson_ratio"] = poissonRatio;
    mech["thermal_coeff"] = thermalCoeff.toJson();
    json["mechanical"] = mech;

    QJsonObject str;
    if (fck.value > 0.0) str["fck"] = fck.toJson();
    if (fy.value > 0.0) str["fy"] = fy.toJson();
    if (ft.value > 0.0) str["ft"] = ft.toJson();
    json["strength"] = str;

    json["visual"] = visual.toJson();
    return json;
}

std::optional<SectionDefinition> SectionDefinition::fromJson(const QJsonObject& json, std::string* outError)
{
    SectionDefinition s;

    if (!json.contains("id") || !json["id"].isString())
    {
        if (outError) *outError = "Champ 'id' manquant pour la section.";
        return std::nullopt;
    }
    s.id = json["id"].toString().toStdString();
    s.ref.definitionId = s.id;

    if (!json.contains("name") || !json["name"].isString())
    {
        if (outError) *outError = "Champ 'name' manquant pour la section.";
        return std::nullopt;
    }
    s.name = json["name"].toString().toStdString();

    if (json.contains("category")) s.category = json["category"].toString().toStdString();
    if (json.contains("shape_type")) s.shapeType = json["shape_type"].toString().toStdString();
    if (json.contains("default_material")) s.defaultMaterialId = json["default_material"].toString().toStdString();

    if (json.contains("version"))
    {
        auto v = SemanticVersion::fromString(json["version"].toString().toStdString());
        if (v)
        {
            s.version = *v;
            s.ref.definitionVersion = *v;
        }
    }

    if (json.contains("standard") && json["standard"].isObject())
    {
        s.standard = StandardReference::fromJson(json["standard"].toObject());
    }

    if (json.contains("dimensions") && json["dimensions"].isObject())
    {
        QJsonObject dims = json["dimensions"].toObject();
        if (dims.contains("width")) s.width = dims["width"].toDouble();
        if (dims.contains("height")) s.height = dims["height"].toDouble();
        if (dims.contains("diameter")) s.diameter = dims["diameter"].toDouble();
        if (dims.contains("tw")) s.webThickness = dims["tw"].toDouble();
        if (dims.contains("tf")) s.flangeThickness = dims["tf"].toDouble();
        if (dims.contains("r")) s.filletRadius = dims["r"].toDouble();
    }

    if (json.contains("properties") && json["properties"].isObject())
    {
        QJsonObject props = json["properties"].toObject();
        if (props.contains("area")) s.area = props["area"].toDouble();
        if (props.contains("ix")) s.ix = props["ix"].toDouble();
        if (props.contains("iy")) s.iy = props["iy"].toDouble();
        if (props.contains("it")) s.it = props["it"].toDouble();
        if (props.contains("wx")) s.wx = props["wx"].toDouble();
        if (props.contains("wy")) s.wy = props["wy"].toDouble();
    }

    if (json.contains("visual") && json["visual"].isObject())
    {
        s.visual = VisualDefinition::fromJson(json["visual"].toObject());
    }

    return s;
}

QJsonObject SectionDefinition::toJson() const
{
    QJsonObject json;
    json["id"] = QString::fromStdString(id);
    json["name"] = QString::fromStdString(name);
    json["category"] = QString::fromStdString(category);
    json["shape_type"] = QString::fromStdString(shapeType);
    json["version"] = QString::fromStdString(version.toString());
    json["default_material"] = QString::fromStdString(defaultMaterialId);
    json["standard"] = standard.toJson();

    QJsonObject dims;
    dims["width"] = width;
    dims["height"] = height;
    dims["diameter"] = diameter;
    dims["tw"] = webThickness;
    dims["tf"] = flangeThickness;
    dims["r"] = filletRadius;
    json["dimensions"] = dims;

    QJsonObject props;
    props["area"] = area;
    props["ix"] = ix;
    props["iy"] = iy;
    props["it"] = it;
    props["wx"] = wx;
    props["wy"] = wy;
    json["properties"] = props;

    json["visual"] = visual.toJson();
    return json;
}

std::optional<CableCatalogDefinition> CableCatalogDefinition::fromJson(const QJsonObject& json, std::string* outError)
{
    CableCatalogDefinition c;

    if (!json.contains("id") || !json["id"].isString())
    {
        if (outError) *outError = "Champ 'id' manquant pour le câble.";
        return std::nullopt;
    }
    c.id = json["id"].toString().toStdString();
    c.ref.definitionId = c.id;

    if (!json.contains("name") || !json["name"].isString())
    {
        if (outError) *outError = "Champ 'name' manquant pour le câble.";
        return std::nullopt;
    }
    c.name = json["name"].toString().toStdString();

    if (json.contains("category")) c.category = json["category"].toString().toStdString();
    if (json.contains("grade")) c.grade = json["grade"].toString().toStdString();

    if (json.contains("version"))
    {
        auto v = SemanticVersion::fromString(json["version"].toString().toStdString());
        if (v)
        {
            c.version = *v;
            c.ref.definitionVersion = *v;
        }
    }

    if (json.contains("standard") && json["standard"].isObject())
    {
        c.standard = StandardReference::fromJson(json["standard"].toObject());
    }

    if (json.contains("geometry") && json["geometry"].isObject())
    {
        QJsonObject geom = json["geometry"].toObject();
        if (geom.contains("diameter")) c.nominalDiameter = geom["diameter"].toDouble();
        if (geom.contains("area")) c.metallicArea = geom["area"].toDouble();
        if (geom.contains("linear_mass")) c.linearMass = geom["linear_mass"].toDouble();
    }

    if (json.contains("mechanical") && json["mechanical"].isObject())
    {
        QJsonObject mech = json["mechanical"].toObject();
        if (mech.contains("elastic_modulus")) c.elasticModulus = mech["elastic_modulus"].toDouble();
        if (mech.contains("density")) c.density = mech["density"].toDouble();
        if (mech.contains("characteristic_strength")) c.characteristicStrength = mech["characteristic_strength"].toDouble();
        if (mech.contains("breaking_force")) c.minimumBreakingForce = mech["breaking_force"].toDouble();
        if (mech.contains("initial_tension")) c.defaultInitialTension = mech["initial_tension"].toDouble();
        if (mech.contains("tension_only")) c.tensionOnly = mech["tension_only"].toBool();
    }

    if (json.contains("visual") && json["visual"].isObject())
    {
        c.visual = VisualDefinition::fromJson(json["visual"].toObject());
    }

    return c;
}

QJsonObject CableCatalogDefinition::toJson() const
{
    QJsonObject json;
    json["id"] = QString::fromStdString(id);
    json["name"] = QString::fromStdString(name);
    json["category"] = QString::fromStdString(category);
    json["grade"] = QString::fromStdString(grade);
    json["version"] = QString::fromStdString(version.toString());
    json["standard"] = standard.toJson();

    QJsonObject geom;
    geom["diameter"] = nominalDiameter;
    geom["area"] = metallicArea;
    geom["linear_mass"] = linearMass;
    json["geometry"] = geom;

    QJsonObject mech;
    mech["elastic_modulus"] = elasticModulus;
    mech["density"] = density;
    mech["characteristic_strength"] = characteristicStrength;
    mech["breaking_force"] = minimumBreakingForce;
    mech["initial_tension"] = defaultInitialTension;
    mech["tension_only"] = tensionOnly;
    json["mechanical"] = mech;

    json["visual"] = visual.toJson();
    return json;
}

} // namespace TSA::ExtensionSystem
