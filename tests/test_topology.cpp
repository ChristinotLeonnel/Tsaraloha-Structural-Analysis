// Suite « topology » : topologie et numérotation des nœuds et des éléments (tests 225-236).
// Stratégies, étiquettes, validation, prévisualisation sans effet, application atomique (géométrie,
// connectivités, charges, appuis et résultats intacts), persistance (chunk TOPO), anciens fichiers,
// correspondance solveur, fenêtre Qt.

#include "test_common.h"

#include "Analysis/CalculationSnapshot.h"
#include "IO/TSAFile.h"
#include "IO/TSAFileFormat.h"
#include "Model/Load/LoadManager.h"
#include "Model/Load/NodalLoad.h"
#include "Topology/TopologyNumberingService.h"
#include "UI/Dialogs/TopologyDialog.h"

#include <QComboBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableWidget>

using namespace TSA::Topology;

namespace
{
/// Étiquettes des nœuds dans l'ordre de la prévisualisation.
std::map<int, std::string> nodeLabels(const NumberingPreview& p)
{
    std::map<int, std::string> out;
    for (const auto& e : p.entries)
        if (e.family == EntityFamily::Node) out[e.id] = e.newLabel;
    return out;
}

/// Grille plane 3 × 2 de nœuds (z = 0) : ids 1..6, x = 0, 5, 10 ; y = 0, 4.
void planarGrid(Model& m, std::vector<int>& ids)
{
    for (double y : { 0.0, 4.0 })
        for (double x : { 0.0, 5.0, 10.0 }) ids.push_back(m.addNode(x, y, 0.0));
}

TopologySettings withNodeStrategy(const std::string& id)
{
    TopologySettings s = TopologySettings::defaults();
    s.nodes.strategy = id;
    s.nodes.preserveCustomNames = false;
    s.elements.strategy = "keep";
    return s;
}

/// Empreinte de tout ce qu'une renumérotation ne doit jamais modifier.
std::string fingerprint(const Model& m)
{
    std::ostringstream o;
    o.precision(17);
    for (const auto& [id, n] : m.nodes())
        o << "N" << id << ':' << n.x() << ',' << n.y() << ',' << n.z() << ',' << static_cast<int>(n.support().supportType()) << ';';
    for (const auto& [id, b] : m.beams())
        o << "B" << id << ':' << b.startNodeId() << '>' << b.endNodeId() << ',' << b.section().name << ',' << b.material().name << ',' << b.rotation() << ';';
    for (const auto& [id, c] : m.columns()) o << "C" << id << ':' << c.startNodeId() << '>' << c.endNodeId() << ';';
    for (const auto& [id, s] : m.slabs())
    {
        o << "S" << id << ':';
        for (int n : s.nodeIds()) o << n << ',';
    }
    for (const auto& [id, nl] : m.loadManager().nodalLoads()) o << "L" << id << ':' << nl.nodeId() << ',' << nl.fz() << ';';
    return o.str();
}
} // namespace

bool runSuite_Topology(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 225 : paramètres — défauts, JSON, lecture tolérante, validation
    // -------------------------------------------------------------------------
    {
        TopologySettings s = TopologySettings::defaults();
        TEST_CHECK(s.nodes.strategy == "sequential" && s.nodes.format.prefix == "N" && s.elementPrefix(EntityFamily::Column) == "C",
                   "Test 225: valeurs par défaut historiques");
        s.nodes.strategy = "grid";
        s.nodes.format = { "P", "{g}", 2, 10, 5 };
        s.nodes.order.descending = { true, false, true };
        s.elements.sharedSequence = true;
        s.elements.prefixes[EntityFamily::Beam] = "PT";
        s.scope = NumberingScope::Selection;
        s.showNodeLabels = true;
        std::string warn;
        const TopologySettings r = TopologySettings::fromJson(s.toJson(), &warn);
        TEST_CHECK(warn.empty() && r.nodes.strategy == "grid" && r.nodes.format.pattern == "{g}" && r.nodes.format.start == 10 &&
                       r.nodes.format.increment == 5 && r.nodes.format.width == 2 && r.nodes.order.descending[0] && r.nodes.order.descending[2] &&
                       r.elements.sharedSequence && r.elementPrefix(EntityFamily::Beam) == "PT" && r.scope == NumberingScope::Selection &&
                       r.showNodeLabels,
                   "Test 225: aller-retour JSON");
        warn.clear();
        TEST_CHECK(TopologySettings::fromJson("", &warn).nodes.strategy == "sequential" && warn.empty(), "Test 225: ancien projet → défauts, sans avertissement");
        TEST_CHECK(TopologySettings::fromJson("{oups", &warn).nodes.format.prefix == "N" && !warn.empty(), "Test 225: JSON illisible → défauts, signalé");
        warn.clear();
        TopologySettings::fromJson(R"({"schema":99,"nodes":{"order":{"axes":"XXZ"}}})", &warn);
        TEST_CHECK(warn.find("récente") != std::string::npos && warn.find("axes") != std::string::npos, "Test 225: version future et axes invalides signalés");

        TopologySettings bad = TopologySettings::defaults();
        bad.nodes.format.increment = 0;
        bad.nodes.format.pattern = "{p}";
        bad.nodes.format.prefix = "N 1";
        bad.elements.prefixes[EntityFamily::Column] = "B"; // même préfixe que les poutres, suites séparées
        const auto errors = validateSettings(bad);
        auto has = [&](const char* text) {
            return std::any_of(errors.begin(), errors.end(), [&](const std::string& e) { return e.find(text) != std::string::npos; });
        };
        TEST_CHECK(has("incrément") && has("{n}") && has("espace") && has("Préfixe « B »"), "Test 225: erreurs de paramètres détectées");
        bad = TopologySettings::defaults();
        bad.elements.format.pattern = "{g}";
        TEST_CHECK(!validateSettings(bad).empty(), "Test 225: {g} refusé pour les éléments");
        std::cout << "[PASS] Test 225: Paramètres de topologie (défauts, JSON, validation)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 226 : étiquettes et regroupement des coordonnées presque identiques
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(formatLabel({ "N", "{p}{n}", 3, 1, 1 }, "N", 7, "", 1) == "N007", "Test 226: N007");
        TEST_CHECK(formatLabel({ "", "{p}{l}-{n}", 2, 1, 1 }, "P", 3, "", 2) == "P2-03", "Test 226: jetons {l} et {n}");
        TEST_CHECK(formatLabel({ "", "{g}", 0, 1, 1 }, "", 1, "B3-2", 2) == "B3-2", "Test 226: jeton {g}");
        TEST_CHECK(isAutomaticLabel("N012", "N", TopologySettings::defaults().nodes.format, 12), "Test 226: étiquette historique = automatique");
        TEST_CHECK(!isAutomaticLabel("Pied gauche", "N", TopologySettings::defaults().nodes.format, 3), "Test 226: étiquette personnalisée détectée");
        TEST_CHECK(isAutomaticLabel("A1-2", "N", { "N", "{g}", 3, 1, 1 }, 3), "Test 226: repère de grille = automatique");

        const auto ranks = detail::clusterRanks({ { 1, 0.0 }, { 2, 0.0004 }, { 3, 5.0 }, { 4, 4.9995 }, { 5, 0.0 } }, 1e-3);
        TEST_CHECK(ranks.at(1) == ranks.at(2) && ranks.at(1) == ranks.at(5) && ranks.at(3) == ranks.at(4) && ranks.at(3) == ranks.at(1) + 1,
                   "Test 226: coordonnées à moins de la tolérance regroupées, sans égalité exacte");
        AxisOrder zyx;
        const auto order = detail::sortByAxes({ { 9, 0, 0, 0 }, { 3, 0, 0, 0 }, { 5, 0, 0, 0 } }, zyx, 1e-3);
        TEST_CHECK(order == std::vector<int>({ 3, 5, 9 }), "Test 226: points confondus → ordre déterministe par identifiant");
        std::cout << "[PASS] Test 226: Étiquettes et tolérance de regroupement" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 227 : stratégies par coordonnées sur un modèle plan (rangées, colonnes, sens, format)
    // -------------------------------------------------------------------------
    {
        std::vector<int> ids;
        Model m;
        planarGrid(m, ids); // ids 1..6 : (0,0) (5,0) (10,0) (0,4) (5,4) (10,4)
        NumberingInput in;

        auto run = [&](const TopologySettings& s) { return nodeLabels(computePreview(m, s, in)); };
        auto rows = run(withNodeStrategy("rows"));
        TEST_CHECK(rows[ids[0]] == "N001" && rows[ids[2]] == "N003" && rows[ids[3]] == "N004", "Test 227: rangées (X le long de Y=0 d'abord)");
        auto cols = run(withNodeStrategy("columns"));
        TEST_CHECK(cols[ids[0]] == "N001" && cols[ids[3]] == "N002" && cols[ids[1]] == "N003", "Test 227: colonnes (Y le long de X=0 d'abord)");
        TopologySettings xyz = withNodeStrategy("coordinates");
        AxisOrder::fromCode("XYZ", xyz.nodes.order);
        xyz.nodes.order.descending[0] = true; // X décroissant
        auto desc = run(xyz);
        TEST_CHECK(desc[ids[2]] == "N001" && desc[ids[5]] == "N002" && desc[ids[0]] == "N005", "Test 227: X décroissant");
        TopologySettings fmt = withNodeStrategy("rows");
        fmt.nodes.format = { "P", "{p}-{n}", 2, 100, 10 };
        auto f = run(fmt);
        TEST_CHECK(f[ids[0]] == "P-100" && f[ids[1]] == "P-110" && f[ids[5]] == "P-150", "Test 227: préfixe, départ 100, incrément 10, format");
        TEST_CHECK(computePreview(m, withNodeStrategy("layers"), in).errors.size() == 1, "Test 227: couches indisponibles sur un modèle plan");
        TEST_CHECK(nodeLabels(computePreview(m, withNodeStrategy("sequential"), in))[ids[4]] == "N005", "Test 227: séquentielle");

        Model empty;
        const auto pe = computePreview(empty, withNodeStrategy("rows"), in);
        TEST_CHECK(pe.valid() && pe.entries.empty() && pe.dimension == 0, "Test 227: modèle vide");
        std::cout << "[PASS] Test 227: Stratégies par coordonnées (2D)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 228 : modèles 3D (couches, redémarrage par couche, niveaux) et 1D
    // -------------------------------------------------------------------------
    {
        Model m;
        std::vector<int> ids;
        for (double z : { 0.0, 3.0 })
            for (double x : { 0.0, 6.0 }) ids.push_back(m.addNode(x, 0, z));
        NumberingInput in;
        TopologySettings s = withNodeStrategy("layers");
        s.nodes.restartPerLayer = true;
        s.nodes.format = { "N", "{p}{l}.{n}", 2, 1, 1 };
        const auto p = computePreview(m, s, in);
        const auto l = nodeLabels(p);
        TEST_CHECK(p.valid() && p.dimension == 2, "Test 228: portique plan vertical (dimension 2)");
        TEST_CHECK(l.at(ids[0]) == "N1.01" && l.at(ids[1]) == "N1.02" && l.at(ids[2]) == "N2.01" && l.at(ids[3]) == "N2.02",
                   "Test 228: couches + redémarrage par couche");
        const int top = m.addNode(3, 4, 6); // rend le modèle 3D
        TEST_CHECK(computePreview(m, s, in).dimension == 3, "Test 228: dimension 3D détectée");
        in.levelElevations = { 0.0, 3.0, 6.0 };
        TopologySettings lv = withNodeStrategy("level");
        lv.nodes.format = { "N", "{p}{l}-{n}", 1, 1, 1 };
        lv.nodes.restartPerLayer = true;
        const auto ll = nodeLabels(computePreview(m, lv, in));
        TEST_CHECK(ll.at(ids[0]) == "N1-1" && ll.at(ids[3]) == "N2-2" && ll.at(top) == "N3-1", "Test 228: numérotation par niveau");

        Model line;
        for (int i = 0; i < 4; ++i) line.addNode(i * 2.0, 0, 0);
        TEST_CHECK(computePreview(line, withNodeStrategy("xyz"), NumberingInput{}).dimension == 1, "Test 228: modèle 1D (aligné)");
        std::cout << "[PASS] Test 228: Modèles 1D, 2D, 3D, couches et niveaux" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 229 : grille d'axes (A1…), hors grille sans faux repère, couches
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a1 = m.addNode(0, 0, 0), a2 = m.addNode(6, 0, 0), b1 = m.addNode(0, 5, 0);
        const int off = m.addNode(3, 2.5, 0);       // milieu de travée : hors intersection
        const int near = m.addNode(6.0004, 5.0003, 0); // à moins de la tolérance (1 mm) de B2
        NumberingInput in;
        TopologySettings s = withNodeStrategy("grid");
        s.nodes.format.pattern = "{g}";
        TEST_CHECK(!computePreview(m, s, in).valid(), "Test 229: sans grille active → indisponible");
        in.grid = GridAxes{ 0, 0, 0, { 0, 6 }, { "1", "2" }, { 0, 5 }, { "A", "B" } };
        const auto p = computePreview(m, s, in);
        const auto l = nodeLabels(p);
        TEST_CHECK(p.valid() && l.at(a1) == "A1" && l.at(a2) == "A2" && l.at(b1) == "B1" && l.at(near) == "B2",
                   "Test 229: repères A1, A2, B1, B2 (tolérance)");
        TEST_CHECK(l.at(off) == "N005" && !p.warnings.empty(), "Test 229: nœud hors grille numéroté avec le préfixe, signalé");
        const int up = m.addNode(0, 0, 3);
        const auto l3 = nodeLabels(computePreview(m, s, in));
        TEST_CHECK(l3.at(a1) == "A1-1" && l3.at(up) == "A1-2", "Test 229: couche ajoutée au repère en 3D");
        const int dupe = m.addNode(0, 0, 3); // confondu avec « up »
        const auto pd = computePreview(m, s, in);
        const auto ld = nodeLabels(pd);
        TEST_CHECK(pd.valid() && ld.at(dupe) == "A1-2.2" &&
                       std::any_of(pd.warnings.begin(), pd.warnings.end(), [](const std::string& w) { return w.find("confondu") != std::string::npos; }),
                   "Test 229: nœuds confondus → suffixe .2, signalé, pas de doublon");
        GridAxes rot{ 10, 0, 90, { 0, 6 }, { "1", "2" }, { 0, 5 }, { "A", "B" } }; // rotation 90° autour de (10, 0)
        in.grid = rot;
        Model r;
        const int q = r.addNode(10, 6, 0); // local (6, 0) → A2
        TEST_CHECK(nodeLabels(computePreview(r, s, in)).at(q) == "A2", "Test 229: grille tournée");
        std::cout << "[PASS] Test 229: Numérotation par grille d'axes" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 230 : connectivité (BFS, DFS, Cuthill-McKee inverse), composantes multiples
    // -------------------------------------------------------------------------
    {
        Model m;
        // Chaîne 1-2-3-4 (barres) avec une branche 2-5, et une seconde composante 6-7.
        const int n1 = m.addNode(0, 0, 0), n2 = m.addNode(1, 0, 0), n3 = m.addNode(2, 0, 0), n4 = m.addNode(3, 0, 0);
        const int n5 = m.addNode(1, 1, 0), n6 = m.addNode(10, 0, 0), n7 = m.addNode(11, 0, 0);
        const int iso = m.addNode(20, 0, 0); // nœud isolé
        for (auto [a, b] : std::vector<std::pair<int, int>>{ { n1, n2 }, { n2, n3 }, { n3, n4 }, { n2, n5 }, { n6, n7 } })
            m.addBar(a, b, Section::ipe(200), Material::steelS235());
        NumberingInput in;
        for (const char* strat : { "bfs", "dfs", "rcm" })
        {
            const auto p = computePreview(m, withNodeStrategy(strat), in);
            std::set<std::string> labels;
            for (const auto& e : p.entries) labels.insert(e.newLabel);
            TEST_CHECK(p.valid() && p.entries.size() == 13 && labels.size() == 13, "Test 230: " << strat << " : chaque nœud une fois ("
                       << p.entries.size() << " entrées, " << (p.errors.empty() ? std::string() : p.errors.front()) << ")");
            TEST_CHECK(std::any_of(p.warnings.begin(), p.warnings.end(), [](const std::string& w) { return w.find("non reliées") != std::string::npos; }),
                       "Test 230: " << strat << " : composantes signalées");
        }
        auto bfs = nodeLabels(computePreview(m, withNodeStrategy("bfs"), in));
        TEST_CHECK(bfs[n1] == "N001" && bfs[n2] == "N002" && bfs[n3] == "N003" && bfs[n5] == "N004" && bfs[n4] == "N005" &&
                       bfs[n6] == "N006" && bfs[iso] == "N008",
                   "Test 230: BFS niveau par niveau, puis composantes suivantes");
        auto dfs = nodeLabels(computePreview(m, withNodeStrategy("dfs"), in));
        TEST_CHECK(dfs[n1] == "N001" && dfs[n2] == "N002" && dfs[n3] == "N003" && dfs[n4] == "N004" && dfs[n5] == "N005",
                   "Test 230: DFS en profondeur d'abord");
        TopologySettings start = withNodeStrategy("bfs");
        start.nodes.startNodeId = n4;
        auto fromN4 = nodeLabels(computePreview(m, start, in));
        TEST_CHECK(fromN4[n4] == "N001" && fromN4[n3] == "N002", "Test 230: nœud de départ choisi");
        Model loose;
        loose.addNode(0, 0, 0);
        TEST_CHECK(!computePreview(loose, withNodeStrategy("bfs"), in).valid(), "Test 230: sans élément → parcours indisponible");
        TEST_CHECK(!computePreview(m, withNodeStrategy("mesh_structured"), in).valid() &&
                       !computePreview(m, withNodeStrategy("group"), in).valid(),
                   "Test 230: stratégies sans prérequis (maillage, groupes) jamais disponibles");
        std::cout << "[PASS] Test 230: Connectivité BFS, DFS, Cuthill-McKee, composantes" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 231 : éléments — séquentielle, suite commune, proximité, topologique, orientation
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(10, 0, 0), b = m.addNode(0, 0, 0), c = m.addNode(0, 0, 3), d = m.addNode(10, 0, 3);
        const int beamFar = m.addBar(c, d, Section::ipe(200), Material::steelS235()); // centre x = 5
        const int colRole = m.addBar(a, d, Section::ipe(200), Material::steelS235(), BarRole::Column);
        const int col = m.addColumn(b, c, Section::rectangular(0.3, 0.3), Material::concreteC25_30());
        const int slab = m.addSlab({ a, b, c, d });
        NumberingInput in;
        TopologySettings s = TopologySettings::defaults();
        s.nodes.strategy = "keep";
        s.elements.preserveCustomNames = false;
        auto labelOf = [](const NumberingPreview& p, EntityFamily f, int id) {
            for (const auto& e : p.entries)
                if (e.family == f && e.id == id) return e.newLabel;
            return std::string("?");
        };
        auto p = computePreview(m, s, in);
        // Séquentielle (identifiant, famille) : poutre 1, poteau 1, barre 2 (rôle poteau). La barre de rôle
        // poteau prend le préfixe C et partage la suite des poteaux : pas de doublon C001.
        TEST_CHECK(p.valid() && labelOf(p, EntityFamily::Beam, beamFar) == "B001" && labelOf(p, EntityFamily::Column, col) == "C001" &&
                       labelOf(p, EntityFamily::Beam, colRole) == "C002",
                   "Test 231: barre de rôle poteau → préfixe C, suite partagée avec les poteaux");
        s.elements.sharedSequence = true;
        s.elements.strategy = "by_type";
        p = computePreview(m, s, in);
        TEST_CHECK(p.valid() && labelOf(p, EntityFamily::Beam, beamFar) == "B001" && labelOf(p, EntityFamily::Beam, colRole) == "C002" &&
                       labelOf(p, EntityFamily::Column, col) == "C003" && labelOf(p, EntityFamily::Slab, slab) == "S004",
                   "Test 231: suite commune par type");
        s.elements.strategy = "proximity";
        AxisOrder::fromCode("XZY", s.elements.order);
        p = computePreview(m, s, in);
        TEST_CHECK(labelOf(p, EntityFamily::Column, col) == "C001", "Test 231: proximité (centre le plus à gauche d'abord)");
        s.elements.strategy = "topological";
        s.nodes.strategy = "xyz";
        p = computePreview(m, s, in);
        TEST_CHECK(p.valid() && labelOf(p, EntityFamily::Column, col) == "C001", "Test 231: topologique (suit l'ordre des nœuds)");

        const auto before = fingerprint(m);
        TEST_CHECK(applyPreview(m, p, "renum"), "Test 231: application");
        TEST_CHECK(m.getBeam(colRole)->startNodeId() == a && m.getBeam(colRole)->endNodeId() == d && m.getColumn(col)->startNodeId() == b,
                   "Test 231: extrémités et orientation des barres conservées");
        TEST_CHECK(fingerprint(m) == before, "Test 231: géométrie, connectivités et propriétés inchangées");
        std::cout << "[PASS] Test 231: Numérotation des éléments" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 232 : doublons, étiquettes personnalisées, portée « sélection »
    // -------------------------------------------------------------------------
    {
        std::vector<int> ids;
        Model m;
        planarGrid(m, ids);
        m.getNode(ids[0])->setName("Pied gauche");
        NumberingInput in;
        TopologySettings s = withNodeStrategy("rows");
        s.nodes.preserveCustomNames = true;
        auto p = computePreview(m, s, in);
        auto l = nodeLabels(p);
        TEST_CHECK(p.valid() && l[ids[0]] == "Pied gauche" && l[ids[1]] == "N001", "Test 232: étiquette personnalisée conservée, numéros non consommés");
        m.getNode(ids[0])->setName("N003"); // personnalisée hors format ? non : N003 est automatique
        m.getNode(ids[5])->setName("X-1");
        s.nodes.format = { "X-", "{p}{n}", 0, 1, 1 };
        p = computePreview(m, s, in);
        // « X-1 » est désormais au format choisi : renumérotée ; aucune collision.
        TEST_CHECK(p.valid(), "Test 232: étiquette au format choisi traitée comme automatique");
        m.getNode(ids[5])->setName("X-1 ");
        s.nodes.format = { "X-", "{p}{n}", 0, 1, 1 };
        m.getNode(ids[4])->setName("X-1");
        s.scope = NumberingScope::Selection;
        in.selectedNodes = { ids[0], ids[1] };
        p = computePreview(m, s, in);
        TEST_CHECK(!p.valid() && p.errors.front().find("double") != std::string::npos,
                   "Test 232: collision avec une étiquette hors portée détectée");
        in.selectedNodes = { ids[1], ids[2] };
        s.nodes.format = { "S", "{p}{n}", 0, 1, 1 };
        p = computePreview(m, s, in);
        TEST_CHECK(p.valid() && p.entries.size() == 2 && nodeLabels(p).count(ids[3]) == 0, "Test 232: seuls les nœuds sélectionnés sont renumérotés");
        // Doublons préexistants hors portée et inchangés : pas d'erreur.
        m.getNode(ids[3])->setName("Dup");
        m.getNode(ids[4])->setName("Dup");
        TEST_CHECK(computePreview(m, s, in).valid(), "Test 232: doublons préexistants non modifiés tolérés");
        std::cout << "[PASS] Test 232: Doublons, conservation, portée sélection" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 233 : sécurité — aperçu sans effet, application atomique, résultats, Annuler, aperçu périmé
    // -------------------------------------------------------------------------
    {
        std::vector<int> ids;
        Model m;
        planarGrid(m, ids);
        m.addBar(ids[0], ids[1], Section::ipe(200), Material::steelS235());
        m.addBar(ids[1], ids[2], Section::ipe(200), Material::steelS235());
        m.getNode(ids[0])->setSupport(TSA::Model::SupportDefinition::fixed());
        m.loadManager().addNodalLoad(TSA::Model::NodalLoad(0, ids[2], 1, 0, 0, -10.0, 0, 0, 0, TSA::Model::LoadCoordSystem::Global, "P"));
        NumberingInput in;
        const auto before = fingerprint(m);
        const auto rev = m.revision();
        std::map<int, std::string> labelsBefore;
        for (const auto& [id, n] : m.nodes()) labelsBefore[id] = n.formattedName();

        TopologySettings s = withNodeStrategy("columns");
        s.elements.strategy = "sequential";
        s.elements.format.start = 50;
        const auto p = computePreview(m, s, in);
        bool unchanged = true;
        for (const auto& [id, n] : m.nodes()) unchanged = unchanged && labelsBefore[id] == n.formattedName();
        TEST_CHECK(p.valid() && unchanged && m.revision() == rev, "Test 233: la prévisualisation ne modifie pas le modèle");

        const bool couldUndo = m.canUndo();
        TEST_CHECK(applyPreview(m, p, "renum") && m.getNode(ids[3])->formattedName() == "N002", "Test 233: application");
        TEST_CHECK(fingerprint(m) == before, "Test 233: coordonnées, appuis, charges, connectivités, matériaux inchangés");
        TEST_CHECK(m.revision() == rev && m.isModified(), "Test 233: révision inchangée (résultats conservés), document modifié");
        TEST_CHECK(m.getBeam(1)->formattedName() == "B050", "Test 233: éléments renumérotés (départ 50)");
        TEST_CHECK(m.canUndo() && (!couldUndo || true) && m.undo() && m.getNode(ids[3])->formattedName() == labelsBefore[ids[3]],
                   "Test 233: Annuler rétablit les étiquettes");
        // Après Annuler, le modèle correspond de nouveau à l'aperçu ; une modification ultérieure le rend périmé.
        m.getNode(ids[4])->setName("Modifié à la main");
        std::string err;
        TEST_CHECK(!applyPreview(m, p, "renum", &err) && m.getNode(ids[3])->formattedName() == labelsBefore[ids[3]],
                   "Test 233: aperçu périmé refusé, modèle inchangé");
        TEST_CHECK(err.find("prévisualisez") != std::string::npos, "Test 233: message de prévisualisation périmée");
        NumberingPreview invalid;
        invalid.errors.push_back("x");
        TEST_CHECK(!applyPreview(m, invalid, "renum"), "Test 233: aperçu invalide refusé");
        std::cout << "[PASS] Test 233: Prévisualisation sans effet, application atomique, Annuler" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 234 : persistance — chunk TOPO, rechargement, anciens fichiers
    // -------------------------------------------------------------------------
    {
        std::vector<int> ids;
        Model m;
        planarGrid(m, ids);
        TopologySettings s = withNodeStrategy("rows");
        s.nodes.format = { "P", "{p}{n}", 2, 1, 1 };
        TEST_CHECK(applyPreview(m, computePreview(m, s, NumberingInput{}), "renum"), "Test 234: renumérotation");
        m.setTopologySettingsJson(s.toJson());
        const std::string path = "test_topology_settings.tsa";
        TSA::IO::TSAFileWriter writer;
        std::string err;
        TEST_CHECK(writer.saveToFile(path, m, nullptr, "Topo", "TSA Testing", &err), "Test 234: sauvegarde");
        TSA::IO::TSAFileHeader header;
        TEST_CHECK(TSA::IO::TSAFileReader::readHeader(path, header) && header.versionMinor == TSA::IO::TSA_FORMAT_VERSION_MINOR &&
                       TSA::IO::TSA_FORMAT_VERSION_MINOR >= 5,
                   "Test 234: format ≥ 1.5");
        Model loaded;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, loaded, nullptr, "", nullptr, nullptr, nullptr, &err), "Test 234: rechargement");
        const TopologySettings r = TopologySettings::fromJson(loaded.topologySettingsJson());
        TEST_CHECK(r.nodes.strategy == "rows" && r.nodes.format.prefix == "P" && loaded.getNode(ids[0])->formattedName() == "P01",
                   "Test 234: paramètres et étiquettes rechargés");

        // Ancien projet : sans chunk TOPO → paramètres vides → valeurs par défaut, calcul possible.
        Model old;
        old.addNode(0, 0, 0);
        const std::string oldPath = "test_topology_old.tsa";
        TEST_CHECK(writer.saveToFile(oldPath, old, nullptr, "Ancien", "TSA Testing", &err), "Test 234: projet sans paramètres");
        Model oldLoaded;
        TEST_CHECK(reader.loadFromFile(oldPath, oldLoaded, nullptr, "", nullptr, nullptr, nullptr, &err) && oldLoaded.topologySettingsJson().empty() &&
                       TopologySettings::fromJson(oldLoaded.topologySettingsJson()).nodes.strategy == "sequential",
                   "Test 234: ancien projet ouvert avec les valeurs par défaut");
        oldLoaded.clear();
        TEST_CHECK(oldLoaded.topologySettingsJson().empty(), "Test 234: nouveau projet sans paramètres hérités");
        std::filesystem::remove(path);
        std::filesystem::remove(oldPath);
        std::cout << "[PASS] Test 234: Persistance (chunk TOPO, anciens projets)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 235 : correspondance solveur — tags indépendants des étiquettes
    // -------------------------------------------------------------------------
    {
        Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0), c = m.addNode(5, 0, 3);
        const int beam = m.addBar(a, b, Section::ipe(200), Material::steelS235());
        const int col = m.addColumn(b, c, Section::rectangular(0.3, 0.3), Material::concreteC25_30());
        m.addSlab({ a, b, c });
        const auto snapBefore = TSA::Analysis::CalculationSnapshot::capture(m);
        const auto rows = solverMapping(m);
        auto tagOf = [&](EntityFamily f, int id) {
            for (const auto& r : rows)
                if (r.family == f && r.id == id) return r.solverTag;
            return -2;
        };
        int beamTag = -1;
        for (const auto& [tag, e] : snapBefore.elements())
            if (e.type == TSA::Analysis::StructuralElementKind::Beam && e.id == beam) beamTag = tag;
        TEST_CHECK(tagOf(EntityFamily::Node, b) == b && tagOf(EntityFamily::Beam, beam) == beamTag && tagOf(EntityFamily::Slab, 1) == -1 &&
                       tagOf(EntityFamily::Column, col) > 0,
                   "Test 235: tags (nœud = id interne, éléments = tags du snapshot, dalle non transmise)");
        TopologySettings s = withNodeStrategy("xyz");
        s.nodes.format.prefix = "Q";
        s.elements.strategy = "proximity";
        TEST_CHECK(applyPreview(m, computePreview(m, s, NumberingInput{}), "renum"), "Test 235: renumérotation");
        const auto snapAfter = TSA::Analysis::CalculationSnapshot::capture(m);
        bool same = snapAfter.elements().size() == snapBefore.elements().size();
        for (const auto& [tag, e] : snapAfter.elements())
        {
            const auto* prev = snapBefore.getElementByTag(tag);
            same = same && prev && prev->id == e.id && prev->type == e.type && prev->startNodeId == e.startNodeId && prev->endNodeId == e.endNodeId;
        }
        TEST_CHECK(same, "Test 235: export solveur identique après renumérotation");
        std::cout << "[PASS] Test 235: Correspondance identifiant ↔ étiquette ↔ solveur" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 236 : fenêtre Qt — ouvrir, modifier, prévisualiser, appliquer, indisponibles
    // -------------------------------------------------------------------------
    {
        std::vector<int> ids;
        Model m;
        planarGrid(m, ids);
        TSA::UI::TopologyDialog dlg(&m, [] { return NumberingInput{}; });
        auto* combo = dlg.findChild<QComboBox*>(QStringLiteral("nodeStrategyCombo"));
        TEST_CHECK(combo != nullptr, "Test 236: fenêtre ouverte, liste des stratégies");
        auto* model = qobject_cast<QStandardItemModel*>(combo->model());
        const int gridIdx = combo->findData(QStringLiteral("grid"));
        const int meshIdx = combo->findData(QStringLiteral("mesh_structured"));
        TEST_CHECK(model && !model->item(gridIdx)->isEnabled() && !model->item(meshIdx)->isEnabled() &&
                       model->item(gridIdx)->toolTip().contains(QStringLiteral("grille")),
                   "Test 236: stratégies indisponibles désactivées, raison en infobulle");
        combo->setCurrentIndex(combo->findData(QStringLiteral("columns")));
        TEST_CHECK(dlg.preview(), "Test 236: prévisualisation");
        auto* table = dlg.findChild<QTableWidget*>(QStringLiteral("previewTable"));
        TEST_CHECK(table && table->rowCount() == 6 && m.getNode(ids[3])->formattedName() == "N004", "Test 236: aperçu affiché, modèle intact");
        TEST_CHECK(dlg.apply(), "Test 236: application");
        TEST_CHECK(m.getNode(ids[3])->formattedName() == "N002" &&
                       TopologySettings::fromJson(m.topologySettingsJson()).nodes.strategy == "columns",
                   "Test 236: étiquettes appliquées et paramètres enregistrés dans le projet");
        TSA::UI::TopologyDialog reopened(&m, [] { return NumberingInput{}; });
        auto* combo2 = reopened.findChild<QComboBox*>(QStringLiteral("nodeStrategyCombo"));
        TEST_CHECK(combo2->currentData().toString() == QStringLiteral("columns"), "Test 236: réouverture avec les paramètres du projet");
        reopened.resetToDefaults();
        TEST_CHECK(combo2->currentData().toString() == QStringLiteral("sequential"), "Test 236: réinitialisation");
        reopened.restorePrevious();
        TEST_CHECK(combo2->currentData().toString() == QStringLiteral("columns"), "Test 236: restauration de la configuration précédente");
        std::cout << "[PASS] Test 236: Fenêtre Topologie et numérotation" << std::endl;
        ++passed;
    }
    return true;
}
