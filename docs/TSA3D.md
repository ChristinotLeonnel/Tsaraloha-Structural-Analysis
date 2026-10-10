# Format d'échange TSA3D — version 1.0

TSA3D sert à échanger un modèle de structure 3D entre logiciels. Ce document s'adresse aux auteurs de
générateurs, de convertisseurs et de modules.

| Élément | Emplacement |
|---|---|
| Schéma JSON | `docs/schemas/tsa3d-1.schema.json` (draft 2020-12) |
| Exemples | `sdk/examples/tsa3d/` |
| Validateur | `tsa3d-validate` (livré avec l'application) |
| Implémentation | `src/IO/Tsa3d/` : lecture, validation, import, export séparés, sans interface graphique |

## 1. Principes

- **JSON UTF-8 versionné.** Un document commence par `"format": "TSA3D"` et `"version": "1.0"`.
- **Identifiants stables.** Chaque objet a un `id` texte non vide, unique dans **tout** le document (un seul espace de noms). Les références se font par identifiant.
- **Unités déclarées.** Le bloc `units` donne les unités utilisées ; à défaut, ce sont les unités TSA (§3). L'import convertit tout.
- **Entrées séparées des résultats.** Le maillage (`mesh`) et les résultats (`results`) sont informatifs. Ils ne sont **jamais** appliqués au modèle et des résultats importés ne sont **jamais** considérés comme validés.
- **Aucune perte silencieuse.**
  - Un champ ou un bloc inconnu est conservé tel quel et restitué à l'export, y compris à travers un fichier `.tsa` : chunk `XTND`, format 1.7.
  - Une donnée TSA non représentable en v1 est signalée dans le rapport d'export.
- **Aucune migration destructive.** Une version majeure plus récente est **refusée**. Une version mineure plus récente est acceptée avec un avertissement, et ses champs inconnus sont conservés.
- **Import atomique.** L'import valide d'abord le document. En cas d'erreur, le modèle n'est pas modifié.

## 2. Structure

```json
{
  "format": "TSA3D", "version": "1.0",
  "metadata": { "name": "…", "description": "…", "id": "uuid", "created": "ISO-8601", "modified": "…",
                "producer": { "name": "…", "version": "…" } },
  "units": { "length": "m", "force": "kN", "stress": "Pa", "pressure": "kPa", "angle": "deg" },
  "coordinateSystem": { "type": "cartesian", "up": "Z", "handedness": "right" },
  "materials": [], "sections": [], "levels": [], "nodes": [], "members": [], "surfaces": [], "foundations": [],
  "loads": { "cases": [], "nodal": [], "member": [], "combinations": [] },
  "groups": [], "analysis": { "settings": {} },
  "mesh": { "source": {}, "nodes": [], "elements": [] },
  "results": { "status": "computed-not-validated", "displacements": [], "reactions": [] },
  "extensions": { "com.exemple.outil": {} }
}
```

Seuls `format` et `version` sont obligatoires. Un document sans nœud est valide (vide).

## 3. Unités

Les unités par défaut sont celles de TSA, indiquées en gras dans le tableau.

| Clé | Valeurs acceptées | Grandeurs |
|---|---|---|
| `length` | **m**, cm, mm | coordonnées, dimensions, épaisseurs, positions |
| `force` | **kN**, N, MN | forces, charges (kN/m ou kN·m suivent `force` et `length`), tension des câbles |
| `stress` | **Pa**, kPa, MPa, N/mm2, GPa | `E`, `fk` des matériaux |
| `pressure` | **kPa**, kN/m2, Pa, N/m2, MPa | portance du sol |
| `angle` | **deg**, rad | rotation des barres |

La masse volumique est toujours en kg/m³ et le coefficient de dilatation en 1/K. Une unité inconnue est une erreur.

## 4. Objets

**`materials[]`**
- Champs : `id`, `name`, `type`, `E`, `nu` dans ]-1 ; 0,5[, `density` ≥ 0, `fk`, `thermalCoeff`, `library{source,ref}`.
- `type` vaut `concrete`, `steel`, `timber`, `masonry`, `custom`, `reinforcedConcrete`, `rebarSteel`, `galvanizedSteel`, `aluminum`, `brick`, `glass`, `soil`, `sand`, `gravel` ou `rock`.

**`sections[]`**
- Champs : `id`, `name`, `shape`, `dimensions`, `properties`, `library`.
- Dimensions strictement positives exigées selon la forme :

  | `shape` | Dimensions obligatoires |
  |---|---|
  | `rectangular` | `b`, `h` |
  | `circular` | `d` |
  | `pipe` | `d`, `tw` |
  | `angle` | `b`, `h`, `tw` |
  | `i`, `box`, `channel`, `tee` | `b`, `h`, `tw`, `tf` |

- `properties` (A, Iy, Iz, It, Wy, Wz) est informatif : TSA recalcule ces valeurs à partir des dimensions.

**`levels[]`** : `id`, `name`, `elevation` (obligatoire).

**`nodes[]`**
- Champs : `id`, `name`, `position` `[x,y,z]` (obligatoire), `level`, `globalId`, `color`, `support`.
- `support` : chaque degré de liberté `tx ty tz rx ry rz` vaut `free`, `fixed` ou `spring`.
- `stiffness{kx…krz}` donne la raideur des degrés de liberté élastiques, en kN/m et kN·m/rad.
- `orientation{type: global|member|vector, direction}` oriente l'appui.

**`members[]`**
- Champs communs : `id`, `type`, `nodes [début, fin]` (deux nœuds distincts et non confondus), `section`, `material`, `rotation`.
- `type` vaut `beam`, `column`, `truss` ou `cable`.
- Selon le type :
  - poutre : `role` (`generic`, `beam`, `column`, `brace`, `tie`, `truss`, `steelMember`, `cable`), `eccentricity` (`none`, `topFlange`, `bottomFlange`, `leftFlange`, `rightFlange`), `releases{start,end}{fx…mz}` ;
  - treillis : `truss{role}` ;
  - câble : `cable{type, geometry, sag, initialTension, tensionOnly}`.
- Une section ou un matériau absent donne un avertissement ; la valeur par défaut de TSA est alors utilisée.

**`surfaces[]`**
- `type` vaut `slab` ou `wall`.
- Dalle : `nodes` (au moins trois, sans répétition), `thickness`, `material`, `slabType` (`twoWay`, `oneWay` ou `flat`).
- Voile : deux nœuds de pied, `height`, `thickness`, `offset`.

**`foundations[]`**
- Champs : `type`, `node`, `dimensions{a,b,h}`, `material`, `soilBearingCapacity`.
- `type` vaut `isolated`, `strip`, `raft` ou `pile`.

**`loads`**

| Bloc | Champs |
|---|---|
| `cases[]` | `id`, `name`, `category`, `selfWeight`, `selfWeightFactor`, `description` |
| `nodal[]` | `node`, `case`, `force[3]` et / ou `moment[3]`, `coordinateSystem` (`global` ou `local`) |
| `member[]` | `member`, `case`, `kind`, `direction`, `q1`, `q2`, `x1`, `x2`, `relative` (positions dans [0 ; 1]) |
| `combinations[]` | `id`, `name`, `type`, `factors[{case, factor}]` |

Valeurs des énumérations :
- `category` : `dead`, `live`, `wind`, `snow`, `seismic`, `temperature`, `accidental` ou `custom` ;
- `kind` : `uniform`, `trapezoidal` (exige `q2`), `point` (exige `x1`) ou `moment` ;
- `direction` : `globalX`, `globalY`, `globalZ`, `gravity`, `localX`, `localY` ou `localZ` ;
- `type` d'une combinaison : `ulsFundamental`, `ulsAccidental`, `ulsSeismic`, `slsCharacteristic`, `slsFrequent`, `slsQuasiPermanent` ou `custom`.

`loads.surface` (charges surfaciques) est conservé mais n'est pas appliqué : le modèle TSA actuel ne les prend pas en charge.

**`groups[]`** : `id`, `name`, `objects[]`, qui doivent désigner des identifiants existants. Le bloc est conservé.

**`mesh`**
- Le maillage d'éléments finis est distinct des nœuds géométriques.
- `nodes[{id, position, origin (nœud géométrique ou null), role}]`.
- `elements[{id, type, nodes, origin (objet d'origine), class}]`.
- `type` vaut `line2`, `zeroLength`, `tri3`, `quad4`, `tet4` ou `hex8`. Le nombre de nœuds est contrôlé.
- L'export joint le maillage réellement transmis au moteur, s'il correspond au modèle actuel.

**`results`**
- `status` vaut toujours `computed-not-validated`.
- Autres champs : `engine`, `case`, `timestamp`, `displacements[{node, u[3], r[3]}]`, `reactions[{node, force[3], moment[3]}]`.
- Exportés seulement sur demande. À l'import, ils sont signalés et ne sont pas chargés comme résultats.

## 5. Propriétés personnalisées et extensions

- `properties` (sur un objet) : paires libres, conservées.
- `extensions` (sur un objet ou au niveau du document) : données d'un programme, regroupées sous un nom de domaine inversé (`com.societe.outil`). Elles sont conservées.
- Tout autre champ inconnu, sur un objet ou au premier niveau, est conservé et signalé à titre d'information par la validation.

Voir `sdk/examples/tsa3d/custom-properties.tsa3d`.

## 6. Identifiants et import

- **Convention de TSA** : préfixe et numéro. Les préfixes sont `N` (nœud), `B` (poutre), `C` (poteau), `T` (treillis), `K` (câble), `SL` (dalle), `W` (voile), `F` (fondation), `LC` (cas), `NL`, `ML` (charges), `CO` (combinaison), `MAT`, `SEC`, `MN`, `ME`.
- **Identifiant conforme** : un identifiant qui suit cette convention garde son numéro à l'import.
- **Autres identifiants** (`pt-a`, `poutre-1`…) : ils reçoivent un numéro libre. La correspondance est conservée et l'identifiant d'origine est **restitué à l'export**.
- **Contrôles à la validation** : identifiants en double, références inconnues ou de mauvais type, nombres non finis, barres de longueur nulle, dimensions non positives, nœuds de maillage inconnus.

## 7. Versions et migration

| Version du fichier | Comportement |
|---|---|
| 1.0 | lu |
| 1.x, x > 0 | lu, avertissement, champs inconnus conservés |
| 2.0 et plus | refusé, aucune migration, message explicite |
| absente ou invalide | erreur |

Une future version 1.x ajoutera des champs **facultatifs** seulement. Une version 2 fera l'objet d'un convertisseur
explicite qui sauvegardera l'original.

## 8. Outils et exemples

| Élément | Rôle |
|---|---|
| `tsa3d-validate fichier.tsa3d [--quiet]` | mêmes contrôles que l'import de TSA. Codes de sortie : 0 valide, 1 erreur, 2 usage |
| `sdk/examples/tsa3d/` | `minimal`, `portal-2d`, `structure-3d`, `materials-sections` (mm, N, MPa), `mesh`, `custom-properties` |
| `sdk/examples/tsa3d-generator/portal_generator.cpp` | programme externe en C++17, sans Qt ni TSA, qui écrit un portique |
| `sdk/examples/tsa3d-roundtrip/main.cpp` | lecture, import dans la bibliothèque TSA, statistiques, export |
| Interface | **Fichier > Importer TSA3D...**, **Exporter TSA3D...**, **Importer via un module...** (convertisseur en processus séparé, voir `docs/SDK.md`) |

API C++ (`src/IO/Tsa3d/Tsa3d.h`) :

```cpp
QJsonObject doc; TSA::IO::Tsa3d::Report report;
if (TSA::IO::Tsa3d::readFile(path, &doc, &report)) {
    auto im = TSA::IO::Tsa3d::importDocument(doc, model);   // valide d'abord ; modèle intact si refus
}
auto ex = TSA::IO::Tsa3d::exportModel(model, { "Projet", "", &results, /*mesh*/ true, /*results*/ false });
TSA::IO::Tsa3d::writeFile(out, ex.document);
```

## 9. Limites de la version 1

Les données suivantes ne sont pas représentées en v1. L'export signale chaque perte.

- Définition détaillée des câbles (torons, ancrages, points intermédiaires) : sont exportés le type, la géométrie, la flèche, la tension et la section.
- Cotations (annotations TSA).
- Charges surfaciques appliquées : le bloc est conservé, mais rien n'est appliqué au modèle.
