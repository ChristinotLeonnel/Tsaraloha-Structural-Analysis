# Raccourcis clavier — TSA

Référence complète, générée depuis le catalogue : [`docs/shortcut.txt`](shortcut.txt) (même format que le
fichier utilisateur). Aide dans TSA : **F1** (liste des raccourcis actifs) et **Aide > Personnaliser les
raccourcis** (**Ctrl+F1**).

## Architecture

| Élément | Fichier | Rôle |
| :--- | :--- | :--- |
| Catalogue | `src/Commands/CommandCatalog.cpp` | **source unique des valeurs par défaut** : identifiant stable (`cmd.<domaine>.<nom>`), libellé, description, raccourci(s) par défaut (alias séparés par « ; »), catégorie |
| Configuration | `src/UI/Shortcuts/ShortcutConfig.*` | lecture / écriture de `shortcut.txt`, validation des combinaisons, raccourcis effectifs, conflits (exacts et de préfixe), portées ; sans widget, testé seul |
| Gestionnaire | `src/UI/Shortcuts/ShortcutManager.*` | source de vérité des raccourcis actifs : relie chaque `QAction` existante à son identifiant (`bind`), pose les raccourcis (seulement ceux qui changent), infobulles, rechargement à chaud, dernière configuration valide, « Répéter la dernière commande », garde des champs de saisie |
| Liaison | `src/UI/MainWindow_Shortcuts.cpp` | table identifiant → action de la fenêtre principale (aucune logique métier) ; outils du registre `cmd.tool.<id>`, panneaux `cmd.window.<id>`, représentations, boutons Z/X/Y |
| Éditeur | `src/UI/Dialogs/ShortcutEditorDialog.*` | recherche, filtre par catégorie, enregistreur, alias, conflits immédiats, activation, restauration, ouverture du fichier |

Le gestionnaire **déclenche les actions existantes** (même slot que le menu ou le ruban) : il ne réimplémente
aucune commande. Une action indisponible (désactivée) ne réagit pas à son raccourci.

Avant cette refonte : 57 raccourcis codés en dur (`MainWindow_Actions.cpp`, `MainWindow_AI.cpp`,
`WindowManager`, `ViewportContainer`, `AppShell`), un catalogue jamais utilisé à l'exécution, un manuel
(`docs/shortcuts.txt`) et une rubrique d'aide recopiés à la main et divergents (F10, « M ouvre le dialogue »,
Ctrl+I « coupe » inexistants), F / Maj+F / R traités en double par `OccView::keyPressEvent`.

## Fichier `shortcut.txt`

Emplacement : `QStandardPaths::AppConfigLocation` (Windows : `%LOCALAPPDATA%\TSA Engineering\TSA\shortcut.txt`).
Créé avec les valeurs par défaut s'il manque ; **jamais réécrit sans action de l'utilisateur** (éditeur).

```
identifiant | raccourci(s) | état | par défaut | description
cmd.display.grid                    | G ; F7                | on   | G ; F7                | Afficher la grille — …
cmd.tool.dim_aligned                | D, A                  | on   | D, A                  | Cotation alignée — …
```

- alias séparés par « ; » ; suite de touches façon AutoCAD « D, A » ; vide ou « aucun » : pas de raccourci ;
- noms français acceptés (Maj, Suppr, Échap, Inser, Gauche…) ; état `on` / `off` (raccourci conservé) ;
- colonnes « par défaut » et « description » informatives (régénérées) ;
- commande absente : valeur par défaut ; commande inconnue : ignorée avec avertissement.

**Une configuration invalide n'est jamais appliquée** (syntaxe, combinaison inconnue, modificateur seul,
état invalide, identifiant en double, conflit) : la dernière configuration valide reste active et chaque
erreur est signalée dans la console avec son numéro de ligne. Écriture par l'éditeur : `QSaveFile`
(fichier temporaire puis remplacement, jamais de fichier à moitié écrit).

## Rechargement à chaud

`QFileSystemWatcher` sur le fichier **et** son dossier (le fichier remplacé par un éditeur est surveillé de
nouveau), notifications regroupées par un minuteur (250 ms). Contenu identique ou écrit par TSA lui-même
(fins de ligne CRLF comprises) : ignoré, pas de boucle. Modifier un raccourci ou en ajouter un à une commande
existante : sans redémarrage ni recompilation. Une nouvelle fonctionnalité exige du code.

## Conflits et contexte

- Deux commandes de même portée ne partagent pas un raccourci, et un raccourci ne commence pas la suite d'une
  autre (« D » et « D, A » : Qt attendrait la touche suivante). Portées : `window` (fenêtre principale, vue 3D
  comprise) recouvre `viewport` ; `startcenter` et `dialog:*` sont disjointes. Toutes les commandes du
  catalogue sont actuellement de portée `window`.
- Raccourcis de la fenêtre principale (`Qt::WindowShortcut`) : inactifs dans les dialogues (fenêtres
  séparées).
- Champs de saisie (`ShortcutInputGuard`) : lettres, chiffres, Suppr, flèches, Ctrl+C / V / X / Z / Y / A vont
  au champ ; aucune commande de modélisation ne part pendant une saisie. L'enregistreur de raccourci reçoit
  toutes les touches.
- Start Center : Nouveau / Ouvrir reprennent les raccourcis actifs de `cmd.file.new` / `cmd.file.open`.

## Conventions (inspirées d'AutoCAD)

| Famille | Raccourcis |
| :--- | :--- |
| Windows / Qt conservés | Ctrl+N, O, S, Maj+S, Z, Y, C, V, A, Suppr, F1, F11, Ctrl+W / Ctrl+F4 (fermer le projet) |
| Dessin (lettre seule, existants) | N, B, C, Alt+C, L, W ; M, Ctrl+R, Ctrl+D ; I, H, Alt+H… |
| Suites AutoCAD | Z, E / Z, W / Z, P (zoom étendu, fenêtre, précédent) ; T, R (ajuster) ; E, X (prolonger) ; O, F (décaler) ; D, I (mesurer) |
| Cotation « D, … » | D, A alignée ; D, L linéaire ; D, H ; D, X / Y / Z ; D, N angulaire ; D, V niveau ; D, C chaîne ; D, B cumulée ; D, E modifier |
| Charges « Q, … » (notation Eurocode) | Q, N nodale ; Q, U uniforme ; Q, T trapézoïdale ; Q, P ponctuelle sur barre ; Q, S surfacique ; Q, G poids propre ; Q, C cas et combinaisons |
| Appuis « A, … » | A, E encastrement ; A, A articulation ; A, S appui simple |
| Sélection Ctrl+Alt | I inverser ; N nœuds ; B poutres ; P poteaux ; S même section ; M même matériau |
| Représentations Alt+chiffre | Alt+1 physique, Alt+2 filaire, Alt+3 éléments finis, Alt+4 superposition |
| Panneaux Ctrl+chiffre | Ctrl+1 résultats, 2 propriétés, 3 navigateur, 4 plans, 5 visibilité, 6 éléments, 7 données d'analyse ; F2 console |
| Touches de fonction | F3 OSNAP, F5 calcul, Ctrl+F5 paramètres d'analyse, F7 grille, F8 note de calcul, F9 déformée, Maj+F9 réactions |
| Divers | Ctrl+Entrée répéter, Ctrl+G / Ctrl+Maj+G grilles, Ctrl+L niveaux, Ctrl+B bibliothèque, Ctrl+I / Ctrl+E IFC, Num+0 vue normale au plan, Ctrl+Maj+L charges, Ctrl+Maj+N nœud par coordonnées |

Écarts documentés : Alt+F4 n'est pas un raccourci TSA (Windows) ; F10 est laissé à Windows (barre de menus) ;
les lettres déjà utilisées par TSA (S = snap, M = déplacer, C = poteau…) gardent leur sens TSA plutôt que celui
d'AutoCAD ; Ctrl+Alt évite les combinaisons AltGr produisant un caractère sur clavier AZERTY (chiffres, E).

## Tests

Suite `shortcuts` (tests 257-266) : catalogue (identifiants, valeurs par défaut valides, aucun conflit,
raccourcis historiques conservés), lecture (BOM, CRLF, alias, accords, noms français, erreurs avec numéro de
ligne), écriture / relecture, conflits et portées, gestionnaire (création du fichier, refus, désactivation,
pas de connexion en double, action détruite), rechargement réel par `QFileSystemWatcher` (fichier remplacé
deux fois, contenu inchangé, contenu invalide, écriture propre sans écho), frappes simulées avec Qt Test
(nouveau / ancien raccourci, « D, A », champ de saisie, raccourci désactivé, commande indisponible), éditeur,
répétition, infobulles, `docs/shortcut.txt` synchronisé (régénérer :
`TSA_UPDATE_SHORTCUT_REFERENCE=1 TSA_TestSuite --suite=shortcuts`). Contrôle statique :
`python tools/check_shortcuts.py --strict`.
