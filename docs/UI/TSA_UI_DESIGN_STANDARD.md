# TSA — Standard UI/UX & Règles de Conception Graphique

> **Document Officiel de Référence UI/UX pour TSA (Tsaraloha Structural Analysis)**  
> **Normes applicables** : ISO/IEC/IEEE 26514, IEEE Std 1063, conventions CAO/BIM professionnelles (Revit, Robot Structural Analysis).  
> **Conformité technique** : Qt 6.2+, C++20, système de thèmes `ThemeManager` (jetons sémantiques, § 2).
> **Mise à jour** : 2026-11-04 — jetons des deux thèmes, états, accessibilité, propriétés dynamiques, matrice mesurée.

---

## 1. Vision & Principes Directeurs

L'interface utilisateur de **TSA** doit refléter l'exigence d'un logiciel professionnel de calcul de structures et de modélisation BIM de premier plan (à l'instar d'Autodesk Revit ou Autodesk Robot Structural Analysis), tout en conservant l'identité visuelle propre à TSA, son logo, son ruban métier et son ergonomie réactive.

### Les 5 Règles d'Or UI/UX de TSA

1. **Hiérarchie Spatiale Inspirée BIM (Organisation Revit-like)** :
   - **Haut** : Barre de titre avec identité TSA (logo SVG officiel) + accès rapide + Ruban de commandes 9 onglets métier.
   - **Gauche** : Navigateur de projet (Arbre du modèle, Calques & Visibilité, Volet de dessin d'éléments).
   - **Centre** : Espace de modélisation 3D (OpenCASCADE) avec règles graduées, axes et ViewCube (priorité absolue à la zone de travail).
   - **Droite** : Inspecteur contextuel de propriétés (`PropertyPanel`), Vues & Plans de travail, État du projet, Résultats 3D.
   - **Bas** : Panneaux repliables (Console de messages, Données numériques d'analyse) et Barre d'état (unités SI, coordonnées, état du calcul).

2. **Source Unique de Vérité Métier** :
   - Aucun widget ni boîte de dialogue ne stocke l'état métier de manière autonome. Tout élément lit et écrit dans `TSA::Model::Model` par le biais de commandes réversibles (`ICommand` / `EditTransaction`).

3. **Thématisation Globale Centralisée (Zéro Style Arbitraire Inline)** :
   - Aucune couleur ou fond n'est hardcodé en dur dans les fichiers `.cpp` (`color: #00adb5`, `background-color: #2E7D32`, etc.).
   - Tous les composants utilisent les propriétés dynamiques (`primary="true"`, `destructive="true"`, `status="warning"`) et les feuilles de style globales de `ThemeManager` pour basculer sans friction entre le **Thème Sombre** et le **Thème Clair**.

4. **Standardisation Rigoureuse de Toutes les Boîtes de Dialogue** :
   - Chaque fenêtre modale ou non-modale adopte une bannière d'entête contextuelle (`StandardDialogHeader`), des marges uniformes (16 px), un espacement standardisé (10–12 px), des champs typés avec suffixes d'unités physiques ($m, mm, kN, kNm, MPa$) et une barre de boutons prédictive (`[Aide]? [Stretch] [Annuler/Fermer] [Action Primaire]`).

5. **Clavier, Accessibilité & Préservation des Données** :
   - `Entrée` valide l'action par défaut (`setDefault(true)`).
   - `Échap` annule ou ferme le dialogue.
   - Une validation échouée ne ferme jamais la boîte de dialogue : elle surligne l'erreur en rouge/orange et préserve les valeurs saisies par l'utilisateur.

---

## 2. Système Visuel & Jetons de Design

### 2.0. Principe : une seule source, des rôles et non des teintes

- Les couleurs de l'interface sont des **jetons sémantiques** (`TSA::UI::ThemeTokens`, `src/UI/Theme/ThemeManager.h`) : un
  jeton nomme une fonction (« fond d'un champ », « texte d'erreur »), jamais une teinte. Deux jeux existent :
  `ThemeManager::darkTokens()` et `ThemeManager::lightTokens()` ; `ThemeManager::instance().tokens()` rend celui du thème actif.
- **Une seule feuille de style** pour l'application, écrite avec des jetons (`@panel`, `@textMuted`…) et résolue par
  `ThemeManager::resolve()` à chaque changement de thème. Le changement de thème réapplique la feuille et la palette Qt
  (`QPalette`) : tout widget sans style local suit sans redémarrage.
- **Feuille locale** (cas particulier d'un composant) : `setStyleSheet(ThemeManager::instance().resolve("… @accentText …"))`,
  jamais de valeur hexadécimale. HTML généré (aide, console) : `tokens().<jeton>.name()`.
- **Hors jetons, et conservées** : couleurs **métier** du viewport (axes globaux et locaux, éléments, charges, résultats,
  sélection OCCT) et code couleur du **Blueprint** (catégories de nœuds, types de broches, canevas sombre). Elles portent une
  signification indépendante du thème.
- Contrôle automatique : test 611 (suite `appearance`) — jetons définis, contrastes WCAG, feuille sans jeton non résolu,
  bascule de thème, surfaces migrées sans couleur locale. `TSA_TEST_CAPTURE=<dossier>` y enregistre une planche du système
  et les surfaces migrées dans les deux thèmes.

### 2.1. Jetons de couleur

| Jeton | Sombre | Clair | Usage |
| :--- | :--- | :--- | :--- |
| `canvas` | `#1E2328` | `#E7EAEE` | Fond de la fenêtre principale, entre les docks |
| `panel` | `#1E2328` | `#F7F8FA` | Docks, dialogues, menus, arbres |
| `surface` | `#161B22` | `#FFFFFF` | Champs, listes, tableaux (zones qui reçoivent du contenu) |
| `surfaceAlt` | `#212830` | `#F1F3F6` | Lignes alternées, champs en lecture seule ou désactivés |
| `raised` | `#212830` | `#FFFFFF` | Boutons, onglet actif, fond du ruban |
| `chrome` | `#161B22` | `#EEF1F4` | Barres de menus, d'état, d'outils ; titres de dock ; en-têtes de tableau |
| `hover` / `pressed` | `#2D333B` / `#373E47` | `#E8EEF6` / `#D5E3F5` | Survol, enfoncement |
| `selection` / `selectionText` | `#1F3A5A` / `#79C0FF` | `#D7E6FA` / `#0A3D7C` | Élément sélectionné (arbres, listes, menus) |
| `border` / `borderSubtle` / `borderStrong` | `#30363D` / `#282E37` / `#484F58` | `#CDD4DC` / `#E1E5EA` / `#A9B3BE` | Cadres ; grilles et onglets inactifs ; survol neutre et poignées |
| `focus` | `#58A6FF` | `#0B63CE` | Contour du focus clavier |
| `text` / `textSecondary` / `textMuted` / `textDisabled` | `#E6EDF3` / `#C9D1D9` / `#8B949E` / `#6E7681` | `#1F2328` / `#3B434C` / `#59636E` / `#8C959F` | Quatre niveaux de texte |
| `accent` / `accentHover` / `accentPressed` | `#1F6FEB` / `#388BFD` / `#1158C7` | `#0B63CE` / `#0A55B0` / `#08458F` | Bouton principal (une seule teinte d'accent) |
| `accentBorder` / `accentText` / `accentSubtle` | `#388BFD` / `#58A6FF` / `#1F3A5A` | `#0B63CE` / `#0B5CC0` / `#E3EEFC` | Contour actif ; accent lisible comme texte ; fond d'élément actif ou de badge |
| `success` / `successText` / `successSubtle` | `#238636` / `#3FB950` / `#12261A` | `#1A7F37` / `#18732F` / `#E3F5E8` | Validation, vérification satisfaite |
| `warning` / `warningText` / `warningSubtle` | `#D29922` / `#D29922` / `#2E2410` | `#9A6700` / `#8A5A00` / `#FFF4D6` | Avertissement non bloquant |
| `danger` / `dangerText` / `dangerSubtle` | `#DA3633` / `#F85149` / `#3A1D1F` | `#C21F2C` / `#C21F2C` / `#FDECEC` | Suppression, erreur, champ invalide |
| `console` / `consoleText` | `#0D1117` / `#C9D1D9` | `#FFFFFF` / `#1F2328` | Console de commandes et journaux |
| `logTime`, `logInfo`, `logCommand`, `logSystem`, `logWarning`, `logError`, `logCritical`, `logDebug` | voir `ThemeManager.cpp` | idem | Niveaux de journal (`ThemeManager::logLevelColor`) |
| `rulerMajorTick` / `rulerMinorTick` | `#6E7681` / `#38414D` | `#6E7781` / `#AFB8C1` | Graduations des règles du viewport |

Le thème clair n'est pas une inversion du sombre : gris froids d'une seule teinte, fond de fenêtre plus soutenu que les
panneaux pour distinguer les docks, champs blancs, un seul accent bleu. Les anciens accesseurs (`primaryColor()`,
`windowColor()`, `surfaceColor()`, `borderColor()`, `textColor()`, `textMutedColor()`, `successColor()`, `warningColor()`,
`destructiveColor()`, `infoColor()`, `ruler*()`) restent et lisent les jetons.

### 2.2. Typographie & Échelle de Texte

- **Police principale** : `'Segoe UI'` (Windows), repli système. **Monospace** (console, code, matrices) : `'Cascadia Code', 'Consolas', monospace`.
- Tailles en pixels logiques : Qt les met à l'échelle du facteur d'affichage (100 % à 200 %).

| Niveau | Taille | Graisse | Usage |
| :--- | :--- | :--- | :--- |
| **H1 (Entête Dialogue)** | `13 px` | Bold (`700`) | `QLabel[dialogTitle="true"]` |
| **H2 (Titre Groupe/Section)** | `11 px` | SemiBold (`600`) | Titre de `QGroupBox`, onglet, en-tête de panneau |
| **Corps (Body)** | `11 px` | Regular (`400`) | Libellés, boutons, cellules, champs |
| **Légende (Caption)** | `10 px` | Regular / Bold | Unités, sous-titres, `QLabel[caption="true"]`, en-têtes de tableau |
| **Ruban** | `9–10 px` | Medium (`500`) | Libellés des commandes (contrainte de place) |

La hiérarchie passe d'abord par la **graisse et le niveau de texte** (`text` > `textSecondary` > `textMuted`), puis par la taille.

### 2.3. Espacements & Rythme Visuel (Grille de 4 px)

- **Marge externe des dialogues** : `16 px` ; **entre sections** : `12 px` ; **lignes de formulaire** : `8 px`
- **Marges internes des GroupBox** : `8 px` horizontal, `12 px` haut, `8 px` bas
- **Padding** : boutons `4–5 px × 10–14 px` ; boutons compacts (`compact="true"`) `2 × 8 px` ; champs `3 × 6 px`
- **Densité des panneaux** (Explorateur, Calques, Propriétés) : densité « établi », lignes de 24 à 28 px ; un panneau montre
  beaucoup d'éléments sans défilement.

### 2.4. Bordures, Rayons & Profondeur

- **Stratégie de profondeur unique : les bordures** (outil technique dense). Pas d'ombre portée dans les panneaux.
- Élévation par **étagement des surfaces** : `canvas` < `panel` < `surface` / `raised`, écarts de quelques points de luminosité.
- Rayons : `2–3 px` champs et boutons, `4 px` groupes et onglets. Jamais de grand rayon sur un petit élément.
- Bordures `1 px` : `border` (standard), `borderSubtle` (grilles), `borderStrong` (survol neutre), `accentBorder` (actif), `focus`.

### 2.5. États interactifs (obligatoires)

| État | Rendu |
| :--- | :--- |
| Survol | fond `hover` ; bordure `focus` (boutons, listes déroulantes) ou `borderStrong` (champs) |
| Focus clavier | bordure `focus` sur champs, listes déroulantes, boutons, boutons du ruban, onglets |
| Enfoncé | fond `pressed` (`accentPressed` pour le bouton principal) |
| Sélectionné | fond `selection`, texte `selectionText` (arbres, listes, tableaux, menus) |
| Coché / actif | fond `accentSubtle`, bordure `accentBorder`, texte `accentText` (`:checked`, `[active="true"]`) |
| Désactivé | texte `textDisabled`, fond `surfaceAlt`, bordure `borderSubtle` |
| Lecture seule | fond `surfaceAlt`, texte `textSecondary` (`QLineEdit:read-only`, `QAbstractSpinBox[readOnly="true"]`) |
| Invalide | bordure `danger`, fond `dangerSubtle` (`[invalid="true"]`) ; la valeur saisie est conservée |
| Vide | `QLabel[panelEmptyState="true"]` : une phrase qui dit quoi faire, jamais une zone blanche |

### 2.6. Accessibilité

- **Contraste** (vérifié par le test 611) : texte courant, atténué, accent et textes d'état ≥ **4,5:1** sur `panel`,
  `surface` et `chrome`, dans les deux thèmes ; texte sélectionné et texte du bouton principal ≥ 4,5:1 ; couleurs de journal
  ≥ 4,5:1 sur la console. `textDisabled` n'est pas soumis au minimum (WCAG 1.4.3, composants inactifs).
- **Jamais la couleur seule** pour porter une information : une erreur s'accompagne d'un texte (`status="error"`), une valeur
  invalide d'un message, un élément actif d'une forme (bordure, graisse).
- **Clavier** : focus visible partout (§ 2.5), ordre de tabulation logique, `Entrée` = action par défaut, `Échap` = annuler.
- **Zones de clic** : 24 px de haut au minimum pour les contrôles des panneaux, 28 px pour les boutons de dialogue.

## 3. Architecture des Fenêtres & Boîtes de Dialogue

### 3.1. Les 4 Catégories Standard de Dimensions

Pour garantir une expérience fluide sur tous les facteurs d'échelle Windows (100 %, 125 %, 150 %, 200 %), les dialogues TSA sont classés en 4 catégories formelles :

| Catégorie | Dimensions Minimales | Dimensions Recommandées | Usage Typique |
| :--- | :--- | :--- | :--- |
| **Compact** | $420 \times 300\text{ px}$ | $460 \times 360\text{ px}$ | Création nœud, niveau, paramètres simples, édition de cote |
| **Standard** | $520 \times 480\text{ px}$ | $560 \times 600\text{ px}$ | Création barre/poutre/poteau, câble, dalle, charges nodales/barres |
| **Large** | $740 \times 540\text{ px}$ | $860 \times 640\text{ px}$ | Cas de charge & combinaisons, préréglages structures, grille, matériaux |
| **Full / Studio** | $960 \times 620\text{ px}$ | $1080 \times 700\text{ px}$ | Gestionnaire d'extensions, analyse multi-moteurs, topologie avancée |

### 3.2. Structure Canonique d'une Boîte de Dialogue TSA

Toute boîte de dialogue professionnelle TSA comporte 3 zones verticales clairement délimitées :

```text
┌────────────────────────────────────────────────────────────────────────┐
│ [Icone 32x32]  TITRE DU DIALOGUE (H1 Bold)                             │
│                Sous-titre descriptif contextuel (10px Muted)          │
├────────────────────────────────────────────────────────────────────────┤  ◄── Séparateur 1px
│                                                                        │
│   [Groupe 1 : Paramètres géométriques]                                 │
│   ┌────────────────────────────────────────────────────────────────┐   │
│   │  Origine :    [  0.000 m  ]    Extrémité : [ 6.000 m  ]        │   │
│   │  Section :    [ IPE 300   ▾]   [ ⚙ Personnaliser... ]          │   │
│   └────────────────────────────────────────────────────────────────┘   │
│                                                                        │
│   [Groupe 2 : Propriétés mécaniques & Repère]                          │
│   ...                                                                  │
│                                                                        │
├────────────────────────────────────────────────────────────────────────┤  ◄── Espace / Ligne discrète
│ [ ? Aide ]                           [ Annuler ]  [ Action Primaire ]  │
└────────────────────────────────────────────────────────────────────────┘
```

1. **Bannière d'entête (`StandardDialogHeader`)** :
   - Icône vectorielle thémée (32×32 px).
   - Titre explicite sans jargon superflu.
   - Sous-titre guidant l'utilisateur sur l'objectif de la commande.
   - Ligne séparatrice horizontale propre.
2. **Zone de contenu (`Content Layout`)** :
   - Organisée en `QGroupBox` thématiques distincts.
   - `QFormLayout` ou `QGridLayout` avec libellés alignés à droite (`Qt::AlignRight | Qt::AlignVCenter`).
   - Saisie numérique toujours accompagnée de son unité physique via `setSuffix(" m")`, `setSuffix(" kN")`, `setSuffix(" MPa")`.
   - `QLineEdit` équipés de texte indicatif (`setPlaceholderText`) et bouton d'effacement rapide (`setClearButtonEnabled(true)`).
3. **Pied d'action standardisé (`StandardDialogButtonBox`)** :
   - Gauche : Bouton contextuel « Aide » (icône TSA ou `help.svg`, menant au topic d'aide en ligne officiel).
   - Centre : Espace étirable (`addStretch()`).
   - Droite :
     - Bouton d'annulation : « Annuler » ou « Fermer » (raccourci `Échap`, rejet sans mutation du modèle).
     - Bouton d'action principale : `QPushButton[primary="true"]` (ex. « Créer », « Appliquer », « Valider », « Enregistrer »), configuré comme bouton par défaut (`setDefault(true)`, raccourci `Entrée`).

---

## 4. Composants Communs Développés

Infrastructure partagée réellement présente dans `src/UI/Widgets/` et `src/UI/Dialogs/` (vérifiée dans le code le
2026-11-04). Il n'existe **pas** de classe de base `StandardDialog` : un dialogue reste un `QDialog` et applique le standard
avec les fonctions ci-dessous.

### 4.1. `StandardDialogHeader` (`src/UI/Widgets/StandardDialogHeader.h`)
Widget autonome réutilisable :
- Affiche une icône SVG vectorielle thémée (32×32 px).
- Gère le titre (`dialogTitle="true"`) et le sous-titre (`dialogSubtitle="true"`).
- Inclut le séparateur inférieur discret (`dialogHeaderDivider="true"`).

### 4.2. `DialogStandardHelper` (`src/UI/Dialogs/DialogStandardHelper.h`)
Fonctions libres (utilisées par 14 fichiers de `src/UI`) :
- `applyStandardDialogSizing(QDialog* dialog, DialogSizeCategory category)` : tailles minimale et recommandée (§ 3.1).
- `addStandardDialogHeader(QVBoxLayout* mainLayout, title, subtitle, …)` : en-tête `StandardDialogHeader`.
- `createStandardButtonRow(QDialog* dialog, QPushButton* primary, QPushButton* cancel, helpTopic, …)` : barre
  `[Aide] [étirement] [Annuler/Fermer] [Action principale]`, bouton principal marqué `primary="true"` et par défaut.

### 4.3. Aide contextuelle (`src/UI/HelpLauncher.h`)
- `createHelpButton(QWidget* parent, const QString& topicId)` : bouton « Aide » vers la rubrique officielle.

### 4.4. Thème (`src/UI/Theme/ThemeManager.h`)
- `ThemeManager::tokens()`, `resolve()`, `logLevelColor()` (§ 2.0) ; `setStyleProperty()` (§ 5).

---

## 5. Propriétés Dynamiques QSS & Remplacement des Styles Inline

Les feuilles de style locales avec des couleurs écrites en dur sont interdites. Un composant exprime son **rôle** par une
propriété dynamique ; la feuille de l'application (`ThemeManager`) le rend dans le thème actif. Une propriété qui change après
l'affichage se pose avec `TSA::UI::setStyleProperty(widget, "nom", valeur)` : elle réapplique le style (un simple
`setProperty` ne suffit pas).

| Sélecteur QSS | Propriété C++ | Rendu |
| :--- | :--- | :--- |
| `QPushButton[primary="true"]` | `setProperty("primary", true)` | Bouton principal, accent plein, texte blanc |
| `QPushButton[destructive="true"]` | `setProperty("destructive", true)` | Bouton de suppression, `danger` plein |
| `QPushButton[success="true"]` | `setProperty("success", true)` | Bouton de validation, `success` plein |
| `QPushButton[accentOutline="true"]` | `setProperty("accentOutline", true)` | Action principale secondaire (Ajouter, Appliquer) : fond `accentSubtle` |
| `QPushButton[active="true"/"false"]` | `setStyleProperty(btn, "active", bool)` | Bascule d'un groupe (axes Z/X/Y) : actif mis en avant, inactifs atténués |
| `QPushButton[compact="true"]`, `QComboBox[compact="true"]` | `setProperty("compact", true)` | Contrôle compact d'une barre d'outils de panneau |
| `QLineEdit[invalid="true"]`, `QAbstractSpinBox[invalid="true"]`, `QComboBox[invalid="true"]` | `setStyleProperty(w, "invalid", bool)` | Champ invalide (bordure `danger`) |
| `QLineEdit[emphasis="true"]`, `QAbstractSpinBox[emphasis="true"]` | `setProperty("emphasis", true)` | Saisie principale d'un dialogue |
| `QCheckBox[emphasis="true"]` | `setProperty("emphasis", true)` | Option importante (synchronisation en direct) |
| `QCheckBox[chip="true"]` | `setProperty("chip", true)` | Bascule mise en avant dans une barre d'outils (2D) |
| `QLabel[dialogTitle="true"]`, `QLabel[dialogSubtitle="true"]` | `setProperty(…, true)` | Titre et sous-titre d'en-tête de dialogue |
| `QLabel[status="info"/"success"/"warning"/"error"]` | `setStyleProperty(lbl, "status", …)` | Message d'état |
| `QLabel[hint="true"]` | `setProperty("hint", true)` | Aide, astuce (atténué, italique) |
| `QLabel[caption="true"]` | `setProperty("caption", true)` | Légende de barre d'outils (« Filtres : ») |
| `QLabel[badge="accent"]` | `setProperty("badge", "accent")` | Badge d'état (mode 2D) |
| `QLabel[panelHeader="true"]`, `QLabel[panelEmptyState="true"]` | `setProperty(…, true)` | En-tête de panneau ; état vide |
| `QWidget[toolStrip="true"]` | `setProperty("toolStrip", true)` et `Qt::WA_StyledBackground` | Barre d'outils d'une vue (fond `chrome`) ; ses enfants y sont compacts |
| `QWidget[cornerPatch="true"]` | idem | Coin de raccord entre les règles et la barre d'outils |
| `QTextEdit[console="true"]`, `QLineEdit[console="true"]`, `QLabel[consolePrompt="true"]` | `setProperty(…, true)` | Console de commandes (monospace, fond `console`) |

## 6. Inventaire & Matrice de Conformité des Dialogues TSA

Couleurs locales **mesurées dans le code** le 2026-11-04 (lignes du fichier contenant une couleur hexadécimale ; 0 attendu).
La version précédente de ce tableau déclarait toutes les fenêtres « purifiées », ce qui était faux pour sept d'entre elles.

| Boîte de Dialogue | Catégorie | Couleurs locales | `setStyleSheet` | Statut |
| :--- | :--- | :--- | :--- | :--- |
| `NewNodeDialog` (Création de nœud) | Compact | 0 | 0 | Conforme |
| `NodalLoadDialog` (Charges nodales) | Standard | 0 | 0 | Conforme |
| `LevelDialog` (Gestion des niveaux) | Large | 0 | 0 | Conforme |
| `LoadCaseDialog` (Cas & Combinaisons) | Large | 0 | 4 (mise en page) | Conforme |
| `SectionCustomizationDialog` (Sections) | Standard | 0 | 5 (mise en page) | Conforme |
| `GridDialog` (Lignes de construction) | Propre (min. 450 × 560, hors catégorie) | 0 | 0 | Conforme (migré le 2026-11-04) |
| `HelpDialog` (Aide) | Large | 0 (jetons résolus) | 1 | Conforme (migré le 2026-11-04) |
| `MemberLoadDialog` (Charges sur barres) | Standard | 1 | 1 | À migrer |
| `CableCreationDialog` (Câbles) | Standard | 3 | 4 | À migrer |
| `SurfaceCreationDialog` (Dalles / Voiles) | Standard | 3 | 3 | À migrer |
| `LibraryDialog` (Matériaux / Sections) | Large | 4 | 3 | À migrer |
| `BarCreationDialog` (Barres / Poutres / Poteaux) | Standard | 6 | 4 | À migrer |
| `StructurePresetDialog` (Structures types) | Large | 18 | 1 | À migrer |
| `ExtensionManagerDialog` (Modules TSALib) | Studio | 27 | 4 | À migrer |

---

## 7. Directives pour les Développeurs et Agents IA

Lorsqu'un développeur ou un agent IA ajoute ou modifie une boîte de dialogue dans TSA :

1. **Obligation de Standardisation** :
   - Utiliser `src/UI/Dialogs/DialogStandardHelper.h` (taille, en-tête, barre de boutons) et `src/UI/HelpLauncher.h` (aide).
   - Définir une catégorie de taille adaptée (`Compact`, `Standard`, `Large` ou `Studio`).
   - Ne jamais introduire de taille fixe rigide (`setFixedSize`), mais toujours définir une taille minimale garantissant la non-troncature des composants.
2. **Zéro couleur locale** :
   - Ne jamais écrire de couleur hexadécimale dans un `.cpp` de `src/UI` (sauf exceptions métier du § 2.0).
   - Exprimer le rôle par une propriété dynamique (§ 5) ; à défaut, `ThemeManager::instance().resolve("… @jeton …")`.
3. **Bouton d'Aide & Documentation** :
   - Tout dialogue inclut un lien vers la rubrique d'aide officielle via `TSA::UI::createHelpButton(this, "topic.name")`.
4. **Vérification Automatisée** :
   - Tout dialogue ajouté dispose d'un test dans la suite `appearance` (`TSA_TestSuite.exe --suite=appearance`) : taille
     minimale, en-tête, état des boutons et absence de couleur locale (modèle : test 611).

---

## 8. Migration des Couleurs Locales (état au 2026-11-04)

Règle : toute couleur d'une feuille locale est un jeton du thème actif (contrôlé par le test 611 sur les surfaces migrées).
Ordre : fichiers les plus chargés d'abord, sans remplacement automatique (chaque couleur reçoit le rôle de sa fonction).

| Fichier | État | Remarque |
| :--- | :--- | :--- |
| `src/UI/Theme/ThemeManager.cpp` | Migré | Une feuille de style modèle au lieu de deux copies (sombre, clair) ; états manquants ajoutés |
| `src/UI/Dialogs/GridDialog.cpp` | Migré | Feuille propre supprimée (copie de la feuille globale, figée au thème d'ouverture) ; « Appliquer » gardait les couleurs claires en sombre |
| `src/UI/Dock/LogConsoleDock.cpp` | Migré | Palette propre (gris ardoise) remplacée par les jetons `console` et `log*` ; journal recoloré au changement de thème |
| `src/UI/Ruler/ViewportContainer.cpp` | Migré | Barre d'outils, bascule 2D, badge, boutons d'axe : propriétés dynamiques |
| `src/UI/Dialogs/HelpDialog.cpp` | Migré | Feuille locale et HTML de l'aide résolus par jetons |
| `src/UI/Blueprint/BlueprintScene.cpp` | Exception | Code couleur métier du graphe (catégories, types de broches), canevas sombre volontaire |
| Autres fichiers de `src/UI` (environ 42) | À migrer | Inventaire : `grep -rlE "#[0-9A-Fa-f]{6}" src/UI --include=*.cpp` |
