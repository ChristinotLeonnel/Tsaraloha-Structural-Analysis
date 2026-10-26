// Suite « bim » : couche BIM, identifiants stables, mapping physique → analytique (tests 170-180).

#include "test_common.h"

#include "BIM/Core/BimModel.h"
#include "BIM/Core/IfcGuid.h"
#include "BIM/IFC/IfcExporter.h"
#include "BIM/IFC/IfcImporter.h"
#include "BIM/IFC/IfcMapper.h"
#include "BIM/IFC/IfcStepWriter.h"
#include "Coordinate/LevelManager.h"
#include "IO/TSAFile.h"
#include "Model/StructuralClipboard.h"

#include <QJsonObject>

#include <filesystem>
#include <set>

using namespace TSA::Model;
using TSA::BIM::AnalyticalRef;
using TSA::BIM::BimCategory;

namespace
{
/// Portique 3D de référence : poteaux, poutre divisée, contreventement, dalle, voile, semelle, pieu.
void buildFrame(Model& m)
{
    auto* lm = m.levelManager();
    lm->addLevel("Fondations", 50.0);
    lm->addLevel("Toiture", 54.0);
    const int a = m.addNode(0, 0, 50), b = m.addNode(6, 0, 50), c = m.addNode(6, 4, 50), d = m.addNode(0, 4, 50);
    const int a2 = m.addNode(0, 0, 54), b2 = m.addNode(6, 0, 54), c2 = m.addNode(6, 4, 54), d2 = m.addNode(0, 4, 54);
    for (int n : { a, b, c, d }) m.getNode(n)->setSupport(SupportDefinition::fixed());
    m.addColumn(a, a2, Section::heb(300), Material::steelS235());
    m.addColumn(b, b2, Section::heb(300), Material::steelS235());
    m.addColumn(c, c2, Section::rectangular(0.3, 0.3), Material::steelS235());
    m.addColumn(d, d2, Section::circular(0.4), Material::steelS235());
    const int g = m.addBar(a2, b2, Section::ipe(400), Material::steelS235(), BarRole::Beam);
    m.splitBeam(g, 2);
    m.addBar(b2, c2, Section::upn(200), Material::steelS235(), BarRole::Beam);
    m.addBar(c2, d2, Section::pipe(0.2, 0.01), Material::steelS235(), BarRole::Beam);
    m.addBar(d2, a2, Section::boxHollow(0.2, 0.3, 0.01), Material::steelS235(), BarRole::Beam);
    m.addBar(a, b2, Section::angle(0.1, 0.1, 0.01), Material::steelS235(), BarRole::Brace);
    m.addSlab({ a2, b2, c2, d2 }, 0.2);
    m.addWall(c, d, 4.0, 0.2);
    m.addFoundation(a, 1.5, 1.5, 0.5);
    m.addFoundation(b, 0.8, 0.8, 12.0, "", FoundationType::Pile);
}

std::string IfcMapperPredef(const TSA::BIM::PhysicalElement& e)
{
    return TSA::BIM::Ifc::IfcMapper::predefinedType(e.category, e.predefinedType);
}

int count(const std::string& doc, const std::string& entity)
{
    int n = 0;
    for (std::size_t p = doc.find("=" + entity + "("); p != std::string::npos; p = doc.find("=" + entity + "(", p + 1)) ++n;
    return n;
}

int bar(Model& m, int a, int b) { return m.addBar(a, b, Section::ipe(300), Material::steelS235(), BarRole::Beam); }
AnalyticalRef beamRef(int id) { return { ElementKind::Beam, id }; }
} // namespace

bool runSuite_Bim(int& passed)
{
    // TEST 170 : Test_CreateBeam — produit physique, GlobalId IFC valide et distinct de l'id interne
    {
        std::array<std::uint8_t, 16> bytes {};
        for (int i = 0; i < 16; ++i) bytes[i] = static_cast<std::uint8_t>(i * 17 + 3);
        const std::string g = TSA::BIM::IfcGuid::compress(bytes);
        std::array<std::uint8_t, 16> back {};
        TEST_CHECK(TSA::BIM::IfcGuid::isValid(g) && TSA::BIM::IfcGuid::expand(g, back) && back == bytes,
                   "Test 170: encodage GlobalId IFC réversible");

        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0);
        const int beam = bar(m, a, b);
        const auto* p = m.bim().physicalOf(beamRef(beam));
        TEST_CHECK(p && p->category == BimCategory::Beam && p->predefinedType == "BEAM", "Test 170: IfcBeam créé");
        TEST_CHECK(TSA::BIM::IfcGuid::isValid(p->globalId) && p->globalId != std::to_string(p->id),
                   "Test 170: GlobalId valide, distinct de l'id interne");
        const std::string na = m.bim().analyticalGlobalId({ ElementKind::Node, a });
        TEST_CHECK(TSA::BIM::IfcGuid::isValid(na) && na != p->globalId, "Test 170: GlobalId analytique du nœud");
        TEST_CHECK(m.bim().physicalOf({ ElementKind::Node, a }) == nullptr, "Test 170: un nœud n'est pas un produit physique");

        const int f = m.addNode(10, 0, 0), t = m.addNode(10, 0, 3);
        const int col = m.addColumn(f, t, Section::heb(200), Material::steelS235());
        const auto* pc = m.bim().physicalOf({ ElementKind::Column, col });
        TEST_CHECK(pc && pc->category == BimCategory::Column && std::string(TSA::BIM::ifcEntity(pc->category)) == "IfcColumn",
                   "Test 170: poteau → IfcColumn");
        std::cout << "[PASS] Test 170: Test_CreateBeam (produit physique + GlobalId)" << std::endl;
        ++passed;
    }

    // TEST 171 : Test_ModifyBeam — une modification ne change jamais l'identité
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0);
        const int beam = bar(m, a, b);
        const std::string g = m.bim().physicalOf(beamRef(beam))->globalId;
        const int pid = m.bim().physicalOf(beamRef(beam))->id;
        m.getBeam(beam)->setSection(Section::ipe(400));
        m.notifyBeamModified(beam);
        m.moveNodes({ b }, 1.0, 0.0, 0.0);
        const auto* p = m.bim().physicalOf(beamRef(beam));
        TEST_CHECK(p && p->globalId == g && p->id == pid, "Test 171: GlobalId et id conservés après modification");
        std::cout << "[PASS] Test 171: Test_ModifyBeam (identité stable)" << std::endl;
        ++passed;
    }

    // TEST 172 : Test_DeleteBeam — suppression puis Annuler : même GlobalId restauré
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0), c = m.addNode(12, 0, 0);
        const int b1 = bar(m, a, b);
        const int b2 = bar(m, b, c);
        const std::string g1 = m.bim().physicalOf(beamRef(b1))->globalId;
        const std::string g2 = m.bim().physicalOf(beamRef(b2))->globalId;
        m.pushUndoState("Supprimer");
        m.removeBeam(b1);
        TEST_CHECK(!m.bim().physicalOf(beamRef(b1)) && m.bim().elements().size() == 1, "Test 172: produit supprimé avec la barre");
        TEST_CHECK(m.bim().physicalOf(beamRef(b2))->globalId == g2, "Test 172: l'autre produit est intact");
        TEST_CHECK(m.undo(), "Test 172: Annuler");
        const auto* p = m.bim().physicalOf(beamRef(b1));
        TEST_CHECK(p && p->globalId == g1, "Test 172: même GlobalId après Annuler");
        std::cout << "[PASS] Test 172: Test_DeleteBeam (suppression + Annuler)" << std::endl;
        ++passed;
    }

    // TEST 173 : Test_PhysicalAnalyticalMapping — 1 poutre physique → N barres analytiques
    {
        Model m;
        const int a = m.addNode(0, 0, 3), b = m.addNode(9, 0, 3);
        const int beam = bar(m, a, b);
        const std::string g = m.bim().physicalOf(beamRef(beam))->globalId;
        const auto parts = m.splitBeam(beam, 3);
        TEST_CHECK(parts.size() == 3, "Test 173: division en 3");
        const auto* p = m.bim().physicalOf(beamRef(beam));
        TEST_CHECK(p && p->globalId == g && p->analytical.size() == 3 && m.bim().elements().size() == 1,
                   "Test 173: une seule poutre physique, 3 éléments analytiques");
        bool ordered = true;
        for (size_t i = 0; i < parts.size(); ++i) ordered &= p->analytical[i] == beamRef(parts[i]);
        TEST_CHECK(ordered, "Test 173: éléments dans l'ordre de l'axe");

        int created = 0;
        m.splitBarAt(ElementKind::Beam, parts[1], 0.5, 0, &created);
        TEST_CHECK(created > 0 && m.bim().physicalOf(beamRef(created)) == m.bim().physicalOf(beamRef(beam))
                       && m.bim().physicalOf(beamRef(beam))->analytical.size() == 4,
                   "Test 173: division ponctuelle rattachée au même produit");

        std::set<int> beams;
        for (const auto& [id, bm] : m.beams()) beams.insert(id);
        m.copyElements({}, beams, {}, {}, 0.0, 5.0, 0.0, 1);
        TEST_CHECK(m.bim().elements().size() == 2, "Test 173: la copie forme un second produit physique");
        std::set<std::string> guids;
        for (const auto& [id, e] : m.bim().elements())
        {
            guids.insert(e.globalId);
            TEST_CHECK(e.analytical.size() == 4, "Test 173: 4 éléments analytiques par produit");
        }
        TEST_CHECK(guids.size() == 2, "Test 173: GlobalId distincts");

        const int sep = m.bimForEdit().ungroup(beamRef(beam));
        TEST_CHECK(sep > 0 && m.bim().elements().size() == 3, "Test 173: séparer un élément analytique");
        m.bimForEdit().group({ beamRef(parts[1]), beamRef(beam) });
        TEST_CHECK(m.bim().elements().size() == 2 && m.bim().physicalOf(beamRef(beam))->analytical.size() == 4,
                   "Test 173: regrouper");

        m.removeBeam(parts[2]);
        TEST_CHECK(m.bim().physicalOf(beamRef(beam))->analytical.size() == 3, "Test 173: référence retirée à la suppression");
        std::cout << "[PASS] Test 173: Test_PhysicalAnalyticalMapping (1:N, division, copie, regroupement)" << std::endl;
        ++passed;
    }

    // TEST 174 : Test_Level — étages IFC = niveaux TSA ; étage déduit de la géométrie
    {
        Model m;
        auto* lm = m.levelManager();
        TEST_CHECK(lm != nullptr, "Test 174: gestionnaire de niveaux");
        // Cotes distinctes des niveaux par défaut du modèle
        const std::string l0id = lm->addLevel("RDC", 100.0)->id;
        const std::string l1id = lm->addLevel("R+1", 103.5)->id;
        const int a = m.addNode(0, 0, 103.5), b = m.addNode(6, 0, 103.5);
        const int beam = bar(m, a, b);
        const int f = m.addNode(0, 0, 100.0);
        const int col = m.addColumn(f, a, Section::heb(200), Material::steelS235());
        const auto& bim = m.bim();
        TEST_CHECK(bim.spatial().storeyGlobalIds.count(l0id) && bim.spatial().storeyGlobalIds.count(l1id),
                   "Test 174: un étage par niveau");
        const auto* pb = bim.physicalOf(beamRef(beam));
        const auto* pc = bim.physicalOf({ ElementKind::Column, col });
        TEST_CHECK(bim.resolvedStorey(m, *pb) == l1id, "Test 174: poutre à +103,50 → R+1");
        TEST_CHECK(bim.resolvedStorey(m, *pc) == l0id, "Test 174: poteau (pied à 0) → RDC");
        m.bimForEdit().element(pb->id)->storeyLevelId = l0id;
        TEST_CHECK(m.bim().resolvedStorey(m, *m.bim().physicalOf(beamRef(beam))) == l0id, "Test 174: étage imposé prioritaire");
        std::cout << "[PASS] Test 174: Test_Level (structure spatiale)" << std::endl;
        ++passed;
    }

    // TEST 175 : persistance .tsa (chunk BIMM) — GlobalId, mapping, Psets conservés
    {
        const std::string dir = "./build/test_tsa_data/";
        std::filesystem::create_directories(dir);
        const QString file = QString::fromStdString(dir + "test_bim.tsa");

        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(8, 0, 0);
        const int beam = bar(m, a, b);
        m.splitBeam(beam, 2);
        auto* p = m.bimForEdit().element(m.bim().physicalOf(beamRef(beam))->id);
        p->name = "Poutre P1";
        TSA::BIM::PropertySet ps;
        ps.name = "Pset_BeamCommon";
        ps.properties["LoadBearing"] = TSA::BIM::PropertyValue::ofBool(true);
        ps.properties["Span"] = TSA::BIM::PropertyValue::ofReal(8.0, TSA::BIM::PropertyValue::Type::Length);
        p->propertySets.push_back(ps);
        p->classifications.push_back({ "Uniclass 2015", "Ss_20_05_15", "Beams" });
        const auto before = m.bim().toJson();

        std::string err;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 175: sauvegarde");
        Model r;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(file, r, nullptr, &err), "Test 175: chargement");
        const auto* q = r.bim().physicalOf(beamRef(beam));
        TEST_CHECK(q && q->globalId == p->globalId && q->name == "Poutre P1" && q->analytical.size() == 2,
                   "Test 175: produit, GlobalId et mapping relus");
        const auto* rps = q ? q->propertySet("Pset_BeamCommon") : nullptr;
        TEST_CHECK(rps && rps->properties.at("Span").number == 8.0 && rps->properties.at("LoadBearing").flag
                       && q->classifications.size() == 1,
                   "Test 175: Psets et classification relus");
        TEST_CHECK(r.bim().toJson() == before, "Test 175: couche BIM identique après aller-retour");
        TEST_CHECK(r.bim().spatial().project.globalId == m.bim().spatial().project.globalId, "Test 175: GlobalId du projet conservé");
        std::cout << "[PASS] Test 175: Persistance .tsa (chunk BIMM)" << std::endl;
        ++passed;
    }

    // TEST 176 : robustesse — schéma futur refusé, couche vide complétée (fichiers ≤ 1.2)
    {
        TSA::BIM::BimModel out;
        TEST_CHECK(!TSA::BIM::BimModel::fromJson(QJsonObject { { "schema", 99 } }, out), "Test 176: schéma futur refusé");

        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0);
        const int beam = bar(m, a, b);
        m.setBim(TSA::BIM::BimModel());   // comme un fichier sans chunk BIMM
        const auto* p = m.bim().physicalOf(beamRef(beam));
        TEST_CHECK(p && TSA::BIM::IfcGuid::isValid(p->globalId), "Test 176: GlobalId attribués à un ancien modèle");
        const std::string g = p->globalId;
        TEST_CHECK(m.bim().physicalOf(beamRef(beam))->globalId == g, "Test 176: synchronisation idempotente");
        std::cout << "[PASS] Test 176: Robustesse de la couche BIM" << std::endl;
        ++passed;
    }

    // TEST 177 : Test_IFCExport — structure spatiale, produits, analytique, Psets ; GlobalId = couche BIM
    {
        using TSA::BIM::Ifc::IfcStepWriter;
        TEST_CHECK(IfcStepWriter::real(1e-5) == "1.E-05" && IfcStepWriter::real(3.0) == "3."
                       && IfcStepWriter::str("B\xC3\xA2ti 'A'") == R"('B\X2\00E2\X0\ti ''A''')",
                   "Test 177: encodage STEP (réels, apostrophes, Unicode)");
        Model m;
        buildFrame(m);
        std::string doc;
        TSA::BIM::Ifc::IfcExportOptions opt;
        opt.timeStamp = "2026-10-06T12:00:00";
        const auto r = TSA::BIM::Ifc::IfcExporter::exportToString(m, opt, doc);
        TEST_CHECK(r.ok && doc.rfind("ISO-10303-21;", 0) == 0 && doc.find("FILE_SCHEMA(('IFC4X3_ADD2'))") != std::string::npos,
                   "Test 177: en-tête STEP IFC 4.3");
        TEST_CHECK(count(doc, "IFCPROJECT") == 1 && count(doc, "IFCSITE") == 1 && count(doc, "IFCBUILDING") == 1
                       && count(doc, "IFCBUILDINGSTOREY") == r.storeys && r.storeys >= 2,
                   "Test 177: structure spatiale");
        TEST_CHECK(count(doc, "IFCCOLUMN") == 4 && count(doc, "IFCBEAM") == 4 && count(doc, "IFCMEMBER") == 1
                       && count(doc, "IFCSLAB") == 1 && count(doc, "IFCWALL") == 1 && count(doc, "IFCFOOTING") == 1
                       && count(doc, "IFCPILE") == 1,
                   "Test 177: produits physiques (poutre divisée = 1 IfcBeam)");
        TEST_CHECK(count(doc, "IFCSTRUCTURALCURVEMEMBER") == 10 && count(doc, "IFCSTRUCTURALSURFACEMEMBER") == 2
                       && count(doc, "IFCSTRUCTURALANALYSISMODEL") == 1 && count(doc, "IFCBOUNDARYNODECONDITION") == 4,
                   "Test 177: modèle analytique (10 barres, 2 surfaces, 4 appuis)");
        TEST_CHECK(count(doc, "IFCISHAPEPROFILEDEF") >= 2 && count(doc, "IFCUSHAPEPROFILEDEF") == 1 && count(doc, "IFCLSHAPEPROFILEDEF") == 1
                       && count(doc, "IFCCIRCLEHOLLOWPROFILEDEF") == 1 && count(doc, "IFCRECTANGLEHOLLOWPROFILEDEF") == 1,
                   "Test 177: profils paramétrés");
        TEST_CHECK(doc.find("'Pset_TSA_Structural'") != std::string::npos && doc.find("'Pset_BeamCommon'") != std::string::npos
                       && count(doc, "IFCRELASSIGNSTOPRODUCT") == r.products,
                   "Test 177: Psets et liens physique → analytique (semelle et pieu : vers leur appui)");
        bool guids = true;
        for (const auto& [id, e] : m.bim().elements()) guids &= doc.find("'" + e.globalId + "'") != std::string::npos;
        TEST_CHECK(guids && doc.find("'" + m.bim().spatial().project.globalId + "'") != std::string::npos,
                   "Test 177: GlobalId du fichier = GlobalId de la couche BIM");
        std::string again;
        TSA::BIM::Ifc::IfcExporter::exportToString(m, opt, again);
        TEST_CHECK(count(again, "IFCBEAM") == 4 && again.size() == doc.size(), "Test 177: export reproductible, modèle non modifié");

        const std::string dir = "./build/test_tsa_data/";
        std::filesystem::create_directories(dir);
        std::filesystem::remove(dir + "test_export.ifc");   // pas de fichier d'une exécution précédente
        const auto written = TSA::BIM::Ifc::IfcExporter::exportToFile(m, dir + "test_export.ifc", opt);
        TEST_CHECK(written.ok, "Test 177: écriture du fichier .ifc — " << written.error);
        std::cout << "[PASS] Test 177: Test_IFCExport (" << r.summary() << ")" << std::endl;
        ++passed;
    }

    // TEST 178 : Test_IFCImport — fichier d'un autre logiciel (physique seul) : analytique déduit de la géométrie
    // À LA DEMANDE (deriveAnalyticalFromGeometry) ; par défaut, rien n'est inventé (produits « à préparer »)
    {
        // IFC4 minimal : poutre extrudée (profil I, rotation 90°), dalle à contour, semelle ; placement d'étage décalé
        const std::string ifc = R"(ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('ViewDefinition [ReferenceView]'),'2;1');
FILE_NAME('x.ifc','2026-01-01T00:00:00',(''),(''),'','','');
FILE_SCHEMA(('IFC4'));
ENDSEC;
DATA;
#1=IFCPROJECT('2O2Fr$t4X7Zf8NOew3FLOH',$,'Projet tiers',$,$,$,$,$,$);
#2=IFCCARTESIANPOINT((0.,0.,0.));
#3=IFCAXIS2PLACEMENT3D(#2,$,$);
#4=IFCLOCALPLACEMENT($,#3);
#5=IFCCARTESIANPOINT((0.,0.,3.5));
#6=IFCAXIS2PLACEMENT3D(#5,$,$);
#7=IFCLOCALPLACEMENT(#4,#6);
#8=IFCBUILDINGSTOREY('0DWgwt6o1FOx7466fPk$jl',$,'Niveau 1',$,$,#7,$,$,.ELEMENT.,3.5);
#10=IFCDIRECTION((1.,0.,0.));
#11=IFCDIRECTION((0.,0.,1.));
#12=IFCDIRECTION((0.,1.,0.));
#13=IFCCARTESIANPOINT((0.,0.,0.));
#14=IFCAXIS2PLACEMENT3D(#13,#10,#11);
#15=IFCISHAPEPROFILEDEF(.AREA.,'HEA 200',$,0.2,0.19,0.0065,0.01,$,$,$);
#16=IFCEXTRUDEDAREASOLID(#15,#14,#11,5.);
#17=IFCSHAPEREPRESENTATION($,'Body','SweptSolid',(#16));
#18=IFCPRODUCTDEFINITIONSHAPE($,$,(#17));
#19=IFCLOCALPLACEMENT(#7,#3);
#20=IFCBEAM('1Hq$ZWl5D0P9Mf8yPUe0Wz',$,'Poutre tierce',$,$,#19,#18,'P-01',.BEAM.);
#21=IFCMATERIAL('S355',$,'steel');
#22=IFCRELASSOCIATESMATERIAL('3ZhLBNw2P0Yhq1sl4JuR9q',$,$,$,(#20),#21);
#30=IFCCARTESIANPOINT((0.,0.));
#31=IFCCARTESIANPOINT((5.,0.));
#32=IFCCARTESIANPOINT((5.,4.));
#33=IFCCARTESIANPOINT((0.,4.));
#34=IFCPOLYLINE((#30,#31,#32,#33,#30));
#35=IFCARBITRARYCLOSEDPROFILEDEF(.AREA.,$,#34);
#36=IFCEXTRUDEDAREASOLID(#35,#3,#11,0.25);
#37=IFCSHAPEREPRESENTATION($,'Body','SweptSolid',(#36));
#38=IFCPRODUCTDEFINITIONSHAPE($,$,(#37));
#39=IFCSLAB('2gRXFgjRn2HPE$YoDLX3FV',$,'Dalle',$,$,#19,#38,$,.FLOOR.);
#40=IFCRECTANGLEPROFILEDEF(.AREA.,$,$,1.2,1.,$);
#41=IFCCARTESIANPOINT((0.,0.,-3.5));
#42=IFCAXIS2PLACEMENT3D(#41,$,$);
#43=IFCDIRECTION((0.,0.,-1.));
#44=IFCEXTRUDEDAREASOLID(#40,#42,#43,0.6);
#45=IFCSHAPEREPRESENTATION($,'Body','SweptSolid',(#44));
#46=IFCPRODUCTDEFINITIONSHAPE($,$,(#45));
#47=IFCFOOTING('0L1Yo2h1D1lQ6CazvQGkmq',$,'Semelle',$,$,#19,#46,$,.PAD_FOOTING.);
#48=IFCRELCONTAINEDINSPATIALSTRUCTURE('1N1W6DoZvB3uIaTBaz8Qp5',$,$,$,(#20,#39,#47),#8);
ENDSEC;
END-ISO-10303-21;
)";
        Model plain;
        const auto rp = TSA::BIM::Ifc::IfcImporter::importString(ifc, plain);
        TEST_CHECK(rp.ok && rp.products == 3 && rp.toPrepare == 3 && rp.derived == 0 && plain.beams().empty() && plain.slabs().empty()
                       && plain.foundations().empty() && plain.nodes().empty() && plain.bim().elementByGlobalId("1Hq$ZWl5D0P9Mf8yPUe0Wz")
                       && plain.bim().elementByGlobalId("1Hq$ZWl5D0P9Mf8yPUe0Wz")->status == TSA::BIM::AnalyticalStatus::NeedsPreparation,
                   "Test 178: sans déduction, aucun élément de calcul inventé (3 produits conservés « à préparer »)");

        Model m;
        TSA::BIM::Ifc::IfcImportOptions derive;
        derive.deriveAnalyticalFromGeometry = true;
        const auto r = TSA::BIM::Ifc::IfcImporter::importString(ifc, m, derive);
        TEST_CHECK(r.ok && r.products == 3 && r.derived == 3 && m.beams().size() == 1 && m.slabs().size() == 1 && m.foundations().size() == 1,
                   "Test 178: 3 produits importés, éléments de calcul déduits et signalés (poutre, dalle, semelle)");
        const auto& beam = m.beams().begin()->second;
        const auto* s = m.getNode(beam.startNodeId());
        const auto* e = m.getNode(beam.endNodeId());
        TEST_CHECK(s && e && std::abs(s->x()) < 1e-9 && std::abs(e->x() - 5.0) < 1e-9 && std::abs(s->z() - 3.5) < 1e-9,
                   "Test 178: axe de la poutre (placement d'étage composé)");
        TEST_CHECK(beam.section().shape == SectionShape::IShape && beam.section().name == "HEA 200" && beam.material().name == "S355"
                       && std::abs(beam.material().E - Material::steelS235().E) < 1.0,
                   "Test 178: profil I et matériau acier");
        TEST_CHECK(std::abs(beam.rotation() + 90.0) < 1e-6, "Test 178: rotation du profil déduite (âme horizontale : γ = -90°)");
        const auto& slab = m.slabs().begin()->second;
        TEST_CHECK(slab.nodeIds().size() == 4 && std::abs(slab.thickness() - 0.25) < 1e-12
                       && std::abs(m.getNode(slab.nodeIds().front())->z() - 3.75) < 1e-9,
                   "Test 178: dalle (face supérieure portée par les nœuds)");
        const auto& f = m.foundations().begin()->second;
        TEST_CHECK(std::abs(f.widthA() - 1.2) < 1e-12 && std::abs(f.heightH() - 0.6) < 1e-12 && std::abs(m.getNode(f.nodeId())->z()) < 1e-9,
                   "Test 178: semelle (nœud en tête)");
        const auto* p = m.bim().elementByGlobalId("1Hq$ZWl5D0P9Mf8yPUe0Wz");
        TEST_CHECK(p && p->name == "Poutre tierce" && p->tag == "P-01" && p->category == BimCategory::Beam
                       && m.bim().spatial().project.name == "Projet tiers"
                       && m.bim().spatial().storeyGlobalIds.at(m.bim().resolvedStorey(m, *p)) == "0DWgwt6o1FOx7466fPk$jl",
                   "Test 178: GlobalId, nom, repère, projet conservés");
        TEST_CHECK(!TSA::BIM::Ifc::IfcImporter::importString("pas un fichier", m).ok, "Test 178: fichier invalide refusé");
        std::cout << "[PASS] Test 178: Test_IFCImport (" << r.summary() << ")" << std::endl;
        ++passed;
    }

    // TEST 179 : Test_IFCRoundTrip — export → import → comparaison (identifiants, géométrie, matériaux, profils,
    // niveaux, relations, propriétés) puis ré-export identique en structure
    {
        Model a;
        buildFrame(a);
        a.getBeam(a.beams().begin()->first)->setRotation(30.0);
        {
            auto& bim = a.bimForEdit();
            auto* p = bim.element(bim.physicalOf(beamRef(a.beams().begin()->first))->id);
            p->name = "Poutre de rive";
            p->description = "Façade nord";
            TSA::BIM::PropertySet ps;
            ps.name = "Pset_Projet";
            ps.properties["Lot"] = TSA::BIM::PropertyValue::ofText("Gros œuvre", TSA::BIM::PropertyValue::Type::Label);
            ps.properties["Phase"] = TSA::BIM::PropertyValue::ofReal(2, TSA::BIM::PropertyValue::Type::Integer);
            ps.properties["Fleche"] = TSA::BIM::PropertyValue::ofReal(0.012, TSA::BIM::PropertyValue::Type::Length);
            p->propertySets.push_back(ps);
            p->classifications.push_back({ "Uniclass 2015", "Ss_20_05_15", "Beams" });
        }
        TSA::BIM::Ifc::IfcExportOptions opt;
        opt.timeStamp = "2026-10-06T12:00:00";
        std::string doc;
        TEST_CHECK(TSA::BIM::Ifc::IfcExporter::exportToString(a, opt, doc).ok, "Test 179: export");
        Model b;
        const auto ir = TSA::BIM::Ifc::IfcImporter::importString(doc, b);
        TEST_CHECK(ir.ok, "Test 179: import");

        // Identifiants : mêmes produits (GlobalId), mêmes catégories, mêmes métadonnées
        const auto& A = a.bim();
        const auto& B = b.bim();
        TEST_CHECK(A.elements().size() == B.elements().size() && A.spatial().project.globalId == B.spatial().project.globalId
                       && A.spatial().analysisModel.globalId == B.spatial().analysisModel.globalId,
                   "Test 179: produits et projet");
        bool same = true;
        for (const auto& [id, pa] : A.elements())
        {
            const auto* pb = B.elementByGlobalId(pa.globalId);
            if (!pb) { same = false; std::cout << "  produit manquant " << pa.globalId << std::endl; continue; }
            auto differs = [&](const char* what, bool equal) {
                if (!equal)
                {
                    std::string refs;
                    for (const auto& r : pa.analytical) refs += r.label() + " ";
                    std::cout << "  produit " << pa.globalId << " [" << refs << "→ " << pb->analytical.size() << " élément(s) relu(s), nom relu « "
                              << pb->name << " »] : " << what << " différent" << std::endl;
                }
                same &= equal;
            };
            differs("catégorie", pb->category == pa.category);
            differs("nom", pb->name == pa.name);
            differs("description", pb->description == pa.description);
            differs("éléments de calcul", pb->analytical.size() == pa.analytical.size());
            differs("classifications", pb->classifications.size() == pa.classifications.size());
            differs("type prédéfini", IfcMapperPredef(pa) == IfcMapperPredef(*pb));
            const auto* ua = pa.propertySet("Pset_Projet");
            const auto* ub = pb->propertySet("Pset_Projet");
            differs("Pset_Projet (présence)", (ua == nullptr) == (ub == nullptr));
            if (ua && ub)
                for (const auto& [k, v] : ua->properties)
                    differs(("Pset_Projet." + k).c_str(), ub->properties.count(k) && ub->properties.at(k) == v);
        }
        TEST_CHECK(same, "Test 179: GlobalId, catégories, noms, mapping 1:N, Psets et classifications identiques");

        // Éléments analytiques : même GlobalId → mêmes extrémités, section, matériau, rotation
        std::map<std::string, AnalyticalRef> refB;
        for (const auto& [r, g] : B.analyticalGlobalIds()) refB[g] = r;
        auto ends = [](const Model& m, const AnalyticalRef& r, std::array<double, 6>& out) {
            int s = 0, e = 0;
            if (r.kind == ElementKind::Beam) { s = m.getBeam(r.id)->startNodeId(); e = m.getBeam(r.id)->endNodeId(); }
            else if (r.kind == ElementKind::Column) { s = m.getColumn(r.id)->startNodeId(); e = m.getColumn(r.id)->endNodeId(); }
            else return false;
            const auto* p = m.getNode(s);
            const auto* q = m.getNode(e);
            out = { p->x(), p->y(), p->z(), q->x(), q->y(), q->z() };
            return true;
        };
        int bars = 0;
        bool geom = true;
        for (const auto& [r, g] : A.analyticalGlobalIds())
        {
            if (r.kind != ElementKind::Beam && r.kind != ElementKind::Column) continue;
            auto it = refB.find(g);
            if (it == refB.end() || it->second.kind != r.kind) { geom = false; std::cout << "  absent " << r.label() << std::endl; continue; }
            std::array<double, 6> pa {}, pb {};
            ends(a, r, pa);
            ends(b, it->second, pb);
            for (int i = 0; i < 6; ++i) geom &= std::abs(pa[i] - pb[i]) < 1e-9;
            const Section& sa = r.kind == ElementKind::Beam ? a.getBeam(r.id)->section() : a.getColumn(r.id)->section();
            const Section& sb = r.kind == ElementKind::Beam ? b.getBeam(it->second.id)->section() : b.getColumn(it->second.id)->section();
            const Material& ma = r.kind == ElementKind::Beam ? a.getBeam(r.id)->material() : a.getColumn(r.id)->material();
            const Material& mb = r.kind == ElementKind::Beam ? b.getBeam(it->second.id)->material() : b.getColumn(it->second.id)->material();
            geom &= sa.shape == sb.shape && sa.name == sb.name && std::abs(sa.area() - sb.area()) < 1e-12
                && std::abs(sa.iy() - sb.iy()) < 1e-14 && ma.name == mb.name && ma.E == mb.E && ma.nu == mb.nu
                && ma.density == mb.density && ma.fk == mb.fk;
            const double ra = r.kind == ElementKind::Beam ? a.getBeam(r.id)->rotation() : a.getColumn(r.id)->rotation();
            const double rb = r.kind == ElementKind::Beam ? b.getBeam(it->second.id)->rotation() : b.getColumn(it->second.id)->rotation();
            geom &= std::abs(ra - rb) < 1e-9;
            if (!geom && bars >= 0)
            {
                std::cout << "  écart " << r.label() << " : section " << sa.name << "/" << sb.name << " aire " << sa.area() << "/" << sb.area()
                          << " iy " << sa.iy() << "/" << sb.iy() << " E " << ma.E << "/" << mb.E << " fk " << ma.fk << "/" << mb.fk
                          << " rot " << ra << "/" << rb << " ext " << pa[0] << "," << pa[2] << "/" << pb[0] << "," << pb[2] << std::endl;
                bars = -100;
            }
            ++bars;
        }
        TEST_CHECK(bars == 10 && geom, "Test 179: 10 barres — extrémités, profils, matériaux, rotation identiques");
        TEST_CHECK(b.slabs().size() == 1 && b.walls().size() == 1 && b.foundations().size() == 2
                       && std::abs(b.walls().begin()->second.height() - 4.0) < 1e-12 && std::abs(b.slabs().begin()->second.thickness() - 0.2) < 1e-12,
                   "Test 179: dalle, voile, fondations");
        int fixedA = 0, fixedB = 0;
        for (const auto& [id, n] : a.nodes()) fixedA += n.support().tz() == DOFState::Fixed;
        for (const auto& [id, n] : b.nodes()) fixedB += n.support().tz() == DOFState::Fixed;
        TEST_CHECK(fixedA == fixedB && fixedA == 4 && a.nodes().size() == b.nodes().size(), "Test 179: nœuds et appuis");
        bool levels = true;
        for (const auto& l : a.levelManager()->levels())
        {
            bool found = false;
            for (const auto& k : b.levelManager()->levels()) found |= std::abs(k.elevation - l.elevation) < 1e-9;
            levels &= found && B.spatial().storeyGlobalIds.size() >= A.spatial().storeyGlobalIds.size();
        }
        TEST_CHECK(levels, "Test 179: niveaux / étages");
        for (const auto& [lvl, g] : A.spatial().storeyGlobalIds)
        {
            const auto* l = a.levelManager()->getLevel(lvl);
            std::string gb;
            for (const auto& [lb, g2] : B.spatial().storeyGlobalIds)
                if (std::abs(b.levelManager()->getLevel(lb)->elevation - l->elevation) < 1e-9) gb = g2;
            levels &= gb == g;
        }
        TEST_CHECK(levels, "Test 179: GlobalId des étages conservés");

        std::string doc2;
        TSA::BIM::Ifc::IfcExporter::exportToString(b, opt, doc2);
        bool structure = true;
        for (const char* t : { "IFCBEAM", "IFCCOLUMN", "IFCMEMBER", "IFCSLAB", "IFCWALL", "IFCFOOTING", "IFCPILE", "IFCSTRUCTURALCURVEMEMBER",
                               "IFCSTRUCTURALSURFACEMEMBER", "IFCSTRUCTURALPOINTCONNECTION", "IFCBOUNDARYNODECONDITION", "IFCRELASSIGNSTOPRODUCT",
                               "IFCPROPERTYSINGLEVALUE", "IFCMATERIALPROFILESET", "IFCCLASSIFICATIONREFERENCE" })
            if (count(doc, t) != count(doc2, t)) { structure = false; std::cout << "  " << t << " : " << count(doc, t) << " ≠ " << count(doc2, t) << std::endl; }
        for (const auto& [id, e] : A.elements()) structure &= doc2.find("'" + e.globalId + "'") != std::string::npos;
        TEST_CHECK(structure, "Test 179: ré-export identique en structure et en GlobalId");
        std::cout << "[PASS] Test 179: Test_IFCRoundTrip (" << ir.summary() << ")" << std::endl;
        ++passed;
    }

    // TEST 180 : import d'un fichier IFC4 écrit par IfcOpenShell 0.9 (tests/data/ifcopenshell_fixture.ifc) :
    // unités en millimètres, IfcIndexedPolyCurve, voile à contour polyligne, placements tournés
    {
        std::filesystem::path fixture = std::filesystem::path(__FILE__).parent_path() / "data" / "ifcopenshell_fixture.ifc";
        if (!std::filesystem::exists(fixture)) fixture = "tests/data/ifcopenshell_fixture.ifc";
        Model m;
        TSA::BIM::Ifc::IfcImportOptions derive;
        derive.deriveAnalyticalFromGeometry = true;   // fichier sans modèle analytique : éléments de calcul déduits
        const auto r = TSA::BIM::Ifc::IfcImporter::importFile(fixture.string(), m, derive);
        TEST_CHECK(r.ok && r.products == 4 && m.beams().size() == 1 && m.columns().size() == 1 && m.walls().size() == 1 && m.slabs().size() == 1,
                   "Test 180: poutre, poteau, voile, dalle importés");
        const auto& b = m.beams().begin()->second;
        const auto* s = m.getNode(b.startNodeId());
        const auto* e = m.getNode(b.endNodeId());
        TEST_CHECK(s && e && std::abs(s->x() - 1.0) < 1e-9 && std::abs(s->y() - 2.0) < 1e-9 && std::abs(s->z() - 3.0) < 1e-9
                       && std::abs(e->x() - 7.0) < 1e-9,
                   "Test 180: poutre de 6 m (mm → m, placement tourné)");
        TEST_CHECK(b.section().shape == SectionShape::IShape && std::abs(b.section().height - 0.3) < 1e-12 && std::abs(b.section().tw - 0.0071) < 1e-12
                       && b.material().name == "S355",
                   "Test 180: profil IPE 300 en mètres, matériau du jeu de profils");
        const auto& c = m.columns().begin()->second;
        TEST_CHECK(std::abs(m.getNode(c.endNodeId())->z() - 3.0) < 1e-9 && std::abs(c.section().width - 0.3) < 1e-12,
                   "Test 180: poteau 300×300 de 3 m");
        const auto& w = m.walls().begin()->second;
        TEST_CHECK(std::abs(w.height() - 3.0) < 1e-9 && std::abs(w.thickness() - 0.2) < 1e-9 && std::abs(w.offset() - 0.1) < 1e-9
                       && std::abs(m.getNode(w.startNodeId())->y() - 5.0) < 1e-9
                       && std::abs(std::abs(m.getNode(w.endNodeId())->x() - m.getNode(w.startNodeId())->x()) - 4.0) < 1e-9,
                   "Test 180: voile 4 m × 3 m × 0,20 (contour polyligne, décalage 0,10)");
        const auto& sl = m.slabs().begin()->second;
        TEST_CHECK(sl.nodeIds().size() == 4 && std::abs(sl.thickness() - 0.2) < 1e-9 && std::abs(m.getNode(sl.nodeIds()[2])->x() - 6.0) < 1e-9
                       && std::abs(m.getNode(sl.nodeIds()[0])->z() - 3.2) < 1e-9,
                   "Test 180: dalle 6 × 4 m (IfcIndexedPolyCurve, face supérieure)");
        const auto* p = m.bim().physicalOf({ ElementKind::Beam, m.beams().begin()->first });
        const auto* ps = p ? p->propertySet("Pset_BeamCommon") : nullptr;
        TEST_CHECK(ps && ps->properties.count("FireRating") && ps->properties.at("FireRating").text == "R30" && p->name == "B-101",
                   "Test 180: Pset_BeamCommon saisi conservé (FireRating, Reference)");
        std::cout << "[PASS] Test 180: Import d'un fichier IfcOpenShell (" << r.summary() << ")" << std::endl;
        ++passed;
    }

    // TEST 190 : Copier / Coller transmet les métadonnées BIM et le regroupement 1:N (BUG-029)
    {
        Model m;
        const int a = m.addNode(0, 0, 3), b = m.addNode(9, 0, 3);
        const int beam = bar(m, a, b);
        const auto parts = m.splitBeam(beam, 3);
        {
            auto& bim = m.bimForEdit();
            const int pid = bim.physicalOf(beamRef(beam))->id;
            auto* e = bim.element(pid);
            e->name = "Poutre file A";
            e->objectType = "IPE300 S235";
            TSA::BIM::PropertySet pset;
            pset.name = "Pset_BeamCommon";
            e->propertySets.push_back(pset);
            e->classifications.push_back({ "Uniclass 2015", "Ss_20_10_75", "Poutres" });
        }
        const std::string srcGuid = m.bim().physicalOf(beamRef(beam))->globalId;

        StructuralClipboard clip;
        std::set<int> beams(parts.begin(), parts.end());
        clip.copyFrom(m, std::set<int>{}, beams, std::set<int>{}, std::set<int>{});
        const PasteResult r = clip.pasteTo(m, 0.0, 6.0, 3.0);
        TEST_CHECK(r.beamIds.size() == 3, "Test 190: 3 barres collées");
        TEST_CHECK(m.bim().elements().size() == 2, "Test 190: le collage forme UN nouveau produit (pas 3 produits 1:1)");
        const auto* p = m.bim().physicalOf(beamRef(r.beamIds.front()));
        TEST_CHECK(p && p->analytical.size() == 3 && p->globalId != srcGuid, "Test 190: 3 éléments analytiques, nouveau GlobalId");
        TEST_CHECK(p && p->objectType == "IPE300 S235" && p->propertySets.size() == 1 && p->classifications.size() == 1
                       && p->name.empty(),
                   "Test 190: Psets et classification transmis, nom non repris");
        bool ordered = p != nullptr;
        for (size_t i = 0; ordered && i < 3; ++i) ordered = p->analytical[i] == beamRef(r.beamIds[i]);
        TEST_CHECK(ordered, "Test 190: éléments collés dans l'ordre de l'axe");
        TEST_CHECK(m.bim().physicalOf(beamRef(beam))->globalId == srcGuid, "Test 190: produit source intact");
        std::cout << "[PASS] Test 190: Coller conserve les métadonnées BIM" << std::endl;
        ++passed;
    }

    // TEST 191 : produits importés sans élément de calcul — conservés (géométrie, classe d'origine, état), persistés
    // (chunk BGEO), partagés entre instantanés ; un produit créé dans TSA vidé de ses éléments reste supprimé.
    {
        using TSA::BIM::AnalyticalStatus;
        auto box = [](double x0, double y0, double z0, double dx, double dy, double dz) {
            auto mesh = std::make_shared<TSA::BIM::TriangleMesh>();
            mesh->origin = { x0, y0, z0 };
            for (int k = 0; k < 8; ++k)
            {
                mesh->positions.push_back(static_cast<float>((k & 1) ? dx : 0.0));
                mesh->positions.push_back(static_cast<float>((k & 2) ? dy : 0.0));
                mesh->positions.push_back(static_cast<float>((k & 4) ? dz : 0.0));
            }
            mesh->indices = { 0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4, 2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5 };
            mesh->color = { 0.6f, 0.4f, 0.2f, 1.0f };
            mesh->representation = "Tessellation";
            return std::shared_ptr<const TSA::BIM::TriangleMesh>(mesh);
        };

        Model m;
        // Coordonnées géoréférencées : la précision doit rester submillimétrique
        const auto doorMesh = box(512345.25, 6812345.50, 12.0, 0.9, 0.05, 2.1);
        TSA::BIM::PhysicalElement door;
        door.globalId = TSA::BIM::IfcGuid::create();
        door.category = BimCategory::Proxy;
        door.ifcClass = "IfcDoor";
        door.name = "Porte 90×210";
        door.status = AnalyticalStatus::NotStructural;
        door.geometry = doorMesh;
        door.importNotes.push_back("Ouvrant non converti (OperationType)");
        const int doorId = m.bimForEdit().insert(door);

        TSA::BIM::PhysicalElement girder;   // poutre physique d'un fichier sans modèle analytique
        girder.globalId = TSA::BIM::IfcGuid::create();
        girder.category = BimCategory::Beam;
        girder.ifcClass = "IfcBeam";
        girder.geometry = box(0, 0, 3.0, 6.0, 0.3, 0.5);
        const int girderId = m.bimForEdit().insert(girder);

        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0);
        const int own = bar(m, a, b);   // produit créé dans TSA (1:1)
        const int ownProduct = m.bim().physicalOf(beamRef(own))->id;

        const auto* d = m.bim().element(doorId);
        const auto* g = m.bim().element(girderId);
        TEST_CHECK(d && g, "Test 191: produits importés sans élément de calcul conservés par la synchronisation");
        TEST_CHECK(d && d->status == AnalyticalStatus::NotStructural, "Test 191: porte non porteuse (état conservé)");
        TEST_CHECK(g && g->status == AnalyticalStatus::NeedsPreparation, "Test 191: poutre importée sans calcul → « à préparer »");

        // Le produit importé garde sa géométrie si on supprime ses éléments de calcul ; un produit TSA disparaît.
        m.removeBeam(own);
        TEST_CHECK(m.bim().element(ownProduct) == nullptr, "Test 191: produit TSA vidé de ses éléments supprimé");

        // Instantané d'Annuler : la géométrie est partagée, pas copiée
        const auto snap = m.createSnapshot("test");
        TEST_CHECK(snap.bim.element(doorId) && snap.bim.element(doorId)->geometry.get() == doorMesh.get(),
                   "Test 191: instantané sans copie de la géométrie");

        // Persistance .tsa : chunk BGEO
        const std::string dir = "./build/test_tsa_data/";
        std::filesystem::create_directories(dir);
        const QString file = QString::fromStdString(dir + "test_bim_imported.tsa");
        std::string err;
        TEST_CHECK(TSA::IO::TSAProjectIO::saveToFile(file, m, nullptr, &err), "Test 191: sauvegarde (" + err + ")");
        Model r;
        TEST_CHECK(TSA::IO::TSAProjectIO::loadFromFile(file, r, nullptr, &err), "Test 191: chargement (" + err + ")");
        const auto* rd = r.bim().element(doorId);
        TEST_CHECK(rd && rd->globalId == door.globalId && rd->ifcClass == "IfcDoor" && rd->status == AnalyticalStatus::NotStructural
                       && rd->importNotes.size() == 1 && rd->category == BimCategory::Proxy,
                   "Test 191: classe IFC, état, notes et GlobalId relus");
        bool sameGeometry = rd && rd->geometry && rd->geometry->triangleCount() == 12 && rd->geometry->representation == "Tessellation";
        for (std::size_t i = 0; sameGeometry && i < doorMesh->vertexCount(); ++i)
            for (int k = 0; k < 3; ++k)
                sameGeometry = sameGeometry && std::abs(rd->geometry->vertex(i)[k] - doorMesh->vertex(i)[k]) < 1e-4;
        TEST_CHECK(sameGeometry, "Test 191: géométrie relue à 0,1 mm près en coordonnées géoréférencées");
        const auto* rg = r.bim().element(girderId);
        TEST_CHECK(rg && rg->status == AnalyticalStatus::NeedsPreparation && rg->geometry && rg->analytical.empty(),
                   "Test 191: poutre « à préparer » relue avec sa géométrie, sans élément de calcul inventé");
        std::cout << "[PASS] Test 191: Produits importés sans élément de calcul (conservation, persistance BGEO)" << std::endl;
        ++passed;
    }

    // TEST 501 : raideurs d'appui IFC dans l'unité déclarée par le fichier (BUG-117, X2/X3). Le modèle TSA est en kN/m et
    // kN·m/rad ; IFC « usually measured in N/m, Nm/rad » (IFC 4.3, IfcLinearStiffnessMeasure). L'export déclare
    // LINEARSTIFFNESSUNIT / ROTATIONALSTIFFNESSUNIT et écrit des N/m ; l'import lit l'unité déclarée, sinon FORCEUNIT /
    // LENGTHUNIT / PLANEANGLEUNIT ; un fichier écrit par un ancien TSA (sans unité de raideur) reste lu en kN/m.
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(6, 0, 0);
        m.getNode(a)->setSupport(SupportDefinition::elastic(5000.0, 6000.0, 7000.0, 1200.0, 1300.0, 1400.0));
        m.getNode(b)->setSupport(SupportDefinition::pinned());
        bar(m, a, b);
        TSA::BIM::Ifc::IfcExportOptions opt;
        opt.timeStamp = "2026-10-26T12:00:00";
        std::string doc;
        TEST_CHECK(TSA::BIM::Ifc::IfcExporter::exportToString(m, opt, doc).ok, "Test 501: export");
        const std::string dir = "./build/test_tsa_data/";
        std::filesystem::create_directories(dir);
        TEST_CHECK(TSA::BIM::Ifc::IfcExporter::exportToFile(m, dir + "test_units_stiffness.ifc", opt).ok, "Test 501: fichier .ifc (lecteur indépendant)");
        TEST_CHECK(doc.find("IFCLINEARSTIFFNESSMEASURE(5000000.)") != std::string::npos
                       && doc.find("IFCLINEARSTIFFNESSMEASURE(7000000.)") != std::string::npos
                       && doc.find("IFCROTATIONALSTIFFNESSMEASURE(1200000.)") != std::string::npos
                       && doc.find("IFCROTATIONALSTIFFNESSMEASURE(1400000.)") != std::string::npos,
                   "Test 501: raideurs écrites en N/m et N·m/rad (unités du fichier)");
        TEST_CHECK(count(doc, "IFCDERIVEDUNIT") == 2 && doc.find(".LINEARSTIFFNESSUNIT.") != std::string::npos
                       && doc.find(".ROTATIONALSTIFFNESSUNIT.") != std::string::npos,
                   "Test 501: unités de raideur déclarées dans IFCUNITASSIGNMENT");

        auto springsAt0 = [](const std::string& ifc, std::array<double, 6>& k) {
            Model back;
            TSA::BIM::Ifc::IfcImportOptions o;
            o.importGeometry = false;
            const auto r = TSA::BIM::Ifc::IfcImporter::importString(ifc, back, o);
            for (const auto& [id, n] : back.nodes())
                if (std::abs(n.x()) + std::abs(n.y()) + std::abs(n.z()) < 1e-9)
                {
                    const auto& s = n.support();
                    k = { s.kx(), s.ky(), s.kz(), s.krx(), s.kry(), s.krz() };
                }
            return r;
        };
        auto equals = [](const std::array<double, 6>& k, double f) {
            const double ref[6] = { 5000.0, 6000.0, 7000.0, 1200.0, 1300.0, 1400.0 };
            for (int i = 0; i < 6; ++i)
                if (std::abs(k[i] - f * ref[i]) > 1e-9 * f * ref[i]) return false;
            return true;
        };
        std::array<double, 6> k {};
        TEST_CHECK(springsAt0(doc, k).ok && equals(k, 1.0), "Test 501: aller-retour, raideurs relues en kN/m (" << k[0] << ")");

        // Fichier d'un ancien TSA : signature de l'écrivain, aucune unité de raideur, valeurs en kN/m
        std::string legacy = doc;
        for (const char* v : { "5000000.", "6000000.", "7000000.", "1200000.", "1300000.", "1400000." })
        {
            const std::string s(v), kN = s.substr(0, s.size() - 4) + ".";
            for (std::size_t p = legacy.find("MEASURE(" + s + ")"); p != std::string::npos; p = legacy.find("MEASURE(" + s + ")", p))
                legacy.replace(p + 8, s.size(), kN);
        }
        std::vector<std::string> derived;
        for (std::size_t p = legacy.find("=IFCDERIVEDUNIT("); p != std::string::npos; p = legacy.find("=IFCDERIVEDUNIT(", p + 1))
            derived.push_back("," + legacy.substr(legacy.rfind('#', p), p - legacy.rfind('#', p)));
        for (const auto& ref : derived)
        {
            const auto ua = legacy.find("IFCUNITASSIGNMENT"), end = legacy.find(';', ua);
            auto p = legacy.find(ref, ua);
            while (p < end && legacy[p + ref.size()] != ',' && legacy[p + ref.size()] != ')') p = legacy.find(ref, p + 1);
            if (p < end) legacy.erase(p, ref.size());
        }
        TEST_CHECK(derived.size() == 2, "Test 501: ancien fichier reconstitué (unités de raideur retirées de l'affectation)");
        const auto rl = springsAt0(legacy, k);
        bool warned = false;
        for (const auto& w : rl.warnings) warned |= w.find("kN/m") != std::string::npos;
        TEST_CHECK(rl.ok && equals(k, 1.0) && warned, "Test 501: fichier d'un ancien TSA relu en kN/m, avec avertissement (" << k[0] << ")");

        // Fichier d'un autre logiciel, sans unité de raideur, forces en kN : k = force / longueur = kN/m
        std::string foreign = legacy;
        foreign.replace(foreign.find(" STEP writer'"), 13, " exporter'");
        foreign.replace(foreign.find("*,.FORCEUNIT.,$,.NEWTON."), 24, "*,.FORCEUNIT.,.KILO.,.NEWTON.");
        TEST_CHECK(springsAt0(foreign, k).ok && equals(k, 1.0), "Test 501: autre logiciel en kN sans unité de raideur → kN/m (" << k[0] << ")");

        // Unité de raideur déclarée à partir d'un FORCEUNIT en kN : les valeurs du fichier sont des kN/m
        std::string kilo = doc;
        kilo.replace(kilo.find("*,.FORCEUNIT.,$,.NEWTON."), 24, "*,.FORCEUNIT.,.KILO.,.NEWTON.");
        TEST_CHECK(springsAt0(kilo, k).ok && equals(k, 1000.0), "Test 501: unité dérivée kN/m déclarée → valeurs × 1 (" << k[0] << ")");
        std::cout << "[PASS] Test 501: Raideurs d'appui IFC en N/m, unités déclarées, anciens fichiers TSA" << std::endl;
        ++passed;
    }

    return true;
}
