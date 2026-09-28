#pragma once
#include "AIS_InteractiveObject.hxx"
#include "AIS_ListOfInteractive.hxx"
#include "AIS_DisplayStatus.hxx"
#include <map>
class AIS_InteractiveContext {
public:
  using H = opencascade::handle<AIS_InteractiveObject>;
  std::vector<H> order; std::map<AIS_InteractiveObject *, AIS_DisplayStatus> st; std::vector<H> sel; size_t it{0}; int updates{0};
  // --- API de test ---
  void Add(H o) { order.push_back(o); st[o.get()] = AIS_DS_Displayed; }
  void RemoveObj(H o) { st.erase(o.get()); order.erase(std::remove(order.begin(), order.end(), o), order.end()); sel.erase(std::remove(sel.begin(), sel.end(), o), sel.end()); }
  void Select(std::vector<H> v) { sel = v; }
  // --- API imitant AIS_InteractiveContext ---
  void DisplayedObjects(AIS_ListOfInteractive &l) const { for (auto &o : order) if (st.at(o.get()) == AIS_DS_Displayed) l.push_back(o); }
  void ObjectsInside(AIS_ListOfInteractive &l) const { for (auto &o : order) l.push_back(o); }
  AIS_DisplayStatus DisplayStatus(const H &o) const { auto i = st.find(o.get()); return i == st.end() ? AIS_DS_None : i->second; }
  void Display(const H &o, bool) { if (st.count(o.get())) st[o.get()] = AIS_DS_Displayed; }
  void Erase(const H &o, bool) { if (st.count(o.get())) { st[o.get()] = AIS_DS_Erased; sel.erase(std::remove(sel.begin(), sel.end(), o), sel.end()); } }
  void SetTransparency(const H &o, double t, bool) { o->transp = t; }
  void UnsetTransparency(const H &o, bool) { o->transp = 0.0; }
  void InitSelected() { it = 0; } bool MoreSelected() const { return it < sel.size(); } void NextSelected() { ++it; }
  H SelectedInteractive() const { return sel[it]; }
  void ClearSelected(bool) { sel.clear(); }
  void UpdateCurrentViewer() { ++updates; }
};
