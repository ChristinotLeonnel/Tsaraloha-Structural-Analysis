# Bibliothèques Open Source & Compatibilité des Licences

## 1. Cartographie des Dépendances & Licences

TSA est conçu comme un logiciel professionnel de haute technologie combinant des moteurs de calcul et des bibliothèques géométriques tierces sous des licences permissives ou à copyleft faible compatibles avec la distribution commerciale du binaire.

| Bibliothèque | Version Intégrée | Rôle dans TSA | Licence Officielle | Dépôt / Source Primaire | Compatibilité Commerciale |
| :--- | :---: | :--- | :--- | :--- | :---: |
| **OpenCASCADE** | **8.0.1** | Noyau géométrique CAO 3D B-Rep exact, AIS, OpenGL | **LGPL 2.1 avec exception OCCT** | [https://dev.opencascade.org/](https://dev.opencascade.org/) | ✅ Oui (Liaison dynamique `.dll`) |
| **OpenSees** | **3.8.0** | Solveur d'analyse par éléments finis non-linéaire | **BSD 2-Clause (Regents UC)** | [https://opensees.berkeley.edu/](https://opensees.berkeley.edu/) | ✅ Oui (Processus externe autonome) |
| **Gmsh** | **4.15.2** | Générateur de maillage éléments finis 1D, 2D, 3D | **GPL v2+ (avec mention du code source)** | [https://gitlab.onelab.info/gmsh/gmsh](https://gitlab.onelab.info/gmsh/gmsh) | ✅ Oui (Notice GPL + mention source fournie) |
| **IfcOpenShell** | **0.9.0post1** | Analyse et parsing géométrique IFC STEP | **LGPL v3** | [https://github.com/IfcOpenShell/IfcOpenShell](https://github.com/IfcOpenShell/IfcOpenShell) | ✅ Oui (Liaison dynamique `.dll`) |
| **Eigen** | **3.4.1** | Algèbre linéaire, matrices d'éléments finis | **MPL 2.0 (Mozilla Public License)** | [https://eigen.tuxfamily.org/](https://eigen.tuxfamily.org/) | ✅ Oui (Header-only permissive) |
| **Boost** | **1.92.0** | Algorithmes C++, géométrie, structures de données | **Boost Software License 1.0** | [https://www.boost.org/](https://www.boost.org/) | ✅ Oui (Très permissive, libre d'usage) |
| **Qt 6** | **6.2+ / 6.11.2** | Framework GUI, Widgets, Svg, Core, Réseau | **LGPL v3 / Commerciale** | [https://code.qt.io/cgit/qt/](https://code.qt.io/cgit/qt/) | ✅ Oui (Liaison dynamique via windeployqt) |

---

## 2. Exigences Légales Spécifiques & Conformité

### 2.1 OpenCASCADE Technology (LGPL 2.1)
* **Obligation :** Les fichiers `.dll` d'OCCT (`TKernel.dll`, `TKBRep.dll`, `TKMath.dll`, `TKV3d.dll`, etc.) doivent être liés dynamiquement et livrés à côté de `TSA.exe`.
* **Conformité TSA :** CMake configure `target_link_libraries` en liaison dynamique et `cmake/DeployDependencies.cmake` copie les binaires d'origine sans modification interne du noyau OCCT.

### 2.2 Gmsh (GPL v2)
* **Obligation :** Lorsque le SDK Gmsh est distribué avec un produit, la licence exige que l'utilisateur soit informé des conditions GPL et sache où obtenir le code source correspondant.
* **Conformité TSA :** Le script de packaging [`cmake/PackageTSA.cmake`](file:///d:/Tsaraloha%20Platforme/TSA/cmake/PackageTSA.cmake) génère automatiquement le fichier `licenses/Gmsh/SOURCE.txt` indiquant l'adresse officielle du dépôt du code source ([`kSourceRepositoryUrl`](file:///d:/Tsaraloha%20Platforme/TSA/product/ProductIdentity.h)).

### 2.3 OpenSees (Licence BSD)
* **Obligation :** Conservation des mentions de copyright des Régents de l'Université de Californie et de l'exclusion de garantie.
* **Conformité TSA :** OpenSees est exécuté en tant que sous-processus externe via `QProcess` piloté par scripts Tcl générés à la volée. Aucun code GPL ne contamine le moteur ni le code propriétaire de TSA.

### 2.4 Qt 6 (LGPL v3)
* **Obligation :** Liaison dynamique obligatoire, possibilité pour l'utilisateur de remplacer les DLL Qt par une version compatible.
* **Conformité TSA :** Utilisation standard de `windeployqt` assurant un déploiement modulaire sans liaison statique.
