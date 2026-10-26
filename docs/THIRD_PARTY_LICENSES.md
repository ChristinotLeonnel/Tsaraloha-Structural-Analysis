# Composants tiers distribués avec TSA

Ce récapitulatif accompagne le paquet, dans `licenses/THIRD_PARTY_LICENSES.md`. La liste correspond aux fichiers
réellement copiés par `cmake/PackageTSA.cmake` : seules les DLL importées sont distribuées, avec le détail dans
`MANIFEST.json`. **Ce document ne constitue pas un avis juridique.** Vérifiez les conditions de chaque licence avant
toute diffusion, en particulier commerciale.

| Composant | Version | Fichiers distribués | Licence | Texte dans le paquet | Obligations principales |
|---|---|---|---|---|---|
| Qt | 6.11.2 | `Qt6Core`, `Qt6Gui`, `Qt6Widgets`, `Qt6Network`, `Qt6Svg`, plugins (`platforms`, `styles`, `imageformats`, `iconengines`, `tls`, `networkinformation`, `generic`), `qtbase_fr.qm` | LGPL v3 (ou licence commerciale Qt) | `licenses/Qt/LICENSE` | liaison dynamique (respectée) ; DLL remplaçables par l'utilisateur ; mention et texte de licence ; sources de Qt disponibles (qt.io) |
| Open CASCADE Technology | 8.0.1 | `TK*.dll` (Kernel, Math, G2d, G3d, GeomBase, GeomAlgo, BRep, TopAlgo, Prim, BO, Bool, Fillet, Offset, Mesh, HLR, ShHealing, Service, V3d, OpenGl), `resources/occt` | LGPL 2.1 avec exception OCCT | `licenses/OpenCASCADE/` | liaison dynamique ; texte et exception fournis ; sources disponibles (dev.opencascade.org) |
| FreeType | 2.13.3 | `freetype.dll` | FreeType License (FTL) ou GPL v2 | `licenses/FreeType/LICENSE.TXT` | mention « Portions of this software are copyright © The FreeType Project (www.freetype.org) » dans la documentation |
| oneTBB | 2021.13 | `tbb12.dll` | Apache 2.0 | `licenses/oneTBB/LICENSE.txt` | texte de licence ; mentions de modification |
| FreeImage | 3.18.0 | `FreeImage.dll` | FreeImage Public License 1.0 (ou GPL) | `licenses/FreeImage/license-fi.txt` | texte de licence ; mention « This software uses the FreeImage open source image library » |
| FFmpeg | 3.3.4 | `avcodec-57`, `avformat-57`, `avutil-55`, `swscale-4` | LGPL 2.1 ou plus si compilé sans option GPL (**à confirmer** pour ce binaire du SDK OCCT) | **absent du SDK : à ajouter** | texte LGPL ; accès aux sources de la version distribuée |
| OpenVR | 1.14.15 | `openvr_api.dll` | BSD 3 clauses (Valve) | **absent du SDK : à ajouter** | mention et texte de licence |
| jemalloc | (SDK OCCT) | `jemalloc.dll` | BSD 2 clauses | **absent du SDK : à ajouter** | mention et texte de licence |
| Runtime Microsoft Visual C++ | 14.x (VS 2026) | `msvcp140*.dll`, `vcruntime140*.dll`, `concrt140.dll`, `vccorlib140.dll`, `vcomp140.dll` | licence Visual Studio, « Distributable Code » | référence ci-contre | redistribution app-local autorisée sans modification ; aucune version Debug distribuée |
| **Gmsh** | 4.15.2 (SDK Windows64) | `gmsh-4.15.dll` (moteur de maillage, chargé en différé) | **GNU GPL v2 ou ultérieure**, avec exceptions (Netgen, METIS, OpenCASCADE, ParaView) | `licenses/Gmsh/LICENSE.txt`, `CREDITS.txt` | **voir ci-dessous** : la diffusion de TSA avec Gmsh impose la GPL pour l'ensemble, ou une licence commerciale |
| **IfcOpenShell** | 0.9.0post1 (compilé par `scripts/build_ifcopenshell.ps1`) | `ifcopenshell.parse.dll`, `ifcopenshell.geometry.dll`, `ifcopenshell.plugin.dll`, modules `ifcopenshell_*.dll` (schémas IFC2X3 / IFC4 / IFC4X3_ADD2, noyau OpenCASCADE) | **GNU LGPL v3** (complète la GPL v3) | `licenses/IfcOpenShell/COPYING`, `COPYING.LESSER`, `SOURCE.txt`, `TSA-BUILD.json` | **voir ci-dessous** : liaison dynamique, DLL remplaçables ; code source correspondant offert au même endroit que les binaires |
| Boost | 1.92.0 (statique, dans les DLL d'IfcOpenShell) | — | Boost Software License 1.0 | `licenses/Boost/LICENSE_1_0.txt` | aucune pour le code objet ; texte fourni par précaution |
| Eigen | 3.4.1 (en-têtes, dans les DLL d'IfcOpenShell) | — | MPL 2.0 (quelques fichiers BSD / LGPL, voir `COPYING.README`) | `licenses/Eigen/COPYING.MPL2`, `COPYING.README` | indiquer où obtenir la source (URL et empreinte dans `licenses/IfcOpenShell/SOURCE.txt`) ; non modifié |
| OpenSees | (binaire de `thirdparty/OpenSees`) | `engines/OpenSees/bin/OpenSees.exe` | © The Regents of the University of California : usage **éducatif, de recherche et non lucratif** sans redevance ; **usage commercial soumis à accord** | **absent du dépôt : à ajouter** | **voir ci-dessous** |
| Intel OpenMP | (fourni avec OpenSees) | `engines/OpenSees/bin/libiomp5md.dll` | licence de redistribution Intel | **absent : à ajouter** | redistribution sans modification |
| Tcl | 8.6 | `engines/OpenSees/lib/tcl8.6` (scripts) | licence Tcl/Tk (type BSD) | **absent : à ajouter** | mention et texte |

## Gmsh : licence GPL, point bloquant pour une diffusion non libre

Le moteur de maillage, Gmsh, est distribué sous **GNU GPL version 2 ou ultérieure**. TSA appelle son API (DLL
`gmsh-4.15.dll`, chargée en différé). Selon l'interprétation usuelle de la GPL, un programme lié à une
bibliothèque GPL et distribué avec elle forme une œuvre combinée, qui doit être distribuée sous GPL : sources
fournies, mêmes libertés.

La DLL Gmsh embarque statiquement OpenCASCADE, FreeType, OpenBLAS, HDF5, MED, CGNS, FLTK… La licence et les crédits
de Gmsh couvrent cette distribution (`CREDITS.txt`).

Avant toute diffusion de TSA avec Gmsh, choisir l'une des options :

1. **distribuer TSA sous une licence compatible avec la GPL**, avec ses sources ;
2. **obtenir une licence commerciale** auprès des auteurs de Gmsh ;
3. **diffuser sans Gmsh** (`-DTSA_PACKAGE_GMSH=OFF`) : TSA démarre, mais le maillage est indisponible. L'utilisateur peut éventuellement installer lui-même la DLL, ce qui ne change pas l'analyse juridique et doit être vérifié.

**Décision de l'éditeur (17/10/2026) : option 1.** TSA et TSALab sont sous licence MIT (compatible avec la GPL) et
leurs dépôts de code restent **publics**. Une version qui contient Gmsh est donc distribuée, dans son ensemble,
selon les termes de la GPL ; le code de TSA reste sous MIT. Obligations tenues à chaque version :

- `licenses/Gmsh/LICENSE.txt`, `CREDITS.txt` et `SOURCE.txt` dans le paquet (`SOURCE.txt` : code de TSA à
  l'étiquette `v<version>` du dépôt public, code de Gmsh de la version livrée) ; `tools/verify_package.py`
  refuse un paquet qui contient Gmsh sans ces fichiers ;
- étiquette `v<version>` posée dans le dépôt du code au commit construit ;
- archive source de Gmsh (`gmsh-<version>-source.tgz`, non modifiée) jointe à la version publiée sur
  `Setup-Livrable`, en plus du lien vers gmsh.info.

Rendre un dépôt de code privé remettrait en cause cette décision : il faudrait alors fournir le code source avec
chaque version, obtenir une licence commerciale de Gmsh, ou construire sans Gmsh (`-DTSA_PACKAGE_GMSH=OFF`).

## IfcOpenShell : LGPL v3, compatible avec une diffusion commerciale sous conditions

L'import IFC complet repose sur IfcOpenShell 0.9.0post1, sous **GNU LGPL version 3**. Choisi parce que la LGPL
autorise son usage par un logiciel diffusé commercialement et non libre, contrairement à la GPL, à condition de :

1. **lier dynamiquement** (§ 4 d) 1) de la LGPL v3) : les DLL sont livrées séparément, à côté de l'exécutable, et
   peuvent être remplacées par l'utilisateur par une version modifiée compatible. Le pont de TSA
   (`src/BIM/IFC/IfcOpenShellImport.cpp`) n'utilise que l'interface publique ; aucune liaison statique ;
2. **fournir les textes** : `COPYING` (GPL v3) et `COPYING.LESSER` (LGPL v3) dans `licenses/IfcOpenShell/` ;
3. **mentionner** l'usage de la bibliothèque et sa licence (ce document, `SOURCE.txt`, notes de version) ;
4. **offrir le code source correspondant** de la bibliothèque au même endroit que les binaires (§ 6 d) de la GPL v3,
   incorporée par la LGPL) : l'archive non modifiée `ifcopenshell-0.9.0post1-source.tar.gz` (empreinte vérifiée) est
   jointe à chaque version publiée sur `Setup-Livrable`, avec le script de compilation (options, schémas) dans le
   dépôt public de TSA. Boost et Eigen, compilés dans les DLL sans modification, sont désignés par URL et empreinte.

Composants exclus de la compilation : Python, IfcConvert, CGAL, COLLADA, glTF, USD, PROJ (licences et taille).

Audit des binaires livrés (17/10/2026) :

- dépendances des 13 DLL (table d'import) : Windows, runtime MSVC et DLL d'OpenCASCADE déjà livrées (`TKernel`,
  `TKMath`, `TKBRep`, `TKBO`, `TKBool`, `TKMesh`…), aucune autre bibliothèque ;
- Boost (`program_options`, `regex`) lié statiquement dans les DLL : BSL-1.0, aucune obligation pour le code objet ;
- Eigen (en-têtes) : MPL-2.0 ; certains fichiers d'Eigen sont sous LGPL ou BSD (`COPYING.README`) : la compilation
  n'impose pas `EIGEN_MPL2_ONLY`, la liste des en-têtes réellement inclus reste à établir ;
- code tiers relevé dans les sources compilées : une routine d'intersection rayon / boîte « From Tavian Barnes - MIT
  License » (`src/ifcgeom/kernels/opencascade/clash_utils.cpp`), compilée dans
  `ifcopenshell_geometry_kernel_opencascade.dll` ; avis et texte MIT reproduits dans
  `licenses/IfcOpenShell/THIRD_PARTY_NOTICES.txt` (le source ne mentionne ni année ni formule de copyright) ;
- les dossiers sous licence propre du dépôt IfcOpenShell (Bonsai GPL, outils Python `bcf`, `ifcclash`…) ne sont pas
  compilés.

**À faire confirmer par un juriste** (la réussite de `tools/verify_package.py` ne vaut pas conformité) : suffisance de
l'offre de source (archive jointe à chaque version publiée et script de compilation public), remplaçabilité effective
des DLL d'une interface C++ (même compilateur et mêmes options requis, script fourni), extrait MIT ci-dessus, fichiers
LGPL éventuels d'Eigen. Les paquets ne sont pas reproductibles au bit près (horodatages des binaires) : la traçabilité
repose sur `MANIFEST.json` (SHA-256 de chaque fichier), `TSA-BUILD.json` (versions et empreintes des sources) et
`SHA256SUMS.txt` des versions publiées.
`tools/verify_package.py` refuse un paquet qui contient IfcOpenShell sans tous ses modules ni ses licences ; la
publication échoue si la bibliothèque manque (`-DTSA_REQUIRE_IFCOPENSHELL=ON`).

Les fichiers IFC d'essai livrés avec les sources (`tests/data/ifc/buildingsmart`) sont © buildingSMART
International, sous **CC BY 4.0**, non modifiés, attribution dans le `README.md` du dossier. Ils ne sont pas
distribués dans le paquet.

## OpenSees : point bloquant pour une diffusion commerciale

Le moteur de calcul par défaut, OpenSees, est distribué par l'Université de Californie sous une licence qui autorise
l'usage, la copie, la modification et la distribution **à des fins éducatives, de recherche et non lucratives**. Un
usage commercial suppose un accord de licence avec l'Université.

Avant toute diffusion commerciale de TSA avec OpenSees :

1. obtenir le texte exact de la licence de la version distribuée et le placer dans `licenses/OpenSees/` ;
2. obtenir l'accord nécessaire ;
3. ou bien construire le paquet sans OpenSees (`-DTSA_PACKAGE_OPENSEES=OFF`, réglage par défaut depuis le 2026-10-25). Le moteur 2D intégré reste alors disponible et le calcul OpenSees indique que le moteur est absent.

Le script de packaging affiche cet avertissement à chaque construction de paquet avec OpenSees.

## Textes manquants

Les textes de licence de FFmpeg, OpenVR, jemalloc, Intel OpenMP et OpenSees ne figurent pas dans les SDK fournis avec
le dépôt. Il faut les ajouter dans `licenses/<composant>/` avant diffusion. Le paquet actuel convient aux essais
internes.

## Composants présents dans le dépôt mais NON distribués

Ces composants ne sont pas importés par TSA, et `tools/verify_package.py` refuse leur présence :

- Qt 5.11 du SDK OCCT ;
- VTK ;
- Tcl/Tk (DLL) ;
- Doxygen ;
- ANGLE, Draco, GL2PS, GLFW, LZMA, RapidJSON, zlib, sauf s'ils deviennent importés un jour (la résolution automatique les ajouterait et ils figureraient au manifeste).
