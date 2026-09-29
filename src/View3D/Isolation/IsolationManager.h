// Systeme d'isolation 3D pour TSA (OpenCASCADE AIS).
//
// Isole n'importe quel objet AIS (noeud, poutre, poteau, dalle...) selon :
//   - la selection, ou le meme type que la selection ;
//   - le plan de travail ;
//   - une coupe (tranche entre deux plans paralleles) ;
//   - une projection (fenetre 2D dans un repere de vue) ;
//   - un volume (boite 3D).
// Les isolations s'empilent (retour arriere possible) ; showAll() restaure
// l'etat initial. Les objets "auxiliaires" (grille, reperes, plans...) sont
// exclus via setExcludePredicate() et ne sont jamais masques.
#pragma once

#include "IsolationGeometry.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_ClipPlane.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace TSA::Isolation {

enum class OthersMode {
  Hide,  // les autres objets sont masques
  Ghost  // les autres objets sont estompes (transparence) ; sans effet sur les fils
};

class IsolationManager {
public:
  using ObjectHandle = opencascade::handle<AIS_InteractiveObject>;
  using ObjectPredicate = std::function<bool(const ObjectHandle &)>;
  using TypeKeyFunction = std::function<std::string(const ObjectHandle &)>;
  using ChangedCallback = std::function<void()>;

  explicit IsolationManager(opencascade::handle<AIS_InteractiveContext> context);

  // --- Configuration ---
  void setOthersMode(OthersMode mode) { m_othersMode = mode; }
  void setGhostTransparency(double t) { m_ghost = t < 0.0 ? 0.0 : (t > 0.95 ? 0.95 : t); }
  // Objets jamais touches (grille, trihedre, plan de travail visuel...).
  void setExcludePredicate(ObjectPredicate p) { m_exclude = std::move(p); }
  // Categorie metier d'un objet (ex. "beam", "column"). Defaut : type dynamique OCC.
  void setTypeKeyFunction(TypeKeyFunction f) { m_typeKey = std::move(f); }
  // Appele apres chaque changement (mise a jour de la barre d'etat, des menus...).
  void setChangedCallback(ChangedCallback cb) { m_changed = std::move(cb); }

  // --- Isolation par selection ---
  bool isolateSelection();
  bool isolateSameType();
  bool hideSelection();
  bool invert(); // echange visibles / masques par l'isolation en cours

  // --- Isolation geometrique (tous types d'objets) ---
  // Objets situes sur le plan de travail (tranche d'epaisseur 2*tolerance).
  bool isolateOnWorkPlane(const gp_Pln &plane, double tolerance,
                          MatchMode mode = MatchMode::Intersects);
  // Coupe : tranche de `below` sous le plan a `above` au-dessus (le long de la normale).
  bool isolateBySection(const gp_Pln &plane, double below, double above,
                        MatchMode mode = MatchMode::Intersects);
  // Projection : fenetre [umin,umax]x[vmin,vmax] dans le repere `frame`
  // (typiquement le repere de la vue courante).
  bool isolateByProjection(const gp_Ax3 &frame, double umin, double vmin,
                           double umax, double vmax,
                           MatchMode mode = MatchMode::Intersects);
  bool isolateByBox(const Bnd_Box &region, MatchMode mode = MatchMode::Intersects);

  // --- Historique ---
  bool undo();    // annule la derniere isolation
  void showAll(); // restaure l'etat d'avant la premiere isolation
  bool isIsolating() const { return !m_stack.empty(); }
  size_t depth() const { return m_stack.size(); }
  std::string description() const; // ex. "Selection (3) > Coupe"
  const std::string &lastMessage() const { return m_lastMessage; } // motif du dernier refus

  // --- Plan de coupe visuel (clipping) ---
  void setClipPlane(const gp_Pln &plane, bool capping = true);
  void clearClipPlane();
  bool hasClipPlane() const { return !m_clip.IsNull(); }
  // A appeler quand l'application affiche un nouvel objet pendant qu'une coupe est active.
  void onObjectDisplayed(const ObjectHandle &object);

private:
  enum class Decision { Match, NoMatch, Ignore };
  using DecideFn = std::function<Decision(const ObjectHandle &)>;

  struct Entry {
    ObjectHandle object;
    bool wasDisplayed;
    double transparency;
  };
  struct Snapshot {
    std::string label;
    std::vector<Entry> entries;
  };

  bool isExcluded(const ObjectHandle &o) const;
  std::vector<ObjectHandle> selectedObjects() const;
  std::vector<ObjectHandle> displayedObjects() const;
  std::vector<ObjectHandle> managedObjects() const;

  bool isolateWhere(const std::string &label, const DecideFn &decide);
  bool hideWhere(const std::string &label, const DecideFn &decide);
  bool geometric(const std::string &label, const std::function<bool(const Aabb &)> &test);
  void pushSnapshot(const std::string &label);
  void restore(const Snapshot &s);
  void setTransparency(const ObjectHandle &o, double t);
  void restoreNormal(const ObjectHandle &o);
  bool fail(const std::string &msg);
  void finish();

  opencascade::handle<AIS_InteractiveContext> m_ctx;
  opencascade::handle<Graphic3d_ClipPlane> m_clip;
  OthersMode m_othersMode{OthersMode::Hide};
  double m_ghost{0.85};
  ObjectPredicate m_exclude;
  TypeKeyFunction m_typeKey;
  ChangedCallback m_changed;
  std::vector<Snapshot> m_stack;
  std::unordered_map<const AIS_InteractiveObject *, double> m_original; // transparence d'origine
  std::string m_lastMessage;
};

} // namespace TSA::Isolation
