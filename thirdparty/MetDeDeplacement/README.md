# MetDeDeplacement 2 — méthode des déplacements pour ossatures planes

Modèle de calcul de TSARALOHA Nomenjanahary Christinot Léonnel Calixte, adapté pour TSA.
C'est le solveur du moteur **Custom2D** de TSA ; la bibliothèque reste utilisable seule
(C++20, aucune dépendance).

## Ce que calcule la bibliothèque

- Ossatures planes : 3 DDL par nœud (u, v, θ), barres de direction quelconque.
- Appuis rigides (par DDL) et élastiques (kX, kY, kRz) ; rotules d'extrémité de barre ; barres de treillis.
- Charges nodales, réparties trapézoïdales (partielles) et concentrées sur barre, en repère local.
- Résultats : déplacements, réactions, efforts d'extrémité, et pour chaque barre les courbes
  **N(x), V(x), M(x)**, la **déformée u(x), v(x)** et leurs valeurs caractéristiques (moments
  d'extrémité, extremum en travée et position, zéros de M, efforts tranchants, flèche max et
  position, rotations des sections), ainsi que le résidu d'équilibre global.

## Méthode

Méthode des rotations écrite sous forme matricielle : raideur K = 4EI/L, report K/2, moments
d'encastrement parfait, translation 6EI/L² / 12EI/L³, effort normal EA/L. Les efforts
d'encastrement parfait sont obtenus pour toute charge par la méthode des forces sur la barre
bi-encastrée (intégrales exactes) ; une rotule est traitée par condensation statique (3EI/L, qL²/8).
Résolution : renumérotation Cuthill–McKee inverse + Cholesky bande (O(n·b²)).

Le long d'une barre : M(x) = μ(x) + M_i(1 − x/L) + M_j·x/L, V = dM/dx ; déformée par la ligne
élastique EI·v'' = M avec v(0) = v_i, v(L) = v_j ; EA·u' = N. Conventions détaillées dans
`include/mdd/MetDeDeplacement.h`.

Option `axialStiffnessFactor` : 1 = EA réel ; 10⁴ ≈ barres inextensibles (hypothèse de la
méthode des rotations classique).

## Utilisation

```cpp
#include <mdd/MetDeDeplacement.h>
mdd::Model m;
m.nodes = { { 0, 0, true, true, false }, { 6, 0, false, true, false } };
m.members = { { 0, 1, 210e6, 1e-2, 1e-4 } };                    // E (kPa), A (m²), I (m⁴)
m.distributedLoads = { { 0, 0.0, 6.0, 0, -10, 0, -10 } };       // q = 10 kN/m vers le bas
mdd::Result r = mdd::solve(m);   // r.members[0].summary.MSpanExtremum == 45 (qL²/8)
```

## Validation (TSA, suite `mdd`, tests 150–159)

Poutre isostatique (qL²/8, 5qL⁴/384EI, qL³/24EI), bi-encastrée (qL²/12, qL²/24, qL⁴/384EI, zéros
en L/2 ± L/(2√3)), encastrée-appuyée et rotule (qL²/8, 3qL/8, 9qL²/128 en 5L/8), console (PL³/3EI),
charge trapézoïdale partielle, treillis isostatique, mécanisme détecté, barres inextensibles,
375 barres en ~50 ms (Debug), et portique plan **identique à OpenSees** (10⁻⁷).

## Ancien code

`legacy/` contient la version d'origine (entrée JSON « dessin »), conservée sans modification et
non compilée. Voir `legacy/NOTES.md` pour l'analyse et la correspondance avec la version 2.
