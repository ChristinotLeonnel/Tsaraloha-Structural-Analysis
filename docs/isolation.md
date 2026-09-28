# Système d'isolation 3D

Module : `src/View3D/Isolation/` — `IsolationGeometry.h` (maths pures, sans dépendance)
et `IsolationManager` (OpenCASCADE AIS).

## Ce qu'il fait

| Critère | Méthode | Sens |
|---|---|---|
| Sélection | `isolateSelection()` | garde la sélection, masque le reste |
| Type | `isolateSameType()` | garde les objets du même type que la sélection |
| Plan de travail | `isolateOnWorkPlane(plan, tol)` | objets qui touchent le plan (tranche ±tol) |
| Coupe | `isolateBySection(plan, dessous, dessus)` | objets d'une tranche entre deux plans parallèles |
| Projection | `isolateByProjection(repère, umin, vmin, umax, vmax)` | objets dont la projection tombe dans une fenêtre |
| Volume | `isolateByBox(boîte)` | objets dans une boîte 3D |
| Masquer | `hideSelection()` | masque la sélection |
| Inverser | `invert()` | échange visibles / masqués |
| Historique | `undo()`, `showAll()` | dernière étape / état initial |
| Coupe visuelle | `setClipPlane(plan, capping)`, `clearClipPlane()` | plan de clipping sur la vue |

`MatchMode::Intersects` (défaut) garde tout objet qui touche le critère, `MatchMode::Inside`
seulement ceux qui sont entièrement dedans. Les isolations s'empilent : `description()`
donne par exemple `Plan de travail > Coupe` pour la barre d'état.

Fonctionne pour n'importe quel `AIS_InteractiveObject` (nœud, poutre, poteau, dalle...) :
le test géométrique utilise la boîte englobante de l'objet.

## Règles de comportement

- Objets sans emprise finie (grille, repères, plans infinis) : ignorés par les critères géométriques.
- Objets auxiliaires : déclarez-les avec `setExcludePredicate()` ; ils ne sont jamais masqués ni coupés.
- Une isolation qui ne changerait rien (tout ou rien ne correspond) est refusée ; `lastMessage()` donne le motif.
- Un objet supprimé pendant l'isolation n'est jamais ré-affiché par `undo()` / `showAll()`.
- Mode `OthersMode::Ghost` : les autres objets sont estompés au lieu d'être masqués (effet visible
  uniquement sur les objets ombrés, pas sur les fils).
- Les objets créés pendant une isolation restent visibles.
- La coupe visuelle survit à `showAll()` ; appelez `onObjectDisplayed()` pour les nouveaux objets.

## Intégration

1. **CMake** : ajouter `src/View3D/Isolation/IsolationManager.cpp` à la cible `TSA`
   (le reste est header-only).
2. **Instanciation** (un gestionnaire par vue / contexte) :

```cpp
#include "View3D/Isolation/IsolationManager.h"
using namespace TSA::Isolation;

m_isolation = std::make_unique<IsolationManager>(m_context);
m_isolation->setExcludePredicate([this](const auto &o) {
  return o == m_gridObject || o == m_trihedron;      // objets auxiliaires
});
m_isolation->setTypeKeyFunction([](const auto &o) {   // optionnel : type métier
  return myModelTypeOf(o);                            // "beam", "column", ...
});
m_isolation->setChangedCallback([this] { updateStatusBar(m_isolation->description()); });
```

3. **Commandes** (déjà déclarées dans `CommandCatalog`) :

| Commande | Appel |
|---|---|
| `cmd.isolate.selection` | `isolateSelection()` |
| `cmd.isolate.same_type` | `isolateSameType()` |
| `cmd.isolate.workplane` | `isolateOnWorkPlane(m_workPlane, 0.01)` |
| `cmd.isolate.section` | dialogue « dessous / dessus » puis `isolateBySection(m_workPlane, b, a)` |
| `cmd.isolate.projection` | 2 points → `isolateByProjection(viewFrame, u0, v0, u1, v1)` |
| `cmd.isolate.volume` | boîte 3D → `isolateByBox(box)` |
| `cmd.isolate.hide` | `hideSelection()` |
| `cmd.isolate.ghost` | bascule `setOthersMode(Hide/Ghost)` |
| `cmd.isolate.invert` | `invert()` |
| `cmd.isolate.undo` | `undo()` |
| `cmd.isolate.show_all` | `showAll()` |

   Si un appel renvoie `false`, afficher `lastMessage()` à l'utilisateur.
   Pour brancher `cmd.view.section_cut` : `setClipPlane(plan, true)` / `clearClipPlane()`.

4. **Repère de la vue courante** pour la projection (à adapter) :

```cpp
Handle(Graphic3d_Camera) cam = view->Camera();
gp_Dir dir = cam->Direction(), up = cam->Up();
gp_Dir right = dir.Crossed(up);
gp_Ax3 viewFrame(cam->Center(), gp_Dir(-dir.XYZ()), right); // X = droite, Y = haut
```

## Tests

Sans OpenCASCADE ni Qt (les tests du gestionnaire utilisent des mocks dans `tests/isolation/mock_occ/`) :

```
g++ -std=c++17 -Isrc/View3D/Isolation tests/isolation/test_isolation_geometry.cpp -o t_geom && ./t_geom
g++ -std=c++17 -Itests/isolation/mock_occ -Isrc/View3D/Isolation tests/isolation/test_isolation_manager.cpp src/View3D/Isolation/IsolationManager.cpp -o t_mgr && ./t_mgr
python tools/check_shortcuts.py
```
