#include "Tsa3dInternal.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace TSA::IO::Tsa3d
{

QString versionString()
{
    return QStringLiteral("%1.%2").arg(kVersionMajor).arg(kVersionMinor);
}

bool Report::hasErrors() const
{
    return count(Severity::Error) > 0;
}

int Report::count(Severity s) const
{
    int n = 0;
    for (const auto& i : issues) n += i.severity == s ? 1 : 0;
    return n;
}

QStringList Report::lines() const
{
    QStringList out;
    for (const auto& i : issues)
    {
        const char* tag = i.severity == Severity::Error ? "ERREUR" : i.severity == Severity::Warning ? "AVERT." : "INFO";
        out << QStringLiteral("%1 %2 : %3").arg(QString::fromLatin1(tag), i.path.isEmpty() ? QStringLiteral("(document)") : i.path, i.message);
    }
    return out;
}

bool read(const QByteArray& bytes, QJsonObject* document, Report* report)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError)
    {
        // Position en ligne / colonne pour un message compréhensible.
        int line = 1, col = 1;
        for (int i = 0; i < err.offset && i < bytes.size(); ++i)
        {
            if (bytes[i] == '\n')
            {
                ++line;
                col = 1;
            }
            else
                ++col;
        }
        if (report) report->error({}, QStringLiteral("JSON invalide ligne %1, colonne %2 : %3").arg(line).arg(col).arg(err.errorString()));
        return false;
    }
    if (!doc.isObject())
    {
        if (report) report->error({}, QStringLiteral("le document doit être un objet JSON"));
        return false;
    }
    if (document) *document = doc.object();
    return true;
}

bool readFile(const QString& path, QJsonObject* document, Report* report)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (report) report->error({}, QStringLiteral("lecture impossible de %1 : %2").arg(path, f.errorString()));
        return false;
    }
    return read(f.readAll(), document, report);
}

bool writeFile(const QString& path, const QJsonObject& document, QString* error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) < 0 || !f.commit())
    {
        if (error) *error = QStringLiteral("écriture impossible de %1 : %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

namespace detail
{

QStringList sectionShapes() { return { "rectangular", "circular", "i", "pipe", "box", "channel", "angle", "tee" }; }
QStringList materialTypes()
{
    return { "concrete", "steel", "timber", "masonry", "custom", "reinforcedConcrete", "rebarSteel", "galvanizedSteel",
             "aluminum", "brick", "glass", "soil", "sand", "gravel", "rock" };
}
QStringList dofStates() { return { "free", "fixed", "spring" }; }
QStringList orientationTypes() { return { "global", "member", "vector" }; }
QStringList barRoles() { return { "generic", "beam", "column", "brace", "tie", "truss", "steelMember", "cable" }; }
QStringList eccentricities() { return { "none", "topFlange", "bottomFlange", "leftFlange", "rightFlange" }; }
QStringList slabTypes() { return { "twoWay", "oneWay", "flat" }; }
QStringList foundationTypes() { return { "isolated", "strip", "raft", "pile" }; }
QStringList trussRoles() { return { "topChord", "bottomChord", "vertical", "diagonal", "brace" }; }
QStringList cableTypes()
{
    return { "generic", "strand", "wire", "prestressingBar", "stay", "suspension", "hanger", "externalPrestressing", "groundAnchor" };
}
QStringList cableModes() { return { "straight", "polyline", "parabolic", "catenary", "spline", "throughPoints" }; }
QStringList loadCaseCategories() { return { "dead", "live", "wind", "snow", "seismic", "temperature", "accidental", "custom" }; }
QStringList combinationTypes()
{
    return { "ulsFundamental", "ulsAccidental", "ulsSeismic", "slsCharacteristic", "slsFrequent", "slsQuasiPermanent", "custom" };
}
QStringList loadDirections() { return { "globalX", "globalY", "globalZ", "gravity", "localX", "localY", "localZ" }; }
QStringList memberLoadKinds() { return { "uniform", "trapezoidal", "point", "moment" }; }
QStringList coordSystems() { return { "global", "local" }; }
QStringList memberTypes() { return { "beam", "column", "truss", "cable" }; }
QStringList surfaceTypes() { return { "slab", "wall" }; }
QStringList meshCellTypes() { return { "line2", "zeroLength", "tri3", "quad4", "tet4", "hex8" }; }

std::optional<double> lengthFactor(const QString& u)
{
    if (u == "m") return 1.0;
    if (u == "cm") return 0.01;
    if (u == "mm") return 0.001;
    return std::nullopt;
}
std::optional<double> forceFactor(const QString& u)
{
    if (u == "kN") return 1.0;
    if (u == "N") return 0.001;
    if (u == "MN") return 1000.0;
    return std::nullopt;
}
std::optional<double> stressFactor(const QString& u)
{
    if (u == "Pa") return 1.0;
    if (u == "kPa") return 1e3;
    if (u == "MPa" || u == "N/mm2") return 1e6;
    if (u == "GPa") return 1e9;
    return std::nullopt;
}
std::optional<double> pressureFactor(const QString& u)
{
    if (u == "kPa" || u == "kN/m2") return 1.0;
    if (u == "Pa" || u == "N/m2") return 1e-3;
    if (u == "MPa") return 1e3;
    return std::nullopt;
}
std::optional<double> angleFactor(const QString& u)
{
    if (u == "deg") return 1.0;
    if (u == "rad") return 57.29577951308232;
    return std::nullopt;
}

Units readUnits(const QJsonObject& doc, Report* report)
{
    Units u;
    if (!doc.contains("units")) return u;
    const QJsonObject o = doc.value("units").toObject();
    auto pick = [&](const char* key, auto fn, double& out) {
        if (!o.contains(key)) return;
        const QString name = o.value(key).toString();
        if (const auto f = fn(name)) out = *f;
        else if (report) report->error(QStringLiteral("units.%1").arg(key), QStringLiteral("unité « %1 » non prise en charge").arg(name));
    };
    pick("length", lengthFactor, u.length);
    pick("force", forceFactor, u.force);
    pick("stress", stressFactor, u.stress);
    pick("pressure", pressureFactor, u.pressure);
    pick("angle", angleFactor, u.angle);
    return u;
}

QJsonObject tsaUnits()
{
    return { { "length", "m" },        { "force", "kN" },       { "stress", "Pa" },     { "pressure", "kPa" },
             { "angle", "deg" },       { "density", "kg/m3" },  { "moment", "kN.m" },   { "lineLoad", "kN/m" },
             { "translationalStiffness", "kN/m" }, { "rotationalStiffness", "kN.m/rad" } };
}

QStringList knownKeys(const QString& c)
{
    if (c == "materials") return { "id", "name", "type", "E", "nu", "density", "fk", "thermalCoeff", "library", "properties", "extensions" };
    if (c == "sections") return { "id", "name", "shape", "dimensions", "properties", "library", "extensions" };
    if (c == "nodes") return { "id", "name", "position", "level", "support", "globalId", "color", "properties", "extensions" };
    if (c == "members")
        return { "id", "name", "type", "role", "nodes", "section", "material", "rotation", "eccentricity", "releases", "truss",
                 "cable", "globalId", "color", "properties", "extensions" };
    if (c == "surfaces")
        return { "id", "name", "type", "nodes", "thickness", "material", "slabType", "height", "offset", "globalId", "color",
                 "properties", "extensions" };
    if (c == "foundations")
        return { "id", "name", "type", "node", "dimensions", "material", "soilBearingCapacity", "globalId", "color", "properties", "extensions" };
    if (c == "levels") return { "id", "name", "elevation", "extensions" };
    if (c == "loadCases") return { "id", "name", "category", "selfWeight", "selfWeightFactor", "description", "extensions" };
    if (c == "nodalLoads") return { "id", "name", "node", "case", "force", "moment", "coordinateSystem", "extensions" };
    if (c == "memberLoads")
        return { "id", "name", "member", "case", "kind", "direction", "coordinateSystem", "q1", "q2", "x1", "x2", "relative", "extensions" };
    if (c == "combinations") return { "id", "name", "type", "factors", "description", "extensions" };
    return {};
}

QStringList knownTopLevelKeys()
{
    return { "format", "version", "metadata", "units", "coordinateSystem", "materials", "sections", "levels", "nodes", "members",
             "surfaces", "foundations", "loads", "groups", "analysis", "mesh", "results", "extensions" };
}

QString jsonPath(const QString& collection, int index, const QString& field)
{
    QString p = QStringLiteral("%1[%2]").arg(collection).arg(index);
    if (!field.isEmpty()) p += QLatin1Char('.') + field;
    return p;
}

} // namespace detail
} // namespace TSA::IO::Tsa3d
