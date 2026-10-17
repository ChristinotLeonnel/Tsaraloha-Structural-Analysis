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

## OpenSees : point bloquant pour une diffusion commerciale

Le moteur de calcul par défaut, OpenSees, est distribué par l'Université de Californie sous une licence qui autorise
l'usage, la copie, la modification et la distribution **à des fins éducatives, de recherche et non lucratives**. Un
usage commercial suppose un accord de licence avec l'Université.

Avant toute diffusion commerciale de TSA avec OpenSees :

1. obtenir le texte exact de la licence de la version distribuée et le placer dans `licenses/OpenSees/` ;
2. obtenir l'accord nécessaire ;
3. ou bien construire le paquet sans OpenSees (`-DTSA_PACKAGE_OPENSEES=OFF`). Le moteur 2D intégré reste alors disponible et le calcul OpenSees indique que le moteur est absent.

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
