#pragma once
struct gp_Pnt { double x, y, z; double X() const { return x; } double Y() const { return y; } double Z() const { return z; } };
struct gp_Dir { double x, y, z; double X() const { return x; } double Y() const { return y; } double Z() const { return z; } };
struct gp_Ax1 { gp_Dir d; gp_Dir Direction() const { return d; } };
struct gp_Pln { gp_Pnt loc; gp_Dir n; gp_Pnt Location() const { return loc; } gp_Ax1 Axis() const { return {n}; } };
