# Audit technique de TSA — résumé exécutif

Date : 2026-10-31. Base : commit `5403ed6`, branche `feature/diagram-multi-curves`. Poste : Windows 11, MSVC 19.51, Qt 6.12.0, OCCT 8.0.1, OpenSees 3.8.0.

## Verdict

**TSA repose sur un socle solide, mais ne doit pas être utilisé tel quel pour un projet réel avec OpenSees (moteur par défaut) dès qu'une barre porte une rotule et une charge non uniforme, ni en statique non linéaire.** Dans ces deux cas, le logiciel produit des efforts faux, sans message, alors que tous les tests sont verts et que l'équilibre est vérifié.

Hors de ces cas, le calcul statique linéaire 3D est remarquablement exact : environ 1e-10 d'écart avec un solveur indépendant.

Les corrections de mise en sécurité tiennent en quelques jours. **Tout résultat déjà produit dans ces deux configurations doit être considéré comme non fiable.**

## Chiffres clés

| Indicateur | Valeur |
|---|---:|
| Volume de `src/` | 179 570 lignes, 763 fichiers |
| Suites de tests | 57/57, 538 tests réussis, 0 échec, 1 ignoré |
| Banc officiel | 146/146 |
| Comparaisons à un solveur indépendant | 12 678, dont 1 770 échecs, **tous dus à BUG-148** |
| Fichiers `.tsa` fabriqués | 4 200, dont 23 plantages, tous à l'étape de calcul |
| Analyse statique | 166 fichiers, 110 avertissements, 0 défaut mémoire confirmé |
| Registre | 21 défauts : **2 CRITICAL, 4 HIGH**, 5 MEDIUM, 10 LOW |

## Problèmes confirmés les plus graves

| Défaut | Gravité | Ce qui se passe | Exemple mesuré |
|---|---|---|---|
| BUG-148 | CRITICAL | OpenSees : rotule + charge ponctuelle ou trapézoïdale | M d'encastrement 13,33 au lieu de 26,67 kN·m |
| BUG-149 | CRITICAL | Non linéaire : efforts le long des barres avec un facteur de charge de 0,5 | M = 107,7 au lieu de 54 kN·m |
| BUG-150 | HIGH | P-Delta demandé en statique linéaire, accepté sans effet réel | M = 40 au lieu de 58,9 kN·m |
| BUG-151 | HIGH | Câble comprimé accepté | N = −20,2 kN, 0 avertissement |
| BUG-152 | HIGH | Note de calcul par défaut : flambement sur l'axe fort seul, verdict CONFORME | IPE 200, 5 m : 809 kN au lieu de 105 kN |
| BUG-153 | HIGH | Cornières sur axes géométriques | Flèche sous-estimée de 35 % |

S'y ajoutent deux constats de méthode de gravité HIGH :

- **La CI n'exécute pas OpenSees**, donc le moteur par défaut n'y est jamais validé.
- **Le contrôle des couches ne couvre qu'une partie des dépendances** : 28 modules sur 36 forment un seul bloc cyclique, sans aucun cycle entre en-têtes.

Détail et reproduction : [TSA_BUG_REGISTER.md](TSA_BUG_REGISTER.md).

## Pourquoi ces défauts ont échappé aux tests

Aucun cas du banc ne combine rotule et charge sur barre, ni charge sur barre et non linéaire. Et **l'équilibre global ne les détecte pas** : les forces nodales restent statiquement équivalentes, les réactions d'ensemble sont justes. Un contrôle d'équilibre vert n'est donc pas une preuve de justesse des efforts.

## Ce qui est solide

| Domaine | Preuve |
|---|---|
| Statique linéaire 3D, OpenSees et Fem3D | Écart ≈ 1e-10 sur des portiques inclinés, sections tournées, ressorts |
| Fem3D et Custom2D sur tous les cas à rotule | 100 % justes |
| Matrices, transformations, conventions de signe | Relues et rederivées à la main |
| Formules EC2 et EC3 elles-mêmes | Conformes, à relire sur le texte de la norme |
| Lecture `.tsa`, export IFC, maillage | 3 000 fichiers fabriqués sans plantage |
| Extension Shell, serveur d'automatisation | Bornés, accès limité au compte utilisateur |
| Annuler / Rétablir | API unique, 0 appel direct dans l'interface |
| Dépendances d'en-têtes | 0 cycle |
| Duplication, marqueurs de dette | Faible, 0 `TODO` |

## Priorités de correction

1. **Mise en sécurité (quelques jours)** : refuser ou corriger ce qui produit des résultats faux. BUG-148 à 153.
2. **Revue des résultats déjà produits** sur des projets réels concernés.
3. **Corrections de fond** : condensation des rotules pour OpenSees, cornières, poids propre.
4. **Robustesse** : contrôle référentiel, frontières d'exception, sauvegarde automatique.
5. **Filet de validation** : solveur indépendant et portiques aléatoires dans le banc, OpenSees en CI, fuzzing.

Plan détaillé : [TSA_REMEDIATION_ROADMAP.md](TSA_REMEDIATION_ROADMAP.md).

## Périmètre réel de l'audit

### Examiné

- **Lu directement** : environ 6 000 lignes de `src/` (≈ 3 %), choisies par niveau de risque, et le cœur de Fem3D (≈ 1 300 lignes). Lecture intégrale : `OpenSeesAnalysisBuilder`, `UndoManager`, `LoadResolver`, `ElementTransformation`, `AnalysisController`, `Fem3DAdapter`, `Fem3DEngine`, `SteelDesignEC3`, `ConcreteDesignEC2`, `TSAFile_BinaryUtils`. Lecture partielle : `OpenSeesResultsReader`, `OpenSeesSolver`, `AnalysisManager`, `TSAFile`, `TSAFileReader_Chunks`, `TSAThumbnailProvider`, `Section`, `ModelValidator`, `NDCGenerator`, `NDCProjectNote`, `PluginManager`, `AutomationServer`.
- **Par outils seulement** : tout `src/` (taille, dépendances, complexité, duplication, symboles), 166 fichiers par `clang-tidy`.
- **Non lu** : l'essentiel de `src/UI` et du corps de `src/Viewer`, `src/Mesh/Gmsh`, `src/BIM/IFC`, `src/AI`, `src/Blueprint`, les sources de `MetDeDeplacement`, `TSALab/lab`, TSA3D.

### Testé

Windows MSVC Release uniquement : voir [TSA_TEST_COVERAGE_AUDIT.md](TSA_TEST_COVERAGE_AUDIT.md).

### Non vérifié

| Sujet | Raison |
|---|---|
| Linux, GCC, Clang, MinGW | Aucun outil sur ce poste, aucune distribution WSL |
| Windows Debug | Aucun binaire Debug |
| Interface réelle, démarrage, rendu 3D, GPU | Non lancés, pour ne plus ouvrir de fenêtres sur le poste après le signalement de l'utilisateur |
| Fuites et corruptions mémoire | ASan non exécuté : la propreté mémoire n'est pas démontrée |
| Couverture de code | Aucun outil en place |
| CI GitHub | `gh` absent |
| TSALab | `TSALab_TestSuite` non exécuté. Science : non concluant |
| Références commerciales, NAFEMS | Non rejouées |
| Texte officiel des Eurocodes | Non disponible pendant l'audit |

## Risques ouverts

Listés dans le registre (R-01 à R-05) : vue 3D sur fichier abîmé, modification de la liste d'observateurs pendant une notification, manque de mémoire dans le thread de calcul, OpenSees resté bloqué sans cause établie, greffons non signés. Ce sont des hypothèses, pas des défauts prouvés.

## Incident pendant l'audit

Les règles de la mission interdisaient toute altération. **Une exécution lancée par l'auditeur en a pourtant provoqué une**, dans un dépôt voisin. Heures approximatives, d'après les dates des fichiers.

| Heure | Fait |
|---|---|
| 14:12 | Lancement de `3DDesplacement/build/fem3d_opensees_benchmarks.exe` **sans avoir lu son code** |
| 14:12-14:15 | L'outil ouvre des fenêtres visibles et supprime 41 fichiers de résultats suivis par Git. Il réécrit 2 synthèses avec des valeurs vides et vide 26 journaux |
| 14:15 | Signalement de l'utilisateur, arrêt immédiat, processus restants terminés. L'utilisateur remplace `thirdparty/OpenSees` |
| 14:18 | Vérification : la copie remplacée est identique à celle des tests (851 fichiers sur 851), OpenSees 3.8.0 fonctionne |
| 14:19 | Restauration par Git des 43 fichiers suivis. État de `3DDesplacement` revérifié : aucun écart, aucun commit, aucune configuration modifiée |

**Reliquat non restaurable** : 26 journaux `benchmarks/OpenSees/results/*_opensees.log`, ignorés par Git, sont restés vides. Ils sont régénérables. Un dossier temporaire `tsalab-opensees-18880-1` subsiste dans `%TEMP%`.

Le code de production de TSA n'a pas été touché : aucun commit, aucune fusion, aucun push. La synthèse « cas en écart : 0 » de cet outil **ne vaut pas validation** : aucun cas n'a été comparé (BUG-161).

## État du dépôt constaté (phase 0)

| Point | Constat |
|---|---|
| Branche de départ | `feature/diagram-multi-curves`, commit `5403ed6` |
| `examples/TSA_Projet_Batiment_Complexe_L.tsa`, `examples/TSA_Projet_Pont_Haubans_80-180-80m.tsa` | Modifiés avant l'audit. Sains : réenregistrés au format 2.0 (chunks `SECP`, `SETT`), miniature plus petite, modèle inchangé |
| `TSA.code-workspace` | Modifié pendant l'audit par l'utilisateur (ajout de `../3DDesplacement`) |
| Worktrees | 4 entrées `prunable` vers un ancien chemin Linux, laissées telles quelles |
| Stash | Aucun |

## Rapports

| Rapport | Contenu |
|---|---|
| [TSA_BUG_REGISTER.md](TSA_BUG_REGISTER.md) | Défauts, reproduction, correction, test |
| [TSA_NUMERICAL_AUDIT.md](TSA_NUMERICAL_AUDIT.md) | Fiabilité du calcul |
| [TSA_ARCHITECTURE_AUDIT.md](TSA_ARCHITECTURE_AUDIT.md) | Cartographie, dépendances |
| [TSA_CODE_QUALITY_AUDIT.md](TSA_CODE_QUALITY_AUDIT.md) | C++, Qt, sécurité, dette, documentation |
| [TSA_PERFORMANCE_AUDIT.md](TSA_PERFORMANCE_AUDIT.md) | Mesures et goulots |
| [TSA_CROSS_PLATFORM_AUDIT.md](TSA_CROSS_PLATFORM_AUDIT.md) | Portabilité, CI, licences |
| [TSA_TEST_COVERAGE_AUDIT.md](TSA_TEST_COVERAGE_AUDIT.md) | Exécutions, lacunes, matrice |
| [TSA_REMEDIATION_ROADMAP.md](TSA_REMEDIATION_ROADMAP.md) | Plan de correction |
| [preuves/](preuves/README.md) | Scripts, cas, journaux |
