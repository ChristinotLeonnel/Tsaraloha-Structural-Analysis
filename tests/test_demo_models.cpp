// Suite « demo » : modèles de démonstration construits avec l'API réelle de TSA (Model, Section, Material,
// GridManager, TSAFileWriter) et contrôlés.
// Tests 347-349 — trois modèles filaires (barres seules, poids propre) :
//   A. pont en treillis Warren à montants, trois travées continues de 48 m, piles en palées métalliques, culées ;
//   B. hangar métallique 60 × 40 m : 11 portiques à fermes triangulées, pannes, lisses, contreventements, portes ;
//   C. bâtiment R+5 à ossature métallique : trame 5 × 3, poutres principales et secondaires, palées de stabilité,
//      escalier à volées et paliers, atrium, contreventement de toiture, édicule.
// Tests 350-353 — quatre projets complets (dalles, voiles, semelles, câbles, cas de charges, combinaisons) :
//   1. bâtiment en béton armé R+4 ; 2. bâtiment mixte béton armé + acier ; 3. bâtiment complexe en L ;
//   4. pont à haubans 80 + 180 + 80 m.
// Contrôles : aucun nœud dupliqué, aucune barre dupliquée, superposée ou de longueur nulle, aucun nœud isolé, aucun
// nœud posé à l'intérieur d'une barre sans y être connecté, structure d'un seul tenant, appuis présents, calcul
// OpenSees réel (stabilité ; pour les projets : Σ réactions = Σ q·L sous Q, équilibre ELU, haubans tendus,
// contrôle analytique N = T0), enregistrement .tsa et relecture identique, notes de calcul PDF.
// Avec TSA_DEMO_DIR=<dossier>, les projets sont écrits dans ce dossier (vignette rendue par la vue 3D de TSA) et
// les notes de calcul dans <dossier>/notes.

#include "test_common.h"

#include "Analysis/OpenSeesSolver.h"
#include "Geometry/DiagramGeometry.h"
#include "Grid/GridDefinition.h"
#include "Grid/GridManager.h"
#include "IO/TSAFile.h"
#include "Model/Cable/Cable.h"
#include "Model/Cable/CableDefinition.h"
#include "Model/Load/LoadManager.h"
#include "NDC/ReportManager.h"
#include "Viewer/OccView.h"
#include "Viewer/ResultsVisualManager.h"

#include <QFileInfo>
#include <QThread>
#include <QTemporaryDir>
#include <iomanip>

#include <array>
#include <map>
#include <queue>
#include <set>

using namespace TSA::Analysis;

namespace
{
enum class Kind
{
    Column,   ///< élément vertical porteur (Column)
    Beam,     ///< poutre (BarRole::Beam)
    Truss,    ///< membrure, montant ou diagonale de treillis (BarRole::Truss)
    Brace     ///< contreventement (BarRole::Brace)
};

/// Construction d'un modèle filaire : nœuds uniques (coordonnées au millimètre), segments découpés à chaque nœud
/// situé sur eux (connexions garanties), paires de nœuds dédupliquées.
class WireBuilder
{
public:
    explicit WireBuilder(Model& model)
        : m_model(model)
    {
    }

    int node(double x, double y, double z)
    {
        const auto key = std::make_tuple(std::llround(x * 1000), std::llround(y * 1000), std::llround(z * 1000));
        if (auto it = m_nodes.find(key); it != m_nodes.end()) return it->second;
        const int id = m_model.addNode(x, y, z);
        m_nodes[key] = id;
        m_points[id] = gp_Pnt(x, y, z);
        return id;
    }

    void bar(const gp_Pnt& a, const gp_Pnt& b, Kind kind, const Section& section, const Material& material, const std::string& name)
    {
        m_segments.push_back({ node(a.X(), a.Y(), a.Z()), node(b.X(), b.Y(), b.Z()), kind, section, material, name });
    }

    /// Croix de Saint-André connectée en son centre : diagonales p1-p3 et p2-p4.
    void xBrace(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3, const gp_Pnt& p4, const Section& s, const Material& m, const std::string& name)
    {
        const gp_Pnt c((p1.X() + p3.X()) / 2, (p1.Y() + p3.Y()) / 2, (p1.Z() + p3.Z()) / 2);
        bar(p1, c, Kind::Brace, s, m, name);
        bar(c, p3, Kind::Brace, s, m, name);
        bar(p2, c, Kind::Brace, s, m, name);
        bar(c, p4, Kind::Brace, s, m, name);
    }

    void support(double x, double y, double z, const SupportDefinition& def) { m_model.getNode(node(x, y, z))->setSupport(def); }

    /// Câble (élément Cable, jamais découpé) entre deux nœuds, tension initiale T0 en N.
    void cable(const gp_Pnt& a, const gp_Pnt& b, const TSA::Model::CableDefinition& def, double tensionN, const std::string& name)
    {
        m_cables.push_back({ node(a.X(), a.Y(), a.Z()), node(b.X(), b.Y(), b.Z()), def, tensionN, name });
    }

    /// Crée les barres : chaque segment est découpé aux nœuds qui se trouvent sur lui ; une paire déjà créée
    /// (segments superposés) n'est créée qu'une fois.
    void finalize()
    {
        std::set<std::pair<int, int>> done;
        for (const auto& s : m_segments)
        {
            const gp_Pnt A = m_points[s.a], B = m_points[s.b];
            const gp_Vec AB(A, B);
            const double L2 = AB.SquareMagnitude();
            std::vector<std::pair<double, int>> chain { { 0.0, s.a }, { 1.0, s.b } };
            for (const auto& [id, p] : m_points)
            {
                if (id == s.a || id == s.b) continue;
                const gp_Vec AP(A, p);
                const double t = AP.Dot(AB) / L2;
                if (t <= 1e-9 || t >= 1.0 - 1e-9) continue;
                if (AP.Crossed(AB).Magnitude() / std::sqrt(L2) < 1e-4) chain.push_back({ t, id });
            }
            std::sort(chain.begin(), chain.end());
            for (std::size_t k = 0; k + 1 < chain.size(); ++k)
            {
                const int a = chain[k].second, b = chain[k + 1].second;
                if (!done.insert({ std::min(a, b), std::max(a, b) }).second) continue;
                if (s.kind == Kind::Column)
                {
                    // Poteau orienté de bas en haut
                    const bool up = m_points[a].Z() <= m_points[b].Z();
                    m_model.addColumn(up ? a : b, up ? b : a, s.section, s.material, 0.0, s.name);
                }
                else
                {
                    const BarRole role = s.kind == Kind::Truss ? BarRole::Truss : s.kind == Kind::Brace ? BarRole::Brace : BarRole::Beam;
                    m_model.addBar(a, b, s.section, s.material, role, 0.0, s.name);
                }
            }
        }
        m_segments.clear();
        for (const auto& c : m_cables)
        {
            const int id = m_model.addCable(c.a, c.b, c.def, c.name);
            if (auto* cb = m_model.getCable(id)) cb->setInitialTension(c.tension);
        }
        m_cables.clear();
    }

private:
    struct Segment
    {
        int a, b;
        Kind kind;
        Section section;
        Material material;
        std::string name;
    };
    struct CableSegment
    {
        int a, b;
        TSA::Model::CableDefinition def;
        double tension;
        std::string name;
    };
    Model& m_model;
    std::vector<CableSegment> m_cables;
    std::map<std::tuple<long long, long long, long long>, int> m_nodes;
    std::map<int, gp_Pnt> m_points;
    std::vector<Segment> m_segments;
};

gp_Pnt P(double x, double y, double z) { return gp_Pnt(x, y, z); }

std::vector<double> range(double start, double step, int count)
{
    std::vector<double> v;
    for (int i = 0; i < count; ++i) v.push_back(start + step * i);
    return v;
}

// -------------------------------------------------------------------------------------------------------------
// A. Pont en treillis Warren à montants, 3 travées continues de 48 m (panneaux de 6 m), largeur 10 m, hauteur 7 m
// -------------------------------------------------------------------------------------------------------------
void buildBridge(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material steel = Material::steelS355();
    const Material concrete = Material::reinforcedConcrete();
    const double panel = 6.0, B = 10.0, H = 7.0;
    const int n = 24; // 3 × 48 m
    const Section chord = Section::heb(300), diag = Section::hea(240), post = Section::hea(200);
    const Section floorBeam = Section::ipe(400), stringer = Section::ipe(300), strut = Section::hea(160);
    const Section lateral = Section::angle(0.12, 0.12, 0.012, "L 120x120x12");
    const Section pierLeg = Section::pipe(0.9, 0.02, "Tube 900x20"), pierBrace = Section::pipe(0.32, 0.01, "Tube 323x10");

    for (double y : { 0.0, B })
    {
        const std::string side = y == 0.0 ? "amont" : "aval";
        for (int i = 0; i < n; ++i)
        {
            const double x0 = i * panel, x1 = x0 + panel;
            w.bar(P(x0, y, 0), P(x1, y, 0), Kind::Truss, chord, steel, "Membrure inférieure " + side);
            w.bar(P(x0, y, H), P(x1, y, H), Kind::Truss, chord, steel, "Membrure supérieure " + side);
            // Warren : diagonales alternées
            if (i % 2 == 0) w.bar(P(x0, y, 0), P(x1, y, H), Kind::Truss, diag, steel, "Diagonale " + side);
            else w.bar(P(x0, y, H), P(x1, y, 0), Kind::Truss, diag, steel, "Diagonale " + side);
        }
        for (int i = 0; i <= n; ++i) w.bar(P(i * panel, y, 0), P(i * panel, y, H), Kind::Column, post, steel, "Montant " + side);
    }
    for (int i = 0; i <= n; ++i)
    {
        const double x = i * panel;
        w.bar(P(x, 0, 0), P(x, B, 0), Kind::Beam, floorBeam, steel, "Pièce de pont");
        if (i > 0 && i < n) w.bar(P(x, 0, H), P(x, B, H), Kind::Beam, strut, steel, "Entretoise supérieure");
    }
    for (int i = 0; i < n; ++i)
    {
        const double x0 = i * panel, x1 = x0 + panel;
        for (double y : { 2.5, 5.0, 7.5 }) w.bar(P(x0, y, 0), P(x1, y, 0), Kind::Beam, stringer, steel, "Longeron");
        w.xBrace(P(x0, 0, 0), P(x0, B, 0), P(x1, B, 0), P(x1, 0, 0), lateral, steel, "Contreventement inférieur");
        w.xBrace(P(x0, 0, H), P(x0, B, H), P(x1, B, H), P(x1, 0, H), lateral, steel, "Contreventement supérieur");
    }
    // Portails d'extrémité : entretoise haute et goussets
    for (double x : { 0.0, n * panel })
    {
        w.bar(P(x, 0, H), P(x, B, H), Kind::Beam, strut, steel, "Portail d'extrémité");
        w.bar(P(x, 0, H - 2.0), P(x, 2.0, H), Kind::Brace, lateral, steel, "Gousset de portail");
        w.bar(P(x, B, H - 2.0), P(x, B - 2.0, H), Kind::Brace, lateral, steel, "Gousset de portail");
    }
    // Piles P1, P2 : palées de deux fûts tubulaires contreventés, hauteur 14 m
    for (double x : { 48.0, 96.0 })
    {
        for (double y : { 0.0, B })
        {
            w.bar(P(x, y, -14), P(x, y, 0), Kind::Column, pierLeg, steel, "Fût de pile");
            w.support(x, y, -14, SupportDefinition::fixed());
        }
        w.bar(P(x, 0, -7), P(x, B, -7), Kind::Beam, pierBrace, steel, "Traverse de pile");
        w.bar(P(x, 0, -14), P(x, B, -14), Kind::Beam, pierBrace, steel, "Longrine de pile");
        w.xBrace(P(x, 0, -14), P(x, B, -14), P(x, B, -7), P(x, 0, -7), pierBrace, steel, "Contreventement de pile");
        w.xBrace(P(x, 0, -7), P(x, B, -7), P(x, B, 0), P(x, 0, 0), pierBrace, steel, "Contreventement de pile");
    }
    // Culées C0, C3 : voile de poteaux béton sur semelle filante (barres)
    for (double x : { 0.0, n * panel })
    {
        for (double y : { 0.0, 2.5, 5.0, 7.5, B })
        {
            w.bar(P(x, y, -3), P(x, y, 0), Kind::Column, Section::rectangular(0.8, 0.8, "Poteau 80x80"), concrete, "Poteau de culée");
            w.support(x, y, -3, SupportDefinition::fixed());
        }
        w.bar(P(x, 0, -3), P(x, B, -3), Kind::Beam, Section::rectangular(1.2, 1.0, "Semelle 120x100"), concrete, "Semelle de culée");
    }
    w.finalize();

    grid.setXPositions({ 0.0, 48.0, 96.0, 144.0 });
    grid.setXLabels({ "C0", "P1", "P2", "C3" });
    grid.setYPositions({ 0.0, B });
    grid.setYLabels({ "Amont", "Aval" });
    grid.setZLevels({ -14.0, 0.0, H });
    grid.setZLabels({ "Fondations", "Tablier", "Membrure sup." });
}

// -------------------------------------------------------------------------------------------------------------
// B. Hangar 60 × 40 m : 11 portiques tous les 6 m, fermes triangulées (panneaux de 2,5 m), faîtage à 14 m
// -------------------------------------------------------------------------------------------------------------
double roofZ(double y) { return 10.5 + 3.5 * (1.0 - std::abs(y - 20.0) / 20.0); }

void buildHangar(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material steel = Material::steelS355();
    const double W = 40.0, Lbay = 6.0, zb = 9.0, ze = 10.5;
    const int bays = 10, panels = 16;
    const Section column = Section::heb(300), chord = Section::hea(200), web = Section::angle(0.1, 0.1, 0.01, "L 100x100x10");
    const Section purlin = Section::ipe(200), girt = Section::upn(160), brace = Section::pipe(0.114, 0.006, "Tube 114x6");
    const Section gablePost = Section::hea(220), tie = Section::pipe(0.0889, 0.005, "Tube 89x5");

    for (int f = 0; f <= bays; ++f)
    {
        const double x = f * Lbay;
        const std::string frame = "Portique " + std::to_string(f + 1);
        for (double y : { 0.0, W })
        {
            w.bar(P(x, y, 0), P(x, y, ze), Kind::Column, column, steel, frame + " — poteau");
            w.support(x, y, 0, SupportDefinition::fixed());
        }
        for (int j = 0; j < panels; ++j)
        {
            const double y0 = j * 2.5, y1 = y0 + 2.5;
            w.bar(P(x, y0, roofZ(y0)), P(x, y1, roofZ(y1)), Kind::Truss, chord, steel, frame + " — membrure supérieure");
            w.bar(P(x, y0, zb), P(x, y1, zb), Kind::Truss, chord, steel, frame + " — membrure inférieure");
            // Pratt : diagonales descendant vers le faîtage
            if (y1 <= 20.0) w.bar(P(x, y0, zb), P(x, y1, roofZ(y1)), Kind::Truss, web, steel, frame + " — diagonale");
            else w.bar(P(x, y0, roofZ(y0)), P(x, y1, zb), Kind::Truss, web, steel, frame + " — diagonale");
        }
        for (int j = 1; j < panels; ++j) w.bar(P(x, j * 2.5, zb), P(x, j * 2.5, roofZ(j * 2.5)), Kind::Truss, web, steel, frame + " — montant");
    }
    for (int f = 0; f < bays; ++f)
    {
        const double x0 = f * Lbay, x1 = x0 + Lbay;
        for (int j = 0; j <= panels; ++j)
        {
            const double y = j * 2.5;
            w.bar(P(x0, y, roofZ(y)), P(x1, y, roofZ(y)), Kind::Beam, purlin, steel, j == 0 || j == panels ? "Sablière" : "Panne");
        }
        w.bar(P(x0, 20, zb), P(x1, 20, zb), Kind::Beam, tie, steel, "Lierne de membrure inférieure");
        for (double y : { 0.0, W })
            for (double z : { 2.5, 5.0, 7.5 })
            {
                // Porte de long-pan (y = 0, x de 24 à 36 m) : pas de lisse basse, la lisse à 5 m sert de linteau
                if (y == 0.0 && z == 2.5 && x0 >= 24.0 && x1 <= 36.0) continue;
                w.bar(P(x0, y, z), P(x1, y, z), Kind::Beam, girt, steel, y == 0.0 ? "Lisse long-pan A" : "Lisse long-pan C");
            }
    }
    // Pignons : potelets aux nœuds de ferme (y = 10, 20, 30) et lisses ; grande porte au pignon x = 0 (y de 10 à 30 m)
    for (double x : { 0.0, bays * Lbay })
    {
        const bool door = x == 0.0;
        for (double y : { 10.0, 20.0, 30.0 })
        {
            const double z0 = door && y == 20.0 ? 7.5 : 0.0;
            w.bar(P(x, y, z0), P(x, y, zb), Kind::Column, gablePost, steel, "Potelet de pignon");
            if (z0 == 0.0) w.support(x, y, 0, SupportDefinition::pinned());
        }
        const std::vector<double> ys { 0.0, 10.0, 20.0, 30.0, W };
        for (std::size_t k = 0; k + 1 < ys.size(); ++k)
            for (double z : { 2.5, 5.0, 7.5 })
            {
                if (door && z < 7.5 && ys[k] >= 10.0 && ys[k + 1] <= 30.0) continue; // porte 20 × 7,5 m
                w.bar(P(x, ys[k], z), P(x, ys[k + 1], z), Kind::Beam, girt, steel, "Lisse de pignon");
            }
    }
    // Contreventements dans les travées d'extrémité : long-pans (croix sur deux panneaux) et toiture
    for (double x0 : { 0.0, (bays - 1) * Lbay })
    {
        const double x1 = x0 + Lbay;
        for (double y : { 0.0, W })
        {
            w.xBrace(P(x0, y, 0), P(x1, y, 0), P(x1, y, 5), P(x0, y, 5), brace, steel, "Palée de long-pan");
            w.xBrace(P(x0, y, 5), P(x1, y, 5), P(x1, y, 10), P(x0, y, 10), brace, steel, "Palée de long-pan");
        }
        for (int j : { 0, 2, 4, 6, 8, 10, 12, 14 })
        {
            const double y0 = j * 2.5, y2 = y0 + 5.0;
            w.xBrace(P(x0, y0, roofZ(y0)), P(x1, y0, roofZ(y0)), P(x1, y2, roofZ(y2)), P(x0, y2, roofZ(y2)), brace, steel, "Poutre au vent de toiture");
        }
    }
    w.finalize();

    grid.setXPositions(range(0.0, Lbay, bays + 1));
    grid.setXLabels({ "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11" });
    grid.setYPositions({ 0.0, 20.0, W });
    grid.setYLabels({ "A", "B", "C" });
    grid.setZLevels({ 0.0, zb, ze, 14.0 });
    grid.setZLabels({ "Sol", "Entrait", "Sablière", "Faîtage" });
}

// -------------------------------------------------------------------------------------------------------------
// C. Bâtiment R+5 : trame 6 m (A-F) × 7 m (1-4), RDC 4,5 m puis 3,5 m, ossature métallique
// -------------------------------------------------------------------------------------------------------------
void buildBuilding(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material steel = Material::steelS355();
    const std::vector<double> xs { 0, 6, 12, 18, 24, 30 }, ys { 0, 7, 14, 21 };
    const std::vector<double> zs { 0.0, 4.5, 8.0, 11.5, 15.0, 18.5, 22.0 };
    const std::vector<std::string> levels { "RDC", "R+1", "R+2", "R+3", "R+4", "R+5", "Toiture" };
    const Section colLow = Section::heb(300), colHigh = Section::heb(240), girderX = Section::ipe(400), girderY = Section::ipe(360);
    const Section joist = Section::ipe(270), brace = Section::pipe(0.1683, 0.008, "Tube 168x8"), stair = Section::upn(200);
    const Section roofBrace = Section::angle(0.1, 0.1, 0.01, "L 100x100x10");

    for (std::size_t k = 0; k + 1 < zs.size(); ++k)
        for (double x : xs)
            for (double y : ys) w.bar(P(x, y, zs[k]), P(x, y, zs[k + 1]), Kind::Column, k < 3 ? colLow : colHigh, steel, "Poteau " + levels[k]);
    for (double x : xs)
        for (double y : ys) w.support(x, y, 0, SupportDefinition::fixed());

    auto inAtrium = [](double x0, double y, std::size_t k) { return x0 == 6.0 && y == 3.5 && (k == 1 || k == 2); };
    auto inStairWell = [](double x0, double y) { return x0 == 18.0 && y == 10.5; };
    for (std::size_t k = 1; k < zs.size(); ++k)
    {
        const double z = zs[k];
        for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        {
            for (double y : ys) w.bar(P(xs[i], y, z), P(xs[i + 1], y, z), Kind::Beam, girderX, steel, "Poutre principale " + levels[k]);
            for (double y : { 3.5, 10.5, 17.5 })
            {
                if (inAtrium(xs[i], y, k) || inStairWell(xs[i], y)) continue; // trémies : atrium double hauteur, cage d'escalier
                w.bar(P(xs[i], y, z), P(xs[i + 1], y, z), Kind::Beam, joist, steel, "Solive " + levels[k]);
            }
        }
        for (double x : xs)
            for (std::size_t j = 0; j + 1 < ys.size(); ++j) w.bar(P(x, ys[j], z), P(x, ys[j + 1], z), Kind::Beam, girderY, steel, "Poutre " + levels[k]);
    }
    // Palées de stabilité : façades 1 et 4 (travée C-D), pignons A et F (travée 2-3), sur toute la hauteur
    for (std::size_t k = 0; k + 1 < zs.size(); ++k)
    {
        const double z0 = zs[k], z1 = zs[k + 1];
        for (double y : { 0.0, 21.0 }) w.xBrace(P(12, y, z0), P(18, y, z0), P(18, y, z1), P(12, y, z1), brace, steel, "Palée de façade");
        for (double x : { 0.0, 30.0 }) w.xBrace(P(x, 7, z0), P(x, 14, z0), P(x, 14, z1), P(x, 7, z1), brace, steel, "Palée de pignon");
    }
    // Escalier (cage D-E / 2-3) : deux volées par niveau, palier intermédiaire contre l'axe E
    for (std::size_t k = 0; k + 2 < zs.size(); ++k)
    {
        const double z0 = zs[k], z1 = zs[k + 1], zm = (z0 + z1) / 2;
        w.bar(P(24, 7, zm), P(24, 14, zm), Kind::Beam, stair, steel, "Poutre palière");
        w.bar(P(22.5, 7, zm), P(22.5, 14, zm), Kind::Beam, stair, steel, "Rive de palier");
        for (double y : { 7.0, 14.0 }) w.bar(P(22.5, y, zm), P(24, y, zm), Kind::Beam, stair, steel, "Rive de palier");
        if (k > 0) w.bar(P(19.5, 7, z0), P(19.5, 14, z0), Kind::Beam, stair, steel, "Chevêtre de trémie");
        w.bar(P(19.5, 7, z1), P(19.5, 14, z1), Kind::Beam, stair, steel, "Chevêtre de trémie");
        for (double y : { 8.0, 9.5 }) w.bar(P(19.5, y, z0), P(22.5, y, zm), Kind::Beam, stair, steel, "Limon de volée");
        for (double y : { 11.5, 13.0 }) w.bar(P(22.5, y, zm), P(19.5, y, z1), Kind::Beam, stair, steel, "Limon de volée");
        if (k == 0)
        {
            // Départ de l'escalier au rez-de-chaussée : limons appuyés au sol
            w.bar(P(19.5, 8.0, 0), P(19.5, 9.5, 0), Kind::Beam, stair, steel, "Marche de départ");
            for (double y : { 8.0, 9.5 }) w.support(19.5, y, 0, SupportDefinition::pinned());
        }
    }
    // Toiture : poutres au vent horizontales dans les angles, édicule d'accès au-dessus de la cage d'escalier
    const double zr = zs.back();
    for (auto [x0, y0] : std::vector<std::pair<double, double>> { { 0, 0 }, { 24, 0 }, { 0, 14 }, { 24, 14 } })
        w.xBrace(P(x0, y0, zr), P(x0 + 6, y0, zr), P(x0 + 6, y0 + 7, zr), P(x0, y0 + 7, zr), roofBrace, steel, "Contreventement de toiture");
    const double zt = zr + 3.2;
    for (auto [x, y] : std::vector<std::pair<double, double>> { { 18, 7 }, { 24, 7 }, { 18, 14 }, { 24, 14 } })
        w.bar(P(x, y, zr), P(x, y, zt), Kind::Column, colHigh, steel, "Poteau d'édicule");
    w.bar(P(18, 7, zt), P(24, 7, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.bar(P(18, 14, zt), P(24, 14, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.bar(P(18, 7, zt), P(18, 14, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.bar(P(24, 7, zt), P(24, 14, zt), Kind::Beam, joist, steel, "Poutre d'édicule");
    w.xBrace(P(18, 7, zr), P(24, 7, zr), P(24, 7, zt), P(18, 7, zt), roofBrace, steel, "Contreventement d'édicule");
    w.xBrace(P(18, 7, zr), P(18, 14, zr), P(18, 14, zt), P(18, 7, zt), roofBrace, steel, "Contreventement d'édicule");
    w.finalize();

    grid.setXPositions(xs);
    grid.setXLabels({ "A", "B", "C", "D", "E", "F" });
    grid.setYPositions(ys);
    grid.setYLabels({ "1", "2", "3", "4" });
    grid.setZLevels(zs);
    grid.setZLabels(levels);
}

// =============================================================================================================
// Projets de démonstration complets (tests 350-353) : éléments porteurs, dalles, voiles, semelles, câbles, charges.
// Hypothèses d'exemple (non destinées à la construction) : béton C30/37, acier S355, haubans PSS 37T15,7 ;
// G planchers = poids de la dalle 25 kN/m³ × e + 1,5 kN/m² ; Q bureaux 2,5 à 3,0 kN/m² ; toiture Q = 1,0 kN/m².
// Les dalles et voiles sont modélisés (géométrie, rendu, maillage) mais ne sont pas transmis au moteur : leurs
// charges sont reportées sur les poutres en charges linéiques (largeurs de reprise indiquées).
// =============================================================================================================

struct DemoLoads
{
    int G = 0, Q = 0, W = 0, combELU = 0, combELS = 0;
};

DemoLoads addStandardCases(Model& m, bool wind)
{
    DemoLoads l;
    auto& lm = m.loadManager();
    lm.clear(); // cas et combinaisons Eurocode par défaut remplacés par ceux du projet
    l.G = lm.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead, true, 1.0, "Poids propre et charges permanentes"));
    l.Q = lm.addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live, false, 1.0, "Charges d'exploitation"));
    if (wind) l.W = lm.addLoadCase(LoadCase(0, "W", LoadCaseCategory::Wind, false, 1.0, "Vent (charges nodales en façade)"));
    std::map<int, double> elu { { l.G, 1.35 }, { l.Q, 1.5 } }, els { { l.G, 1.0 }, { l.Q, 1.0 } };
    if (wind)
    {
        elu[l.W] = 0.9; // ψ0 × 1,5 (ψ0 = 0,6) : vent d'accompagnement
        els[l.W] = 0.6;
    }
    l.combELU = lm.addCombination(LoadCombination(0, "ELU fondamental", LoadCombinationType::ULS_Fundamental, elu, "1,35 G + 1,5 Q"));
    l.combELS = lm.addCombination(LoadCombination(0, "ELS caractéristique", LoadCombinationType::SLS_Characteristic, els, "G + Q"));
    return l;
}

/// Charge linéique (kN/m, gravité) sur toutes les poutres dont le nom commence par `prefix`.
std::size_t lineLoad(Model& m, const std::string& prefix, int caseId, double q, const std::string& label)
{
    std::vector<int> ids;
    for (const auto& [id, b] : m.beams())
        if (b.name().rfind(prefix, 0) == 0) ids.push_back(id);
    for (int id : ids) m.loadManager().addMemberLoad(MemberLoad::uniform(id, caseId, q, LoadDirection::Gravity, label, MemberTargetType::Beam));
    return ids.size();
}

/// Finition d'aspect (rendu seulement) des éléments dont le nom commence par `prefix` ; `mat` remplace le matériau
/// quand il est fourni (dalles, voiles et semelles créés avec le matériau par défaut).
void finish(Model& m, const std::string& prefix, const TSA::Model::VisualProperties& v, const Material* mat = nullptr)
{
    auto apply = [&](auto& e) {
        if (e.name().rfind(prefix, 0) != 0) return;
        if (mat) e.setMaterial(*mat);
        e.material().visual = v;
    };
    std::vector<int> ids;
    for (const auto& [id, e] : m.beams()) ids.push_back(id);
    for (int id : ids) apply(*m.getBeam(id));
    ids.clear();
    for (const auto& [id, e] : m.columns()) ids.push_back(id);
    for (int id : ids) apply(*m.getColumn(id));
    ids.clear();
    for (const auto& [id, e] : m.slabs()) ids.push_back(id);
    for (int id : ids) apply(*m.getSlab(id));
    ids.clear();
    for (const auto& [id, e] : m.walls()) ids.push_back(id);
    for (int id : ids) apply(*m.getWall(id));
    ids.clear();
    for (const auto& [id, e] : m.foundations()) ids.push_back(id);
    for (int id : ids) apply(*m.getFoundation(id));
    ids.clear();
    for (const auto& [id, e] : m.cables()) ids.push_back(id);
    for (int id : ids) apply(*m.getCable(id));
}

int nodeAt(const Model& m, double x, double y, double z)
{
    for (const auto& [id, n] : m.nodes())
        if (std::abs(n.x() - x) < 1e-6 && std::abs(n.y() - y) < 1e-6 && std::abs(n.z() - z) < 1e-6) return id;
    return 0;
}

// --- P1. Bâtiment en béton armé R+4 --------------------------------------------------------------------------
void buildConcreteBuilding(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material c = Material::concreteC30_37();
    const std::vector<double> xs { 0, 5, 10, 15, 20, 25, 30 }, ys { 0, 6, 12, 18 }, zs { 0.0, 3.5, 6.7, 9.9, 13.1, 16.3 };
    const std::vector<std::string> lv { "RDC", "R+1", "R+2", "R+3", "R+4", "Toiture" };
    for (std::size_t k = 0; k + 1 < zs.size(); ++k)
        for (double x : xs)
            for (double y : ys)
                w.bar(P(x, y, zs[k]), P(x, y, zs[k + 1]), Kind::Column, Section::rectangular(k < 2 ? 0.40 : 0.35, k < 2 ? 0.40 : 0.35, "Poteau BA"), c, "Poteau " + lv[k]);
    for (double x : xs)
        for (double y : ys) w.support(x, y, 0, SupportDefinition::fixed());
    // Longrines (niveau 0)
    for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        for (double y : ys) w.bar(P(xs[i], y, 0), P(xs[i + 1], y, 0), Kind::Beam, Section::rectangular(0.30, 0.50, "Longrine 30x50"), c, "Longrine");
    for (double x : xs)
        for (std::size_t j = 0; j + 1 < ys.size(); ++j) w.bar(P(x, ys[j], 0), P(x, ys[j + 1], 0), Kind::Beam, Section::rectangular(0.30, 0.50, "Longrine 30x50"), c, "Longrine");
    // Poutres : porteuses selon X (reprennent les dalles portant selon Y), secondaires selon Y
    for (std::size_t k = 1; k < zs.size(); ++k)
    {
        const bool roof = k + 1 == zs.size();
        for (std::size_t i = 0; i + 1 < xs.size(); ++i)
            for (double y : ys)
                w.bar(P(xs[i], y, zs[k]), P(xs[i + 1], y, zs[k]), Kind::Beam, Section::rectangular(0.30, 0.55, "Poutre 30x55"), c,
                      std::string(roof ? "Poutre toiture " : "Poutre porteuse ") + (y > 0 && y < 18 ? "int" : "rive"));
        for (double x : xs)
            for (std::size_t j = 0; j + 1 < ys.size(); ++j)
                w.bar(P(x, ys[j], zs[k]), P(x, ys[j + 1], zs[k]), Kind::Beam, Section::rectangular(0.25, 0.45, "Poutre 25x45"), c, "Poutre secondaire");
    }
    // Escalier dans la trémie C-D / 2-3 : volées sur limons BA et paliers
    for (std::size_t k = 0; k + 2 < zs.size(); ++k)
    {
        const double z0 = zs[k], z1 = zs[k + 1], zm = (z0 + z1) / 2;
        const Section limon = Section::rectangular(0.20, 0.35, "Limon 20x35");
        w.bar(P(15, 6, zm), P(15, 12, zm), Kind::Beam, limon, c, "Poutre palière");
        w.bar(P(13.6, 6, zm), P(13.6, 12, zm), Kind::Beam, limon, c, "Rive de palier");
        for (double y : { 6.0, 12.0 }) w.bar(P(13.6, y, zm), P(15, y, zm), Kind::Beam, limon, c, "Rive de palier");
        if (k > 0) w.bar(P(11.0, 6, z0), P(11.0, 12, z0), Kind::Beam, limon, c, "Chevêtre");
        w.bar(P(11.0, 6, z1), P(11.0, 12, z1), Kind::Beam, limon, c, "Chevêtre");
        for (double y : { 6.8, 8.6 }) w.bar(P(11.0, y, z0), P(13.6, y, zm), Kind::Beam, limon, c, "Limon");
        for (double y : { 9.4, 11.2 }) w.bar(P(13.6, y, zm), P(11.0, y, z1), Kind::Beam, limon, c, "Limon");
        if (k == 0)
        {
            w.bar(P(11.0, 6.8, 0), P(11.0, 8.6, 0), Kind::Beam, limon, c, "Marche de départ");
            for (double y : { 6.8, 8.6 }) w.support(11.0, y, 0, SupportDefinition::pinned());
        }
    }
    w.finalize();
    // Dalles (sauf la trémie d'escalier C-D / 2-3), voiles du noyau, semelles
    for (std::size_t k = 1; k < zs.size(); ++k)
        for (std::size_t i = 0; i + 1 < xs.size(); ++i)
            for (std::size_t j = 0; j + 1 < ys.size(); ++j)
            {
                if (xs[i] == 10 && ys[j] == 6 && k + 1 < zs.size()) continue; // trémie (la toiture la couvre)
                m.addSlab({ nodeAt(m, xs[i], ys[j], zs[k]), nodeAt(m, xs[i + 1], ys[j], zs[k]), nodeAt(m, xs[i + 1], ys[j + 1], zs[k]), nodeAt(m, xs[i], ys[j + 1], zs[k]) },
                          k + 1 == zs.size() ? 0.22 : 0.20, "Dalle " + lv[k]);
            }
    for (std::size_t k = 0; k + 1 < zs.size(); ++k)
    {
        const double h = zs[k + 1] - zs[k];
        m.addWall(nodeAt(m, 10, 12, zs[k]), nodeAt(m, 15, 12, zs[k]), h, 0.20, "Voile noyau " + lv[k]);
        m.addWall(nodeAt(m, 15, 6, zs[k]), nodeAt(m, 15, 12, zs[k]), h, 0.20, "Voile noyau " + lv[k]);
        m.addWall(nodeAt(m, 10, 6, zs[k]), nodeAt(m, 10, 12, zs[k]), h, 0.20, "Voile noyau " + lv[k]);
    }
    for (double x : xs)
        for (double y : ys) m.addFoundation(nodeAt(m, x, y, 0), 1.8, 1.8, 0.6, "Semelle");
    namespace VF = TSA::Model::VisualFinish;
    finish(m, "Poteau", VF::formedConcrete());
    finish(m, "Voile", VF::formedConcrete(), &c);
    finish(m, "Dalle", VF::rawConcrete(), &c);
    finish(m, "Semelle", VF::roughConcrete(), &c);
    finish(m, "Longrine", VF::roughConcrete());
    // Charges : dalles portant selon Y entre poutres porteuses (largeur 6 m sur les files 2 et 3, 3 m en rive)
    const auto L = addStandardCases(m, false);
    lineLoad(m, "Poutre porteuse int", L.G, 6.5 * 6.0, "G dalle 20 cm + revêtement (6,5 kN/m² × 6 m)");
    lineLoad(m, "Poutre porteuse rive", L.G, 6.5 * 3.0, "G dalle 20 cm + revêtement (6,5 kN/m² × 3 m)");
    lineLoad(m, "Poutre porteuse int", L.Q, 2.5 * 6.0, "Q bureaux (2,5 kN/m² × 6 m)");
    lineLoad(m, "Poutre porteuse rive", L.Q, 2.5 * 3.0, "Q bureaux (2,5 kN/m² × 3 m)");
    lineLoad(m, "Poutre toiture int", L.G, 7.0 * 6.0, "G toiture 22 cm + étanchéité (7,0 kN/m² × 6 m)");
    lineLoad(m, "Poutre toiture rive", L.G, 7.0 * 3.0, "G toiture 22 cm + étanchéité (7,0 kN/m² × 3 m)");
    lineLoad(m, "Poutre toiture int", L.Q, 1.0 * 6.0, "Q toiture non accessible (1,0 kN/m² × 6 m)");
    lineLoad(m, "Poutre toiture rive", L.Q, 1.0 * 3.0, "Q toiture non accessible (1,0 kN/m² × 3 m)");

    grid.setXPositions(xs);
    grid.setXLabels({ "A", "B", "C", "D", "E", "F", "G" });
    grid.setYPositions(ys);
    grid.setYLabels({ "1", "2", "3", "4" });
    grid.setZLevels(zs);
    grid.setZLabels(lv);
}

// --- P2. Bâtiment mixte béton armé + acier ----------------------------------------------------------------
void buildMixedBuilding(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material c = Material::concreteC30_37(), s = Material::steelS355();
    const std::vector<double> xs { 0, 8, 16, 24, 32 }, ys { 0, 9, 18 }, zs { 0.0, 4.0, 7.6, 11.2, 15.6 };
    const std::vector<std::string> lv { "RDC", "R+1", "R+2", "R+3", "Toiture" };
    const double zTop = zs[3];
    for (std::size_t k = 0; k + 1 < zs.size(); ++k)
        for (double x : xs)
            for (double y : ys)
            {
                if (k == 3 && y == 9) continue; // dernier niveau : grande salle sans poteau intérieur (fermes de 18 m)
                w.bar(P(x, y, zs[k]), P(x, y, zs[k + 1]), Kind::Column, Section::rectangular(0.45, 0.45, "Poteau BA 45x45"), c, "Poteau BA " + lv[k]);
            }
    for (double x : xs)
        for (double y : ys) w.support(x, y, 0, SupportDefinition::fixed());
    // Planchers R+1 à R+3 : poutres acier selon Y (9 m), solives aux tiers des travées X, poutres de file en caisson
    // soudé selon X (elles reprennent les solives à leurs tiers)
    for (std::size_t k = 1; k <= 3; ++k)
    {
        const double z = zs[k];
        for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        {
            for (double y : ys) w.bar(P(xs[i], y, z), P(xs[i + 1], y, z), Kind::Beam, Section::boxHollow(0.30, 0.70, 0.015, 0.025, "Caisson soudé 300x700"), s, "Poutre de file acier");
            for (double t : { 1.0 / 3, 2.0 / 3 })
            {
                const double x = xs[i] + t * (xs[i + 1] - xs[i]);
                for (std::size_t j = 0; j + 1 < ys.size(); ++j)
                    w.bar(P(x, ys[j], z), P(x, ys[j + 1], z), Kind::Beam, Section::ipe(400), s, "Solive acier");
            }
        }
        for (double x : xs)
            for (std::size_t j = 0; j + 1 < ys.size(); ++j)
                w.bar(P(x, ys[j], z), P(x, ys[j + 1], z), Kind::Beam, Section::ipe(400), s, std::string("Poutre acier ") + (x == 0 || x == 32 ? "rive" : "file"));
    }
    // Toiture de la grande salle : fermes Warren acier de 18 m (y de 0 à 18) sur chaque file X, hauteur 1,8 m
    const double H = 1.8;
    const Section chord = Section::hea(200), web = Section::pipe(0.1143, 0.006, "Tube 114x6");
    for (double x : xs)
    {
        for (int j = 0; j < 6; ++j)
        {
            const double y0 = j * 3.0, y1 = y0 + 3.0;
            w.bar(P(x, y0, zs[4]), P(x, y1, zs[4]), Kind::Beam, chord, s, "Ferme — membrure inférieure");
            w.bar(P(x, y0, zs[4] + H), P(x, y1, zs[4] + H), Kind::Beam, chord, s, "Ferme — membrure supérieure");
            if (j % 2 == 0) w.bar(P(x, y0, zs[4]), P(x, y1, zs[4] + H), Kind::Truss, web, s, "Ferme — diagonale");
            else w.bar(P(x, y0, zs[4] + H), P(x, y1, zs[4]), Kind::Truss, web, s, "Ferme — diagonale");
        }
        for (int j = 0; j <= 6; ++j) w.bar(P(x, j * 3.0, zs[4]), P(x, j * 3.0, zs[4] + H), Kind::Column, web, s, "Ferme — montant");
    }
    for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        for (int j = 0; j <= 6; ++j) w.bar(P(xs[i], j * 3.0, zs[4] + H), P(xs[i + 1], j * 3.0, zs[4] + H), Kind::Beam, Section::ipe(200), s, "Panne de toiture");
    // Contreventements acier : palées en façade (pignons X = 0 et 32, travée 1-2) sur tous les niveaux
    for (std::size_t k = 0; k < 3; ++k)
        for (double x : { 0.0, 32.0 })
            w.xBrace(P(x, 0, zs[k]), P(x, 9, zs[k]), P(x, 9, zs[k + 1]), P(x, 0, zs[k + 1]), Section::pipe(0.1683, 0.008, "Tube 168x8"), s, "Palée acier");
    for (double y : { 0.0, 18.0 })
        w.xBrace(P(0, y, zTop), P(8, y, zTop), P(8, y, zs[4]), P(0, y, zs[4]), Section::pipe(0.1683, 0.008, "Tube 168x8"), s, "Palée acier");
    w.finalize();
    // Noyau BA (voiles autour de la travée C-D / 2-3) jusqu'au R+3, dalles des planchers
    for (std::size_t k = 0; k < 3; ++k)
    {
        const double h = zs[k + 1] - zs[k];
        m.addWall(nodeAt(m, 16, 9, zs[k]), nodeAt(m, 24, 9, zs[k]), h, 0.25, "Voile noyau " + lv[k]);
        m.addWall(nodeAt(m, 16, 18, zs[k]), nodeAt(m, 24, 18, zs[k]), h, 0.25, "Voile noyau " + lv[k]);
        m.addWall(nodeAt(m, 24, 9, zs[k]), nodeAt(m, 24, 18, zs[k]), h, 0.25, "Voile noyau " + lv[k]);
    }
    for (std::size_t k = 1; k <= 3; ++k)
        for (std::size_t i = 0; i + 1 < xs.size(); ++i)
            for (std::size_t j = 0; j + 1 < ys.size(); ++j)
                m.addSlab({ nodeAt(m, xs[i], ys[j], zs[k]), nodeAt(m, xs[i + 1], ys[j], zs[k]), nodeAt(m, xs[i + 1], ys[j + 1], zs[k]), nodeAt(m, xs[i], ys[j + 1], zs[k]) },
                          0.14, "Dalle sur bac acier " + lv[k]);
    for (double x : xs)
        for (double y : ys) m.addFoundation(nodeAt(m, x, y, 0), 2.2, 2.2, 0.7, "Semelle BA");
    namespace VF = TSA::Model::VisualFinish;
    finish(m, "Poteau BA", VF::formedConcrete());
    finish(m, "Voile", VF::formedConcrete(), &c);
    finish(m, "Dalle", VF::rawConcrete(), &c);
    finish(m, "Semelle", VF::roughConcrete(), &c);
    finish(m, "Poutre", VF::paintedSteel());
    finish(m, "Solive", VF::paintedSteel());
    finish(m, "Ferme", VF::galvanizedSteel());
    finish(m, "Panne", VF::galvanizedSteel());
    finish(m, "Palée", VF::paintedSteel("#B03A2E"));
    // Charges : dalle 14 cm portant selon X entre solives et poutres (entraxe 8/3 = 2,67 m)
    const auto L = addStandardCases(m, false);
    const double b = 8.0 / 3.0;
    lineLoad(m, "Solive acier", L.G, 5.0 * b, "G dalle 14 cm + revêtement (5,0 kN/m² × 2,67 m)");
    lineLoad(m, "Poutre acier file", L.G, 5.0 * b, "G dalle (5,0 kN/m² × 2,67 m)");
    lineLoad(m, "Poutre acier rive", L.G, 5.0 * b / 2, "G dalle (5,0 kN/m² × 1,33 m)");
    lineLoad(m, "Solive acier", L.Q, 3.0 * b, "Q bureaux (3,0 kN/m² × 2,67 m)");
    lineLoad(m, "Poutre acier file", L.Q, 3.0 * b, "Q bureaux (3,0 kN/m² × 2,67 m)");
    lineLoad(m, "Poutre acier rive", L.Q, 3.0 * b / 2, "Q bureaux (3,0 kN/m² × 1,33 m)");
    lineLoad(m, "Panne de toiture", L.G, 0.5 * 3.0, "G couverture bac acier (0,5 kN/m² × 3 m)");
    lineLoad(m, "Panne de toiture", L.Q, 1.0 * 3.0, "Q toiture non accessible (1,0 kN/m² × 3 m)");

    grid.setXPositions(xs);
    grid.setXLabels({ "A", "B", "C", "D", "E" });
    grid.setYPositions(ys);
    grid.setYLabels({ "1", "2", "3" });
    grid.setZLevels({ 0.0, 4.0, 7.6, 11.2, 15.6, 17.4 });
    grid.setZLabels({ "RDC", "R+1", "R+2", "R+3", "Salle", "Fermes" });
}

// --- P3. Bâtiment complexe : plan en L, hauteurs différentes, porte-à-faux, voûte polygonale --------------
void buildComplexBuilding(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material c = Material::concreteC30_37(), s = Material::steelS355();
    const std::vector<double> xa { 0, 6, 12, 18, 24, 30 }, ya { 0, 5, 12 };      // aile A (béton armé), travées 5 et 7 m
    const std::vector<double> xb { 18, 24, 30 }, yb { 12, 18, 24, 30, 36, 42 };  // aile B (acier)
    const std::vector<double> za { 0.0, 3.5, 7.0, 10.5, 14.0, 17.5 };            // aile A : R+4
    const std::vector<double> zb { 0.0, 4.2, 8.4, 12.6 };                        // aile B : R+2 (hauteurs d'étage différentes)
    const Section colA = Section::rectangular(0.40, 0.40, "Poteau BA 40x40"), beamA = Section::rectangular(0.30, 0.60, "Poutre BA 30x60");
    // Aile A : portiques BA
    for (std::size_t k = 0; k + 1 < za.size(); ++k)
        for (double x : xa)
            for (double y : ya) w.bar(P(x, y, za[k]), P(x, y, za[k + 1]), Kind::Column, colA, c, "Poteau BA aile A");
    for (std::size_t k = 1; k < za.size(); ++k)
    {
        for (std::size_t i = 0; i + 1 < xa.size(); ++i)
            for (double y : ya) w.bar(P(xa[i], y, za[k]), P(xa[i + 1], y, za[k]), Kind::Beam, beamA, c, "Poutre BA A porteuse");
        for (double x : xa)
            for (std::size_t j = 0; j + 1 < ya.size(); ++j)
            {
                w.bar(P(x, ya[j], za[k]), P(x, ya[j + 1], za[k]), Kind::Beam, Section::rectangular(0.25, 0.50, "Poutre BA 25x50"), c, "Poutre BA A secondaire");
            }
    }
    // Porte-à-faux de 4 m au-delà de la file F (x = 30) aux niveaux R+3, R+4 et toiture
    for (std::size_t k = 3; k < za.size(); ++k)
    {
        for (double y : ya) w.bar(P(30, y, za[k]), P(34, y, za[k]), Kind::Beam, Section::rectangular(0.40, 0.70, "Console BA 40x70"), c, "Console porte-à-faux");
        for (std::size_t j = 0; j + 1 < ya.size(); ++j) w.bar(P(34, ya[j], za[k]), P(34, ya[j + 1], za[k]), Kind::Beam, beamA, c, "Rive de porte-à-faux");
    }
    // Aile B : ossature acier, poteaux HEB, poutres IPE, palées
    for (std::size_t k = 0; k + 1 < zb.size(); ++k)
        for (double x : xb)
            for (double y : yb)
                if (y > 12) w.bar(P(x, y, zb[k]), P(x, y, zb[k + 1]), Kind::Column, Section::heb(260), s, "Poteau acier aile B");
    for (std::size_t k = 1; k < zb.size(); ++k)
    {
        for (double y : yb)
            for (std::size_t i = 0; i + 1 < xb.size(); ++i) w.bar(P(xb[i], y, zb[k]), P(xb[i + 1], y, zb[k]), Kind::Beam, Section::ipe(400), s, "Poutre acier B porteuse");
        for (double x : xb)
            for (std::size_t j = 0; j + 1 < yb.size(); ++j) w.bar(P(x, yb[j], zb[k]), P(x, yb[j + 1], zb[k]), Kind::Beam, Section::ipe(300), s, "Poutre acier B secondaire");
    }
    for (double x : { 18.0, 30.0 })
        for (std::size_t k = 0; k + 1 < zb.size(); ++k)
            w.xBrace(P(x, 24, zb[k]), P(x, 30, zb[k]), P(x, 30, zb[k + 1]), P(x, 24, zb[k + 1]), Section::pipe(0.1397, 0.008, "Tube 140x8"), s, "Palée acier aile B");
    // Toiture de l'aile B : arcs polygonaux (8 segments) de 12 m sur les files 4 à 8, pannes entre arcs
    const double zr = zb.back(), rise = 3.0;
    auto arcZ = [&](double x) { const double t = (x - 18.0) / 12.0; return zr + 4.0 * rise * t * (1.0 - t); };
    for (double y : yb)
        if (y > 12) // la travée 3-4 (y = 12 à 18) est une terrasse de liaison à z = 12,6 m
            for (int k = 0; k < 8; ++k)
            {
                const double x0 = 18 + 1.5 * k, x1 = x0 + 1.5;
                w.bar(P(x0, y, arcZ(x0)), P(x1, y, arcZ(x1)), Kind::Beam, Section::hea(220), s, "Arc de toiture");
            }
    for (std::size_t j = 1; j + 1 < yb.size(); ++j)
        for (int k = 0; k <= 8; ++k)
        {
            const double x = 18 + 1.5 * k;
            w.bar(P(x, yb[j], arcZ(x)), P(x, yb[j + 1], arcZ(x)), Kind::Beam, Section::ipe(160), s, "Panne d'arc");
        }
    w.finalize();
    for (const auto& [id, n] : m.nodes())
        if (std::abs(n.z()) < 1e-9) m.getNode(id)->setSupport(SupportDefinition::fixed());
    // Voiles du noyau d'escalier (aile A, travée C-D / 1-2), dalles de l'aile A, semelles
    for (std::size_t k = 0; k + 1 < za.size(); ++k)
    {
        const double h = za[k + 1] - za[k];
        m.addWall(nodeAt(m, 12, 0, za[k]), nodeAt(m, 18, 0, za[k]), h, 0.20, "Voile noyau");
        m.addWall(nodeAt(m, 12, 0, za[k]), nodeAt(m, 12, 5, za[k]), h, 0.20, "Voile noyau");
        m.addWall(nodeAt(m, 18, 0, za[k]), nodeAt(m, 18, 5, za[k]), h, 0.20, "Voile noyau");
    }
    for (std::size_t k = 1; k < za.size(); ++k)
        for (std::size_t i = 0; i + 1 < xa.size(); ++i)
            for (std::size_t j = 0; j + 1 < ya.size(); ++j)
            {
                if (k == 2 && xa[i] == 6 && ya[j] == 5) continue; // atrium
                m.addSlab({ nodeAt(m, xa[i], ya[j], za[k]), nodeAt(m, xa[i + 1], ya[j], za[k]), nodeAt(m, xa[i + 1], ya[j + 1], za[k]), nodeAt(m, xa[i], ya[j + 1], za[k]) }, 0.20, "Dalle aile A");
            }
    for (const auto& [id, n] : m.nodes())
        if (std::abs(n.z()) < 1e-9 && n.support().isSupported()) m.addFoundation(id, 1.8, 1.8, 0.6, "Semelle");
    namespace VF = TSA::Model::VisualFinish;
    finish(m, "Poteau BA", VF::formedConcrete());
    finish(m, "Console", VF::formedConcrete());
    finish(m, "Voile", VF::formedConcrete(), &c);
    finish(m, "Dalle", VF::rawConcrete(), &c);
    finish(m, "Semelle", VF::roughConcrete(), &c);
    finish(m, "Poteau acier", VF::paintedSteel("#5E6A73"));
    finish(m, "Poutre acier", VF::paintedSteel("#5E6A73"));
    finish(m, "Palée", VF::paintedSteel("#B03A2E"));
    finish(m, "Arc", VF::galvanizedSteel());
    finish(m, "Panne", VF::galvanizedSteel());
    // Charges : G, Q sur les poutres porteuses, vent W en façade sud (y = 0) de l'aile A, charges nodales +Y
    const auto L = addStandardCases(m, true);
    lineLoad(m, "Poutre BA A porteuse", L.G, 6.5 * 3.0, "G dalle 20 cm + revêtement (6,5 kN/m² × 3 m)");
    lineLoad(m, "Poutre BA A porteuse", L.Q, 2.5 * 3.0, "Q bureaux (2,5 kN/m² × 3 m)");
    lineLoad(m, "Console porte-à-faux", L.G, 6.5 * 3.0, "G balcon (6,5 kN/m² × 3 m)");
    lineLoad(m, "Console porte-à-faux", L.Q, 3.5 * 3.0, "Q balcon (3,5 kN/m² × 3 m)");
    lineLoad(m, "Poutre acier B porteuse", L.G, 4.0 * 3.0, "G plancher sur bac acier (4,0 kN/m² × 3 m)");
    lineLoad(m, "Poutre acier B porteuse", L.Q, 4.0 * 3.0, "Q salle de réunion (4,0 kN/m² × 3 m)");
    lineLoad(m, "Panne d'arc", L.G, 0.6 * 1.5, "G couverture (0,6 kN/m² × 1,5 m)");
    for (double z : za)
        if (z > 0)
            for (double x : xa) m.loadManager().addNodalLoad(NodalLoad(0, nodeAt(m, x, 0, z), L.W, 0, x == 0 || x == 30 ? 4.5 : 9.0, 0, 0, 0, 0, LoadCoordSystem::Global, "W façade sud"));

    grid.setXPositions({ 0, 6, 12, 18, 24, 30, 34 });
    grid.setXLabels({ "A", "B", "C", "D", "E", "F", "F'" });
    grid.setYPositions({ 0, 5, 12, 18, 24, 30, 36, 42 });
    grid.setYLabels({ "1", "2", "3", "4", "5", "6", "7", "8" });
    grid.setZLevels({ 0.0, 3.5, 4.2, 7.0, 8.4, 10.5, 12.6, 14.0, 17.5 });
    grid.setZLabels({ "RDC", "A R+1", "B R+1", "A R+2", "B R+2", "A R+3", "B toiture", "A R+4", "A toiture" });
}

// --- P4. Pont à haubans 80 + 180 + 80 m, pylônes en H, haubans en semi-harpe ----------------------------
void buildCableStayedBridge(Model& m, TSA::Grid::GridDefinition& grid)
{
    WireBuilder w(m);
    const Material c = Material::concreteC30_37(), s = Material::steelS355();
    const double L1 = 80, L2 = 180, Ltot = 2 * L1 + L2, B = 10.0, legY = 12.0, zDeck = 0.0, zBase = -20.0, zTop = 70.0;
    const Section girder = Section::boxHollow(1.2, 2.5, 0.025, 0.04, "Caisson 1200x2500"), crossBeam = Section::boxHollow(0.6, 1.6, 0.016, 0.025, "Caisson 600x1600");
    const Section stringer = Section::ipe(400), deckBrace = Section::angle(0.15, 0.15, 0.015, "L 150x150x15");
    const Section leg = Section::boxHollow(3.0, 4.0, 0.6, 0.6, "Fût caisson BA 4,0x3,0 (parois 60 cm)"), pylonBeam = Section::rectangular(2.5, 3.0, "Entretoise 2,5x3,0");
    const Section pier = Section::rectangular(2.0, 2.0, "Pile 2,0x2,0");
    // Tablier : poutres de rive caisson, pièces de pont tous les 10 m, longeron central, contreventement en croix
    for (double x = 0; x < Ltot - 1e-9; x += 10)
    {
        for (double y : { -B, B }) w.bar(P(x, y, zDeck), P(x + 10, y, zDeck), Kind::Beam, girder, s, "Poutre de rive");
        w.bar(P(x, 0, zDeck), P(x + 10, 0, zDeck), Kind::Beam, stringer, s, "Longeron central");
        w.xBrace(P(x, -B, zDeck), P(x, 0, zDeck), P(x + 10, 0, zDeck), P(x + 10, -B, zDeck), deckBrace, s, "Contreventement de tablier");
        w.xBrace(P(x, 0, zDeck), P(x, B, zDeck), P(x + 10, B, zDeck), P(x + 10, 0, zDeck), deckBrace, s, "Contreventement de tablier");
    }
    for (double x = 0; x <= Ltot + 1e-9; x += 10) w.bar(P(x, -B, zDeck), P(x, B, zDeck), Kind::Beam, crossBeam, s, "Pièce de pont");
    // Pylônes en H (béton armé) : fûts, entretoise inférieure sous le tablier, entretoise haute, appareils d'appui
    for (double xp : { L1, L1 + L2 })
    {
        for (double y : { -legY, legY })
        {
            w.bar(P(xp, y, zBase), P(xp, y, zTop), Kind::Column, leg, c, "Fût de pylône");
            w.support(xp, y, zBase, SupportDefinition::fixed());
        }
        w.bar(P(xp, -legY, -2.0), P(xp, legY, -2.0), Kind::Beam, pylonBeam, c, "Entretoise de pylône basse");
        w.bar(P(xp, -legY, 52.0), P(xp, legY, 52.0), Kind::Beam, pylonBeam, c, "Entretoise de pylône haute");
        for (double y : { -B, B }) w.bar(P(xp, y, -2.0), P(xp, y, zDeck), Kind::Column, Section::rectangular(1.0, 1.0, "Appui 1,0x1,0"), c, "Appareil d'appui");
    }
    // Piles auxiliaires des travées de rive et culées
    for (double x : { 40.0, Ltot - 40.0 })
        for (double y : { -B, B })
        {
            w.bar(P(x, y, zBase), P(x, y, zDeck), Kind::Column, pier, c, "Pile auxiliaire");
            w.support(x, y, zBase, SupportDefinition::fixed());
        }
    for (double x : { 0.0, Ltot })
        for (double y : { -B, B }) w.support(x, y, zDeck, SupportDefinition::pinned()); // culées
    // Haubans en semi-harpe : 8 par demi-éventail et par plan, ancrés sur le tablier tous les 10 m (le dernier hauban
    // de retenue sur la culée) et sur le fût entre 54 et 68 m. Tension initiale T0 = g × 10 m / sin θ : part du poids
    // permanent d'un plan reprise par le hauban (hypothèse de réglage simplifiée, sans calcul de réglage itératif).
    TSA::Model::CableDefinition stay = TSA::Model::CableDefinition::stayCablePSS_37x15_7();
    const double gPlane = (8.75 * 10.0 + 25.0) * 1e3; // N/m par plan : superstructure (8,75 kN/m² × 10 m) + acier ≈ 25 kN/m
    for (double xp : { L1, L1 + L2 })
        for (int side : { -1, 1 })
            for (int k = 1; k <= 8; ++k)
            {
                const double xd = xp + side * (10.0 * k), za = 54.0 + 2.0 * (k - 1);
                for (double y : { -B, B })
                {
                    const double yl = y < 0 ? -legY : legY;
                    const double dx = std::abs(xd - xp), dz = za - zDeck, dy = std::abs(yl - y);
                    const double sinTheta = dz / std::sqrt(dx * dx + dy * dy + dz * dz);
                    w.cable(P(xd, y, zDeck), P(xp, yl, za), stay, gPlane * 10.0 / sinTheta, "Hauban");
                }
            }
    w.finalize();
    // Charges : superstructure (dalle 25 cm + revêtement 8,75 kN/m²) et trafic (4,0 kN/m², hypothèse) sur les
    // pièces de pont, entraxe 10 m (5 m aux extrémités)
    const auto L = addStandardCases(m, false);
    std::vector<int> ends, inner;
    for (const auto& [id, b] : m.beams())
        if (b.name() == "Pièce de pont")
        {
            const double x = m.getNode(b.startNodeId())->x();
            (x < 1e-6 || x > Ltot - 1e-6 ? ends : inner).push_back(id);
        }
    for (int id : inner)
    {
        m.loadManager().addMemberLoad(MemberLoad::uniform(id, L.G, 8.75 * 10.0, LoadDirection::Gravity, "G superstructure (8,75 kN/m² × 10 m)", MemberTargetType::Beam));
        m.loadManager().addMemberLoad(MemberLoad::uniform(id, L.Q, 4.0 * 10.0, LoadDirection::Gravity, "Q trafic (4,0 kN/m² × 10 m)", MemberTargetType::Beam));
    }
    for (int id : ends)
    {
        m.loadManager().addMemberLoad(MemberLoad::uniform(id, L.G, 8.75 * 5.0, LoadDirection::Gravity, "G superstructure (8,75 kN/m² × 5 m)", MemberTargetType::Beam));
        m.loadManager().addMemberLoad(MemberLoad::uniform(id, L.Q, 4.0 * 5.0, LoadDirection::Gravity, "Q trafic (4,0 kN/m² × 5 m)", MemberTargetType::Beam));
    }
    for (double xp : { L1, L1 + L2 })
        for (double y : { -legY, legY }) m.addFoundation(nodeAt(m, xp, y, zBase), 10.0, 8.0, 3.0, "Semelle de pylône");
    namespace VF = TSA::Model::VisualFinish;
    finish(m, "Fût", VF::formedConcrete());
    finish(m, "Entretoise de pylône", VF::formedConcrete());
    finish(m, "Pile", VF::roughConcrete());
    finish(m, "Appareil", VF::rawConcrete());
    finish(m, "Semelle", VF::roughConcrete(), &c);
    finish(m, "Poutre de rive", VF::paintedSteel());
    finish(m, "Pièce de pont", VF::paintedSteel());
    finish(m, "Longeron", VF::paintedSteel());
    finish(m, "Contreventement", VF::galvanizedSteel());
    finish(m, "Hauban", VF::cable());

    grid.setXPositions({ 0.0, 40.0, L1, L1 + L2, Ltot - 40.0, Ltot });
    grid.setXLabels({ "C0", "P1", "Pylône 1", "Pylône 2", "P4", "C5" });
    grid.setYPositions({ -legY, -B, 0.0, B, legY });
    grid.setYLabels({ "Fût N", "Rive N", "Axe", "Rive S", "Fût S" });
    grid.setZLevels({ zBase, zDeck, 52.0, zTop });
    grid.setZLabels({ "Fondations", "Tablier", "Entretoise haute", "Tête de pylône" });
}

// --- Contrôles ----------------------------------------------------------------------------------------------
struct Topology
{
    std::size_t nodes = 0, beams = 0, columns = 0, cables = 0, supports = 0, duplicateNodes = 0, duplicateBars = 0, zeroLength = 0;
    std::size_t isolatedNodes = 0, nodesInsideBars = 0, components = 0;
    double length = 0.0;
};

Topology inspect(const Model& m)
{
    Topology t;
    t.nodes = m.nodes().size();
    t.beams = m.beams().size();
    t.columns = m.columns().size();
    t.cables = m.cables().size();
    std::map<std::tuple<long long, long long, long long>, int> coords;
    for (const auto& [id, n] : m.nodes())
    {
        if (++coords[std::make_tuple(std::llround(n.x() * 1000), std::llround(n.y() * 1000), std::llround(n.z() * 1000))] > 1) ++t.duplicateNodes;
        if (n.support().isSupported()) ++t.supports;
    }
    std::vector<std::pair<int, int>> bars;
    for (const auto& [id, b] : m.beams()) bars.push_back({ b.startNodeId(), b.endNodeId() });
    for (const auto& [id, c] : m.columns()) bars.push_back({ c.startNodeId(), c.endNodeId() });
    for (const auto& [id, c] : m.cables()) bars.push_back({ c.startNodeId(), c.endNodeId() });
    std::set<std::pair<int, int>> pairs;
    std::map<int, std::vector<int>> adj;
    for (auto [a, b] : bars)
    {
        if (!pairs.insert({ std::min(a, b), std::max(a, b) }).second) ++t.duplicateBars;
        const auto* na = m.getNode(a);
        const auto* nb = m.getNode(b);
        const gp_Pnt A(na->x(), na->y(), na->z()), B(nb->x(), nb->y(), nb->z());
        const double L = A.Distance(B);
        t.length += L;
        if (L < 1e-6) ++t.zeroLength;
        adj[a].push_back(b);
        adj[b].push_back(a);
        // Nœud à l'intérieur de la barre sans y être connecté (superposition ou connexion manquante)
        const gp_Vec AB(A, B);
        for (const auto& [id, n] : m.nodes())
        {
            if (id == a || id == b) continue;
            const gp_Vec AP(A, gp_Pnt(n.x(), n.y(), n.z()));
            const double s = AP.Dot(AB) / (L * L);
            if (s > 1e-9 && s < 1 - 1e-9 && AP.Crossed(AB).Magnitude() / L < 1e-4) ++t.nodesInsideBars;
        }
    }
    for (const auto& [id, n] : m.nodes())
        if (!adj.count(id)) ++t.isolatedNodes;
    std::set<int> seen;
    for (const auto& [id, n] : m.nodes())
    {
        if (seen.count(id) || !adj.count(id)) continue;
        ++t.components;
        std::queue<int> q;
        q.push(id);
        seen.insert(id);
        while (!q.empty())
        {
            const int u = q.front();
            q.pop();
            for (int v : adj[u])
                if (seen.insert(v).second) q.push(v);
        }
    }
    return t;
}

bool solveSelfWeight(Model& m, double* maxDisp, std::string* error)
{
    // Un seul cas : le LoadManager d'un modèle neuf contient déjà des cas Eurocode par défaut (dont un G avec poids
    // propre) ; les conserver compterait le poids propre deux fois dans le calcul « tous les cas ».
    m.loadManager().clear();
    m.loadManager().addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead, true, 1.0));
    OpenSeesSolver solver;
    AnalysisParameters params;
    params.includeSelfWeight = true;
    QString e;
    const bool ok = solver.solveSynchronous(m, params, &e);
    if (!ok) *error = e.toStdString();
    *maxDisp = 0.0;
    for (const auto& [id, n] : m.nodes())
        if (const auto* d = solver.results().getNodeDisplacement(id)) *maxDisp = std::max(*maxDisp, d->translationMagnitude());
    return ok;
}

QImage thumbnailOf(Model& m)
{
    OccView view;
    view.resize(640, 480);
    view.show();
    for (int k = 0; k < 20; ++k)
    {
        QApplication::processEvents();
        QThread::msleep(5);
    }
    view.setModel(&m);
    view.viewIsometric();
    view.fitModel();
    for (int k = 0; k < 10; ++k) QApplication::processEvents();
    return view.captureViewImage(640, 480);
}

// --- Projets complets : contrôles, calcul, équilibre, haubans, persistance, notes de calcul -------------------
struct Project
{
    const char* file;
    const char* title;
    const char* number;
    const char* description;
    void (*build)(Model&, TSA::Grid::GridDefinition&);
};

/// Somme des charges linéiques verticales d'un cas (kN) : Σ q × L sur les barres chargées.
double sumLineLoads(const Model& m, int caseId)
{
    double total = 0.0;
    for (const auto& [id, l] : m.loadManager().memberLoads())
        if (l.loadCaseId() == caseId)
            if (const auto* b = m.getBeam(l.elementId())) total += l.q1() * b->length(m);
    return total;
}

bool solveCase(Model& m, int caseId, int combId, bool selfWeight, ResultsModel& out, std::string* err)
{
    OpenSeesSolver solver;
    AnalysisParameters params;
    params.includeSelfWeight = selfWeight;
    params.targetLoadCaseId = caseId;
    params.targetCombinationId = combId;
    QString e;
    const bool ok = solver.solveSynchronous(m, params, &e);
    *err = e.toStdString();
    if (ok) out = solver.results();
    return ok;
}

bool runDemoProjects(int& passed, const QString& outDir, const QString& tmpDir)
{
    const Project projects[] = {
        { "TSA_Projet_Batiment_BA_R+4.tsa", "Bâtiment en béton armé R+4", "DEMO-01",
          "Bâtiment de bureaux R+4 en béton armé C30/37, trame 6 × 3 travées (5 m × 6 m) : semelles isolées, longrines, poteaux, "
          "poutres porteuses et secondaires, dalles de 20 cm, noyau en voiles, escalier dans une trémie, toiture-terrasse.",
          buildConcreteBuilding },
        { "TSA_Projet_Batiment_Mixte_BA_Acier.tsa", "Bâtiment mixte béton armé et acier", "DEMO-02",
          "Bâtiment R+3 : poteaux et noyau en béton armé, planchers à poutres et solives en acier S355 sous dalle sur bac de 14 cm "
          "(sans action mixte acier-béton), palées de stabilité, grande salle au dernier niveau sous fermes Warren de 18 m.",
          buildMixedBuilding },
        { "TSA_Projet_Batiment_Complexe_L.tsa", "Bâtiment complexe en L", "DEMO-03",
          "Plan en L : aile A en béton armé R+4 avec noyau, atrium et porte-à-faux de 4 m ; aile B en acier R+2 à hauteurs d'étage "
          "différentes, palées et toiture en arcs polygonaux. Cas G, Q et vent W, combinaisons ELU et ELS.",
          buildComplexBuilding },
        { "TSA_Projet_Pont_Haubans_80-180-80m.tsa", "Pont à haubans 80 + 180 + 80 m", "DEMO-04",
          "Pont à haubans à trois travées (80 + 180 + 80 m), tablier métallique de 20 m (poutres caissons de rive, pièces de pont, "
          "longeron, contreventement), deux pylônes en H en béton armé, 64 haubans PSS 37T15,7 en semi-harpe, piles auxiliaires et culées.",
          buildCableStayedBridge },
    };
    int testNo = 350;
    for (const auto& pr : projects)
    {
        Model m;
        TSA::Grid::GridDefinition def(pr.title, TSA::Grid::GridType::Cartesian);
        pr.build(m, def);
        const Topology t = inspect(m);
        std::cout << "  " << pr.title << " : " << t.nodes << " nœuds, " << t.beams << " poutres, " << t.columns << " poteaux, " << t.cables << " câbles, "
                  << m.slabs().size() << " dalles, " << m.walls().size() << " voiles, " << m.foundations().size() << " semelles, " << t.supports << " appuis, "
                  << m.loadManager().memberLoads().size() + m.loadManager().nodalLoads().size() << " charges" << std::endl;
        TEST_CHECK(t.nodes > 150 && t.beams + t.columns + t.cables > 400, "Test " << testNo << ": modèle complexe (" << pr.title << ")");
        TEST_CHECK(t.duplicateNodes == 0 && t.duplicateBars == 0 && t.zeroLength == 0,
                   "Test " << testNo << ": aucun doublon (nœuds " << t.duplicateNodes << ", barres " << t.duplicateBars << ", nulles " << t.zeroLength << ")");
        TEST_CHECK(t.isolatedNodes == 0 && t.nodesInsideBars == 0,
                   "Test " << testNo << ": connexions (nœuds isolés " << t.isolatedNodes << ", nœuds non connectés sur une barre " << t.nodesInsideBars << ")");
        TEST_CHECK(t.components == 1 && t.supports >= 4, "Test " << testNo << ": structure d'un seul tenant (" << t.components << "), " << t.supports << " appuis");
        const auto& lm = m.loadManager();
        int caseQ = 0, combELU = 0;
        for (const auto& [id, lc] : lm.loadCases())
            if (lc.name() == "Q") caseQ = id;
        for (const auto& [id, c] : lm.combinations())
            if (c.type() == LoadCombinationType::ULS_Fundamental) combELU = id;
        TEST_CHECK(caseQ && combELU && !lm.memberLoads().empty(), "Test " << testNo << ": cas G et Q, combinaison ELU et charges présents");

        // 1. Comparaison analytique : sous Q seul (sans poids propre), Σ réactions verticales = Σ q × L des charges
        std::string err;
        ResultsModel rq;
        const bool okQ = solveCase(m, caseQ, 0, false, rq, &err);
        const double qTotal = sumLineLoads(m, caseQ);
        const auto eqQ = rq.equilibrium();
        std::cout << std::fixed << std::setprecision(1) << "  Q seul : Σ q·L = " << qTotal << " kN, Σ Rz = " << eqQ.reactionFz << " kN, charge appliquée "
                  << -eqQ.appliedFz << " kN" << std::defaultfloat << std::endl;
        TEST_CHECK(okQ && std::abs(eqQ.reactionFz - qTotal) < 1e-3 * qTotal + 0.01,
                   "Test " << testNo << ": Σ réactions = Σ q·L sous Q (" << eqQ.reactionFz << " / " << qTotal << ") " << err);

        // 2. Combinaison ELU : convergence, équilibre global des forces et des moments, déplacement maximal
        ResultsModel ru;
        const bool okU = solveCase(m, 0, combELU, true, ru, &err);
        const auto eq = ru.equilibrium();
        const auto sum = ru.summary();
        const auto* nMax = m.getNode(sum.maxDisplacementNodeId);
        std::cout << std::fixed << std::setprecision(1) << "  ELU : charges verticales " << -eq.appliedFz << " kN, réactions " << eq.reactionFz << " kN, écart max "
                  << std::setprecision(4) << eq.maxError() << " kN, déplacement maximal " << std::setprecision(1) << sum.maxDisplacement * 1000 << " mm au nœud "
                  << sum.maxDisplacementNodeId;
        if (nMax) std::cout << " (" << nMax->x() << " ; " << nMax->y() << " ; " << nMax->z() << ")";
        std::cout << std::defaultfloat << std::endl;
        TEST_CHECK(okU && eq.isBalanced(1e-3) && sum.maxDisplacement > 0 && sum.maxDisplacement < 0.5,
                   "Test " << testNo << ": calcul ELU convergé et équilibré (écart " << eq.maxError() << " kN) " << err);

        // Contrainte normale de fibre extrême (diagramme « σ = N/A ± M/W ») : contrôle analytique, puis valeur maximale
        if (testNo == 350)
        {
            TSA::Analysis::StationForces st;
            st.N = 100.0;  // kN
            st.My = 50.0;  // kN·m
            st.Mz = -10.0; // kN·m
            const double s1 = TSA::Geometry::DiagramGeometry::normalStressMPa(st, 0.01, 0.001, 0.0005);
            st.N = -400.0;
            const double s2 = TSA::Geometry::DiagramGeometry::normalStressMPa(st, 0.01, 0.001, 0.0005);
            // (100/0,01 + 50/0,001 + 10/0,0005) kPa = 80 MPa ; (−400/0,01 − 70 000) kPa = −110 MPa
            TEST_CHECK(std::abs(s1 - 80.0) < 1e-9 && std::abs(s2 + 110.0) < 1e-9, "Test " << testNo << ": σ = N/A ± M/W (" << s1 << " ; " << s2 << " MPa)");
        }
        {
            double smax = 0.0;
            std::string smaxName;
            for (const auto& [key, er] : ru.allElementResults())
            {
                const Section* sec = key.kind == StructuralElementKind::Beam     ? (m.getBeam(key.id) ? &m.getBeam(key.id)->section() : nullptr)
                                     : key.kind == StructuralElementKind::Column ? (m.getColumn(key.id) ? &m.getColumn(key.id)->section() : nullptr)
                                                                                : nullptr;
                if (!sec) continue;
                for (const auto* st : { &er.startForces, &er.endForces })
                {
                    const double sv = std::abs(TSA::Geometry::DiagramGeometry::normalStressMPa(*st, sec->area(), sec->wy(), sec->wz()));
                    if (sv > smax)
                    {
                        smax = sv;
                        smaxName = (key.kind == StructuralElementKind::Beam ? m.getBeam(key.id)->name() : m.getColumn(key.id)->name()) + " — " + sec->name;
                    }
                }
            }
            std::cout << std::fixed << std::setprecision(1) << "  contrainte normale de fibre extrême max (poutres et poteaux, ELU) : " << smax << " MPa (" << smaxName << ")"
                      << std::defaultfloat << std::endl;
        }

        // 3. Haubans : contrôle analytique de la précontrainte (câble bi-encastré : N = T0) et tension dans tous les haubans
        if (!m.cables().empty())
        {
            Model probe;
            const int a = probe.addNode(0, 0, 0), b = probe.addNode(40, 0, 30);
            probe.getNode(a)->setSupport(SupportDefinition::fixed());
            probe.getNode(b)->setSupport(SupportDefinition::fixed());
            const int cid = probe.addCable(a, b, TSA::Model::CableDefinition::stayCablePSS_37x15_7(), "Hauban d'essai");
            probe.getCable(cid)->setInitialTension(2.0e6);
            probe.loadManager().addLoadCase(LoadCase(0, "T0", LoadCaseCategory::Dead, false, 1.0));
            ResultsModel rp;
            const bool okP = solveCase(probe, 0, 0, false, rp, &err);
            const auto* er = rp.getElementResults(StructuralElementKind::Cable, cid);
            const double n0 = er ? er->startForces.N : 0.0;
            std::cout << std::fixed << std::setprecision(2) << "  contrôle analytique hauban bi-encastré : T0 = 2000 kN, N calculé = " << n0 << " kN" << std::defaultfloat << std::endl;
            TEST_CHECK(okP && std::abs(n0 - 2000.0) < 2.0, "Test " << testNo << ": précontrainte d'un câble bi-encastré N = T0 (" << n0 << " kN) " << err);

            double nMin = 1e30, nMaxC = -1e30, fpk = 0.0;
            int counted = 0;
            for (const auto& [id, c] : m.cables())
                if (const auto* r = ru.getElementResults(StructuralElementKind::Cable, id))
                {
                    nMin = std::min(nMin, std::min(r->startForces.N, r->endForces.N));
                    nMaxC = std::max(nMaxC, std::max(r->startForces.N, r->endForces.N));
                    fpk = c.definition().minimumBreakingForce() / 1000.0;
                    ++counted;
                }
            std::cout << std::fixed << std::setprecision(0) << "  haubans sous ELU : " << counted << " résultats, N min " << nMin << " kN, N max " << nMaxC << " kN, Fpk " << fpk << " kN (N max / Fpk = "
                      << std::setprecision(2) << nMaxC / fpk << ")" << std::defaultfloat << std::endl;
            TEST_CHECK(counted == static_cast<int>(m.cables().size()) && nMin > 0.0,
                       "Test " << testNo << ": tous les haubans restent tendus sous ELU (N min = " << nMin << " kN)");
        }

        // 4. Enregistrement au format natif, relecture, recalcul sur le modèle relu
        TSA::Grid::GridManager grids;
        def.setVisible(true);
        def.setShowLabels(true);
        grids.clearAllGrids();
        def.setActive(true);
        if (auto* g = grids.addGrid(def)) grids.setActiveGridId(g->id());
        TSA::IO::TSAFileWriter writer;
        writer.setThumbnail(thumbnailOf(m));
        const QString dir = outDir.isEmpty() ? tmpDir : outDir;
        QDir().mkpath(dir);
        const std::string path = (dir + "/" + pr.file).toStdString();
        TEST_CHECK(writer.saveToFile(path, m, &grids, pr.title, "TSA — projets de démonstration", &err), "Test " << testNo << ": enregistrement .tsa — " << err);
        Model back;
        TSA::Grid::GridManager backGrids;
        std::string name;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, back, &backGrids, "", &name, nullptr, nullptr, &err), "Test " << testNo << ": relecture — " << err);
        const Topology tb = inspect(back);
        const auto& bl = back.loadManager();
        bool tensionsKept = true, visualsKept = true;
        std::size_t textured = 0;
        for (const auto& [id, b] : m.beams())
        {
            const auto* bb = back.getBeam(id);
            visualsKept = visualsKept && bb && TSA::Model::sameVisual(bb->material().visual, b.material().visual);
            textured += b.material().visual.textureName == "formed_concrete" || b.material().visual.textureName == "painted_steel";
        }
        for (const auto& [id, w] : m.walls())
            visualsKept = visualsKept && back.getWall(id) && TSA::Model::sameVisual(back.getWall(id)->material().visual, w.material().visual);
        for (const auto& [id, c] : m.cables())
            visualsKept = visualsKept && back.getCable(id) && TSA::Model::sameVisual(back.getCable(id)->material().visual, c.material().visual);
        for (const auto& [id, c] : m.cables())
        {
            const auto* cb = back.getCable(id);
            tensionsKept = tensionsKept && cb && std::abs(cb->initialTension() - c.initialTension()) < 1.0;
        }
        TEST_CHECK(tb.nodes == t.nodes && tb.beams == t.beams && tb.columns == t.columns && tb.cables == t.cables && tb.supports == t.supports &&
                       back.slabs().size() == m.slabs().size() && back.walls().size() == m.walls().size() && back.foundations().size() == m.foundations().size() &&
                       bl.loadCases().size() == lm.loadCases().size() && bl.combinations().size() == lm.combinations().size() &&
                       bl.memberLoads().size() == lm.memberLoads().size() && bl.nodalLoads().size() == lm.nodalLoads().size() && tensionsKept && visualsKept && name == pr.title &&
                       backGrids.grids().size() == 1,
                   "Test " << testNo << ": relecture identique (éléments, appuis, câbles et tensions, finitions, cas, combinaisons, charges, grille, nom)");
        std::cout << "  finitions d'aspect conservées à la relecture : " << (visualsKept ? "oui" : "NON") << " (" << textured << " poutres en béton coffré ou acier peint)" << std::endl;
        ResultsModel rb;
        const bool okB = solveCase(back, 0, combELU, true, rb, &err);
        const double d0 = sum.maxDisplacement, d1 = rb.summary().maxDisplacement;
        std::cout << "  recalcul ELU du projet relu : déplacement maximal " << std::fixed << std::setprecision(2) << d1 * 1000 << " mm (avant enregistrement "
                  << d0 * 1000 << " mm)" << std::defaultfloat << std::endl;
        TEST_CHECK(okB && std::abs(d1 - d0) < 1e-6 + 1e-6 * d0, "Test " << testNo << ": même résultat après relecture");

        // 5. Notes de calcul (synthèse et note détaillée) produites par le module NDC à partir des résultats ELU
        OccView view;
        view.resize(1280, 800);
        view.show();
        for (int k = 0; k < 20; ++k) QApplication::processEvents();
        view.setModel(&m);
        for (const std::string tpl : { "simplifiee", "detaillee" })
        {
            const bool simple = tpl == "simplifiee";
            TSA::NDC::ReportManager mgr;
            mgr.setModel(&m);
            mgr.setResultsModel(std::make_shared<ResultsModel>(ru));
            mgr.applyTemplate(simple ? TSA::NDC::ReportTemplateType::Minimal : TSA::NDC::ReportTemplateType::BureauEtudes);
            auto& cfg = mgr.configuration();
            cfg.projectTitle = QString::fromUtf8(pr.title);
            cfg.projectDescription = QString::fromUtf8(pr.description);
            cfg.projectNumber = pr.number;
            cfg.documentNumber = QString("%1-NDC-%2").arg(pr.number, simple ? "S" : "D");
            cfg.documentStatus = QStringLiteral("Exemple de démonstration — non destiné à l'exécution");
            cfg.organization = QStringLiteral("TSA — projets de démonstration");
            cfg.engineerName = QStringLiteral("Généré par la suite de tests TSA");
            cfg.clientName = QStringLiteral("—");
            int shots = 0;
            auto resultsForView = std::make_shared<ResultsModel>(ru);
            mgr.setSnapshotProvider([&view, &shots, resultsForView](const QString& type, int w, int h) -> QImage {
                ++shots;
                // Vue réelle de TSA : charges masquées ; « deformed » : résultats du calcul ELU, déformée amplifiée
                view.setLoadsVisible(false);
                view.setResultsModel(type == QLatin1String("deformed") ? resultsForView : nullptr);
                if (type == QLatin1String("deformed") && view.resultsVisual()) view.resultsVisual()->setDeformedVisible(true);
                view.resize(w / 2, h / 2);
                for (int k = 0; k < 10; ++k) QApplication::processEvents();
                view.viewIsometric();
                view.fitModel();
                for (int k = 0; k < 10; ++k) QApplication::processEvents();
                return view.captureViewImage(w, h);
            });
            const auto doc = mgr.generateReport();
            QDir().mkpath(dir + "/notes");
            const QString pdf = dir + "/notes/" + QString(pr.file).replace(".tsa", QString("_note_%1.pdf").arg(QString::fromStdString(tpl)));
            QString perr;
            const bool okPdf = mgr.exportPdf(pdf, &perr);
            std::cout << "  note " << tpl << " : " << doc.chapters.size() << " chapitres, " << doc.allFigures().size() << " figures, " << doc.allTables().size()
                      << " tableaux, " << shots << " vues, PDF " << QFileInfo(pdf).size() / 1024 << " Ko" << (outDir.isEmpty() ? "" : " → " + pdf.toStdString()) << std::endl;
            int pages = 0;
            {
                QFile file(pdf);
                if (file.open(QIODevice::ReadOnly))
                {
                    const QByteArray bytes = file.readAll();
                    pages = int(bytes.count("/Type /Page") - bytes.count("/Type /Pages"));
                }
            }
            std::cout << "  note " << tpl << " : " << pages << " page(s) PDF" << std::endl;
            TEST_CHECK(okPdf && QFileInfo(pdf).size() > 20000 && (simple ? doc.chapters.size() >= 6 : doc.chapters.size() == 18),
                       "Test " << testNo << ": note de calcul " << tpl << " (" << doc.chapters.size() << " chapitres) — " << perr.toStdString());
        }
        if (!outDir.isEmpty()) std::cout << "  projet écrit : " << path << std::endl;
        std::cout << "[PASS] Test " << testNo << ": " << pr.title << std::endl;
        ++passed;
        ++testNo;
    }
    return true;
}

struct Demo
{
    const char* file;
    const char* title;
    void (*build)(Model&, TSA::Grid::GridDefinition&);
};
} // namespace

bool runSuite_DemoModels(int& passed)
{
    const Demo demos[] = {
        { "TSA_Demo_Pont_Treillis_3x48m.tsa", "Pont en treillis — 3 travées de 48 m", buildBridge },
        { "TSA_Demo_Hangar_60x40m.tsa", "Hangar métallique 60 × 40 m", buildHangar },
        { "TSA_Demo_Batiment_R+5.tsa", "Bâtiment R+5 à ossature métallique", buildBuilding },
    };
    const QString outDir = qEnvironmentVariable("TSA_DEMO_DIR");
    QTemporaryDir tmp;
    int testNo = 347;
    for (const auto& demo : demos)
    {
        Model m;
        TSA::Grid::GridDefinition def(demo.title, TSA::Grid::GridType::Cartesian);
        demo.build(m, def);
        const Topology t = inspect(m);
        std::cout << "  " << demo.title << " : " << t.nodes << " nœuds, " << t.beams + t.columns << " barres (" << t.columns << " poteaux/montants, "
                  << t.beams << " poutres, diagonales et contreventements), " << t.supports << " appuis, " << std::fixed << std::setprecision(0)
                  << t.length << " m de barres" << std::defaultfloat << std::endl;
        TEST_CHECK(t.nodes > 150 && t.beams + t.columns > 400, "Test " << testNo << ": modèle complexe (" << demo.title << ")");
        TEST_CHECK(m.slabs().empty() && m.walls().empty() && m.foundations().empty(), "Test " << testNo << ": uniquement des éléments filaires");
        TEST_CHECK(t.duplicateNodes == 0 && t.duplicateBars == 0 && t.zeroLength == 0,
                   "Test " << testNo << ": aucun doublon (nœuds " << t.duplicateNodes << ", barres " << t.duplicateBars << ", nulles " << t.zeroLength << ")");
        TEST_CHECK(t.isolatedNodes == 0 && t.nodesInsideBars == 0,
                   "Test " << testNo << ": connexions (nœuds isolés " << t.isolatedNodes << ", nœuds non connectés sur une barre " << t.nodesInsideBars << ")");
        TEST_CHECK(t.components == 1 && t.supports >= 4, "Test " << testNo << ": structure d'un seul tenant (" << t.components << "), " << t.supports << " appuis");
        // Calcul réel sous poids propre : la structure est stable (matrice non singulière)
        double umax = 0.0;
        std::string err;
        const bool solved = solveSelfWeight(m, &umax, &err); // ajoute le cas G (poids propre), enregistré avec le projet
        std::cout << "  calcul OpenSees sous poids propre : " << (solved ? "réussi" : "ÉCHEC " + err) << ", déplacement maximal " << std::fixed << std::setprecision(1) << umax * 1000 << std::defaultfloat << " mm" << std::endl;
        TEST_CHECK(solved && umax > 0 && umax < 0.2, "Test " << testNo << ": calcul sous poids propre (stabilité) — " << err);
        // Enregistrement au format natif et relecture
        TSA::Grid::GridManager grids;
        def.setVisible(true);
        def.setShowLabels(true);
        grids.clearAllGrids(); // le GridManager crée une grille par défaut : seule celle du modèle est enregistrée
        def.setActive(true);
        if (auto* g = grids.addGrid(def)) grids.setActiveGridId(g->id());
        TSA::IO::TSAFileWriter writer;
        writer.setThumbnail(thumbnailOf(m));
        const QString dir = outDir.isEmpty() ? tmp.path() : outDir;
        QDir().mkpath(dir);
        const std::string path = (dir + "/" + demo.file).toStdString();
        TEST_CHECK(writer.saveToFile(path, m, &grids, demo.title, "TSA — modèles de démonstration", &err), "Test " << testNo << ": enregistrement .tsa — " << err);
        Model back;
        TSA::Grid::GridManager backGrids;
        std::string name;
        TSA::IO::TSAFileReader reader;
        TEST_CHECK(reader.loadFromFile(path, back, &backGrids, "", &name, nullptr, nullptr, &err), "Test " << testNo << ": relecture — " << err);
        const Topology tb = inspect(back);
        std::cout << "  relu : " << tb.nodes << " nœuds, " << tb.beams << " poutres, " << tb.columns << " poteaux, " << tb.supports << " appuis, "
                  << backGrids.grids().size() << " grille(s), nom « " << name << " »" << std::endl;
        TEST_CHECK(tb.nodes == t.nodes && tb.beams == t.beams && tb.columns == t.columns && tb.supports == t.supports && name == demo.title &&
                       backGrids.grids().size() == 1,
                   "Test " << testNo << ": relecture identique (nœuds, barres, appuis, grille, nom)");
        if (!outDir.isEmpty()) std::cout << "  projet écrit : " << path << std::endl;
        std::cout << "[PASS] Test " << testNo << ": " << demo.title << std::endl;
        ++passed;
        ++testNo;
    }
    // Projets complets (tests 350-353)
    return runDemoProjects(passed, outDir, tmp.path());
}
