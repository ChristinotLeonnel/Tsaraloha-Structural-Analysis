# Plan — un `.tsa` qui conserve tout l'état du projet

Statut : **VALIDÉ** le 2026-11-01 (décisions D1 à D6 acceptées). Toutes les étapes faites (0 à 7). Suites possibles : BUG-175, BUG-176, boîte des propriétés du projet, note de calcul reliée au manifeste, mesure sur un grand modèle.
Demande : chaque fichier garde sa propre disposition de fenêtre, sa vue, ses résultats de calcul, ses messages et
erreurs, ses fichiers liés et ses bibliothèques. Il rouvre dans l'état de son dernier enregistrement.

## 1. Principes

1. **Un fichier rouvert est dans l'état de son dernier enregistrement** : même disposition, même vue, mêmes résultats
   affichés, mêmes messages. Rien n'est recalculé à l'ouverture.
2. **Format compatible** : chaque ajout est un chunk **facultatif** (version mineure 3.1, 3.2…). Un TSA plus ancien
   l'ignore (chunk inconnu sauté, `TSAFile.cpp:1083`). Un chunk absent donne le comportement actuel.
3. **Architecture existante** : le modèle garde chaque bloc comme donnée opaque (JSON ou binaire versionné), comme
   `analysisSettingsJson` ou `meshProjectJson`. La couche UI le remplit à l'enregistrement et l'applique à
   l'ouverture. Couches respectées (model < graphics < widgets < app, `tools/check_layers.py`).
4. **L'état d'affichage ne rend pas le document « modifié »**. Déplacer un panneau ou tourner la vue ne fait pas
   apparaître « * » : cet état est relevé au moment de l'enregistrement, comme dans un traitement de texte. Le
   modèle, les réglages et les résultats, eux, marquent le document modifié.
5. **Aucun fichier lié n'est ouvert ni exécuté automatiquement** : à l'ouverture, ils sont seulement contrôlés
   (présence, empreinte) et signalés.

## 2. Ce que le `.tsa` enregistre déjà

| Donnée | Chunk | Remarque |
| :--- | :--- | :--- |
| Modèle (nœuds, barres, poteaux, dalles, voiles, fondations, treillis, câbles, appuis, sections, matériaux) | `NODE` … `SECP` | complet |
| Charges, cas, combinaisons, cas actif | `LOAD`, `SURF` | complet |
| Réglages d'analyse | `SETT` | complet |
| Topologie, cotations, ouvertures, aspect | `TOPO`, `DIMS`, `OPNG`, `MVIS` | complet |
| Vues nommées (caméras enregistrées par l'utilisateur) | `VIEW` | la vue **courante** n'est pas enregistrée |
| Maillage, BIM, données d'échange | `MESH`, `BIMM`, `XTND`, `BGEO` | complet |
| Bibliothèques : référence versionnée + propriétés mécaniques figées par élément | `SNAP` | aucune liste des bibliothèques requises, aucun contrôle à l'ouverture |
| Résultats de calcul | `RSLT` | **déclaré, jamais écrit** (BUG-013) |
| Disposition de la fenêtre | — | dans `QSettings`, **commune à tous les fichiers** (`LayoutManager`, `WorkspaceView`) |
| Messages et erreurs du calcul | — | en mémoire seulement (`ResultsModel::warnings`, `preparationMessages`) |

## 3. Nouveaux blocs

### 3.1 `UIST` — disposition de la fenêtre (format 3.1)

JSON UTF-8 versionné (`schema` 1) :

- géométrie et état de la fenêtre (normale, maximisée, plein écran) ;
- état des panneaux : `QMainWindow::saveState()` en base64, plus la liste lisible des panneaux ouverts (repli si
  l'état binaire de Qt n'est plus applicable) ;
- onglet actif du ruban, espace de travail actif (`WorkspaceView`), multi-vues (1, 2H, 2V, 2×2, onglets) ;
- état des panneaux métier : arbre du modèle (nœuds dépliés, filtre), panneau Résultats (grandeur, mode de
  déformée, filtre des nœuds), console (niveau de détail).

**À l'ouverture** : la disposition du fichier remplace celle de l'utilisateur. Un fichier sans `UIST` garde la
disposition de l'utilisateur. Une préférence « Restaurer la disposition enregistrée dans le projet », cochée par
défaut, permet de la désactiver.

### 3.2 `VSTA` — vue 3D et affichage (format 3.1)

JSON UTF-8 (`schema` 1) ; la caméra reprend le format des vues nommées (`NamedViewSet`, chunk `VIEW`) :

- caméra de chaque vue (œil, cible, haut, échelle, projection) et vue 2D active ;
- mode de rendu (`RenderDisplayMode`), représentation (`ModelDisplayMode` : physique ou analytique) ;
- calques visibles (grilles, niveaux, cotes, repères, charges, étiquettes), magnétisme ;
- niveau actif, plan de travail actif, coupe 3D active ;
- résultats affichés : diagramme ($M_z$, $V_y$…), déformée et son échelle, réactions, mode propre affiché, cas ou
  combinaison affiché.

### 3.3 `RSLT` — résultats de calcul (format 3.2, corrige BUG-013)

**Binaire** (des doubles, pas du JSON), versionné (`layoutVersion`), toujours enregistré ; la charge utile du
`.tsa` est déjà compressée (zlib).

- pas de calcul : déplacements, réactions, efforts par station, déplacements des surfaces ;
- modes propres et masses participantes, efforts des coques aux points de Gauss, équilibre global ;
- métadonnées d'exécution (`AnalysisExecutionMetadata` : moteur, version, méthode, annexe nationale, horodatage) ;
- **empreinte du modèle calculé** : SHA-256 des octets du contenu (`snapshotContentBytes`, déjà utilisés par
  Annuler).

**À l'ouverture** : si l'empreinte correspond au modèle relu, les résultats sont affichés tels quels. Sinon, ils
sont chargés mais marqués **périmés** (bandeau) : la note de calcul les refuse, comme aujourd'hui après une
modification (`ResultsValidityGuard`).

Taille estimée, avant compression : environ 0,5 Kio par barre (11 stations × 6 efforts). Soit 2,5 Mo pour 4 900
barres et 15 Mo pour 29 200 barres, environ trois fois moins une fois compressés.

### 3.4 `DIAG` — messages et erreurs (format 3.3)

JSON UTF-8 (`schema` 1) :

- messages du dernier calcul : préparation, avertissements, erreurs, extrait de la sortie du moteur (500 dernières
  lignes au plus) ;
- rapport de validation du modèle au moment de l'enregistrement (`ModelValidator`) ;
- historique des calculs (20 derniers) : date, moteur et version, durée, statut, empreinte du modèle (traçabilité,
  BUG-111).

Le journal général de l'application (crashs, session) **n'y est pas** : il n'appartient pas au projet et
contiendrait des chemins personnels.

### 3.5 `PMAN` — manifeste du projet (format 3.3)

JSON UTF-8 (`schema` 1) :

- **bibliothèques requises** : identifiant, version et origine de chaque bibliothèque TSALib utilisée (déduites
  des `DefinitionReference`). À l'ouverture : bibliothèque absente ou d'une autre version → message. Le calcul
  reste reproductible grâce aux propriétés figées (`SNAP`).
- **fichiers liés** : rôle (source d'import IFC / TSA3D, export : note de calcul, IFC, paquet ; texture de
  matériau ; document joint), chemin **relatif au `.tsa`** et chemin absolu de repli, taille, SHA-256, date. À
  l'ouverture : présent, modifié depuis, ou introuvable, affiché dans un panneau « Fichiers liés ». Rien n'est ouvert
  automatiquement.
- **historique des enregistrements** (50 derniers) : date, auteur, version de TSA, version du format.
- **propriétés du projet** : description, client, référence et phase d'étude (champs libres).

## 4. Étapes

| Étape | Contenu | Tests |
| :--- | :--- | :--- |
| **0** | BUG-074 : « * » et demande d'enregistrement après Ctrl+S | test 573 — **fait** |
| 1 | Socle : emplacements opaques dans `Model` (hors historique, sans marque « modifié »), écriture et lecture de chunks JSON génériques, format 3.1 | test 574 — **fait** |
| 2 | `VSTA` : relevé et application de la vue et de l'affichage | test 575 — **fait** |
| 3 | `UIST` : disposition par fichier, préférence de restauration | test 576 — **fait** (géométrie de la fenêtre exclue : fenêtre partagée par les onglets) |
| 4 | `RSLT` : sérialisation binaire de `ResultsModel`, empreinte, résultats périmés | test 577 — **fait** (gros modèle : taille et durée non mesurées) |
| 5 | `DIAG` : messages, validation, historique des calculs | test 578 — **fait** |
| 6 | `PMAN` : bibliothèques, fichiers liés, historique, panneau « Fichiers liés » | test 579 — **fait** (boîte Fichier › Fichiers liés ; propriétés sans boîte de saisie ; note de calcul non reliée) |
| 7 | Documentation : `TSA_FILE_FORMAT.md` § 5.w, README, ADR-026, mémoire du projet | **fait** |

Chaque étape est compilée et testée (TSA puis TSALab), puis présentée avant la suivante.

## 5. Décisions à valider

| # | Question | Proposition |
| :--- | :--- | :--- |
| D1 | Changer la disposition ou la vue rend-il le document « modifié » ? | **Non** (§1, point 4) ; l'état est relevé à l'enregistrement |
| D2 | Disposition du fichier ou de l'utilisateur à l'ouverture ? | **Celle du fichier**, avec une préférence pour la désactiver |
| D3 | Résultats d'un modèle modifié hors de TSA (empreinte différente) | **Chargés et marqués périmés**, refusés par la note de calcul |
| D4 | Matrices de rigidité des éléments (`ElementMatrices`, volumineuses, recalculables) | **Non enregistrées** ; les efforts, oui |
| D5 | Chemins des fichiers liés | **Relatif au `.tsa`, absolu en repli** ; les chemins absolus peuvent révéler le nom d'utilisateur (option « Enregistrer sans chemins absolus » pour un fichier à transmettre) |
| D6 | Historique Annuler / Rétablir dans le fichier | **Non** : volumineux (instantanés complets), sans usage après réouverture |

## 6. Risques

- **Taille et durée d'enregistrement** avec les résultats d'un grand modèle : à mesurer à l'étape 4 (29 200 barres).
- **État binaire des panneaux de Qt** fragile d'une version à l'autre : un repli lisible est prévu (§3.1).
- **Plusieurs documents ouverts** (onglets de l'`AppShell`) : la disposition d'un fichier s'applique à sa fenêtre
  de travail, sans toucher aux autres documents.
