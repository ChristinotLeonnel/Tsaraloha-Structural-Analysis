# Tsaraloha Structural Analysis (TSA) — Architecture de l'Installateur Windows

Ce document détaille la conception technique, les choix technologiques, la structure du package et le cycle de vie de l'installateur Windows officiel de **Tsaraloha Structural Analysis (TSA)**.

---

## 1. Choix Technologique : Inno Setup 6

### Justification
Pour la distribution de TSA sous Windows 10/11 x64, **Inno Setup 6** (avec extension ISPP - Inno Setup Preprocessor) a été retenu pour les raisons suivantes :

1. **Robustesse et rapidité de compression** : Compression LZMA2/Ultra64 réduisant les 254 Mo de binaires (Qt 6, OpenCASCADE 8.0.1, OpenSees 3.8, Gmsh, IfcOpenShell, plugins) à **~66.5 Mo** (69 716 178 octets).
2. **Expérience utilisateur professionnelle inspirée d'Autodesk Access / AutoCAD Installer** :
   - Interface moderne (`WizardStyle=modern`) avec bannières graphiques personnalisées aux couleurs officielles de Tsaraloha (`#060913` à `#0A172E`, accents cyan `#00F2FE` et bleu `#0072FF`).
   - Sélection du répertoire d'installation avec calcul précis de l'espace disque requis (environ 600 Mo recommandés, 260 Mo stricts).
   - Détection automatique et gestion propre des montées de version (upgrade in-place sans perte de données utilisateur).
3. **Privilèges adaptables** (`PrivilegesRequired=lowest` avec `PrivilegesRequiredOverridesAllowed=dialog commandline`) :
   - Permet une installation par défaut dans le répertoire utilisateur ou pour tous les utilisateurs si exécuté en mode Administrateur (`C:\Program Files\Tsaraloha Software\TSA`).
4. **Intégration Windows native** :
   - Association du type de fichier `.tsa` (`TSA.Project`).
   - Enregistrement du serveur COM d'icônes et d'aperçus d'explorateur (`TSAThumbnailProvider.dll`).
   - Entrée standard dans le Panneau de configuration / Paramètres Windows "Applications installées".
   - Désinstallation chirurgicale : seuls les fichiers installés sont supprimés, préservant les projets `.tsa`, les notes de calcul et les paramètres dans `%LOCALAPPDATA%\TSA Engineering\TSA`.

---

## 2. Structure et Dépendances du Package Redistribuable

Le package compilé se situe dans `build-ninja-release/dist/TSA` et comporte :

- **Exécutable principal** :
  - `TSA.exe` (Moteur de modélisation 3D et d'analyse structurelle)
- **Bibliothèques Qt 6.11** :
  - `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6OpenGL.dll`, `Qt6OpenGLWidgets.dll`, `Qt6Network.dll`, `Qt6Svg.dll`, etc.
  - Plugins essentiels : `platforms/qwindows.dll`, `styles/qwindowsvistastyle.dll`, `imageformats/*.dll`, `iconengines/*.dll`, `tls/*.dll`.
- **Noyau Géométrique OpenCASCADE (OCCT 8.0.1)** :
  - `TKernel.dll`, `TKMath.dll`, `TKBRep.dll`, `TKGeomBase.dll`, `TKTopAlgo.dll`, `TKMesh.dll`, `TKService.dll`, `TKV3d.dll`, `TKOpenGl.dll`, etc.
- **Moteurs de Calcul et d'Ingénierie** :
  - OpenSees 3.8.0 (`OpenSees.dll` / modules de résolution matricielle)
  - Gmsh 4.15.2 (`gmsh-4.15.dll` pour le maillage volumique et surfacique)
  - IfcOpenShell 0.9.0 (`IfcParse.dll`, `IfcGeom.dll` pour l'import/export BIM IFC)
  - Fem3D solver et extensions de calcul tensoriel
- **Runtime Microsoft Visual C++** :
  - `msvcp140.dll`, `msvcp140_1.dll`, `msvcp140_2.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`
- **Intégration Shell** :
  - `TSAThumbnailProvider.dll` (Génération des vignettes de modèles 3D dans l'explorateur Windows)

---

## 3. Paramètres de l'Installateur (`packaging/installer.iss`)

```ini
[Setup]
AppId={{5E226500-D175-4704-B474-067CE79A6372}
AppName=Tsaraloha Structural Analysis
AppVersion=0.1.0
AppPublisher=Tsaraloha Software
AppPublisherURL=https://tsaraloha.org
AppSupportURL=https://tsaraloha.org/support
AppUpdatesURL=https://tsaraloha.org/downloads
DefaultDirName={autopf}\Tsaraloha Software\TSA
DefaultGroupName=Tsaraloha Software\TSA
WizardStyle=modern
Compression=lzma2/ultra64
SolidCompression=yes
SetupIconFile={#SourcePath}\..\resources\icons\tsa_app.ico
WizardImageFile={#SourcePath}\wizard_banner.bmp
WizardSmallImageFile={#SourcePath}\wizard_small.bmp
```

---

## 4. Cycle de Vie de l'Installation et Sécurité des Données

### Étapes d'installation (Écran par écran)
1. **Accueil (Welcome)** : Présentation de TSA, version installée, éditeur Tsaraloha Software.
2. **Licence (License)** : Consultation intégrale de la licence logicielle TSA (`LICENSE.txt`).
3. **Répertoire de destination (Directory)** : Sélection avec validation de l'espace disponible.
4. **Composants et Raccourcis (Tasks)** :
   - Raccourci sur le Bureau (optionnel).
   - Association des fichiers `.tsa` (activée par défaut).
5. **Extraction des fichiers (Install)** : Déploiement des binaires et enregistrement COM de `TSAThumbnailProvider.dll`.
6. **Fin d'installation (Finished)** : Option de lancement immédiat de `TSA.exe`.

### Protection des Projets et Données Utilisateur
- Lors d'une mise à jour ou d'une désinstallation, l'installateur **ne touche jamais** :
  - Aux fichiers projets `.tsa` créés par l'ingénieur.
  - Aux exports DXF, IFC, VTK, CSV, rapports PDF.
  - Aux préférences et récents projets stockés dans `%LOCALAPPDATA%\TSA Engineering\TSA`.

---

## 5. Automatisation de la Génération (`tools/build_installer.ps1`)

Le script PowerShell `tools/build_installer.ps1` orchestre la validation complète :
```powershell
powershell -ExecutionPolicy Bypass -File tools/build_installer.ps1 -OutputDir "build-ninja-release/dist"
```

Ce script réalise les étapes suivantes :
1. Recherche du compilateur Inno Setup (`ISCC.exe`).
2. Vérification des dépendances redistribuables via `tools/verify_package.py`.
3. Génération des graphiques BMP officiels via `tools/generate_installer_graphics.py`.
4. Compilation de `TSA-0.1.0-windows-x64-setup.exe`.
5. Calcul de la taille en octets et de l'empreinte cryptographique SHA-256.
6. Test d'installation et de désinstallation silencieuse en bac à sable temporaire (`/VERYSILENT /SUPPRESSMSGBOXES`).
7. Émission des métadonnées JSON prêtes pour la publication web.
