# Audit des tests et de la couverture

## Conclusion

La base de tests est **large et saine** : 5 297 contrôles actifs en Release, dépendances absentes jamais comptées comme réussies, lanceur borné. Pourtant, les deux défauts critiques sont passés au travers, avec **538 tests et 146 vérifications de banc au vert**.

La cause n'est pas la quantité, mais une lacune de méthode : les cas sont choisis un par un, sans croiser systématiquement rotules, types de charges et types d'analyse.

## Résultats exécutés pendant l'audit

| Exécution | Commande | Durée | Résultat |
|---|---|---:|---|
| Suites TSA | `tools/ci/run_test_suites.py --exe build-windows-validation/TSA_TestSuite.exe` | 3 min 14 s | **57/57 suites, 538 tests réussis, 0 échec, 1 ignoré** (487, fournisseur IA réel) |
| Banc officiel | `TSA_Benchmark --cases tests/benchmarks` | 7 s | 146/146 |
| Contrôle des couches | `tools/check_layers.py` | < 1 s | 0 violation (portée partielle, ARC-01) |
| Raccourcis | `tools/check_shortcuts.py --strict` | < 1 s | 283 commandes, 141 raccourcis, OK |
| Tests Fem3D | `3DDesplacement/build/fem3d_tests.exe` | < 1 min | 49/49 |
| Portiques aléatoires | `TSA_Benchmark --cases cases-random` | quelques secondes | 10 908 réussies, **1 770 échecs**, tous BUG-148 |
| Rotules | `cases-hinge`, `cases-more` | quelques secondes | **Échecs OpenSees**, BUG-148 et BUG-150 |
| Non linéaire | `cases-nl` | quelques secondes | **Échecs**, BUG-149 |
| Fuzzing lecture, export, maillage | `fuzz_tsa.py`, 3 000 mutations | ≈ 6 min | **0 plantage, 0 délai** |
| Fuzzing voie d'analyse | `fuzz_tsa.py`, 1 200 mutations | ≈ 4 min | **23 plantages**, BUG-154 et BUG-155 |
| Analyse statique | `clang-tidy`, 166 fichiers | ≈ 10 min | 110 avertissements, 0 défaut mémoire confirmé |

### Exécutions non concluantes ou interrompues

| Exécution | Résultat | Interprétation |
|---|---|---|
| `fem3d_opensees_benchmarks.exe` | Tous les cas `NON EXÉCUTÉ` | **Aucune valeur**, malgré « cas en écart : 0 » (BUG-161). Effets de bord réparés, voir le résumé |
| `tsalab_science_tests.exe` | 5 tests passés, arrêt à S6 | Non concluant : OpenSees figé 2 min, code 259 (BUG-160, R-04) |
| `fem3dpost_tests.exe` | Délai de 300 s, sortie vide | Non concluant |
| `TSALab_TestSuite.exe` | **Non exécuté** | Non lancé, pour ne plus ouvrir de fenêtres sur le poste après le signalement de l'utilisateur. Dernier résultat documenté : 27/27 le 29/10, non revérifié |

## Matrice de validation

| Type | État | Détail |
|---|---|---|
| Unitaires | Bon | 5 297 `TEST_CHECK`. 112 `assert` signalés par un audit précédent ont disparu |
| Intégration | Bon | Calcul complet dans `opensees`, `engines`, `demo`, `extraction` |
| Non-régression | Moyen | Banc analytique, mais `TEST_CHECK` arrête la suite au premier échec (BUG-104) et masque les suivants |
| Validation numérique | Moyen | Excellente sur son périmètre. **Aucune rotule, pas de charge sur barre en non linéaire** |
| Propriété ou aléatoire | **Absent** | Introduit par cet audit, il a trouvé BUG-148 |
| Stress | Faible | Test 526 (2 000 nœuds), projets `demo` de quelques centaines de barres |
| Performance | Faible | `TSA_Benchmark --performance` et seuils larges, sans suivi dans le temps |
| Robustesse des fichiers | **Absent** | Introduit par cet audit |
| Mémoire | **Absent** | Pas d'ASan sous Windows. ASan Linux ponctuel documenté (BUG-119) |
| Multiplateforme | Faible | CI lue, non exécutée, sans OpenSees |
| Interface réelle | Faible | `MainWindow` hors tests automatiques, couche `app` non liée. Essais par UI Automation |
| Couverture de code | **Non mesurée** | Aucun outil en place |

## Lacunes les plus coûteuses

| Priorité | Lacune | Défaut qu'elle a laissé passer | Action |
|---|---|---|---|
| 1 | Pas de matrice croisée rotule × charge × moteur | BUG-148 | Reprendre `cases-hinge` et `cases-more` dans `tests/benchmarks` |
| 2 | `RDM-11a` et `b` sans charge sur barre | BUG-149 | Reprendre `cases-nl` |
| 3 | Pas de comparaison à un solveur indépendant | BUG-148 | Intégrer `frame3d.py` et les familles A, B, C, à graine fixe |
| 4 | Pas de test de cohérence réglage ↔ analyse | BUG-150 | Refus attendu, test dédié |
| 5 | CI sans OpenSees | Le moteur par défaut n'y est pas testé | OpenSees en CI, avec cache |
| 6 | Pas de fuzzing en continu | BUG-154, BUG-155 | Corpus `preuves/fuzz/`, 500 mutations à chaque nuit |
| 7 | Vérifications EC3 testées pour leurs formules, pas pour leur appel | BUG-152 | Test sur la note générée |
| 8 | Équilibre pris pour une preuve | Il ne détecte ni BUG-148 ni BUG-149 | Contrôler aussi M = 0 aux rotules et V = dM/dx le long des barres |

Le dernier point est le plus général : **l'équilibre global est nécessaire, pas suffisant**. Les deux défauts critiques le respectent.

## Couverture par module

Nombre de fichiers de test incluant au moins un en-tête du module : `preuves/resultats/couverture_modules.txt`. Bonne couverture de Model (32), Analysis (27), IO (22), UI (21), Viewer (18), Grid (13). Aucune inclusion directe pour `Platform` et `Plugins` : les greffons seraient couverts côté TSALab (L8), non revérifié.

Mesure d'inclusion seulement : elle ne dit rien du code réellement exécuté.
