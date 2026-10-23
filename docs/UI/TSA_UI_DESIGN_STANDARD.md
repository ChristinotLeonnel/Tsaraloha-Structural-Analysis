# TSA — Standard UI/UX & Règles de Conception Graphique

> **Document Officiel de Référence UI/UX pour TSA (Tsaraloha Structural Analysis)**  
> **Normes applicables** : ISO/IEC/IEEE 26514, IEEE Std 1063, conventions CAO/BIM professionnelles (Revit, Robot Structural Analysis).  
> **Conformité technique** : Qt 6.2+, C++20, système de thèmes `ThemeManager`.

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

## 2. Système Visuel & Tokens de Design

### 2.1. Palette Chromatique

| Rôle Sémantique | Thème Sombre (Défaut) | Thème Clair (AutoCAD/Revit Light) | Usage |
| :--- | :--- | :--- | :--- |
| **Fond Fenêtre (`Window`)** | `#1E2328` | `#F6F8FA` | Fond général des fenêtres et docks |
| **Fond Surface (`Base`)** | `#161B22` | `#FFFFFF` | Fond des champs de saisie, listes, arbres, tables |
| **Surface Cartouche (`AltBase`)**| `#212830` | `#EAEFF5` | Fond des en-têtes, badges, cartes |
| **Bordure Standard (`Border`)** | `#30363D` | `#D0D7DE` | Séparateurs, bordures de cadres et widgets |
| **Bordure Focus / Survol** | `#58A6FF` | `#0969DA` | Contour des champs actifs et boutons survolés |
| **Texte Principal (`Text`)** | `#E6EDF3` | `#24292F` | Libellés, valeurs, titres de formulaires |
| **Texte Secondaire (`Muted`)** | `#8B949E` | `#57606A` | Sous-titres, unités, textes d'aide |
| **Accent TSA (`Primary`)** | `#1F6FEB` | `#0969DA` | Bouton d'action principal, onglet sélectionné |
| **Accent Survol (`PrimaryHover`)**| `#388BFD` | `#0854AF` | État survolé du bouton primaire |
| **Succès (`Success`)** | `#238636` | `#1A7F37` | Validation positive, convergence, vert Eurocode |
| **Avertissement (`Warning`)** | `#D29922` | `#9E6A03` | Remarque non bloquante, vérification requise |
| **Erreur / Danger (`Destructive`)**| `#DA3633` | `#CF222E` | Suppression, calcul divergent, champ invalide |
| **Information (`Info`)** | `#58A6FF` | `#0969DA` | Bulle d'information, statut de modélisation |

### 2.2. Typographie & Échelle de Texte

La typographie par défaut repose sur la police système standard de haute lisibilité :
- **Police Principale** : `'Segoe UI', -apple-system, BlinkMacSystemFont, 'Inter', sans-serif`
- **Police Monospace (Code Tcl, logs, matrices)** : `'Cascadia Code', 'Consolas', monospace`

| Niveau | Taille | Graisse | Usage |
| :--- | :--- | :--- | :--- |
| **H1 (Entête Dialogue)** | `13–14 px` | Bold (`700`) | Titre dans la bannière de dialogue (`dialogTitle`) |
| **H2 (Titre Groupe/Section)**| `11–12 px` | SemiBold (`600`) | Titre de `QGroupBox`, onglet de ruban |
| **Corps (Body)** | `11 px` | Regular (`400`) | Libellés de formulaires, textes de boutons, cellules |
| **Légende / Suffixe (Caption)**| `10 px` | Regular (`400`) | Suffixes d'unités ($kN, m, °$), sous-titre de dialogue, infobulles |

### 2.3. Espacements & Rythme Visuel (Grille de 4 px)

- **Marge externe des dialogues** : `16 px`
- **Espacement vertical entre sections** : `12 px`
- **Espacement formulaire (lignes)** : `8 px`
- **Marges internes des GroupBox** : `8 px` horizontal, `12 px` haut, `8 px` bas
- **Padding des boutons** : `5 px` vertical, `14 px` horizontal (Revit standard)
- **Padding des champs de saisie** : `4 px` vertical, `8 px` horizontal
- **Rayon de courbure (`border-radius`)** : `3 px` (sobre, professionnel)

---

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

Pour éliminer la duplication de code et garantir l'uniformité, TSA propose l'infrastructure partagée suivante dans `src/UI/Widgets/` et `src/UI/Dialogs/` :

### 4.1. `StandardDialog` (`src/UI/Dialogs/StandardDialog.h`)
Classe de base héritant de `QDialog`, fournissant nativement :
- L'application automatique du dimensionnement (`DialogSizeCategory::Compact`, `Standard`, `Large`, `Studio`).
- L'intégration de la bannière `StandardDialogHeader`.
- La mise en place de la barre de boutons normalisée `StandardDialogButtonBox`.
- La gestion uniforme des touches clavier (`Entrée` pour valider, `Échap` pour annuler).
- Le centrage intelligent au premier plan au-dessus du viewport ou de la fenêtre parente.

### 4.2. `StandardDialogHeader` (`src/UI/Widgets/StandardDialogHeader.h`)
Widget autonome réutilisable :
- Affiche une icône SVG vectorielle thémée (32×32 px).
- Gère le titre (`dialogTitle="true"`) et le sous-titre (`dialogSubtitle="true"`).
- Inclut le séparateur inférieur discret (`dialogHeaderDivider="true"`).

### 4.3. `DialogStandardHelper` (`src/UI/Dialogs/DialogStandardHelper.h`)
Fonctions libres d'aide pour les boîtes de dialogue existantes :
- `applyStandardDialogLayout(QDialog* dlg, DialogSizeCategory cat)`
- `addStandardHeader(QVBoxLayout* layout, const QString& title, const QString& subtitle, const QString& iconPath)`
- `createStandardButtonRow(QDialog* dlg, QPushButton* primaryBtn, QPushButton* cancelBtn, const QString& helpTopic, QWidget* extraLeftWidget)`

---

## 5. Propriétés Dynamiques QSS & Remplacement des Styles Inline

Les styles CSS en dur (`setStyleSheet("...")`) sont strictement interdits dans les dialogues. Le tableau suivant récapitule les sélecteurs QSS centralisés dans `ThemeManager` :

| Sélecteur QSS | Propriété C++ | Rendu Visuel |
| :--- | :--- | :--- |
| `QPushButton[primary="true"]` | `btn->setProperty("primary", true);` | Bouton accent bleu TSA, texte blanc, gras, survol lumineux |
| `QPushButton[destructive="true"]`| `btn->setProperty("destructive", true);` | Bouton rouge d'alerte, texte blanc, pour suppressions |
| `QPushButton[success="true"]` | `btn->setProperty("success", true);` | Bouton vert de validation Eurocodes ou de succès |
| `QLabel[dialogTitle="true"]` | `lbl->setProperty("dialogTitle", true);` | Titre 13–14 px, gras, couleur primaire du texte |
| `QLabel[dialogSubtitle="true"]` | `lbl->setProperty("dialogSubtitle", true);`| Sous-titre 10 px, couleur estompée |
| `QLabel[status="info"]` | `lbl->setProperty("status", "info");` | Texte bleu info |
| `QLabel[status="warning"]` | `lbl->setProperty("status", "warning");`| Texte ambre avertissement |
| `QLabel[status="error"]` | `lbl->setProperty("status", "error");` | Texte rouge vif erreur |
| `QLabel[status="success"]` | `lbl->setProperty("status", "success");`| Texte vert vif validation |

---

## 6. Inventaire & Matrice de Conformité des Dialogues TSA

| Boîte de Dialogue | Catégorie | Entête | Boutons Standardisés | Styles Inline Purifiés | Statut |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `BarCreationDialog` (Barres / Poutres / Poteaux) | Standard ($540 \times 620$) | Oui | Oui (`[Aide] [Stretch] [Fermer] [Ajouter]`) | Oui | Conforme |
| `CableCreationDialog` (Câbles) | Standard ($540 \times 620$) | Oui | Oui (`[Aide] [Stretch] [Fermer] [Ajouter]`) | Oui | Conforme |
| `SurfaceCreationDialog` (Dalles / Voiles) | Standard ($540 \times 620$) | Oui | Oui (`[Aide] [Stretch] [Fermer] [Ajouter]`) | Oui | Conforme |
| `NodalLoadDialog` (Charges nodales) | Standard ($520 \times 580$) | Oui | Oui (`[Aide] [Stretch] [Fermer] [Appliquer]`) | Oui | Conforme |
| `MemberLoadDialog` (Charges sur barres) | Standard ($540 \times 600$) | Oui | Oui (`[Aide] [Stretch] [Fermer] [Appliquer]`) | Oui | Conforme |
| `LoadCaseDialog` (Cas & Combinaisons) | Large ($840 \times 620$) | Oui | Oui (`[Aide] [Stretch] [Fermer]`) | Oui | Conforme |
| `NewNodeDialog` (Création de nœud) | Compact ($460 \times 360$) | Oui | Oui (`[Aide] [Stretch] [Annuler] [Créer]`) | Oui | Conforme |
| `StructurePresetDialog` (Structures types) | Large ($860 \times 620$) | Oui | Oui (`[Aide] [Stretch] [Annuler] [Générer]`) | Oui | Conforme |
| `SectionCustomizationDialog` (Sections) | Large ($860 \times 620$) | Oui | Oui (`[Aide] [Stretch] [Annuler] [Valider]`) | Oui | Conforme |
| `GridDialog` / `GridSettingsDialog` | Large / Compact | Oui | Oui | Oui | Conforme |
| `LevelDialog` (Gestion des niveaux) | Compact ($460 \times 360$) | Oui | Oui (`[Aide] [Stretch] [Fermer] [Appliquer]`) | Oui | Conforme |
| `LibraryDialog` (Matériaux / Sections) | Large ($860 \times 620$) | Oui | Oui | Oui | Conforme |
| `ExtensionManagerDialog` (Modules TSALib) | Studio ($1020 \times 680$)| Oui | Oui | Oui | Conforme |

---

## 7. Directives pour les Développeurs et Agents IA

Lorsqu'un développeur ou un agent IA ajoute une nouvelle boîte de dialogue dans TSA :

1. **Obligation de Standardisation** :
   - Inclure `src/UI/Widgets/StandardDialogHeader.h` et `src/UI/Dialogs/StandardDialog.h` (ou `DialogStandardHelper.h`).
   - Définir une catégorie de taille adaptée (`Compact`, `Standard`, `Large` ou `Studio`).
   - Ne jamais introduire de taille fixe rigide (`setFixedSize`), mais toujours définir une taille minimale garantissant la non-troncature des composants.
2. **Zéro CSS Sauvage** :
   - Ne jamais écrire de `setStyleSheet(...)` contenant des codes de couleur hexadécimaux arbitraires.
   - Utiliser `btn->setProperty("primary", true);` pour le bouton d'action principal et laisser `ThemeManager` appliquer les styles du thème actif.
3. **Bouton d'Aide & Documentation** :
   - Tout dialogue doit inclure un lien vers le topic d'aide officiel correspondant via `TSA::UI::createHelpButton(this, "topic.name")`.
4. **Vérification Automatisée** :
   - Tout dialogue ajouté doit disposer d'un test unitaire dans la suite de tests UI (`TSA_TestSuite.exe --suite=appearance` ou `--suite=dialogs`), vérifiant la taille minimale, la conformité de l'entête et l'état des boutons.
