// g++ -std=c++17 -Isrc/View3D/Isolation tests/isolation/test_isolation_geometry.cpp
#include "IsolationGeometry.h"
#include <cstdio>
#include <cstdlib>

using namespace TSA::Isolation;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("ECHEC l.%d : %s\n", __LINE__, #c); ++failures; } } while (0)

static Aabb box(double x0, double y0, double z0, double x1, double y1, double z1) {
  return {{x0, y0, z0}, {x1, y1, z1}, true};
}

int main() {
  // --- Plan de travail / coupe horizontale z = 3 ---
  const Vec3 o{0, 0, 3}, nz{0, 0, 1};
  Slab wp = makeSlab(o, nz, 0.01, 0.01);
  CHECK(matchSlab(box(0, 0, 3, 5, 0, 3), wp, MatchMode::Inside));      // poutre au niveau
  CHECK(matchSlab(box(0, 0, 0, 0, 0, 6), wp, MatchMode::Intersects));  // poteau traversant
  CHECK(!matchSlab(box(0, 0, 0, 0, 0, 6), wp, MatchMode::Inside));
  CHECK(!matchSlab(box(0, 0, 6, 5, 0, 6), wp, MatchMode::Intersects)); // poutre a 6 m
  CHECK(matchSlab(box(0, 0, 0, 1, 1, 3), wp, MatchMode::Intersects));  // touche le plan
  Slab cut = makeSlab(o, nz, 3.0, 0.0);                                // tranche [0,3]
  CHECK(matchSlab(box(0, 0, 1, 1, 1, 2), cut, MatchMode::Inside));
  CHECK(!matchSlab(box(0, 0, 1, 1, 1, 4), cut, MatchMode::Inside));
  CHECK(matchSlab(box(0, 0, 1, 1, 1, 4), cut, MatchMode::Intersects));
  Slab flipped = makeSlab(o, {0, 0, -1}, 0.0, 3.0);                    // meme tranche, normale inversee
  CHECK(matchSlab(box(0, 0, 1, 1, 1, 2), flipped, MatchMode::Inside));
  // Plan oblique
  const double s = 1.0 / std::sqrt(2.0);
  Slab obl = makeSlab({0, 0, 0}, {s, s, 0}, 0.1, 0.1);
  CHECK(matchSlab(box(0, 0, 0, 1, 1, 1), obl, MatchMode::Intersects));
  CHECK(!matchSlab(box(0, 0, 0, 1, 1, 1), obl, MatchMode::Inside));
  CHECK(!matchSlab(box(2, 2, 0, 3, 3, 1), obl, MatchMode::Intersects));

  // --- Projection ---
  Frame xy{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  Rect2 win{0, 0, 10, 10};
  CHECK(matchProjection(box(2, 2, -50, 3, 3, 50), xy, win, MatchMode::Inside));   // profondeur ignoree
  CHECK(matchProjection(box(9, 9, 0, 12, 12, 1), xy, win, MatchMode::Intersects));
  CHECK(!matchProjection(box(9, 9, 0, 12, 12, 1), xy, win, MatchMode::Inside));
  CHECK(!matchProjection(box(20, 20, 0, 21, 21, 1), xy, win, MatchMode::Intersects));
  CHECK(matchProjection(box(2, 2, 0, 3, 3, 1), xy, {10, 10, 0, 0}, MatchMode::Inside)); // rect inverse
  Frame side{{0, 0, 0}, {0, 1, 0}, {0, 0, 1}}; // vue depuis +X
  CHECK(matchProjection(box(100, 2, 2, 200, 3, 3), side, win, MatchMode::Inside));

  // --- Volume ---
  Aabb region = box(0, 0, 0, 10, 10, 10);
  CHECK(matchBox(box(1, 1, 1, 2, 2, 2), region, MatchMode::Inside));
  CHECK(matchBox(box(9, 9, 9, 11, 11, 11), region, MatchMode::Intersects));
  CHECK(!matchBox(box(11, 0, 0, 12, 1, 1), region, MatchMode::Intersects));

  // --- Objet sans emprise finie ---
  Aabb none; // valid=false
  CHECK(!matchSlab(none, wp, MatchMode::Intersects));
  CHECK(!matchProjection(none, xy, win, MatchMode::Intersects));

  // --- projectOnAxis exact vs. force brute sur les 8 coins ---
  unsigned seed = 12345;
  auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / double(1 << 24) * 2.0 - 1.0; };
  for (int i = 0; i < 2000; ++i) {
    Aabb b = box(rnd() * 5, rnd() * 5, rnd() * 5, 0, 0, 0);
    b.max = {b.min.x + std::fabs(rnd()) * 5, b.min.y + std::fabs(rnd()) * 5, b.min.z + std::fabs(rnd()) * 5};
    Vec3 a{rnd(), rnd(), rnd()};
    double n = std::sqrt(dot(a, a)); if (n < 1e-3) continue;
    a = {a.x / n, a.y / n, a.z / n};
    Vec3 org{rnd(), rnd(), rnd()};
    double lo = 1e300, hi = -1e300;
    for (int c = 0; c < 8; ++c) {
      Vec3 p{(c & 1) ? b.max.x : b.min.x, (c & 2) ? b.max.y : b.min.y, (c & 4) ? b.max.z : b.min.z};
      double d = dot(p - org, a); lo = std::min(lo, d); hi = std::max(hi, d);
    }
    Interval iv = projectOnAxis(b, org, a);
    CHECK(std::fabs(iv.lo - lo) < 1e-9 && std::fabs(iv.hi - hi) < 1e-9);
  }

  std::printf(failures ? "%d ECHEC(S)\n" : "Geometrie : tous les tests passent\n", failures);
  return failures ? 1 : 0;
}
