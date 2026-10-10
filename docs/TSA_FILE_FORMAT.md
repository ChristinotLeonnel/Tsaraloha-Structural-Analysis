# Spécification Technique du Format de Fichier Natif TSA (.tsa)

**Logiciel :** TSA — MadaStructEngine  
**Format :** TSA Binary Project File  
**Extension native :** `.tsa`  
**Magic Signature :** `TSAF` (`0x46415354` en little-endian)  
**Version actuelle :** Spécification 1.5 (Major = 1, Minor = 5) — 1.1 : chunk `LOAD` (2026-10-03) ; 1.2 : bloc d'aperçu ; 1.3 : chunk `BIMM` (2026-10-06) ; 1.4 : chunk `SETT` (2026-10-06) ; 1.5 : chunk `TOPO` (2026-10-10, topologie et numérotation, JSON UTF-8 `TopologySettings`, voir docs/TOPOLOGY.md), ajouts non cassants  

---

## 1. Vue d'Ensemble & Principes Fondamentaux

Le format `.tsa` est le format binaire propriétaire et natif de stockage et d'échange du logiciel **TSA / MadaStructEngine**.

### Principes Clés d'Architecture :
1. **Modèle Structural Source de Vérité :** Le fichier `.tsa` sérialise exclusivement les données structurelles réelles (Nœuds, Barres, Sections canoniques, Matériaux, Dalles, Voiles, Semelles, Treillis, Grilles). Il ne sérialise **JAMAIS** de maillages tessellés arbitraires ou de formes B-Rep brutes `TopoDS_Shape`.
2. **Reconstruction Géométrique OCCT Déterministe :** Au chargement, le module géométrique (`BeamGeometry`, `OccView`) reconstruit dynamiquement et analytiquement la géométrie 3D OpenCASCADE (cylindres analytiques `Geom_CylindricalSurface`, profilés IPE/UPN, prismes polygonaux) garantissant une fidélité mathématique absolue.
3. **Architecture Extensible par Chunks (FourCC) :** Les données utiles sont structurées en blocs identifiés par des codes 4 caractères (`CHUNK_PROJ`, `CHUNK_NODE`, etc.). Tout lecteur d'une version antérieure ou future peut ignorer les chunks inconnus sans altérer la lecture du reste de la structure.
4. **Intégrité et Sécurité d'Entrée :** Le header contient un checksum IEEE 802.3 CRC32 vérifiant l'intégrité de la charge utile. Des limites de sécurité strictes (`MAX_SAFE_NODES`, `MAX_SAFE_PAYLOAD_SIZE`, etc.) protègent l'application contre les fichiers corrompus, altérés ou tronqués.
5. **Compression & Chiffrement Abstraits :** Le format supporte nativement la compression Zlib (`qCompress` / `qUncompress`) et prépare l'extensibilité du chiffrement sécurisé sans divulgation de clé.

---

## 2. Structure Binaire Globale

Un fichier `.tsa` se compose de deux grandes parties contiguës :

```
┌─────────────────────────────────────────────────────────────┐
│                      HEADER BINAIRE                         │
│                    (Fixe : 256 octets)                      │
├─────────────────────────────────────────────────────────────┤
│                      CHARGE UTILE                           │
│     (Données sérialisées par Chunks FourCC, compressées)    │
│  [CHUNK_PROJ] [CHUNK_COOR] [CHUNK_GRID] [CHUNK_NODE] ...    │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Spécification Détaillée du Header (256 octets)

Le header est aligné sur 1 octet (`#pragma pack(push, 1)`) et possède une taille fixe garantie de 256 octets :

| Champ | Type | Taille (octets) | Description |
| :--- | :--- | :--- | :--- |
| `magic` | `uint32_t` | 4 | Signature `'T','S','A','F'` (`0x46415354`) |
| `formatMajorVersion` | `uint16_t` | 2 | Version majeure du format (ex: 1) |
| `formatMinorVersion` | `uint16_t` | 2 | Version mineure du format (ex: 0) |
| `appMajorVersion` | `uint16_t` | 2 | Version majeure du logiciel TSA |
| `appMinorVersion` | `uint16_t` | 2 | Version mineure du logiciel TSA |
| `appPatchVersion` | `uint16_t` | 2 | Version patch du logiciel TSA |
| `flags` | `uint16_t` | 2 | Drapeaux binaires (Bit 0: Compressé, Bit 1: Chiffré) |
| `headerSize` | `uint32_t` | 4 | Taille du header (256 octets) |
| `fileSize` | `uint64_t` | 8 | Taille totale attendue du fichier sur disque |
| `payloadOffset` | `uint64_t` | 8 | Offset de début de la charge utile (256) |
| `payloadCompressedSize` | `uint64_t` | 8 | Taille en octets du payload compressé sur disque |
| `uncompressedSize` | `uint64_t` | 8 | Taille en octets du payload après décompression |
| `checksumCRC32` | `uint32_t` | 4 | Checksum CRC-32 (IEEE 802.3) du payload |
| `compressionMethod` | `uint8_t` | 1 | 0 = Aucune, 1 = Zlib, 2 = ZSTD, 3 = LZ4 |
| `encryptionMethod` | `uint8_t` | 1 | 0 = Aucune, 1 = AES-256-GCM |
| `reservedFlags` | `uint16_t` | 2 | Réservé pour alignement |
| `projectName` | `char[64]` | 64 | Nom du projet (chaîne UTF-8 null-terminated) |
| `author` | `char[48]` | 48 | Auteur / Ingénieur structure |
| `creationTimestamp` | `char[24]` | 24 | Date ISO 8601 de création |
| `lastModifiedTimestamp` | `char[24]` | 24 | Date ISO 8601 de dernière sauvegarde |
| `reserved` | `uint8_t[24]` | 24 | Octets de réserve pour évolutions futures |

---

## 4. Architecture des Chunks (FourCC)

La charge utile décompressée est une séquence linéaire de Chunks.  
Chaque chunk commence par un en-tête `ChunkHeader` de 16 octets :

```cpp
struct ChunkHeader {
    uint32_t chunkId;      // Identifiant FourCC (ex: 'PROJ', 'NODE', 'BARS')
    uint32_t chunkSize;    // Taille totale du payload du chunk en octets
    uint32_t elementCount; // Nombre d'entités contenues dans ce chunk
    uint32_t reserved;     // Réservé / flags spécifiques au chunk
};
```

### Table des Chunks Officiels Spécification 1.0

| Code FourCC | Constante C++ | Description |
| :--- | :--- | :--- |
| `PROJ` (`0x4A4F5250`) | `CHUNK_PROJ` | Métadonnées de projet, nom, auteur, unités de travail |
| `COOR` (`0x524F4F43`) | `CHUNK_COOR` | Système d'axes orthonormé et grille cartésienne/cylindrique |
| `GRID` (`0x44495247`) | `CHUNK_GRID` | Définition des grilles architecturales (`GridManager`) |
| `NODE` (`0x45444F4E`) | `CHUNK_NODE` | Nœuds structuraux 3D (ID, X, Y, Z en mètres) |
| `BARS` (`0x53524142`) | `CHUNK_BARS` | Poutres et barres universelles filaires |
| `COLS` (`0x534C4F43`) | `CHUNK_COLS` | Poteaux verticaux et inclinés |
| `SLAB` (`0x42414C53`) | `CHUNK_SLAB` | Dalles et planchers 2D |
| `WALL` (`0x4C4C4157`) | `CHUNK_WALL` | Voiles porteurs et murs de contreventement |
| `FNDN` (`0x4E444E46`) | `CHUNK_FNDN` | Semelles et fondations superficielles |
| `TRUS` (`0x53555254`) | `CHUNK_TRUS` | Membres de treillis articulés |
| `SECT` (`0x54434553`) | `CHUNK_SECT` | Catalogue de sections personnalisées |
| `MATE` (`0x4554414D`) | `CHUNK_MATE` | Bibliothèque de matériaux personnalisés |
| `SUPP` (`0x50505553`) | `CHUNK_SUPP` | Conditions aux limites et appuis nodaux |
| `LOAD` (`0x44414F4C`) | `CHUNK_LOAD` | Cas de charges, combinaisons, charges nodales & linéiques — **écrit et lu depuis la spécification 1.1** (voir §5.x) |
| `BIMM` (`0x4D4D4942`) | `CHUNK_BIMM` | Couche BIM : produits physiques, GlobalId IFC, Psets, classifications, mapping physique → analytique — **spécification 1.3** (voir §5.y) |
| `SETT` (`0x54544553`) | `CHUNK_SETT` | Paramètres d'analyse (`AnalysisContext`) — **spécification 1.4** (voir §5.z) |
| `TOPO` (`0x4F504F54`) | `CHUNK_TOPO` | Topologie et numérotation (`TopologySettings`, JSON UTF-8 versionné) — **spécification 1.5**, facultatif ; absent : valeurs par défaut (docs/TOPOLOGY.md) |
| `RSLT` (`0x544C5352`) | `CHUNK_RSLT` | Résultats de calcul (déplacements, efforts internes N, Vy, Vz, Mt, My, Mz) |

**COOR / GRID** : un JSON UTF-8 préfixé de sa longueur (u16). Au-delà de 65 535 octets, préfixe `0` puis le JSON
jusqu'à la fin du chunk (`writeChunkText` / `readChunkText`, 2026-10-09, BUG-041). Une version antérieure lit alors
un JSON vide (comportement d'avant la correction) ; aucun fichier existant n'est réinterprété. Les chaînes courtes
(noms…) sont limitées à 65 535 octets, tronquées sur une frontière UTF-8.

---

## 5. Spécification des Enregistrements Structuraux

### 5.1 Nœuds (`CHUNK_NODE`)
Chaque nœud sérialise :
- `id` : `int32_t`
- `x` : `double` (mètres)
- `y` : `double` (mètres)
- `z` : `double` (mètres)

### 5.2 Barres / Poutres (`CHUNK_BARS`) & Poteaux (`CHUNK_COLS`)
Chaque élément filaire sérialise :
- `id` : `int32_t`
- `name` : chaîne UTF-8 (longueur `uint32_t` + octets)
- `role` : `uint8_t` (0=Generic, 1=Beam, 2=Column, 3=Brace, 4=Tie, 5=Truss, 6=SteelMember)
- `startNodeId` : `int32_t` (référence stricte vers l'ID du nœud de début)
- `endNodeId` : `int32_t` (référence stricte vers l'ID du nœud de fin)
- `rotation` : `double` (angle $\gamma$ en degrés)
- `eccentricity` : `uint8_t` (0=None, 1=TopFlange, 2=BottomFlange, 3=LeftFlange, 4=RightFlange)
- `endRelease` : 6 octets booléens (`fx, fy, fz, mx, my, mz`)
- **Section canonique intégrée :**
  - `name` : chaîne
  - `shape` : `uint8_t` (0=Rectangular, 1=Circular, 2=IShape, 3=Pipe, 4=BoxHollow, 5=UPN, 6=Angle)
  - `width` ($b$) : `double` (mètres)
  - `height` ($h$) : `double` (mètres)
  - `diameter` ($D$) : `double` (mètres)
  - `tw` : `double` (mètres)
  - `tf` : `double` (mètres)
- **Matériau mécanique intégré :**
  - `name` : chaîne
  - `type` : `uint8_t` (0=Concrete, 1=Steel, 2=Timber, 3=Masonry, 4=Custom)
  - `E` : `double` (Pa, ex: $2.1 \times 10^{11}$ pour l'acier)
  - `nu` : `double` (sans dimension, ex: $0.3$)
  - `density` : `double` ($kg/m^3$, ex: $7850$)
  - `fk` : `double` (Pa, limite élastique $f_y$ ou résistance béton $f_{ck}$)
  - `thermalCoeff` : `double` ($1/K$)

### 5.3 Dalles (`CHUNK_SLAB`)
- `id` : `int32_t`
- `name` : chaîne
- `nodeIds` : liste d'entiers `int32_t` ordonnés décrivant le contour polygonal
- `thickness` : `double` (mètres)
- `material` : données du matériau

---

### 5.x Charges (`CHUNK_LOAD`, spécification 1.1)

Avant la spécification 1.1, ce chunk était déclaré mais **jamais écrit ni lu** : les cas de charge,
combinaisons, charges nodales et charges sur barres étaient perdus à chaque enregistrement, et un
projet rouvert ne contenait plus aucun cas de charge.

Le chunk commence par une version de disposition interne (`LOAD_CHUNK_LAYOUT_VERSION`, actuellement 1),
indépendante de la version du format. `elementCount` = nombre de charges nodales + charges sur barres.

| Champ | Type | Description |
| :--- | :--- | :--- |
| `layoutVersion` | `uint32_t` | Version de disposition du chunk (1) |
| `activeLoadCaseId`, `nextNodalLoadId`, `nextMemberLoadId`, `nextLoadCaseId`, `nextCombinationId` | 5 × `int32_t` | État du `LoadManager` |
| `nCases` puis `nCases` × { `id` i32, `name` str, `category` u8, `selfWeight` u8, `selfWeightFactor` f64, `description` str } | | Cas de charge (`LoadCaseCategory`) |
| `nCombos` puis `nCombos` × { `id` i32, `name` str, `type` u8, `nFactors` u32 × { `caseId` i32, `factor` f64 } } | | Combinaisons (`LoadCombinationType`) |
| `nNodal` puis `nNodal` × { `id`, `nodeId`, `caseId` i32, `fx fy fz mx my mz` 6 × f64, `coordSys` u8, `name` str } | | Charges nodales |
| `nMember` puis `nMember` × { `id`, `elementId`, `caseId` i32, `type` u8, `q1 q2` f64, `direction` u8, `coordSys` u8, `x1 x2` f64, `relative` u8, `name` str, `targetType` u8 } | | Charges sur barres (`MemberTargetType` : poutre, poteau, treillis, câble) |

Règles du lecteur : toute valeur d'énumération hors plage ou tout enregistrement tronqué rejette le
fichier avec un message explicite ; les compteurs `next*Id` sont relevés au-delà du plus grand
identifiant lu. **Compatibilité :** un fichier 1.0 (sans chunk `LOAD`) se charge avec les cas et
combinaisons Eurocodes par défaut (`LoadManager::resetToDefaults`) ; un lecteur 1.0 ignore le chunk
`LOAD` d'un fichier 1.1 (chunk inconnu sauté grâce à `chunkSize`).

### 5.y Couche BIM (`CHUNK_BIMM`, spécification 1.3)

Contenu : document JSON UTF-8 compact (`TSA::BIM::BimModel::toJson`), versionné par le champ `schema`
(actuellement 1). `elementCount` = nombre de produits physiques. Clés principales : `project`, `site`,
`building` (`guid`, `name`, `description`), `storeys` (id de niveau → GlobalId), `elements[]`
(`id` interne, `guid` IFC, `category`, `predefinedType`, `name`, `objectType`, `tag`, `description`,
`storey`, `analytical` = liste ordonnée `"beam:12"`, `"column:3"`…, `psets[]`, `classifications[]`),
`analytical` (clé `"node:7"` → GlobalId de l'objet analytique), `nextId`.

Règles : chunk facultatif et jamais bloquant — absent (fichier ≤ 1.2), illisible ou de schéma futur,
il est ignoré (avertissement journalisé) et les GlobalId sont attribués au chargement puis
enregistrés à la sauvegarde suivante. Un lecteur ≤ 1.2 saute le chunk grâce à `chunkSize`.

### 5.z Paramètres d'analyse (`CHUNK_SETT`, spécification 1.4)

Contenu : document JSON UTF-8 compact (`TSA::Analysis::AnalysisContext::toJson`), versionné par le
champ `schemaVersion` (actuellement 1) : moteur, dimension, type d'analyse, portée, cas / combinaison,
poids propre, réglages par moteur. `elementCount` = 1. Écrit seulement si des paramètres ont été
définis (fenêtre Analyse) ; conservé par le modèle comme JSON opaque (`Model::analysisSettingsJson`).

Règles : facultatif et jamais bloquant — absent (fichier ≤ 1.3), illisible ou de schéma futur, la
fenêtre principale revient aux réglages par défaut (avertissement dans la console) ; un moteur
absent de l'installation est remplacé par le premier moteur disponible. Les **résultats** (`RSLT`)
ne sont toujours pas enregistrés : un projet rouvert doit être recalculé.

---

## 6. Système d'Intégrité et Sécurité du Lecteur

Pour prévenir toute corruption de mémoire ou exploitation par fichier malveillant, le lecteur `TSAFileReader` applique un protocole d'assainissement strict :
1. **Magic Number Check :** Rejet immédiat si `magic != TSA_FILE_MAGIC`.
2. **Version Compatibility Check :** Rejet explicite avec message clair si `formatMajorVersion > TSA_FORMAT_VERSION_MAJOR`.
3. **Contrôle de Taille Réelle :** Vérification que la taille sur le système de fichiers correspond exactement à `fileSize`.
4. **Validation CRC32 :** Calcul du CRC32 du payload brut avant décompression. Rejet si différence avec `checksumCRC32`.
5. **Limites Numériques de Sécurité :**
   - Taille maximale de payload : 1 Go (`MAX_SAFE_PAYLOAD_SIZE`).
   - Nombre maximal de nœuds : 1 000 000 (`MAX_SAFE_NODES`).
   - Nombre maximal de barres : 500 000 (`MAX_SAFE_BARS`).
   - Longueur maximale de chaîne de caractères : 65 536 caractères (`MAX_SAFE_STRING_LENGTH`).
6. **Isolation Atomique :** La reconstruction utilise un snapshot `ModelStateSnapshot`. En cas d'erreur de lecture, le modèle en mémoire n'est pas pollué.
7. **Écriture atomique (depuis 1.1) :** l'enregistrement passe par `QSaveFile` (fichier temporaire puis renommage à `commit()`). Une erreur d'écriture (disque plein, droits, crash) laisse le fichier existant intact. Le CRC32 est calculé par table (résultat identique à l'implémentation bit à bit d'origine).

---

## 7. Flux de Reconstruction OCCT 3D

```
    Fichier Projet.tsa
           ↓
    TSAFileReader
    (Validation CRC32 + Décompression Zlib + Désérialisation Chunks)
           ↓
    ModelStateSnapshot
           ↓
    Model::restoreSnapshot(snapshot)
           ↓
    Notification IModelObserver::onModelCleared()
           ↓
    OccView::rebuildAllShapes()
           ↓
    BeamGeometry::createBeamShape(nA, nB, section)
    (Génération analytique BRepPrimAPI_MakeCylinder pour sections Circulaires / Pipe,
     BRepPrimAPI_MakePrism pour sections rectangulaires et profilés métalliques)
           ↓
    Affichage direct dans le Viewport 3D AIS_InteractiveContext
```
