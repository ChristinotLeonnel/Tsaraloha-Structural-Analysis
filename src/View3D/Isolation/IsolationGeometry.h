// Geometrie pure de l'isolation 3D : aucune dependance (ni Qt, ni OpenCASCADE).
// Permet de tester les criteres (plan de travail, coupe, projection, volume)
// independamment du moteur de rendu.
#pragma once

#include <algorithm>
#include <cmath>

namespace TSA::Isolation {

struct Vec3 {
  double x{0}, y{0}, z{0};
};

inline double dot(const Vec3 &a, const Vec3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 operator-(const Vec3 &a, const Vec3 &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// Boite englobante alignee sur les axes. `valid == false` : objet sans
// emprise finie (grille, repere, plan infini...) -> jamais teste geometriquement.
struct Aabb {
  Vec3 min, max;
  bool valid{false};

  Vec3 center() const {
    return {(min.x + max.x) * 0.5, (min.y + max.y) * 0.5, (min.z + max.z) * 0.5};
  }
  Vec3 halfExtent() const {
    return {(max.x - min.x) * 0.5, (max.y - min.y) * 0.5, (max.z - min.z) * 0.5};
  }
};

// Intersects : la boite touche le critere. Inside : elle est entierement dedans.
enum class MatchMode { Intersects, Inside };

struct Interval {
  double lo{0}, hi{0};
};

// Projection exacte d'une AABB sur un axe UNITAIRE, mesuree depuis `origin`.
inline Interval projectOnAxis(const Aabb &b, const Vec3 &origin,
                              const Vec3 &axis) {
  const Vec3 h = b.halfExtent();
  const double c = dot(b.center() - origin, axis);
  const double r = h.x * std::fabs(axis.x) + h.y * std::fabs(axis.y) +
                   h.z * std::fabs(axis.z);
  return {c - r, c + r};
}

inline bool intervalMatches(const Interval &v, double lo, double hi,
                            MatchMode mode, double tol) {
  if (mode == MatchMode::Inside)
    return v.lo >= lo - tol && v.hi <= hi + tol;
  return v.hi >= lo - tol && v.lo <= hi + tol;
}

// Tranche entre deux plans paralleles : offsets [below, above] le long de la
// normale UNITAIRE, mesures depuis `origin`. Sert au plan de travail (tranche
// tres fine) et a la coupe.
struct Slab {
  Vec3 origin;
  Vec3 normal;
  double d0{0}, d1{0};
};

inline Slab makeSlab(const Vec3 &origin, const Vec3 &unitNormal, double below,
                     double above) {
  // below/above : distances positives de part et d'autre du plan.
  double lo = -std::fabs(below), hi = std::fabs(above);
  return {origin, unitNormal, lo, hi};
}

inline bool matchSlab(const Aabb &b, const Slab &s, MatchMode mode,
                      double tol = 1e-9) {
  if (!b.valid)
    return false;
  return intervalMatches(projectOnAxis(b, s.origin, s.normal), s.d0, s.d1, mode,
                         tol);
}

// Fenetre de projection : repere orthonorme (origin, xdir, ydir) -> rectangle
// [umin,umax] x [vmin,vmax]. La boite est projetee le long de la normale.
struct Frame {
  Vec3 origin, xdir, ydir;
};
struct Rect2 {
  double umin{0}, vmin{0}, umax{0}, vmax{0};
};

inline Rect2 normalized(Rect2 r) {
  if (r.umin > r.umax)
    std::swap(r.umin, r.umax);
  if (r.vmin > r.vmax)
    std::swap(r.vmin, r.vmax);
  return r;
}

inline bool matchProjection(const Aabb &b, const Frame &f, Rect2 window,
                            MatchMode mode, double tol = 1e-9) {
  if (!b.valid)
    return false;
  window = normalized(window);
  const Interval u = projectOnAxis(b, f.origin, f.xdir);
  const Interval v = projectOnAxis(b, f.origin, f.ydir);
  return intervalMatches(u, window.umin, window.umax, mode, tol) &&
         intervalMatches(v, window.vmin, window.vmax, mode, tol);
}

// Volume (boite) : isolation par region 3D.
inline bool matchBox(const Aabb &b, const Aabb &region, MatchMode mode,
                     double tol = 1e-9) {
  if (!b.valid || !region.valid)
    return false;
  auto axis = [&](double bl, double bh, double rl, double rh) {
    return intervalMatches({bl, bh}, rl, rh, mode, tol);
  };
  return axis(b.min.x, b.max.x, region.min.x, region.max.x) &&
         axis(b.min.y, b.max.y, region.min.y, region.max.y) &&
         axis(b.min.z, b.max.z, region.min.z, region.max.z);
}

} // namespace TSA::Isolation
