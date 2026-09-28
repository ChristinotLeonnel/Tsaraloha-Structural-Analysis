// Teste IsolationManager avec un MOCK d'OpenCASCADE (voir mock_occ/).
// Compilation (une seule ligne) :
// g++ -std=c++17 -Itests/isolation/mock_occ -Isrc/View3D/Isolation tests/isolation/test_isolation_manager.cpp src/View3D/Isolation/IsolationManager.cpp
#include "IsolationManager.h"
#include <cstdio>
#include <memory>

using namespace TSA::Isolation;
using H = opencascade::handle<AIS_InteractiveObject>;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("ECHEC l.%d : %s\n", __LINE__, #c); ++failures; } } while (0)

struct Scene {
  opencascade::handle<AIS_InteractiveContext> ctx{new AIS_InteractiveContext};
  H beam3, beam6, col, slab3, node, grid;
  static H make(Bnd_Box b, const char *type) { auto *o = new AIS_InteractiveObject; o->box = b; o->type = Standard_Type{type}; return H(o); }
  Scene() {
    beam3 = make(Bnd_Box::Make(0, 0, 3, 5, 0, 3), "Beam");
    beam6 = make(Bnd_Box::Make(0, 0, 6, 5, 0, 6), "Beam");
    col   = make(Bnd_Box::Make(0, 0, 0, 0, 0, 6), "Column");
    slab3 = make(Bnd_Box::Make(0, 0, 3, 5, 5, 3), "Slab");
    node  = make(Bnd_Box::Make(9, 9, 9, 9, 9, 9), "Node");
    grid  = make(Bnd_Box{}, "Grid"); grid->box.void_ = false; grid->box.open_ = true; // emprise infinie
    for (H o : {beam3, beam6, col, slab3, node, grid}) ctx->Add(o);
  }
  bool shown(const H &o) const { return ctx->DisplayStatus(o) == AIS_DS_Displayed; }
};
static gp_Pln planeZ(double z) { return {{0, 0, z}, {0, 0, 1}}; }

int main() {
  { // Plan de travail z=3 : poutre, poteau, dalle restent ; poutre a 6 m et noeud partent ; grille intacte
    Scene s; IsolationManager m(s.ctx); int changes = 0; m.setChangedCallback([&] { ++changes; });
    CHECK(m.isolateOnWorkPlane(planeZ(3), 0.01));
    CHECK(s.shown(s.beam3) && s.shown(s.col) && s.shown(s.slab3) && s.shown(s.grid));
    CHECK(!s.shown(s.beam6) && !s.shown(s.node));
    CHECK(m.isIsolating() && m.description() == "Plan de travail" && changes == 1);
    // Isolation imbriquee : tranche stricte -> seules les pieces entierement dans [2.9,3.1]
    CHECK(m.isolateBySection(planeZ(3), 0.1, 0.1, MatchMode::Inside));
    CHECK(s.shown(s.beam3) && s.shown(s.slab3) && !s.shown(s.col));
    CHECK(m.depth() == 2 && m.description() == "Plan de travail > Coupe");
    CHECK(m.undo());
    CHECK(s.shown(s.col) && !s.shown(s.beam6));           // retour a l'etape 1
    m.showAll();
    for (H o : {s.beam3, s.beam6, s.col, s.slab3, s.node, s.grid}) CHECK(s.shown(o));
    CHECK(!m.isIsolating() && m.description().empty());
  }
  { // Refus : aucun objet / tous les objets
    Scene s; IsolationManager m(s.ctx);
    CHECK(!m.isolateOnWorkPlane(planeZ(100), 0.01)); CHECK(!m.lastMessage().empty()); CHECK(!m.isIsolating());
    CHECK(!m.isolateBySection(planeZ(0), 100, 100)); CHECK(!m.isIsolating());       // tout correspond deja
    CHECK(!m.isolateSelection()); CHECK(!m.undo()); CHECK(!m.invert());
    for (H o : {s.beam3, s.beam6, s.col, s.slab3, s.node}) CHECK(s.shown(o));
  }
  { // Selection, meme type, masquage, inversion
    Scene s; IsolationManager m(s.ctx);
    s.ctx->Select({s.beam3});
    CHECK(m.isolateSameType());
    CHECK(s.shown(s.beam3) && s.shown(s.beam6) && !s.shown(s.col) && !s.shown(s.slab3) && !s.shown(s.node));
    CHECK(m.invert());
    CHECK(!s.shown(s.beam3) && !s.shown(s.beam6) && s.shown(s.col) && s.shown(s.slab3) && s.shown(s.node));
    CHECK(m.undo());
    CHECK(s.shown(s.beam3) && !s.shown(s.col));
    m.showAll();
    s.ctx->Select({s.col, s.node});
    CHECK(m.isolateSelection() && !s.shown(s.beam3) && s.shown(s.col) && s.shown(s.node));
    m.showAll();
    s.ctx->Select({s.col});
    CHECK(m.hideSelection() && !s.shown(s.col) && s.shown(s.beam3));
    m.showAll(); CHECK(s.shown(s.col));
  }
  { // Type metier fourni par l'application
    Scene s; IsolationManager m(s.ctx);
    m.setTypeKeyFunction([&](const H &o) { return std::string((o == s.beam3 || o == s.col) ? "structure" : "autre"); });
    s.ctx->Select({s.beam3});
    CHECK(m.isolateSameType() && s.shown(s.beam3) && s.shown(s.col) && !s.shown(s.beam6));
  }
  { // Mode estompe : rien de masque, transparence appliquee puis restauree
    Scene s; IsolationManager m(s.ctx); m.setOthersMode(OthersMode::Ghost); m.setGhostTransparency(0.8);
    s.beam6->transp = 0.3;                          // transparence d'origine a preserver
    CHECK(m.isolateOnWorkPlane(planeZ(3), 0.01));
    CHECK(s.shown(s.beam6) && s.beam6->transp > 0.79 && s.beam3->transp == 0.0);
    CHECK(m.isolateSelection() == false);           // rien de selectionne
    s.ctx->Select({s.beam6});
    CHECK(m.isolateSelection());                    // beam6 gardee -> redevient a sa transparence d'origine
    CHECK(std::abs(s.beam6->transp - 0.3) < 1e-9);
    m.showAll(); CHECK(std::abs(s.beam6->transp - 0.3) < 1e-9 && s.beam3->transp == 0.0);
  }
  { // Objet supprime pendant l'isolation : jamais ressuscite
    Scene s; IsolationManager m(s.ctx);
    CHECK(m.isolateOnWorkPlane(planeZ(3), 0.01));
    s.ctx->RemoveObj(s.beam6);
    m.showAll();
    CHECK(s.ctx->DisplayStatus(s.beam6) == AIS_DS_None && s.shown(s.node));
  }
  { // Objet exclu (repere auxiliaire) : jamais masque, meme s'il a une emprise finie
    Scene s; IsolationManager m(s.ctx); m.setExcludePredicate([&](const H &o) { return o == s.node; });
    CHECK(m.isolateOnWorkPlane(planeZ(3), 0.01));
    CHECK(s.shown(s.node) && !s.shown(s.beam6));
  }
  { // Projection et volume
    Scene s; IsolationManager m(s.ctx);
    gp_Ax3 xy{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    CHECK(m.isolateByProjection(xy, -1, -1, 6, 6, MatchMode::Inside));
    CHECK(s.shown(s.beam6) && s.shown(s.slab3) && s.shown(s.col) && !s.shown(s.node)); // noeud a (9,9) hors fenetre
    m.showAll();
    CHECK(m.isolateByBox(Bnd_Box::Make(8, 8, 8, 10, 10, 10), MatchMode::Inside));
    CHECK(s.shown(s.node) && !s.shown(s.beam3));
    m.showAll(); Bnd_Box bad; CHECK(!m.isolateByBox(bad));
  }
  { // Plan de coupe visuel : applique a TOUS les objets (meme masques), sauf exclus ; retire proprement
    Scene s; IsolationManager m(s.ctx); m.setExcludePredicate([&](const H &o) { return o == s.grid; });
    CHECK(m.isolateOnWorkPlane(planeZ(3), 0.01));   // beam6 masque
    m.setClipPlane(planeZ(3));
    CHECK(m.hasClipPlane() && s.beam6->clips.size() == 1 && s.beam3->clips.size() == 1 && s.grid->clips.empty());
    m.showAll(); CHECK(s.beam6->clips.size() == 1);  // la coupe survit a showAll
    H late = Scene::make(Bnd_Box::Make(0, 0, 0, 1, 1, 1), "Beam"); s.ctx->Add(late);
    m.onObjectDisplayed(late); CHECK(late->clips.size() == 1);
    m.onObjectDisplayed(late); CHECK(late->clips.size() == 1);   // pas de doublon
    m.clearClipPlane();
    CHECK(!m.hasClipPlane() && s.beam3->clips.empty() && late->clips.empty());
  }
  std::printf(failures ? "%d ECHEC(S)\n" : "Gestionnaire : tous les tests passent\n", failures);
  return failures ? 1 : 0;
}
