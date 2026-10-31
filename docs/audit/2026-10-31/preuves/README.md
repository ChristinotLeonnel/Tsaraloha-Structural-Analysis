# Preuves de l'audit du 2026-10-31

Tout ce qui suit a été exécuté hors du dépôt, sur les binaires Release de `build-windows-validation` (commit `5403ed6`).

## Contenu

| Dossier | Contenu |
|---|---|
| `cas/rotules/` | Cas `tsa.benchmark/1` : `HINGE-*`, `GERBER-*`, `PIN2-*`, `PDELTA-LIN` |
| `cas/non-lineaire/` | Cas `NLIN-*` |
| `resultats/` | Sorties brutes : tests, bancs, mesures, cartographie, tri du fuzzing, analyse statique, journal de l'audit |
| `fuzz/` | Deux fichiers `.tsa` qui font planter l'étape de calcul |
| `scripts/` | Solveur indépendant, générateurs de cas, fuzzeur, harnais C++, scripts d'analyse |
| `linear-only/` | Retrait de l'analyse non linéaire (même jour) : scripts de compilation et de comparaison avant/après, essai du câble précontraint, résultats. Voir [LINEAR_ONLY_REFACTORING_REPORT.md](../LINEAR_ONLY_REFACTORING_REPORT.md) |

Les 15 portiques aléatoires ne sont pas stockés : `scripts/gen_cases_random.py` les régénère à l'identique, à graine fixe.

## Rejouer

**Toujours lancer depuis un dossier hors dépôt.**

```powershell
# Autovalidation du solveur indépendant, puis portiques aléatoires
python scripts/check_frame3d.py
python scripts/gen_cases_random.py <dossier> 5

# Banc, sans fenêtre (OpenSees est lancé par QProcess depuis un parent masqué)
python scripts/hidden_bench.py <cas> <sortie> <dossier de travail> <journal>

# Cas livrés ici
python scripts/hidden_bench.py cas\rotules <sortie> <travail> <journal>
python scripts/hidden_bench.py cas\non-lineaire <sortie> <travail> <journal>

# Fuzzing : semence, dossier de travail, dossier de build, itérations, graine
python scripts/fuzz_tsa.py <semence.tsa> <travail> <build> 1500 11
```

`scripts/harness.cpp` se compile avec `scripts/make_harness_cmd.py`, qui reprend les drapeaux de `TSA_Benchmark` et se lie aux objets existants. Modes : `numerics`, `perf`, `perffem`, `fuzzload`, `fuzzdiag`.

## À ne pas relancer tel quel

`3DDesplacement/build/fem3d_opensees_benchmarks.exe` supprime `benchmarks/OpenSees/results/*_opensees.txt`, fichiers suivis par Git, avant chaque calcul, et ouvre des fenêtres (BUG-161). Le corriger d'abord, ou lui donner une copie du dépôt.

`TSA_TestSuite.exe` et `TSALab_TestSuite.exe` ouvrent de vraies fenêtres pendant les suites de rendu, et le premier écrit dans le dossier courant : passer par `tools/ci/run_test_suites.py`, depuis un dossier hors dépôt.

## Limites des mesures

- Les temps de `resultats/perf-api.txt` portent sur deux passages, un seul pour 29 200 barres.
- Les portiques de `perf-api.txt` et `perffem.txt` ont été mesurés avec d'autres applications ouvertes.
- `resultats/numerics.txt` provient d'une version du harnais lancée sous `QCoreApplication`.
