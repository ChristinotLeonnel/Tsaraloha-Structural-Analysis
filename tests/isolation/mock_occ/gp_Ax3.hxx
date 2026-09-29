#pragma once
#include "gp_Pln.hxx"
struct gp_Ax3 { gp_Pnt loc; gp_Dir xd, yd; gp_Pnt Location() const { return loc; } gp_Dir XDirection() const { return xd; } gp_Dir YDirection() const { return yd; } };
