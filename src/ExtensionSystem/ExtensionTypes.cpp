#include "ExtensionTypes.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <QRegularExpression>

namespace TSA::ExtensionSystem
{

double PhysicalValue::toBaseSI() const
{
    std::string u = unit;
    std::transform(u.begin(), u.end(), u.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Pressions / Contraintes / Modules (Base SI: Pascal [Pa])
    if (u == "pa") return value;
    if (u == "kpa") return value * 1.0e3;
    if (u == "mpa" || u == "n/mm2") return value * 1.0e6;
    if (u == "gpa") return value * 1.0e9;
    if (u == "bar") return value * 1.0e5;

    // Masses volumiques (Base SI: kg/m³)
    if (u == "kg/m3" || u == "kg/m^3") return value;
    if (u == "t/m3" || u == "g/cm3") return value * 1000.0;

    // Forces (Base SI: Newton [N])
    if (u == "n") return value;
    if (u == "kn") return value * 1.0e3;
    if (u == "mn") return value * 1.0e6;

    // Longueurs (Base SI: mètre [m])
    if (u == "m") return value;
    if (u == "dm") return value * 0.1;
    if (u == "cm") return value * 0.01;
    if (u == "mm") return value * 0.001;

    // Angles
    if (u == "rad") return value;
    if (u == "deg" || u == "°") return value * (3.14159265358979323846 / 180.0);

    return value; // Pas de conversion reconnue, renvoyer la valeur telle quelle
}

PhysicalValue PhysicalValue::fromJson(const QJsonObject& obj)
{
    PhysicalValue pv;
    if (obj.contains("value") && obj["value"].isDouble())
    {
        pv.value = obj["value"].toDouble();
    }
    if (obj.contains("unit") && obj["unit"].isString())
    {
        pv.unit = obj["unit"].toString().toStdString();
    }
    return pv;
}

QJsonObject PhysicalValue::toJson() const
{
    QJsonObject obj;
    obj["value"] = value;
    obj["unit"] = QString::fromStdString(unit);
    return obj;
}


std::optional<ExtensionManifest> ExtensionManifest::fromJson(const QJsonObject& json, std::string* outError)
{
    ExtensionManifest m;

    if (!json.contains("id") || !json["id"].isString())
    {
        if (outError) *outError = "Champ requis manquant ou invalide : 'id'.";
        return std::nullopt;
    }
    m.id = json["id"].toString().toStdString();

    if (!json.contains("name") || !json["name"].isString())
    {
        if (outError) *outError = "Champ requis manquant ou invalide : 'name'.";
        return std::nullopt;
    }
    m.name = json["name"].toString().toStdString();

    if (!json.contains("version") || !json["version"].isString())
    {
        if (outError) *outError = "Champ requis manquant ou invalide : 'version'.";
        return std::nullopt;
    }
    auto parsedVer = SemanticVersion::fromString(json["version"].toString().toStdString());
    if (!parsedVer)
    {
        if (outError) *outError = "Format de version invalide pour 'version'. Attendu SemVer (ex: 1.0.0).";
        return std::nullopt;
    }
    m.version = *parsedVer;

    if (json.contains("format_version") && json["format_version"].isString())
    {
        m.formatVersion = json["format_version"].toString().toStdString();
    }

    if (json.contains("minimum_tsa_version") && json["minimum_tsa_version"].isString())
    {
        auto minTsa = SemanticVersion::fromString(json["minimum_tsa_version"].toString().toStdString());
        if (minTsa) m.minimumTsaVersion = *minTsa;
    }

    if (json.contains("author") && json["author"].isString())
    {
        m.author = json["author"].toString().toStdString();
    }
    if (json.contains("license") && json["license"].isString())
    {
        m.license = json["license"].toString().toStdString();
    }
    if (json.contains("description") && json["description"].isString())
    {
        m.description = json["description"].toString().toStdString();
    }
    if (json.contains("website") && json["website"].isString())
    {
        m.website = json["website"].toString().toStdString();
    }

    if (json.contains("kind") && json["kind"].isString())
    {
        QString kindStr = json["kind"].toString().toLower();
        if (kindStr == "code" || kindStr == "plugin")
        {
            m.kind = ExtensionKind::CodeExtension;
        }
        else
        {
            m.kind = ExtensionKind::DataExtension;
        }
    }

    if (json.contains("categories") && json["categories"].isArray())
    {
        for (const auto& catVal : json["categories"].toArray())
        {
            if (catVal.isString())
            {
                m.categories.push_back(catVal.toString().toStdString());
            }
        }
    }

    if (json.contains("checksum") && json["checksum"].isString())
    {
        m.checksum = json["checksum"].toString().toStdString();
    }

    return m;
}

QJsonObject ExtensionManifest::toJson() const
{
    QJsonObject obj;
    obj["id"] = QString::fromStdString(id);
    obj["name"] = QString::fromStdString(name);
    obj["version"] = QString::fromStdString(version.toString());
    obj["format_version"] = QString::fromStdString(formatVersion);
    obj["minimum_tsa_version"] = QString::fromStdString(minimumTsaVersion.toString());
    obj["author"] = QString::fromStdString(author);
    obj["license"] = QString::fromStdString(license);
    obj["description"] = QString::fromStdString(description);
    obj["website"] = QString::fromStdString(website);
    obj["kind"] = (kind == ExtensionKind::CodeExtension) ? "code" : "data";

    QJsonArray cats;
    for (const auto& c : categories)
    {
        cats.append(QString::fromStdString(c));
    }
    obj["categories"] = cats;

    if (!checksum.empty())
    {
        obj["checksum"] = QString::fromStdString(checksum);
    }
    return obj;
}

} // namespace TSA::ExtensionSystem
