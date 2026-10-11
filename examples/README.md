# Projets de démonstration TSA

Trois projets au format natif `.tsa`, construits uniquement avec des barres par l'API de TSA (`Model`, `Section`,
`Material`, `GridManager`, `TSAFileWriter`) dans la suite de tests `demo` (`tests/test_demo_models.cpp`).

| Fichier | Ouvrage | Nœuds | Barres | Appuis |
|---|---|---|---|---|
| `TSA_Demo_Pont_Treillis_3x48m.tsa` | Pont en treillis Warren à montants, 3 travées de 48 m | 253 | 665 | 14 |
| `TSA_Demo_Hangar_60x40m.tsa` | Hangar métallique 60 × 40 m, 11 portiques à fermes | 514 | 1 209 | 27 |
| `TSA_Demo_Batiment_R+5.tsa` | Bâtiment R+5 à ossature métallique | 380 | 798 | 26 |

Contrôles à chaque génération :
- aucun nœud, aucune barre en double ;
- aucune barre de longueur nulle, aucun nœud isolé ;
- aucun nœud posé sur une barre sans y être connecté ;
- structure d'un seul tenant ;
- calcul OpenSees réussi sous poids propre (cas « G » enregistré dans le projet) ;
- relecture identique.

Régénérer : `TSA_DEMO_DIR=<dépôt>/examples TSA_TestSuite --suite=demo`.
Captures de l'application : `tools/ui_tests/demo_screenshots.ps1 -OutDir <dossier>`.
