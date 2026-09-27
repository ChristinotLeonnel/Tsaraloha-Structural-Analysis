# Guide Complet du Système d'Extensions et Bibliothèques TSALib
# TSA - Tsaraloha Structural Analysis

---

## 📌 Table des Matières

1. [Introduction & Vision Architecturale](#1-introduction--vision-architecturale)
2. [Architecture Technique de l'ExtensionSystem](#2-architecture-technique-de-lextensionsystem)
3. [Guide d'Utilisation de l'Interface Utilisateur](#3-guide-dutilisation-de-linterface-utilisateur)
   - [3.1 Accès depuis le Ruban Principal](#31-accès-depuis-le-ruban-principal)
   - [3.2 Navigation dans le Catalogue Master-Detail](#32-navigation-dans-le-catalogue-master-detail)
   - [3.3 Recherche et Filtrage Instantané](#33-recherche-et-filtrage-instantané)
   - [3.4 Fiches Techniques Interactives](#34-fiches-techniques-interactives)
   - [3.5 Rechargement à Chaud (Hot Reload)](#35-rechargement-à-chaud-hot-reload)
   - [3.6 Validation Globale de Conformité](#36-validation-globale-de-conformité)
   - [3.7 Exportation & Importation de Packages (.tsalib)](#37-exportation--importation-de-packages-tsalib)
4. [Tutoriel : Créer sa Propre Extension TSALib](#4-tutoriel--créer-sa-propre-extension-tsalib)
   - [4.1 Arborescence Standard d'une Extension](#41-arborescence-standard-dune-extension)
   - [4.2 Fichier manifest.json](#42-fichier-manifestjson)
   - [4.3 Ajouter un Matériau Personnalisé](#43-ajouter-un-matériau-personnalisé)
   - [4.4 Ajouter une Section ou un Profilé](#44-ajouter-une-section-ou-un-profilé)
   - [4.5 Ajouter un Câble ou un Toron](#45-ajouter-un-câble-ou-un-toron)
   - [4.6 Ajouter une Texture PBR](#46-ajouter-une-texture-pbr)
5. [Format de Packaging Autonome (.tsalib)](#5-format-de-packaging-autonome-tsalib)
   - [5.1 Structure Binaire du Fichier](#51-structure-binaire-du-fichier)
   - [5.2 Sécurité & Protection Anti-Path-Traversal](#52-sécurité--protection-anti-path-traversal)
6. [Garantie de Reproductibilité des Calculs (CHUNK_SNAP)](#6-garantie-de-reproductibilité-des-calculs-chunk_snap)
7. [Bancs de Tests Automatisés](#7-bancs-de-tests-automatisés)

---

## 1. Introduction & Vision Architecturale

Dans les logiciels de calcul traditionnels, les caractéristiques des matériaux (résistance du béton, module d'élasticité de l'acier) et les catalogues de profilés sont fréquemment codés en dur (*hardcodés*) dans le code C++ de l'exécutable. Cette approche rigide présente des inconvénients majeurs :
- **Recompilation obligatoire** pour ajouter un matériau ou corriger une propriété.
- **Risque de divergence** entre projets si les valeurs changent dans une nouvelle version du logiciel.
- **Impossibilité pour l'utilisateur** de créer ses propres catalogues normalisés d'entreprise sans toucher au code source.

**TSALib** révolutionne cette approche dans **TSA** en externalisant **100% des données d'ingénierie structurale** sous forme de fichiers ouverts JSON, d'images PNG PBR et de packages autonomes `.tsalib`.

### Avantages Clés :
- 🚀 **Zéro Recompilation** : Ajoutez des matériaux, modifiez des profilés métalliques, ajoutez des câbles ou remplacez des textures en temps réel.
- ⚡ **Démarrage Ultra-Rapide (< 1.2 ms)** : Grâce au chargement paresseux (*Lazy Indexing On-Demand*), l'application s'ouvre instantanément sans analyser prématurément des centaines de fichiers.
- 🎨 **Rendu PBR Réaliste** : Textures physiques sans raccord (*seamless*) intégrées au pipeline graphique OpenCASCADE (diffuse, albedo, rugosité, reflets métalliques).
- 🔒 **Traçabilité Normative & Reproductibilité Juridique** : Chaque calcul est scellé avec un instantané bitwise de ses propriétés mécaniques dans le fichier `.tsa` (*Mechanical Snapshot*).
- 📦 **Distribution en 1 Clic** : Empaquetez toute une bibliothèque d'entreprise dans un unique fichier `.tsalib` signé par empreinte cryptographique SHA-256.

---

## 2. Architecture Technique de l'ExtensionSystem

Le système est articulé autour de modules hautement découplés situés dans `src/ExtensionSystem/` :

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                      TSA::UI::ExtensionManagerDialog                    │
│      (Interface Utilisateur Moderne Master-Detail, Recherche, Télémétrie)│
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────────────┐
│                    TSA::ExtensionSystem::LibraryManager                 │
│         (Orchestrateur central du cycle de vie des bibliothèques)       │
└──────┬──────────────┬──────────────┬──────────────┬──────────────┬──────┘
       │              │              │              │              │
┌──────▼──────┐┌──────▼──────┐┌──────▼──────┐┌──────▼──────┐┌──────▼──────┐
│  Registry   ││   Loader    ││  Validator  ││    Cache     ││  Packager   │
│ In-Memory   ││ Lazy Loading││ Vérification││ Multi-Niveau ││ Compression │
│  Catalog    ││ Indexation  ││  Stricte    ││ Solides B-Rep││ SHA-256     │
│  Fast Lookup││  On-Demand  ││ Normative   ││ OpenCASCADE  ││ .tsalib     │
└─────────────┘└─────────────┘└─────────────┘└─────────────┘└─────────────┘
```

| Composant | Rôle Technique |
|---|---|
| `LibraryRegistry` | Registre central thread-safe contenant toutes les définitions actives (`MaterialDefinition`, `SectionDefinition`, `CableCatalogDefinition`). Lookup en $O(1)$. |
| `LibraryLoader` | Scanner de répertoires et indexeur asynchrone paresseux (*Lazy Loading*). Lit uniquement les en-têtes légères au boot (< 1.2 ms) et ne parse les corps JSON complets que lorsqu'un élément est réellement instancié dans le modèle. |
| `LibraryValidator` | Moteur d'audit garantissant l'intégrité absolue : syntaxe JSON, conformité des unités SI, cohérence dimensionnelle et validation des références normatives. Empêche tout crash. |
| `LibraryCache` | Cache mémoire multi-niveaux à haute vitesse (Hit Ratio > 80%) et gestionnaire de cache pour les solides 3D B-Rep OpenCASCADE (`TopoDS_Shape`). Évite de recalculer inutilement les géométries complexes des profilés IPE/HEA. |
| `TextureManager` | Découverte, mise en cache et application dynamique des textures PBR dans le moteur de rendu 3D OpenCASCADE. Gère le hot-reload des images sans perte de contexte OpenGL. |
| `ExtensionPackager` | Compresseur/Décompresseur de packages `.tsalib` avec protection anti-Path-Traversal (*Zip Slip prevention*) et vérification cryptographique bitwise SHA-256. |

---

## 3. Guide d'Utilisation de l'Interface Utilisateur

### 3.1 Accès depuis le Ruban Principal

Dans l'interface principale de TSA :
1. Cliquez sur l'onglet **Structure & Sections** du ruban supérieur.
2. Dans le panneau **Bibliothèques & Matériaux** (à droite), cliquez sur le bouton **Gestionnaire TSALib...** (icône livre bleu).
3. Le dialogue modale non-bloquant de gestion des extensions s'affiche instantanément.

```
+---------------------------------------------------------------------------------+
|  Structure & Sections  |  Analyse  |  Affichage                                 |
+---------------------------------------------------------------------------------+
|  [Poutre] [Poteau] [Dalle] [Voile] | [Matériaux...] [Gestionnaire TSALib...]    |
+---------------------------------------------------------------------------------+
```

---

### 3.2 Navigation dans le Catalogue Master-Detail

Le dialogue adopte une disposition ergonomique **Master-Detail** :
- **Volet Gauche (Arborescence des catégories)** :
  - 📁 **Extensions Installées** : Liste des packages déployés (ex: `TSALib Base Engineering Libraries v1.0.0`).
  - 🧱 **Matériaux** : Sous-catégories *Béton*, *Acier*, *Bois*, *Aluminium*, *Sol*, *Maçonnerie*, *Verre*.
  - 📐 **Sections & Profilés** : Sections rectangulaires, circulaires, caissons, profilés Eurocodes (*IPE*, *HEA*, *HEB*, *UPN*, *Cornières L*, *Tés*).
  - ⛓️ **Câbles & Haubans** : Torons 7 fils (EN 10138-3), Barres de post-tension (EN 10138-4), Haubans multitorons (PSS), Câbles clos porteurs (EN 1993-1-11), Suspentes.
  - 🎨 **Textures PBR** : Catalogue visuel de toutes les textures de matériaux disponibles pour le rendu 3D.
  - 📖 **Normes Eurocodes** : Liste des normes officielles associées (EN 1990, EN 1991, EN 1992, EN 1993, ASTM A416).

- **Volet Droit Supérieur (Tableau interactif)** :
  - Affiche les éléments de la catégorie sélectionnée avec leurs caractéristiques principales (Identifiant, Nom, Résistance caractéristique, Module d'Young, Section nette, Masse linéique).

- **Volet Droit Inférieur (Fiche Technique)** :
  - Présentation détaillée de l'élément sélectionné avec mise en forme typographique soignée, équations, références aux clauses normatives et propriétés de calcul.

---

### 3.3 Recherche et Filtrage Instantané

En haut à gauche du dialogue, tapez simplement un mot-clé dans la barre de recherche :
- Exemple : tapez `C25` pour filtrer instantanément le béton C25/30.
- Exemple : tapez `IPE` pour afficher tous les profilés en I (IPE 100, 160, 200, 240, 300).
- Exemple : tapez `Stay` ou `PSS` pour isoler les haubans de ponts.

Le filtrage est temps réel (mise à jour à chaque frappe) et conserve le contexte de la catégorie sélectionnée.

---

### 3.4 Fiches Techniques Interactives

Lorsque vous cliquez sur un élément du tableau, la zone inférieure génère une fiche complète :
- **Nom et Identifiant technique** (ex: `concrete.c25_30`, `steel.ipe200`, `cable.stay_pss_19_15_7`).
- **Référence normative certifiée** : Norme de référence, année d'édition et clause d'application (ex: `EN 1992-1-1:2004 Clause 3.1.2`).
- **Tableau des Propriétés Mécaniques & Physiques** :
  - Module d'Young $E$ (en GPa).
  - Résistance caractéristique $f_{ck}$ ou limite d'élasticité $f_y$ (en MPa).
  - Résistance en traction $f_{ctm}$ ou limite à la rupture $f_{tk}$.
  - Masse volumique $\rho$ (en kg/m³) ou masse linéique (en kg/m).
  - Coefficients thermiques, coefficient de Poisson $\nu$.

---

### 3.5 Rechargement à Chaud (Hot Reload)

Avez-vous modifié un fichier JSON de matériau, ajouté un profilé métallique ou retouché une texture PNG sur votre disque ?
- Cliquez simplement sur le bouton **Recharger à chaud** dans la barre d'outils du gestionnaire.
- **Résultat immédiat** :
  1. Tous les fichiers JSON et images du disque sont réanalysés.
  2. Le registre en mémoire vive est mis à jour instantanément.
  3. Le cache de solides 3D OpenCASCADE est synchronisé.
  4. La vue 3D OpenCASCADE est rafraîchie sans aucun artefact.
  5. Les menus déroulants de la fenêtre principale (Panneau des propriétés) intègrent immédiatement les nouveaux éléments.
  6. **Aucun redémarrage de TSA n'est nécessaire !**

---

### 3.6 Validation Globale de Conformité

Cliquez sur le bouton **Valider l'intégrité** :
- Le moteur `LibraryValidator` analyse l'intégralité des extensions actives.
- Si tout est conforme : un message de succès s'affiche confirmant que 100% des fiches sont rigoureusement valides.
- En cas d'anomalie dans un fichier JSON (ex: unité inconnue, valeur physique négative pour une masse, syntaxe JSON invalide) : un rapport précis liste les erreurs et avertissements pour correction immédiate.

---

### 3.7 Exportation & Importation de Packages (.tsalib)

#### Exporter une extension en package autonome :
1. Dans le gestionnaire, sélectionnez l'extension à distribuer (ou laissez TSALib par défaut).
2. Cliquez sur le bouton **Exporter (.tsalib)...**.
3. Choisissez le dossier de destination et le nom de fichier (ex: `MaBibliothequeBois.tsalib`).
4. TSA compresse automatiquement tous les manifests, fiches JSON et textures en appliquant une compression zlib haute performance et en apposant une empreinte numérique SHA-256.

#### Importer et installer un package :
1. Cliquez sur le bouton **Importer (.tsalib)...**.
2. Sélectionnez le fichier `.tsalib` téléchargé ou reçu d'un confrère.
3. TSA effectue les opérations suivantes en quelques millisecondes :
   - Vérifie la signature magic et l'intégrité cryptographique SHA-256.
   - Contrôle la sécurité des chemins relatifs (protection anti-Path Traversal).
   - Extrait les fichiers dans le répertoire officiel d'extensions de l'utilisateur (`Extensions/<extensionId>/`).
   - Déclenche automatiquement le rechargement à chaud.
4. Les nouveaux matériaux et profilés sont immédiatement utilisables dans tous vos projets !

---

## 4. Tutoriel : Créer sa Propre Extension TSALib

### 4.1 Arborescence Standard d'une Extension

Pour créer une extension manuellement (par exemple pour un projet spécifique ou des matériaux locaux), créez un dossier dans `Extensions/` (ex: `Extensions/BureauEtudes_Bois/`) avec la structure suivante :

```text
BureauEtudes_Bois/
├── manifest.json              # Obligatoire : Carte d'identité de l'extension
├── Materials/                 # Fiches JSON des matériaux
│   ├── azobe_d70.json
│   └── bilinga_d50.json
├── Sections/                  # Fiches JSON des sections de calcul
│   └── poutre_lamelle_colle.json
└── Textures/                  # Textures PBR de rendu 3D (optionnel)
    └── azobe_diffuse.png
```

---

### 4.2 Fichier manifest.json

Le fichier `manifest.json` doit obligatoirement se trouver à la racine du dossier :

```json
{
  "$schema": "https://tsaraloha.org/schemas/tsalib-manifest-v1.json",
  "id": "com.bureau.bois_exotiques",
  "name": "Catalogue Bois Tropicaux & Lamellé-Collé",
  "version": "1.0.0",
  "formatVersion": "1.0",
  "author": "Bureau d'Études Structures Bois",
  "license": "MIT",
  "description": "Matériaux bois haute densité selon EN 1995-1-1 (Eurocode 5).",
  "kind": "data_extension",
  "categories": [
    "Materials",
    "Sections",
    "Textures"
  ]
}
```

---

### 4.3 Ajouter un Matériau Personnalisé

Créez un fichier JSON dans le sous-dossier `Materials/` (ex: `Materials/azobe_d70.json`) :

```json
{
  "id": "timber.azobe_d70",
  "name": "Bois Azobé D70",
  "category": "Timber",
  "version": "1.0",
  "standard": {
    "name": "EN 338 / EN 1995-1-1",
    "edition": "2016",
    "clause": "Table 1 - Classes D"
  },
  "physicalProperties": {
    "youngModulus": { "value": 20000.0, "unit": "MPa" },
    "shearModulus": { "value": 1250.0, "unit": "MPa" },
    "poissonRatio": 0.35,
    "density": { "value": 1050.0, "unit": "kg/m3" },
    "thermalExpansion": 5.0e-6
  },
  "mechanicalProperties": {
    "bendingStrength": { "value": 70.0, "unit": "MPa" },
    "tensionParallel": { "value": 42.0, "unit": "MPa" },
    "compressionParallel": { "value": 34.0, "unit": "MPa" },
    "shearStrength": { "value": 5.6, "unit": "MPa" }
  },
  "visualProperties": {
    "color": "#4A2E18",
    "texture": "wood.png",
    "roughness": 0.75,
    "metallic": 0.0
  }
}
```

> **Unités acceptées par TSA** :
> - Modules et résistances : `"MPa"`, `"GPa"`, `"Pa"`, `"N/mm2"`, `"kN/cm2"`.
> - Masses volumiques : `"kg/m3"`, `"t/m3"`, `"g/cm3"`, `"kN/m3"`.
> - Longueurs et dimensions : `"m"`, `"mm"`, `"cm"`, `"in"`, `"ft"`.

---

### 4.4 Ajouter une Section ou un Profilé

Créez un fichier JSON dans `Sections/` ou `Profiles/` (ex: `Sections/glulam_200x600.json`) :

```json
{
  "id": "timber.glulam_200x600",
  "name": "Lamellé-Collé GL28h 200x600",
  "category": "Glulam",
  "shapeType": "Rectangular",
  "version": "1.0",
  "dimensions": {
    "width": { "value": 200.0, "unit": "mm" },
    "height": { "value": 600.0, "unit": "mm" }
  },
  "materialRef": "timber.gl28h",
  "standard": {
    "name": "EN 14080",
    "edition": "2013",
    "clause": "Table 4"
  }
}
```

---

### 4.5 Ajouter un Câble ou un Toron

Créez un fichier JSON dans `Cables/` (ex: `Cables/monotoron_t15.json`) :

```json
{
  "id": "prestressing.strand_15_7_y1860",
  "name": "Toron 7 fils 15.7mm T15S Y1860S7",
  "category": "PrestressingStrand",
  "version": "1.0",
  "standard": {
    "name": "EN 10138-3",
    "edition": "2009",
    "clause": "Table 4"
  },
  "nominalDiameter": { "value": 15.7, "unit": "mm" },
  "nominalArea": { "value": 150.0, "unit": "mm2" },
  "linearMass": { "value": 1.18, "unit": "kg/m" },
  "characteristicBreakingForce": { "value": 279.0, "unit": "kN" },
  "characteristicYieldForce": { "value": 246.0, "unit": "kN" },
  "elasticModulus": { "value": 195000.0, "unit": "MPa" },
  "relaxationClass": 2,
  "relaxation1000hPercent": 2.5
}
```

---

### 4.6 Ajouter une Texture PBR

Déposez votre fichier image PNG dans le sous-dossier `Textures/` (ex: `Textures/mon_bois.png`).
- Format recommandé : PNG 24-bit ou 32-bit sans compression avec perte.
- Dimensions recommandées : 512x512 ou 1024x1024 en motif répétitif sans raccord (*seamless*).
- Référencez simplement le nom du fichier dans le bloc `visualProperties.texture` de vos matériaux.

---

## 5. Format de Packaging Autonome (.tsalib)

Le format `.tsalib` est un format d'archive binaire scellé conçu spécifiquement pour le partage et la distribution d'extensions TSA.

### 5.1 Structure Binaire du Fichier

```text
+--------------------------------------------------------------------------+
| Magic Bytes (8 octets) : "TSALIB\1\0" (0x000142494C415354)               |
+--------------------------------------------------------------------------+
| Version du Format (uint32) : 1 | Flags réservés (uint32) : 0             |
+--------------------------------------------------------------------------+
| Manifeste JSON Brut (QByteArray sérialisé avec taille UTF-8)             |
+--------------------------------------------------------------------------+
| Nombre de Fichiers (uint32) : N                                          |
+--------------------------------------------------------------------------+
| Pour chaque fichier i de 1 à N :                                         |
|  - Chemin relatif (ex: "Materials/concrete_c25_30.json")                 |
|  - Taille décompressée brute (uint64 en octets)                          |
|  - Empreinte cryptographique SHA-256 du contenu brut (32 octets)          |
|  - Données du fichier compressées (algorithme zlib niveau 9)              |
+--------------------------------------------------------------------------+
```

### 5.2 Sécurité & Protection Anti-Path-Traversal

Lors de l'installation d'un package `.tsalib`, le moteur [ExtensionPackager](file:///e:/Book/Dev/TSA/src/ExtensionSystem/ExtensionPackager.h) applique une politique de sécurité stricte :
1. **Rejet des chemins absolus** : Tout chemin débutant par `/`, `\` ou contenant une lettre de lecteur Windows (`C:`) est immédiatement bloqué.
2. **Rejet du Path Traversal (Zip Slip)** : Les séquences de remontée de dossier (`..` ou `../` ou `..\`) sont interdites.
3. **Vérification cryptographique bitwise** : Chaque fichier extrait voit son empreinte SHA-256 recalculée et comparée à celle certifiée dans l'archive. Si un seul octet a été altéré ou corrompu, l'extraction est rejetée et le modèle structural est préservé.

---

## 6. Garantie de Reproductibilité des Calculs (CHUNK_SNAP)

Dans un bureau d'études ou un cabinet d'expertise judiciaire, un calcul structural certifié aujourd'hui doit pouvoir être réouvert et recalculé avec une **stricte identité de résultats** dans 10 ou 30 ans, même si les bibliothèques logicielles ont évolué entre-temps.

### Comment TSA garantit cette pérennité :
1. **Au moment de la modélisation** : Lorsqu'un poteau ou une poutre se voit attribuer le matériau `concrete.c25_30`, TSA associe au composant structural une référence versionnée (`libraryId`, `definitionId`, `definitionVersion`).
2. **Lors de la sauvegarde du projet (`.tsa`)** : TSA injecte le bloc binaire **`CHUNK_SNAP`** (`0x534E4150` = "SNAP") dans le fichier de projet. Ce bloc sérialise la totalité des propriétés physiques réelles employées pour le calcul (Module d'Young, résistance, densité, etc.).
3. **À la réouverture du fichier** :
   - TSA compare le snapshot figé du projet avec la bibliothèque active sur le poste.
   - Si la bibliothèque a été modifiée entre-temps, TSA **détecte la dérive normative** et continue d'utiliser fidèlement les valeurs certifiées du snapshot d'origine.
   - L'ingénieur a le choix d'actualiser son modèle vers la nouvelle version de la norme ou de conserver la certification historique.

---

## 7. Bancs de Tests Automatisés

Le système d'extensions et bibliothèques TSALib est couvert par **11 suites de tests unitaires dédiées** (Tests 38 à 48) intégrées dans le banc d'essai général (`TSA_TestSuite.exe`) :

| N° Test | Domaine Couvert | Sous-tests |
|---|---|---|
| **Test 38** | Fondations `ExtensionTypes` (Version sémantique, conversions SI, snapshots) | 4 subtests |
| **Test 39** | Noyau `ExtensionSystem` (Registry, Loader, Validator, Cache, Versioning) | 6 subtests |
| **Test 40** | Fichier `manifest.json` et découverte automatique sur disque | 4 subtests |
| **Test 41** | Externalisation des 16 fiches matériaux Eurocode et ponts `Material` | 4 subtests |
| **Test 42** | Externalisation des 14 textures PBR et gestionnaire `TextureManager` | 4 subtests |
| **Test 43** | Externalisation des 22 sections/profilés Eurocodes et solides 3D OCCT | 4 subtests |
| **Test 44** | Externalisation des 19 câbles, torons et haubans Eurocode/ASTM | 4 subtests |
| **Test 45** | Snapshots de calcul `CHUNK_SNAP` dans le format de fichier `.tsa` | 4 subtests |
| **Test 46** | Démarrage ultra-rapide (< 1.2 ms), Lazy Loading et cache multi-niveaux | 4 subtests |
| **Test 47** | Interface graphique `ExtensionManagerDialog`, navigation, recherche et télémétrie | 5 subtests |
| **Test 48** | Packaging `.tsalib`, contrôle d'intégrité SHA-256, extraction sécurisée et export 1-clic | 5 subtests |

Pour exécuter la suite complète des tests :
```powershell
.\build\Release\TSA_TestSuite.exe
```
Résultat garanti : **48 / 48 tests validés avec succès (0 échec, 0 régression)**.
