#include "IsolationManager.h"

#include <AIS_DisplayStatus.hxx>
#include <AIS_ListOfInteractive.hxx>
#include <Standard_Version.hxx>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace TSA::Isolation {

namespace {

bool toAabb(const Bnd_Box &box, Aabb &out) {
  // Boite vide ou ouverte (grille, plan infini...) : pas d'emprise finie.
  if (box.IsVoid() || box.IsOpen())
    return false;
  double x0, y0, z0, x1, y1, z1;
  box.Get(x0, y0, z0, x1, y1, z1);
  out = {{x0, y0, z0}, {x1, y1, z1}, true};
  return true;
}

bool objectAabb(const opencascade::handle<AIS_InteractiveObject> &o, Aabb &out) {
  if (o.IsNull())
    return false;
#if OCC_VERSION_HEX >= 0x070600
  const Bnd_Box box = o->BoundingBox();
#else
  Bnd_Box box;
  o->BoundingBox(box);
#endif
  return toAabb(box, out);
}

Vec3 v3(double x, double y, double z) { return {x, y, z}; }

} // namespace

IsolationManager::IsolationManager(opencascade::handle<AIS_InteractiveContext> context)
    : m_ctx(std::move(context)) {
  m_typeKey = [](const ObjectHandle &o) { return std::string(o->DynamicType()->Name()); };
}

// ---------------------------------------------------------------- helpers

bool IsolationManager::isExcluded(const ObjectHandle &o) const {
  return o.IsNull() || (m_exclude && m_exclude(o));
}

std::vector<IsolationManager::ObjectHandle> IsolationManager::selectedObjects() const {
  std::vector<ObjectHandle> result;
  std::unordered_set<const AIS_InteractiveObject *> seen;
  for (m_ctx->InitSelected(); m_ctx->MoreSelected(); m_ctx->NextSelected()) {
    ObjectHandle o = m_ctx->SelectedInteractive();
    if (!isExcluded(o) && seen.insert(o.get()).second)
      result.push_back(o);
  }
  return result;
}

std::vector<IsolationManager::ObjectHandle> IsolationManager::displayedObjects() const {
  std::vector<ObjectHandle> result;
  AIS_ListOfInteractive list;
  m_ctx->DisplayedObjects(list);
  for (const ObjectHandle &o : list)
    if (!isExcluded(o))
      result.push_back(o);
  return result;
}

// Objets visibles + objets masques par l'isolation en cours (pour undo / invert).
std::vector<IsolationManager::ObjectHandle> IsolationManager::managedObjects() const {
  std::vector<ObjectHandle> result = displayedObjects();
  if (m_stack.empty())
    return result;
  std::unordered_set<const AIS_InteractiveObject *> seen;
  for (const ObjectHandle &o : result)
    seen.insert(o.get());
  for (const Entry &e : m_stack.front().entries) {
    if (e.wasDisplayed && !seen.count(e.object.get()) &&
        m_ctx->DisplayStatus(e.object) == AIS_DS_Erased) {
      result.push_back(e.object);
      seen.insert(e.object.get());
    }
  }
  return result;
}

void IsolationManager::setTransparency(const ObjectHandle &o, double t) {
  if (t <= 1e-6)
    m_ctx->UnsetTransparency(o, false);
  else
    m_ctx->SetTransparency(o, t, false);
}

void IsolationManager::restoreNormal(const ObjectHandle &o) {
  auto it = m_original.find(o.get());
  const double original = it != m_original.end() ? it->second : 0.0;
  if (std::fabs(o->Transparency() - original) > 1e-6)
    setTransparency(o, original);
}

bool IsolationManager::fail(const std::string &msg) {
  m_lastMessage = msg;
  return false;
}

void IsolationManager::finish() {
  m_lastMessage.clear();
  m_ctx->UpdateCurrentViewer();
  if (m_changed)
    m_changed();
}

void IsolationManager::pushSnapshot(const std::string &label) {
  Snapshot s;
  s.label = label;
  for (const ObjectHandle &o : managedObjects())
    s.entries.push_back({o, m_ctx->DisplayStatus(o) == AIS_DS_Displayed, o->Transparency()});
  if (m_stack.empty())
    for (const Entry &e : s.entries)
      m_original[e.object.get()] = e.transparency;
  m_stack.push_back(std::move(s));
}

void IsolationManager::restore(const Snapshot &s) {
  for (const Entry &e : s.entries) {
    const AIS_DisplayStatus st = m_ctx->DisplayStatus(e.object);
    if (st == AIS_DS_None) // objet supprime entre-temps : ne jamais le ressusciter
      continue;
    if (e.wasDisplayed) {
      if (st == AIS_DS_Erased)
        m_ctx->Display(e.object, false);
      if (std::fabs(e.object->Transparency() - e.transparency) > 1e-6)
        setTransparency(e.object, e.transparency);
    } else if (st == AIS_DS_Displayed) {
      m_ctx->Erase(e.object, false);
    }
  }
}

// ------------------------------------------------------------ noyau commun

bool IsolationManager::isolateWhere(const std::string &label, const DecideFn &decide) {
  const std::vector<ObjectHandle> candidates = displayedObjects();
  if (candidates.empty())
    return fail("Aucun objet visible a isoler.");

  std::vector<Decision> decisions;
  decisions.reserve(candidates.size());
  size_t matches = 0, misses = 0;
  for (const ObjectHandle &o : candidates) {
    decisions.push_back(decide(o));
    if (decisions.back() == Decision::Match)
      ++matches;
    else if (decisions.back() == Decision::NoMatch)
      ++misses;
  }
  if (matches == 0)
    return fail("Aucun objet ne correspond au critere d'isolation.");
  if (misses == 0)
    return fail("Tous les objets visibles correspondent deja au critere.");

  pushSnapshot(label);
  for (size_t i = 0; i < candidates.size(); ++i) {
    const ObjectHandle &o = candidates[i];
    if (decisions[i] == Decision::NoMatch) {
      if (m_othersMode == OthersMode::Hide)
        m_ctx->Erase(o, false);
      else
        setTransparency(o, std::max(o->Transparency(), m_ghost));
    } else if (decisions[i] == Decision::Match) {
      restoreNormal(o); // un objet precedemment estompe redevient opaque
    }
  }
  finish();
  return true;
}

bool IsolationManager::hideWhere(const std::string &label, const DecideFn &decide) {
  const std::vector<ObjectHandle> candidates = displayedObjects();
  std::vector<ObjectHandle> toHide;
  for (const ObjectHandle &o : candidates)
    if (decide(o) == Decision::Match)
      toHide.push_back(o);
  if (toHide.empty())
    return fail("Aucun objet a masquer.");
  pushSnapshot(label);
  for (const ObjectHandle &o : toHide)
    m_ctx->Erase(o, false);
  finish();
  return true;
}

bool IsolationManager::geometric(const std::string &label,
                                 const std::function<bool(const Aabb &)> &test) {
  return isolateWhere(label, [&](const ObjectHandle &o) {
    Aabb box;
    if (!objectAabb(o, box))
      return Decision::Ignore; // pas d'emprise finie : laisse tel quel
    return test(box) ? Decision::Match : Decision::NoMatch;
  });
}

// -------------------------------------------------------- par selection

bool IsolationManager::isolateSelection() {
  const std::vector<ObjectHandle> sel = selectedObjects();
  if (sel.empty())
    return fail("Selectionnez d'abord un ou plusieurs objets.");
  std::unordered_set<const AIS_InteractiveObject *> ids;
  for (const ObjectHandle &o : sel)
    ids.insert(o.get());
  return isolateWhere("Selection (" + std::to_string(sel.size()) + ")",
                      [&](const ObjectHandle &o) {
                        return ids.count(o.get()) ? Decision::Match : Decision::NoMatch;
                      });
}

bool IsolationManager::isolateSameType() {
  const std::vector<ObjectHandle> sel = selectedObjects();
  if (sel.empty())
    return fail("Selectionnez d'abord un objet du type a isoler.");
  std::unordered_set<std::string> keys;
  for (const ObjectHandle &o : sel)
    keys.insert(m_typeKey(o));
  return isolateWhere("Type", [&](const ObjectHandle &o) {
    return keys.count(m_typeKey(o)) ? Decision::Match : Decision::NoMatch;
  });
}

bool IsolationManager::hideSelection() {
  const std::vector<ObjectHandle> sel = selectedObjects();
  if (sel.empty())
    return fail("Selectionnez d'abord un ou plusieurs objets.");
  std::unordered_set<const AIS_InteractiveObject *> ids;
  for (const ObjectHandle &o : sel)
    ids.insert(o.get());
  const bool ok = hideWhere("Masquage (" + std::to_string(sel.size()) + ")",
                            [&](const ObjectHandle &o) {
                              return ids.count(o.get()) ? Decision::Match : Decision::NoMatch;
                            });
  if (ok) {
    m_ctx->ClearSelected(true);
  }
  return ok;
}

bool IsolationManager::invert() {
  if (m_stack.empty())
    return fail("Aucune isolation en cours a inverser.");
  const std::vector<ObjectHandle> managed = managedObjects();
  pushSnapshot("Inversion");
  for (const ObjectHandle &o : managed) {
    if (m_ctx->DisplayStatus(o) == AIS_DS_Displayed)
      m_ctx->Erase(o, false);
    else
      m_ctx->Display(o, false);
  }
  finish();
  return true;
}

// ----------------------------------------------------------- geometrique

bool IsolationManager::isolateOnWorkPlane(const gp_Pln &plane, double tolerance,
                                          MatchMode mode) {
  const gp_Pnt p = plane.Location();
  const gp_Dir n = plane.Axis().Direction();
  const Slab slab = makeSlab(v3(p.X(), p.Y(), p.Z()), v3(n.X(), n.Y(), n.Z()),
                             tolerance, tolerance);
  return geometric("Plan de travail",
                   [&](const Aabb &b) { return matchSlab(b, slab, mode); });
}

bool IsolationManager::isolateBySection(const gp_Pln &plane, double below, double above,
                                        MatchMode mode) {
  const gp_Pnt p = plane.Location();
  const gp_Dir n = plane.Axis().Direction();
  const Slab slab = makeSlab(v3(p.X(), p.Y(), p.Z()), v3(n.X(), n.Y(), n.Z()), below, above);
  return geometric("Coupe", [&](const Aabb &b) { return matchSlab(b, slab, mode); });
}

bool IsolationManager::isolateByProjection(const gp_Ax3 &frame, double umin, double vmin,
                                           double umax, double vmax, MatchMode mode) {
  const gp_Pnt p = frame.Location();
  const gp_Dir x = frame.XDirection();
  const gp_Dir y = frame.YDirection();
  const Frame f{v3(p.X(), p.Y(), p.Z()), v3(x.X(), x.Y(), x.Z()), v3(y.X(), y.Y(), y.Z())};
  const Rect2 window{umin, vmin, umax, vmax};
  return geometric("Projection",
                   [&](const Aabb &b) { return matchProjection(b, f, window, mode); });
}

bool IsolationManager::isolateByBox(const Bnd_Box &region, MatchMode mode) {
  Aabb r;
  if (!toAabb(region, r))
    return fail("Volume d'isolation invalide.");
  return geometric("Volume", [&](const Aabb &b) { return matchBox(b, r, mode); });
}

// ------------------------------------------------------------- historique

bool IsolationManager::undo() {
  if (m_stack.empty())
    return fail("Aucune isolation a annuler.");
  restore(m_stack.back());
  m_stack.pop_back();
  if (m_stack.empty())
    m_original.clear();
  finish();
  return true;
}

void IsolationManager::showAll() {
  if (m_stack.empty())
    return;
  restore(m_stack.front());
  m_stack.clear();
  m_original.clear();
  finish();
}

std::string IsolationManager::description() const {
  std::string out;
  for (const Snapshot &s : m_stack) {
    if (!out.empty())
      out += " > ";
    out += s.label;
  }
  return out;
}

// ------------------------------------------------------------ plan de coupe

void IsolationManager::setClipPlane(const gp_Pln &plane, bool capping) {
  clearClipPlane();
  m_clip = new Graphic3d_ClipPlane(plane);
  m_clip->SetCapping(capping);
  m_clip->SetOn(true);
  AIS_ListOfInteractive all; // affiches ET masques : une coupe survit a un showAll()
  m_ctx->ObjectsInside(all);
  for (const ObjectHandle &o : all)
    if (!isExcluded(o))
      o->AddClipPlane(m_clip);
  m_ctx->UpdateCurrentViewer();
  if (m_changed)
    m_changed();
}

void IsolationManager::clearClipPlane() {
  if (m_clip.IsNull())
    return;
  AIS_ListOfInteractive all;
  m_ctx->ObjectsInside(all);
  for (const ObjectHandle &o : all)
    o->RemoveClipPlane(m_clip);
  m_clip.Nullify();
  m_ctx->UpdateCurrentViewer();
  if (m_changed)
    m_changed();
}

void IsolationManager::onObjectDisplayed(const ObjectHandle &object) {
  if (!m_clip.IsNull() && !isExcluded(object))
    object->AddClipPlane(m_clip);
}

} // namespace TSA::Isolation
