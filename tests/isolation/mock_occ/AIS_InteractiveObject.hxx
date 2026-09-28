#pragma once
#include "Standard_Handle_Mock.hxx"
#include "Bnd_Box.hxx"
#include "Graphic3d_ClipPlane.hxx"
#include <algorithm>
#include <vector>
class AIS_InteractiveObject {
public:
  virtual ~AIS_InteractiveObject() = default;
  Bnd_Box box; double transp{0.0}; Standard_Type type{"AIS_Shape"}; std::vector<Graphic3d_ClipPlane *> clips;
  const Bnd_Box &BoundingBox() { return box; }
  double Transparency() const { return transp; }
  opencascade::handle<Standard_Type> DynamicType() const { return opencascade::handle<Standard_Type>(new Standard_Type(type)); /* fuite volontaire (mock) */ }
  void AddClipPlane(const opencascade::handle<Graphic3d_ClipPlane> &p) { if (std::find(clips.begin(), clips.end(), p.get()) == clips.end()) clips.push_back(p.get()); }
  void RemoveClipPlane(const opencascade::handle<Graphic3d_ClipPlane> &p) { clips.erase(std::remove(clips.begin(), clips.end(), p.get()), clips.end()); }
};
