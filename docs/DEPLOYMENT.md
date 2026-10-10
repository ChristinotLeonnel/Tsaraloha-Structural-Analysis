# Distribution autonome Windows (TSA, TSALab)

Ce document explique comment produire un dossier qui s'exécute sur un poste Windows 10/11 x64 sans Qt, sans
Visual Studio et sans les sources. Il s'adresse aux personnes qui préparent et vérifient une distribution.

## 1. Trois catégories de fichiers

| Catégorie | Contenu | Emplacement |
|---|---|---|
| **Ressources embarquées** (qrc) | icônes, thème, templates intégrés (`resources/templates.qrc`), registre IA | dans l'exécutable |
| **Dépendances livrées** | exécutables, DLL réellement importées, plugins Qt, traduction Qt française, runtime MSVC, ressources OCCT, moteur OpenSees, bibliothèques TSALib (`Extensions/`), modules livrés, documentation et licences | arborescence contrôlée à côté de l'exécutable |
| **Données utilisateur** | `shortcut.txt`, approbations des modules, templates importés ou personnalisés, modules et extensions de l'utilisateur, journaux | `QStandardPaths` (voir ci-dessous) |

Emplacement des données utilisateur :

| Données | Emplacement (`QStandardPaths`) | Exemple sous Windows |
|---|---|---|
| Configuration (`shortcut.txt`, `modules-trust.json`) | `AppConfigLocation` | `%LOCALAPPDATA%/TSA Engineering/TSA` |
| Données (`templates/`, `modules/`, `Extensions/`, `logs/`) | `AppLocalDataLocation` | `%LOCALAPPDATA%/TSA Engineering/TSA` |

Les données utilisateur se comportent ainsi :

- **Premier lancement** : les dossiers sont créés et `shortcut.txt` est écrit avec les valeurs par défaut.
- **Mise à jour** : les fichiers utilisateur ne sont jamais écrasés. Un format plus ancien est migré **après sauvegarde** (`shortcut.txt.bak-v<N>`, numérotée si elle existe déjà). Un format plus récent n'est jamais réécrit.
- **`shortcut.txt` absent, vide, corrompu ou invalide** : aucun plantage. La dernière configuration valide, ou celle par défaut, reste active et l'erreur est signalée (test 288).

## 2. Résolution des chemins

Toute la résolution passe par `src/Core/AppPaths`. Le dossier de travail n'est **jamais** utilisé : un lancement
par raccourci, par association de fichier ou depuis un autre dossier donne les mêmes chemins (test 286).

| Fonction | Paquet | Développement |
|---|---|---|
| `applicationDir()` | dossier de l'exécutable | idem |
| `occtResourcesDir()` | `resources/occt` | `opencascade-8.0.1-vc14-64/src` |
| `openSeesDir()` | `engines/OpenSees` (ancien : `thirdparty/OpenSees`) | `thirdparty/OpenSees` |
| `shippedModulesDir()` | `modules` | `<build>/modules`, puis `sdk/modules` |
| `shippedExtensionsDir()` | `Extensions` | `Extensions` des sources |
| `logsDir()` | `logs` à côté de l'exécutable s'il existe et est inscriptible, sinon `<données>/logs` | `logs` des sources |

Le dossier des sources (`TSA_SOURCE_DIR`) n'est utilisé que s'il contient encore `CMakeLists.txt`, et **jamais** par un
paquet (reconnu à son `MANIFEST.json`). Un paquet ne
dépend donc jamais d'un chemin de compilation, même sur le poste qui l'a construit.

## 3. Produire le paquet

```powershell
cmake --preset ninja-release
cmake --build --preset ninja-release
cmake --build --preset ninja-release --target package_TSA       # TSALab : package_TSALab dans son arbre
python tools/verify_package.py build-ninja-release/dist/TSA
```

Le script `cmake/PackageTSA.cmake` construit un dossier **propre**, `build-ninja-release/dist/<produit>`. Le
dossier de compilation n'est jamais réutilisé. Les étapes sont les suivantes :

1. **Exécutables** : produit, `tsaraloha-mcp.exe`, `tsa3d-validate.exe`, `tsa-template.exe`, fournisseur de vignettes.
2. **Qt** : `windeployqt --release --no-translations --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --no-quick-import`, puis `translations/qtbase_fr.qm`.
3. **Autres DLL** :
   - seules les DLL **réellement importées** sont copiées, récursivement (lecture des tables d'import avec `dumpbin`, `cmake/RuntimeDependencies.cmake`), depuis OCCT puis les dossiers `3rdparty-vc14-64/*/bin` (Qt 5 exclu) ;
   - une DLL introuvable autre que le runtime arrête le paquet avec une erreur.
4. **Runtime MSVC** : les DLL `Microsoft.VC*.CRT` et `OpenMP` du dossier `VCToolsRedistDir` sont copiées à côté de l'exécutable (déploiement app-local autorisé pour le « Redistributable Code » de Visual Studio). En Debug, le runtime de débogage n'est pas redistribuable : le paquet Debug ne sert qu'aux essais.
5. **Ressources OCCT** : `Shaders`, `Textures`, `StdResource`, `SHMessage`, `XSMessage`, `UnitsAPI`, `XSTEPResource`, `TObj`.
6. **OpenSees** (option `TSA_PACKAGE_OPENSEES`, activée par défaut) : `engines/OpenSees/bin` (exécutable, `libiomp5md.dll`, runtime) et `lib/tcl8.6`. **Vérifiez la licence avant toute diffusion** (§6).
7. **Données livrées** :
   - `Extensions/` ;
   - `plugins/` (vide) ;
   - `modules/`, avec la disposition assemblée par la compilation (`<build>/modules/<id>` : `module.json`, `bin/`…).
8. **Documentation et licences** :
   - `docs/` : `shortcut.txt` de référence, `TSA3D.md`, `SDK.md`, `TEMPLATES.md`, `DEPLOYMENT.md`, `SHORTCUTS.md`, `schemas/`, `examples/tsa3d`, `examples/templates` ;
   - `licenses/` : ce récapitulatif et les licences fournies par OCCT et Qt.
9. **`MANIFEST.json`** : produit, version, configuration, date, nombre et taille des fichiers, présence d'OpenSees, puis, pour chaque fichier, chemin, taille, SHA-256 et catégorie.

## 4. Vérifier le paquet

`tools/verify_package.py <dist>` vérifie les points suivants :

- **Dépendances** : chaque import (normal ou différé) de chaque `.exe` et `.dll` est présent dans le paquet ou fourni par Windows. Le runtime MSVC et l'OpenMP ne sont **jamais** considérés comme fournis par le système. Le dossier d'un exécutable secondaire est pris en compte : OpenSees et les convertisseurs de modules.
- **Fichiers interdits** : aucun fichier Qt 5, VTK, Tcl/Tk 8.6 (DLL), Doxygen.
- **Fichiers obligatoires** : plugin de plateforme `qwindows`, icônes et images SVG, `qtbase_fr.qm`, `Shaders` et `StdResource` d'OCCT, `MANIFEST.json`, `docs/shortcut.txt`, `licenses/THIRD_PARTY_LICENSES.md`, `Extensions`, et les fichiers d'OpenSees s'il est inclus.
- **Intégrité** : les empreintes du manifeste correspondent aux fichiers ; un fichier absent du manifeste est signalé.

## 5. Essai sur un environnement isolé

Le dossier `dist` est copié hors de l'arbre de compilation. Il est lancé avec un `PATH` réduit au système
(`C:\Windows\System32;C:\Windows`), depuis un autre dossier de travail. Ensuite, on vérifie :

- les modules chargés par le processus (`Get-Process -Module`) : aucun ne doit provenir de Qt, de Visual Studio ou des sources ;
- le journal de démarrage ;
- l'exécution de `tsa3d-validate.exe` et `tsa-template.exe`.

Les résultats obtenus pour cette version figurent dans le rapport de livraison. Un essai sur un poste Windows
**neuf**, sans aucun outil de développement installé, reste à faire. Il n'est pas simulable complètement sur le poste de compilation : le
runtime MSVC y est installé dans `System32`.

## 6. Licences

Voir `docs/THIRD_PARTY_LICENSES.md`. Points d'attention :

- **Qt 6 (LGPL v3)** : liaison dynamique, DLL remplaçables, texte de licence fourni.
- **OpenCASCADE (LGPL 2.1 avec exception OCCT)** : liaison dynamique, textes fournis.
- **OpenSees** : licence de l'Université de Californie, réservée à l'usage éducatif, de recherche et non lucratif. **Un usage commercial exige un accord.** Le paquet affiche un avertissement ; pour exclure OpenSees, utilisez `-DTSA_PACKAGE_OPENSEES=OFF`.

## 7. Limites connues

- Pas d'installateur (MSI ou NSIS) : le livrable est un dossier portable. L'association de fichiers est enregistrée au premier lancement dans le registre de l'utilisateur.
- Signature de code des exécutables non faite.
