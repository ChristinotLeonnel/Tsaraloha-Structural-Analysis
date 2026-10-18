// tsa-ifc : échange IFC en ligne de commande (docs/IFC_MAPPING.md). Sert aux essais sur des fichiers réels, aux
// tests automatisés et au flux de publication.
//
//   tsa-ifc info <fichier.ifc>                    import dans un projet vide, rapport (produits présents / importés)
//   tsa-ifc export <projet.tsa | .tsa3d> <sortie.ifc>
//   tsa-ifc roundtrip <fichier.ifc> <sortie.ifc>  import, export du projet obtenu, réimport et comparaison
//
// Options (info, roundtrip) : --derive (éléments de calcul déduits de la géométrie), --no-geometry.
// Le rapport est écrit en JSON sur la sortie standard. Code de sortie : 0 succès, 1 échec (fichier illisible,
// import ou export refusé, aller-retour avec pertes), 2 usage.

#include "BIM/IFC/IfcComparison.h"
#include "BIM/IFC/IfcExporter.h"
#include "BIM/IFC/IfcImporter.h"
#include "BIM/IFC/IfcStepReader.h"
#include "IO/TSAFile.h"
#include "IO/Tsa3d/Tsa3d.h"
#include "Model/Model.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
using namespace TSA::BIM::Ifc;

/// Produits du fichier, par classe IFC (lecture STEP seule, sans conversion) : base de comparaison
/// « présents / importés » du rapport.
QJsonObject productsInFile(const std::string& path)
{
    static const char* classes[] = {
        "IFCBEAM", "IFCBEAMSTANDARDCASE", "IFCCOLUMN", "IFCCOLUMNSTANDARDCASE", "IFCMEMBER", "IFCMEMBERSTANDARDCASE",
        "IFCSLAB", "IFCSLABSTANDARDCASE", "IFCWALL", "IFCWALLSTANDARDCASE", "IFCFOOTING", "IFCPILE", "IFCPLATE",
        "IFCBUILDINGELEMENTPROXY", "IFCROOF", "IFCSTAIR", "IFCSTAIRFLIGHT", "IFCRAMP", "IFCRAILING", "IFCDOOR",
        "IFCWINDOW", "IFCCOVERING", "IFCCURTAINWALL", "IFCOPENINGELEMENT", "IFCREINFORCINGBAR", "IFCBEARING",
        "IFCTENDON", "IFCSTRUCTURALCURVEMEMBER", "IFCSTRUCTURALSURFACEMEMBER", "IFCSTRUCTURALPOINTCONNECTION",
        "IFCBUILDINGSTOREY", "IFCGRID",
    };
    QJsonObject out;
    IfcStepReader reader;
    std::string error;
    if (!reader.parseFile(path, &error))
    {
        out.insert(QStringLiteral("parseError"), QString::fromStdString(error));
        return out;
    }
    for (const char* c : classes)
        if (const auto n = reader.byType(c).size()) out.insert(QString::fromLatin1(c), static_cast<int>(n));
    return out;
}

QJsonArray strings(const std::vector<std::string>& lines)
{
    QJsonArray out;
    for (const auto& l : lines) out.append(QString::fromStdString(l));
    return out;
}

QJsonObject importJson(const IfcImportReport& r)
{
    QJsonObject byClass;
    for (const auto& [cls, n] : r.byClass) byClass.insert(QString::fromStdString(cls), n);
    return QJsonObject {
        { QStringLiteral("ok"), r.ok },
        { QStringLiteral("canceled"), r.canceled },
        { QStringLiteral("error"), QString::fromStdString(r.error) },
        { QStringLiteral("schema"), QString::fromStdString(r.schema) },
        { QStringLiteral("engine"), QString::fromStdString(r.engine) },
        { QStringLiteral("lengthUnit"), r.lengthUnit },
        { QStringLiteral("products"), r.products },
        { QStringLiteral("productsWithGeometry"), r.productsWithGeometry },
        { QStringLiteral("toPrepare"), r.toPrepare },
        { QStringLiteral("notStructural"), r.notStructural },
        { QStringLiteral("ignored"), r.ignored },
        { QStringLiteral("derived"), r.derived },
        { QStringLiteral("nodes"), r.nodes },
        { QStringLiteral("members"), r.members },
        { QStringLiteral("surfaces"), r.surfaces },
        { QStringLiteral("foundations"), r.foundations },
        { QStringLiteral("storeys"), r.storeys },
        { QStringLiteral("byClass"), byClass },
        { QStringLiteral("summary"), QString::fromStdString(r.summary()) },
        { QStringLiteral("warnings"), strings(r.warnings) },
    };
}

QJsonObject exportJson(const IfcExportReport& r, const QString& path)
{
    return QJsonObject {
        { QStringLiteral("ok"), r.ok },
        { QStringLiteral("summary"), QString::fromStdString(r.summary()) },
        { QStringLiteral("file"), path },
        { QStringLiteral("bytes"), static_cast<double>(QFileInfo(path).size()) },
        { QStringLiteral("products"), r.products },
        { QStringLiteral("entities"), r.entities },
        { QStringLiteral("verifiedElements"), r.verifiedElements },
        { QStringLiteral("verification"), QString::fromStdString(r.verification) },
        { QStringLiteral("warnings"), strings(r.warnings) },
    };
}

QJsonObject comparisonJson(const IfcComparison& c)
{
    return QJsonObject {
        { QStringLiteral("identical"), c.identical() },
        { QStringLiteral("summary"), QString::fromStdString(c.summary()) },
        { QStringLiteral("tolerance"), c.tolerance },
        { QStringLiteral("productsA"), c.productsA },
        { QStringLiteral("productsB"), c.productsB },
        { QStringLiteral("guidsPreserved"), c.guidsPreserved },
        { QStringLiteral("classesPreserved"), c.classesPreserved },
        { QStringLiteral("namesPreserved"), c.namesPreserved },
        { QStringLiteral("propertySetsPreserved"), c.propertySetsPreserved },
        { QStringLiteral("statusPreserved"), c.statusPreserved },
        { QStringLiteral("geometryCompared"), c.geometryCompared },
        { QStringLiteral("geometryWithinTolerance"), c.geometryWithinTolerance },
        { QStringLiteral("maxBoundsDeviation"), c.maxBoundsDeviation },
        { QStringLiteral("differences"), strings(c.differences) },
    };
}

bool loadProject(const QString& path, TSA::Model::Model& model, std::string& error)
{
    if (path.endsWith(QStringLiteral(".tsa3d"), Qt::CaseInsensitive))
    {
        QJsonObject doc;
        TSA::IO::Tsa3d::Report report;
        if (TSA::IO::Tsa3d::readFile(path, &doc, &report) && TSA::IO::Tsa3d::importDocument(doc, model).ok) return true;
        error = report.lines().join(QStringLiteral(" ; ")).toStdString();
        return false;
    }
    return TSA::IO::TSAProjectIO::loadFromFile(path, model, nullptr, &error);
}

int print(const QJsonObject& o, bool ok)
{
    std::printf("%s\n", QJsonDocument(o).toJson(QJsonDocument::Indented).constData());
    return ok ? 0 : 1;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
#ifdef _WIN32
    SetConsoleOutputCP(65001);   // UTF-8
#endif
    QStringList args = app.arguments().mid(1);
    IfcImportOptions options;
    options.deriveAnalyticalFromGeometry = args.removeAll(QStringLiteral("--derive")) > 0;
    options.importGeometry = args.removeAll(QStringLiteral("--no-geometry")) == 0;
    const QString cmd = args.value(0);
    if (!((cmd == QLatin1String("info") && args.size() == 2) || (cmd == QLatin1String("export") && args.size() == 3)
          || (cmd == QLatin1String("roundtrip") && args.size() == 3)))
    {
        std::fprintf(stderr, "Usage : tsa-ifc info <fichier.ifc> [--derive] [--no-geometry]\n"
                             "        tsa-ifc export <projet.tsa | modèle.tsa3d> <sortie.ifc>\n"
                             "        tsa-ifc roundtrip <fichier.ifc> <sortie.ifc> [--derive] [--no-geometry]\n");
        return 2;
    }

    QElapsedTimer timer;
    timer.start();
    QJsonObject out { { QStringLiteral("command"), cmd }, { QStringLiteral("input"), args[1] } };
    TSA::Model::Model model;

    if (cmd == QLatin1String("export"))
    {
        std::string error;
        if (!loadProject(args[1], model, error))
        {
            out.insert(QStringLiteral("error"), QString::fromStdString(error));
            return print(out, false);
        }
        const auto r = IfcExporter::exportToFile(model, args[2].toStdString());
        out.insert(QStringLiteral("export"), exportJson(r, args[2]));
        out.insert(QStringLiteral("ms"), static_cast<double>(timer.elapsed()));
        return print(out, r.ok);
    }

    out.insert(QStringLiteral("inFile"), productsInFile(args[1].toStdString()));
    const auto imported = IfcImporter::importFile(args[1].toStdString(), model, options);
    out.insert(QStringLiteral("import"), importJson(imported));
    bool ok = imported.ok;
    if (ok && cmd == QLatin1String("roundtrip"))
    {
        // Aller-retour : export du projet obtenu, réimport du fichier écrit, comparaison des deux projets
        const auto r = IfcExporter::exportToFile(model, args[2].toStdString());
        out.insert(QStringLiteral("export"), exportJson(r, args[2]));
        ok = r.ok;
        if (ok)
        {
            TSA::Model::Model again;
            IfcImportOptions reimport;   // sans déduction : le fichier écrit par TSA porte son modèle de calcul
            reimport.importGeometry = options.importGeometry;
            const auto back = IfcImporter::importFile(args[2].toStdString(), again, reimport);
            out.insert(QStringLiteral("reimport"), importJson(back));
            const auto comparison = compareModels(model, again);
            out.insert(QStringLiteral("comparison"), comparisonJson(comparison));
            ok = back.ok && comparison.identical();
        }
    }
    out.insert(QStringLiteral("ms"), static_cast<double>(timer.elapsed()));
    return print(out, ok);
}
