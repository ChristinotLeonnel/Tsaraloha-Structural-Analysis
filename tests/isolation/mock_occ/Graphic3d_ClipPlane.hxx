#pragma once
#include "gp_Pln.hxx"
struct Graphic3d_ClipPlane { gp_Pln pln; bool cap{false}, on{false};
  explicit Graphic3d_ClipPlane(const gp_Pln &p) : pln(p) {}
  void SetCapping(bool c) { cap = c; } void SetOn(bool o) { on = o; } };
