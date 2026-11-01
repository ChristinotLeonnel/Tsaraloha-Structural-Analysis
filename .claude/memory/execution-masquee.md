---
name: execution-masquee
description: "Comment compiler et lancer TSA_TestSuite, TSALab_TestSuite, TSA_Benchmark et OpenSees sans aucune fenêtre visible : scripts conservés dans docs/audit/2026-10-31/preuves"
metadata:
  node_type: memory
  type: reference
  originSessionId: 84fdaea9-ba6e-422a-80ce-1f048ba5db29
  modified: 2026-10-31T19:58:14.826Z
---

Scripts réutilisables (copiés depuis le scratchpad, qui disparaît) : `docs/audit/2026-10-31/preuves/scripts/` et `preuves/linear-only/scripts/`.

- `preuves/scripts/run_hidden_desktop.py <journal> <cwd> <commande…>` : bureau Windows séparé (`CreateDesktopW`, ctypes `CreateProcessW` avec `lpDesktop`), aucune fenêtre ni console ; validé pour les 60 suites de TSA (`python tools/ci/run_test_suites.py --exe <build>/TSA_TestSuite.exe --out <hors dépôt> --available opensees,gmsh,ifcopenshell,display`), `TSALab_TestSuite` et `TSA_Benchmark --cases tests/benchmarks --out <hors dépôt>`. Réécrit le 2026-10-31 soir : il manquait au dépôt.
- `preuves/linear-only/scripts/build_hidden.py <build> <journal>` : vcvars64 + `cmake --build -j 4` sans console (une cible `--target TSA_TestSuite` est refusée par Ninja : compiler tout).
- `hidden_bench.py` (code en dur vers `build-windows-validation`), `compare_dumps.py`, `compare_bench.py`, `perf_run.py` : bancs et comparaisons avant/après. `dump_all.py` et son harnais ne sont PAS dans le dépôt.

Release de TSA : `build-windows-validation` a disparu le 2026-10-31 ; Release actuelle = preset `ninja-release` (`build-ninja-release`). TSALab : `TSALab/build-windows-validation`. Le 2026-11-01 au matin, `build-ninja-release` (TSA) et `TSALab/build-windows-validation` avaient aussi disparu (cause inconnue) : toujours vérifier l'existence du dossier de build avant de lancer les tests. TSA reconstruit (preset ninja-release, ~15 min) ; la compilation de TSALab (preset ninja-release, `TSALab/build-ninja-release`) a été arrêtée par Claude Code faute de mémoire (3 Go libres sur 16) : ne pas compiler TSALab juste après TSA sans mémoire libre.

Dépôt TSALab : Git signale « dubious ownership » ; utiliser `git -c "safe.directory=D:/Tsaraloha Platforme/TSALab" …` par commande plutôt que de modifier la configuration globale. Son test L9 fige le nombre de cas de `TSA/tests/benchmarks` (39 depuis le 2026-10-31) : à mettre à jour quand on ajoute ou retire un cas.

`tools/i18n/update_translations.py` (lupdate) réécrit tout le `.ts` (`&apos;`, fins de ligne) : remettre `'` et LF, puis `<translation type="unfinished" />`, pour ne garder que les vrais changements.

Relatif à [[essais-sans-effet-de-bord]] et [[audit-2026-10-31]].
