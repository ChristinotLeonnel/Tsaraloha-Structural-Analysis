// Suite « ifc » : import, export et aller-retour sur des fichiers IFC réels (tests 400-407).
// Fichiers buildingSMART non modifiés (CC BY 4.0, tests/data/ifc/buildingsmart/README.md) : IFC2X3, IFC4, IFC4X3_ADD2.
// Facultatif : TSA_IFC_SAMPLES=<dossier> ajoute des échantillons locaux (ex. geometrygym_great_court_roof.ifc, grand
// fichier), non livrés avec les sources faute de licence déclarée.

#include "test_common.h"

#include "BIM/Core/BimModel.h"
#include "BIM/IFC/IfcComparison.h"
#include "BIM/IFC/IfcExporter.h"
#include "BIM/IFC/IfcImporter.h"
#include "Coordinate/LevelManager.h"
#include "IO/TSAFile.h"
#include "Viewer/OccView.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_ListOfInteractive.hxx>
#include <QApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QThread>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>

namespace fs = std::filesystem;
using namespace TSA::Model;
using namespace TSA::BIM::Ifc;
using TSA::BIM::AnalyticalStatus;
using TSA::BIM::PhysicalElement;

namespace
{
fs::path dataDir()
{
    fs::path d = fs::path(__FILE__).parent_path() / "data" / "ifc" / "buildingsmart";
    if (!fs::exists(d)) d = "tests/data/ifc/buildingsmart";
    return d;
}

fs::path workDir()
{
    const fs::path d = fs::path("./build/test_tsa_data/ifc");
    fs::create_directories(d);
    return d;
}

IfcImportReport load(const std::string& name, Model& m, const IfcImportOptions& opt = {})
{
    return IfcImporter::importFile((dataDir() / name).string(), m, opt);
}

/// Union des emprises des géométries importées (m) : xmin, ymin, zmin, xmax, ymax, zmax.
std::array<double, 6> bounds(const Model& m)
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    std::array<double, 6> b { inf, inf, inf, -inf, -inf, -inf };
    for (const auto& [id, e] : m.bim().elements())
        if (e.geometry)
        {
            const auto g = e.geometry->bounds();
            for (int k = 0; k < 3; ++k)
            {
                b[k] = std::min(b[k], g[k]);
                b[3 + k] = std::max(b[3 + k], g[3 + k]);
            }
        }
    return b;
}

const PhysicalElement* byClass(const Model& m, const std::string& ifcClass)
{
    for (const auto& [id, e] : m.bim().elements())
        if (e.ifcClass == ifcClass) return &e;
    return nullptr;
}

const TSA::BIM::PropertyValue* imported(const PhysicalElement& e, const std::string& key)
{
    const auto* ps = e.propertySet("TSA_Import");
    if (!ps) return nullptr;
    auto it = ps->properties.find(key);
    return it == ps->properties.end() ? nullptr : &it->second;
}

std::string readText(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}

int countEntity(const std::string& doc, const std::string& entity)
{
    int n = 0;
    for (std::size_t p = doc.find("=" + entity + "("); p != std::string::npos; p = doc.find("=" + entity + "(", p + 1)) ++n;
    return n;
}

/// Aller-retour complet d'un fichier : import, export IFC4X3 relu par un lecteur indépendant, réimport, comparaison.
bool roundTrip(const fs::path& source, std::string* failure)
{
    Model a;
    const auto ra = IfcImporter::importFile(source.string(), a);
    if (!ra.ok)
    {
        *failure = "import : " + ra.error;
        return false;
    }
    const fs::path out = workDir() / (source.stem().string() + ".roundtrip.ifc");
    const auto e = IfcExporter::exportToFile(a, out.string());
    if (!e.ok || e.verifiedElements != e.products || e.products != ra.products)
    {
        *failure = "export : " + e.summary() + " ; relus " + std::to_string(e.verifiedElements) + " pour " + std::to_string(ra.products);
        return false;
    }
    Model b;
    const auto rb = IfcImporter::importFile(out.string(), b);
    if (!rb.ok || rb.schema != "IFC4X3_ADD2")
    {
        *failure = "réimport : " + rb.error + " (" + rb.schema + ")";
        return false;
    }
    const auto c = compareModels(a, b, 1e-3);
    if (!c.identical())
    {
        *failure = c.summary();
        for (const auto& d : c.differences) *failure += "\n    " + d;
        return false;
    }
    return true;
}
} // namespace

bool runSuite_IfcFiles(int& passed)
{
    TEST_CHECK(IfcImporter::fullEngineAvailable(),
               "Test 400: noyau IFC complet (IfcOpenShell) présent dans cette compilation (scripts/build_ifcopenshell.ps1)");

    // TEST 400 : un même bâtiment en IFC2X3, IFC4 et IFC4X3_ADD2 (Building-Structural) : schéma reconnu, unité (mm → m),
    // étage, tous les produits avec leur GlobalId, rien d'inventé, même géométrie d'un schéma à l'autre
    {
        std::array<double, 6> ref {};
        bool first = true;
        for (const auto& [file, schema] : { std::pair<const char*, const char*> { "ifc2x3_Building-Structural.ifc", "IFC2X3" },
                                            { "ifc4_Building-Structural.ifc", "IFC4" },
                                            { "ifc4x3_Building-Structural.ifc", "IFC4X3_ADD2" } })
        {
            Model m;
            const auto r = load(file, m);
            TEST_CHECK(r.ok && r.schema == schema && std::abs(r.lengthUnit - 0.001) < 1e-12 && r.storeys == 1,
                       std::string("Test 400: ") + file + " lu (schéma, millimètres, étage)");
            const auto& levels = m.levelManager()->levels();
            TEST_CHECK(levels.size() == 1 && levels.front().name == "00 groundfloor" && std::abs(levels.front().elevation) < 1e-6,
                       std::string("Test 400: ") + file + " : l'étage du fichier seul, sous son nom (aucun niveau du gabarit par défaut)");
            TEST_CHECK(r.products == 17 && r.productsWithGeometry == 16 && m.bim().elements().size() == 17,
                       std::string("Test 400: ") + file + " : 17 produits conservés, 16 avec géométrie");
            std::set<std::string> guids;
            for (const auto& [id, e] : m.bim().elements())
                if (e.globalId.size() == 22) guids.insert(e.globalId);
            TEST_CHECK(guids.size() == 17, std::string("Test 400: ") + file + " : GlobalId d'origine, uniques");
            TEST_CHECK(r.byClass.at("IfcBeam") == 6 && r.byClass.at("IfcWall") == 4 && r.byClass.at("IfcFooting") == 1
                           && r.byClass.at("IfcRoof") == 1,
                       std::string("Test 400: ") + file + " : classes d'origine (6 poutres, 4 murs, semelle, toiture)");
            TEST_CHECK(m.nodes().empty() && m.beams().empty() && m.slabs().empty() && r.toPrepare == 11 && r.notStructural == 6,
                       std::string("Test 400: ") + file + " : aucun élément de calcul inventé (11 porteurs à préparer)");
            const auto b = bounds(m);
            if (first) ref = b;
            double deviation = 0.0;
            for (int k = 0; k < 6; ++k) deviation = std::max(deviation, std::abs(b[k] - ref[k]));
            TEST_CHECK(deviation < 1e-3 && b[5] - b[2] > 5.0, std::string("Test 400: ") + file + " : même géométrie que l'IFC2X3 (1 mm)");
            first = false;
        }
        std::cout << "[PASS] Test 400: Building-Structural IFC2X3 / IFC4 / IFC4X3_ADD2 (17 produits, géométrie identique)" << std::endl;
        ++passed;
    }

    // TEST 401 : architecture (IFC2X3, IFC4, IFC4X3_ADD2) : non porteurs conservés (mobilier, proxy…), toiture sans
    // géométrie propre conservée et ses parties rattachées ; à l'export, décomposition écrite (IfcRelAggregates)
    {
        for (const char* file : { "ifc2x3_Building-Architecture.ifc", "ifc4_Building-Architecture.ifc", "ifc4x3_Building-Architecture.ifc" })
        {
            Model m;
            const auto r = load(file, m);
            TEST_CHECK(r.ok && r.products == 14 && r.productsWithGeometry == 12 && r.notStructural == 8 && r.toPrepare == 6,
                       std::string("Test 401: ") + file + " : 14 produits (8 non porteurs, 6 à préparer)");
            const auto* roof = byClass(m, "IfcRoof");
            TEST_CHECK(roof && !roof->geometry, std::string("Test 401: ") + file + " : toiture conservée sans géométrie propre");
            int parts = 0;
            for (const auto& [id, e] : m.bim().elements())
                if (const auto* p = imported(e, "Parent"); p && p->text.size() > 22 && p->text.compare(p->text.size() - 22, 22, roof->globalId) == 0)
                    ++parts;
            TEST_CHECK(parts >= 1, std::string("Test 401: ") + file + " : parties de la toiture rattachées (TSA_Import.Parent)");
            std::string doc;
            TEST_CHECK(IfcExporter::exportToString(m, {}, doc).ok, std::string("Test 401: ") + file + " : export");
            // Spatial : projet → site, site → bâtiment, bâtiment → étage ; plus au moins un tout → parties
            TEST_CHECK(countEntity(doc, "IFCRELAGGREGATES") >= 4 && countEntity(doc, "IFCROOF") == 1,
                       std::string("Test 401: ") + file + " : décomposition de la toiture exportée");
        }
        std::cout << "[PASS] Test 401: Building-Architecture IFC2X3 / IFC4 / IFC4X3_ADD2 (non porteurs, agrégats)" << std::endl;
        ++passed;
    }

    // TEST 402 : mur avec ouverture et fenêtre (IFC4 Reference View) : mur percé (géométrie avec ouverture soustraite),
    // ouverture et dimensions de la fenêtre conservées, l'ouverture elle-même n'est pas un produit
    {
        Model m;
        const auto r = load("ifc4rv_wall-with-opening-and-window.ifc", m);
        TEST_CHECK(r.ok && r.products == 2 && r.byClass.count("IfcWall") && r.byClass.count("IfcWindow") && !r.byClass.count("IfcOpeningElement"),
                   "Test 402: mur et fenêtre importés, ouverture non comptée comme produit");
        const auto* wall = byClass(m, "IfcWall");
        const auto* window = byClass(m, "IfcWindow");
        TEST_CHECK(wall && window && wall->geometry && window->geometry, "Test 402: géométrie du mur et de la fenêtre");
        const auto* openings = imported(*wall, "Openings");
        TEST_CHECK(openings && openings->number == 1.0 && wall->geometry->triangleCount() > 12,
                   "Test 402: ouverture conservée (comptée, soustraite de la géométrie : plus de 12 triangles)");
        const auto* h = imported(*window, "OverallHeight");
        const auto* w = imported(*window, "OverallWidth");
        TEST_CHECK(h && w && h->number > 0.1 && h->number < 5.0 && w->number > 0.1 && w->number < 5.0 && window->status == AnalyticalStatus::NotStructural,
                   "Test 402: dimensions de la fenêtre en mètres, non porteuse");
        std::cout << "[PASS] Test 402: Ouverture et fenêtre (fenêtre " << h->number << " × " << w->number << " m)" << std::endl;
        ++passed;
    }

    // TEST 403 : unités et placements imbriqués : poteau tessellé en pouces (unité convertie IfcConversionBasedUnit),
    // placé à (432, 288, 48) po par deux placements relatifs → mètres
    {
        Model m;
        const auto r = load("ifc4rv_column-straight-rectangle-tessellation.ifc", m);
        TEST_CHECK(r.ok && std::abs(r.lengthUnit - 0.0254) < 1e-12 && r.products == 1 && r.productsWithGeometry == 1,
                   "Test 403: unité de longueur en pouces reconnue (0,0254 m)");
        const auto b = bounds(m);
        auto approx = [](double a, double e) { return std::abs(a - e) < 1e-6; };
        TEST_CHECK(approx(b[5] - b[2], 120 * 0.0254) && approx(b[3] - b[0], 8 * 0.0254) && approx(b[4] - b[1], 8 * 0.0254),
                   "Test 403: poteau 8 × 8 × 120 po = 0,2032 × 0,2032 × 3,048 m");
        TEST_CHECK(approx(b[2], 48 * 0.0254) && approx((b[0] + b[3]) / 2, 432 * 0.0254) && approx((b[1] + b[4]) / 2, 288 * 0.0254),
                   "Test 403: position issue des placements imbriqués (base à 1,2192 m, centre 10,9728 ; 7,3152 m)");
        std::cout << "[PASS] Test 403: Unités (pouces) et placements imbriqués" << std::endl;
        ++passed;
    }

    // TEST 404 : aller-retour sur les 9 fichiers : import → export IFC4X3_ADD2 relu par un lecteur indépendant → import →
    // tout est conservé (identités, classes, noms, propriétés, états, géométrie à 1 mm, modèle de calcul)
    {
        int files = 0;
        for (const auto& entry : fs::directory_iterator(dataDir()))
        {
            if (entry.path().extension() != ".ifc") continue;
            std::string failure;
            const bool ok = roundTrip(entry.path(), &failure);
            if (!ok) std::cout << "  " << entry.path().filename().string() << " : " << failure << std::endl;
            TEST_CHECK(ok, "Test 404: aller-retour sans perte : " + entry.path().filename().string());
            ++files;
        }
        TEST_CHECK(files == 9, "Test 404: les 9 fichiers d'essai sont présents");
        std::cout << "[PASS] Test 404: Aller-retour IFC sans perte (" << files << " fichiers, tolérance géométrique 1 mm)" << std::endl;
        ++passed;
    }

    // TEST 405 : fichiers invalides : tronqué, texte quelconque, absent → refus motivé, projet cible non modifié
    {
        const std::string full = readText(dataDir() / "ifc4_Building-Structural.ifc");
        const fs::path truncated = workDir() / "tronque.ifc";
        std::ofstream(truncated, std::ios::binary) << full.substr(0, full.size() * 2 / 5);
        const fs::path text = workDir() / "texte.ifc";
        std::ofstream(text, std::ios::binary) << "Ceci n'est pas un fichier IFC.";
        for (const fs::path& p : { truncated, text, workDir() / "absent.ifc" })
        {
            Model m;
            const auto r = IfcImporter::importFile(p.string(), m);
            TEST_CHECK(!r.ok && !r.error.empty() && m.nodes().empty() && m.bim().elements().empty(),
                       "Test 405: refus motivé, rien d'importé : " + p.filename().string() + " (" + r.error + ")");
        }
        Model m;
        TEST_CHECK(IfcImporter::importFile(truncated.string(), m).error.find("incomplet") != std::string::npos,
                   "Test 405: fichier tronqué désigné comme incomplet");
        std::cout << "[PASS] Test 405: Fichiers invalides refusés (tronqué, non IFC, absent)" << std::endl;
        ++passed;
    }

    // TEST 406 : grand fichier : pont IFC2X3 (68 produits dont 20 assemblages sans géométrie propre) en temps borné ;
    // avec TSA_IFC_SAMPLES, toiture de la grande cour (5 178 produits) importée en moins de 60 s
    {
        QElapsedTimer t;
        t.start();
        Model m;
        const auto r = load("ifc2x3_Infra-Bridge.ifc", m);
        const auto ms = t.elapsed();
        TEST_CHECK(r.ok && r.products == 68 && r.productsWithGeometry == 48 && r.byClass.at("IfcElementAssembly") == 20 && ms < 30000,
                   "Test 406: pont IFC2X3 (68 produits, 48 géométries, 20 assemblages) en moins de 30 s");
        std::string extra;
        if (const QByteArray dir = qgetenv("TSA_IFC_SAMPLES"); !dir.isEmpty())
        {
            const fs::path big = fs::path(dir.toStdString()) / "geometrygym_great_court_roof.ifc";
            if (fs::exists(big))
            {
                t.restart();
                Model g;
                const auto rg = IfcImporter::importFile(big.string(), g);
                const auto gms = t.elapsed();
                TEST_CHECK(rg.ok && rg.products == 5178 && rg.productsWithGeometry == 5178 && gms < 60000,
                           "Test 406: toiture de la grande cour (5 178 produits) en moins de 60 s");
                extra = ", grande cour " + std::to_string(gms) + " ms";
            }
        }
        std::cout << "[PASS] Test 406: Grand fichier (pont " << ms << " ms" << extra << ")" << std::endl;
        ++passed;
    }

    // TEST 407 : projet .tsa d'un IFC importé : enregistrement puis relecture sans perte (géométrie BGEO, Psets, états,
    // classes d'origine) — le format natif conserve les données IFC
    {
        Model a;
        TEST_CHECK(load("ifc4_Building-Architecture.ifc", a).ok, "Test 407: import");
        const QString path = QString::fromStdString((workDir() / "architecture.tsa").string());
        std::string error;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveToFile(path, a), "Test 407: enregistrement .tsa");
        Model b;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(path, b, nullptr, &error), "Test 407: relecture .tsa (" + error + ")");
        const auto c = compareModels(a, b, 1e-6);
        if (!c.identical())
            for (const auto& d : c.differences) std::cout << "    " << d << std::endl;
        TEST_CHECK(c.identical(), "Test 407: projet relu identique (" + c.summary() + ")");
        std::cout << "[PASS] Test 407: Projet .tsa d'un IFC importé relu sans perte" << std::endl;
        ++passed;
    }

    // TEST 409 : export robuste : dossier inexistant refusé, cible en lecture seule laissée intacte, aucune copie
    // temporaire de relecture laissée sur le disque (succès comme échec)
    {
        Model m;
        TEST_CHECK(load("ifc4rv_wall-with-opening-and-window.ifc", m).ok, "Test 409: import");
        const fs::path dir = workDir() / "export409";
        fs::remove_all(dir);
        fs::create_directories(dir);
        auto leftovers = [&dir] {
            int n = 0;
            for (const auto& e : fs::directory_iterator(dir))
                if (e.path().filename().string().rfind(".tsa-ifc-", 0) == 0) ++n;
            return n;
        };
        const auto missing = IfcExporter::exportToFile(m, (dir / "absent" / "x.ifc").string());
        TEST_CHECK(!missing.ok && !missing.error.empty() && !fs::exists(dir / "absent"), "Test 409: dossier inexistant : refus motivé (" + missing.error + ")");

        const fs::path locked = dir / "verrouille.ifc";
        std::ofstream(locked, std::ios::binary) << "contenu d'origine";
        fs::permissions(locked, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
        const auto ro = IfcExporter::exportToFile(m, locked.string());
        fs::permissions(locked, fs::perms::owner_write, fs::perm_options::add);
        TEST_CHECK(!ro.ok && readText(locked) == "contenu d'origine" && leftovers() == 0,
                   "Test 409: cible en lecture seule : refus, fichier existant intact, aucune copie temporaire (" + ro.error + ")");

        const auto ok = IfcExporter::exportToFile(m, (dir / "export.ifc").string());
        TEST_CHECK(ok.ok && ok.verifiedElements == ok.products && fs::file_size(dir / "export.ifc") > 1000 && leftovers() == 0,
                   "Test 409: export relu, aucune copie temporaire laissée");
        std::cout << "[PASS] Test 409: Export robuste (dossier absent, cible protégée, aucun fichier temporaire)" << std::endl;
        ++passed;
    }

    // TEST 408 : couche des objets IFC dans la vue 3D réelle : objets tracés, masquage puis réaffichage sans toucher au
    // projet ni aux autres objets, état conservé au changement de projet (comme les autres aides visuelles).
    // Vue OCCT : contexte OpenGL sur une vraie fenêtre ; la plateforme Qt « offscreen » n'en fournit pas (arrêt du
    // programme, 0xC0000409) : test annoncé comme sauté, jamais compté réussi.
    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
        std::cout << "[SKIP] Test 408: vue 3D indisponible sur la plateforme Qt « offscreen » (lancer avec QT_QPA_PLATFORM=windows)" << std::endl;
    else
    {
        auto pump = [](int n = 10) {
            for (int k = 0; k < n; ++k)
            {
                QApplication::processEvents();
                QThread::msleep(5);
            }
        };
        auto displayed = [](OccView& v) {
            AIS_ListOfInteractive list;
            v.context()->DisplayedObjects(list);
            return list.Extent();
        };
        Model m;
        TEST_CHECK(load("ifc4_Building-Architecture.ifc", m).ok, "Test 408: import");
        const int n = m.addNode(0, 0, 0);   // un objet TSA ordinaire à côté des objets importés
        TEST_CHECK(n > 0, "Test 408: nœud TSA");
        OccView view;
        view.resize(640, 480);
        view.show();
        pump(20);
        view.setModel(&m);
        pump();
        const int all = displayed(view);
        TEST_CHECK(view.bimGeometryVisible() && view.bimGeometryCount() == 12 && view.bimGeometryDisplayedCount() == 12,
                   "Test 408: 12 objets importés avec géométrie tracés");
        view.setBimGeometryVisible(false);
        pump();
        TEST_CHECK(!view.bimGeometryVisible() && view.bimGeometryDisplayedCount() == 0 && view.bimGeometryCount() == 12
                       && displayed(view) == all - 12 && m.bim().elements().size() == 14 && m.nodes().size() == 1,
                   "Test 408: couche masquée, projet intact (14 produits), autres objets toujours affichés");
        view.setBimGeometryVisible(true);
        pump();
        TEST_CHECK(view.bimGeometryDisplayedCount() == 12 && displayed(view) == all, "Test 408: couche réaffichée à l'identique");

        view.setBimGeometryVisible(false);
        Model other;
        TEST_CHECK(load("ifc4_Building-Structural.ifc", other).ok, "Test 408: second projet");
        view.setModel(&other);
        pump();
        TEST_CHECK(!view.bimGeometryVisible() && view.bimGeometryCount() == 16 && view.bimGeometryDisplayedCount() == 0,
                   "Test 408: autre projet : objets préparés, choix de masquage conservé");
        view.setBimGeometryVisible(true);
        pump();
        TEST_CHECK(view.bimGeometryDisplayedCount() == 16, "Test 408: autre projet réaffiché");
        view.setModel(nullptr);
        pump();
        std::cout << "[PASS] Test 408: Couche des objets IFC (afficher / masquer, changement de projet)" << std::endl;
        ++passed;
    }
    return true;
}
