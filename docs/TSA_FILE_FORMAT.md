# Spécification Technique du Format de Fichier Natif TSA (.tsa)

**Logiciel :** TSA — MadaStructEngine  
**Format :** TSA Binary Project File  
**Extension native :** `.tsa`  
**Magic Signature :** `TSAF` (`0x46415354` en little-endian)  
**Version actuelle :** Spécification 1.0 (Major = 1, Minor = 0)  

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
| `LOAD` (`0x44414F4C`) | `CHUNK_LOAD` | Cas de charges, combinaisons, charges nodales & linéiques |
| `ANLY` (`0x594C4E41`) | `CHUNK_ANLY` | Paramètres et réglages de l'analyse par éléments finis (MEF) |
| `RSLT` (`0x544C5352`) | `CHUNK_RSLT` | Résultats de calcul (déplacements, efforts internes N, Vy, Vz, Mt, My, Mz) |

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
