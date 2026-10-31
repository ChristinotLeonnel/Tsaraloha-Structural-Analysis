# Tests — catégories, exécution bornée, validation graphique

Mesures du 2026-10-29 (Windows 10, MSVC Release, `build-windows-validation`, Intel UHD OpenGL 4.6) : 57 suites,
528 tests, **149 s** au total ; les plus longues : `demo` 49 s, `views` 36 s (rendu OCCT) ; aucune suite ne bloque.

## 1. Exécuter sans jamais rester bloqué

Toujours passer par le lanceur de suites (un processus par suite, délai par suite, dossier jetable) :

```powershell
py tools/ci/run_test_suites.py --exe build-windows-validation/TSA_TestSuite.exe --out <dossier hors dépôt> `
   --available opensees,gmsh,ifcopenshell,display --timeout 600 [--suites blueprint,shortcuts]
```

- Statuts : RÉUSSIE, ÉCHOUÉE, DÉLAI DÉPASSÉ, PLANTÉE, NON EXÉCUTÉE (dépendance absente, jamais comptée réussie) ;
  durée par suite et lignes `[SKIP]` dans `report.md`.
- `TSA_TestSuite.exe` seul n'a **aucun** délai interne et écrit des fichiers dans le dossier courant
  (`test_bug110_*.tsa`, `empty_project.tsa`) : ne pas le lancer depuis la racine du dépôt.
- Un test non exécuté (Qt Test absent, hors Windows) affiche `[SKIP]` et n'incrémente pas le compteur de réussite.
- Diagnostic : suite seule (`--suites x`), puis le binaire avec `--suite=x` ; reconstruire la cible utile seulement
  (`cmake --build build-windows-validation --target TSA_Tests`, quelques secondes) avant la suite complète.

## 2. Catégories

| Catégorie | Suites / tests | Environnement |
|-----------|----------------|---------------|
| Logique d'interface (actions, commandes, préférences, états) | `blueprint` 524–528, `shortcuts`, `i18n`, `ribbon`, `undo`, `commands` | widgets Qt, pas d'OpenGL ; hors écran possible |
| Qt hors écran | toutes les suites sans vue OCCT | `QT_QPA_PLATFORM=offscreen` (Linux sans affichage) |
| Vue 3D OCCT (OpenGL réel) | `views` (sautés sous offscreen), `drawing` 419, `coordinates` 408, `meshview` 343–346, `display`, `axes`, `multidoc` 387–388, `audit` 370, `demo` | plateforme native ou Xvfb ; **jamais offscreen** pour `display`, `multidoc`, `meshview`, `audit`, `demo` (BUG-076 : arrêt brutal) |
| Performance | `benchmarks` ; test 526 (2 000 nœuds, seuils larges) | durées indicatives propres au poste |
| Intégration graphique réelle (MainWindow, AppShell) | aucune suite : couche `app` non liée aux tests | validation manuelle ou pilotée (§3) |

Le masquage / réaffichage d'une vraie vue OCCT par l'espace de travail n'est couvert par aucun test automatique ; il a
été vérifié dans TSA.exe (§3).

## 3. Validation graphique réelle (Windows)

Pilotage possible sans outil supplémentaire : PowerShell + UI Automation + `SendKeys` + captures d'écran.

- Processus **DPI-aware** (`SetProcessDPIAware`) : sinon, à 125 %, les captures ne couvrent que le coin supérieur gauche.
- Ne jamais appeler `InvokePattern.Invoke()` sur un bouton qui ouvre une boîte modale : l'appel ne revient qu'à sa
  fermeture (blocage constaté). Cliquer réellement (souris) puis attendre la boîte avec un délai borné.
- Attentes conditionnelles bornées (fenêtre, titre du projet, fermeture du processus), jamais d'attente fixe longue.
- Fermeture : `CloseMainWindow()` puis attente bornée de la fin du processus ; une boîte « Enregistrer les
  modifications » peut s'ouvrir : y répondre explicitement.
- Disposition clavier : celle du fil de TSA (`GetKeyboardLayout`), pas celle de l'utilisateur ; pour un essai AZERTY,
  `WM_INPUTLANGCHANGEREQUEST` vers la fenêtre de TSA (changement limité à ce fil).
- Les préférences de TSA sont dans `HKCU\Software\TSA Engineering\TSA` : un essai les modifie (exporter avant).
- UI Automation : un même nom désigne le titre d'un panneau ET son onglet ; juger l'onglet actif sur capture.

## 4. Risques de blocage connus (inspection du code, non reproduits)

- Boîtes modales sans répondeur : `MeshAnalysisBridge` / `AnalysisManagerPanel::run` (suite `mesh`, tests 390 et 299)
  si OpenSees signale une compatibilité partielle ; sous Windows la plateforme native affiche une vraie boîte.
- `OpenSeesSolver` attend la fin du calcul sans délai (`waitForFinished(100)` en boucle).
- Destructeurs `MeshController` / `AnalysisController` : `wait()` sans délai après un délai de test dépassé.
- Aucun mécanisme global de suppression des boîtes modales en test (répondeurs locaux : `test_undo_characterization`,
  `test_appearance`).
