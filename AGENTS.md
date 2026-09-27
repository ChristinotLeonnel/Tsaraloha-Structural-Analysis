# TSA — Architecture interconnectée des bibliothèques, fenêtres, modèle, commandes et OCCT

Dans le projet **TSA — Tsaraloha Structural Analysis**, je veux améliorer l'architecture globale afin que **toutes les fonctionnalités soient réellement interconnectées**.

Le problème à éviter est d'avoir plusieurs systèmes indépendants qui font presque la même chose.

Je veux une architecture cohérente où :

```text
Bibliothèques
     ↕
Fenêtres Qt
     ↕
Modèle TSA
     ↕
Commandes
     ↕
Sélection
     ↕
Undo / Redo
     ↕
Sauvegarde / Chargement
     ↕
OCCT / 3D
     ↕
Calculs
```

utilisent des données et des interfaces cohérentes.

---

# 0. RÈGLE ABSOLUE — AUDIT AVANT MODIFICATION

Avant toute modification du code :

**NE MODIFIE RIEN.**

Ne pas :

* créer de nouvelle classe ;
* créer de nouvelle bibliothèque ;
* supprimer du code ;
* déplacer des fichiers ;
* refactoriser ;
* modifier CMake ;
* modifier les fenêtres ;
* modifier `TsaLib`.

Commencer par analyser l'architecture actuelle.

Identifier notamment :

```text
TsaLib
LibraryManager
SectionManager
MaterialManager

Element
ElementFilaire
ElementSurfacique

Beam
Column
Bar
Cable
Surface

Property Windows
Command System
Selection System
Undo / Redo

Model
ModelManager
Document

OCCT
AIS
Viewport

Save / Load
Import / Export

Analysis / Calculation
```

Identifier également les dépendances entre ces systèmes.

**Après l'audit, STOP et attends mon approbation avant toute modification.**

---

# 1. PRINCIPLE — UNE SEULE SOURCE DE VÉRITÉ

Je veux éviter les copies indépendantes des mêmes données.

Par exemple, ne pas avoir :

```text
TsaLib
   │
   ├── BeamWindow → copie des sections
   │
   ├── CableWindow → autre copie
   │
   └── ColumnWindow → autre copie
```

Je veux :

```text
                 TsaLib
                    │
        ┌───────────┼───────────┐
        │           │           │
      Beam        Column       Cable
      Window      Window       Window
        │           │           │
        └───────────┼───────────┘
                    ↓
                TSA Model
```

Les fenêtres consultent les bibliothèques centrales.

Elles ne doivent pas devenir des bases de données secondaires.

---

# 2. TsaLib doit être réellement intégrée

TSA possède déjà des bibliothèques telles que :

```text
TsaLib
├── Sections
├── Materials
├── Profiles
├── ...
```

Il faut déterminer exactement ce qui existe déjà.

**Ne recrée pas TsaLib.**

Cherche comment elle est actuellement chargée, enregistrée, utilisée et sauvegardée.

Si une fonctionnalité existe déjà dans `TsaLib`, les fenêtres doivent l'utiliser.

---

# 3. Synchronisation automatique

Lorsqu'une bibliothèque évolue :

```text
TsaLib
   ↓
Library changed
   ↓
Property system
   ↓
Windows
```

les fenêtres concernées doivent pouvoir récupérer automatiquement les nouvelles données.

Exemple :

```text
Ajout section IPE 500
        ↓
TsaLib
        ↓
Section enregistrée
        ↓
BeamPropertiesWindow
        ↓
IPE 500 disponible
```

Même principe pour les matériaux et les autres bibliothèques.

Ne pas coder manuellement dans chaque fenêtre :

```cpp
comboBox->addItem("IPE 500");
```

si cette donnée appartient à `TsaLib`.

---

# 4. Toutes les fonctionnalités doivent être interconnectées

Je veux que les fonctionnalités principales de TSA ne soient pas des systèmes isolés.

Architecture cible :

```text
                         TSA
                          │
                    ┌─────┴─────┐
                    │  TsaLib   │
                    └─────┬─────┘
                          │
                    Property System
                          │
             ┌────────────┼────────────┐
             │            │            │
           Beam        Column        Cable
             │            │            │
             └────────────┼────────────┘
                          │
                       Model
                          │
            ┌─────────────┼─────────────┐
            │             │             │
        Selection      Commands     Properties
            │             │             │
            └─────────────┼─────────────┘
                          │
                    Undo / Redo
                          │
                    Save / Load
                          │
                     OCCT / 3D
                          │
                      Analysis
```

Une action utilisateur doit donc traverser proprement les couches concernées.

---

# 5. Exemple concret — création d'un câble

Lorsque l'utilisateur clique sur :

```text
Cable
```

puis dessine :

```text
Point A → Point B
```

le workflow doit être :

```text
Cable Tool
    ↓
Geometry Input
    ↓
Cable Creation Command
    ↓
Cable Model
    ↓
CablePropertiesWindow
    ↓
TsaLib
    ↓
Cable Section / Material
    ↓
Model Update
    ↓
OCCT Geometry
    ↓
Viewport
    ↓
Selection / Tree / Properties
```

Le câble ne doit pas être simplement dessiné dans OCCT sans être correctement enregistré dans le modèle.

---

# 6. Cable — fenêtre indépendante

Le **Cable possède sa propre fenêtre** :

```text
CablePropertiesWindow
```

Il ne doit pas utiliser la fenêtre de :

```text
Beam
Column
Bar
```

Le câble possède ses propres :

* sections ;
* paramètres ;
* propriétés ;
* règles de représentation ;
* workflow.

Cependant, il peut réutiliser les infrastructures génériques :

```text
Property system
Command system
Undo/Redo
Selection
Library system
Model system
```

La réutilisation de l'infrastructure est souhaitée.

La confusion des propriétés ne l'est pas.

---

# 7. TsaLib + Cable

Le câble doit pouvoir utiliser les bibliothèques compatibles :

```text
CablePropertiesWindow
        ↓
TsaLib
        ├── Cable Sections
        ├── Materials
        └── autres données compatibles
```

Exemple :

```text
Section :
[ Cable Ø20 ]

Matériau :
[ Steel ... ]
```

Les données affichées doivent provenir de la bibliothèque réelle.

---

# 8. Interconnexion avec les propriétés

Lorsqu'une propriété est modifiée :

```text
Property Window
      ↓
Property System
      ↓
Model
      ↓
OCCT
      ↓
Viewport
```

La modification doit être immédiatement cohérente avec le modèle.

Exemple :

```text
Cable Ø20
     ↓
Modification
     ↓
Cable Ø30
     ↓
Model = Ø30
     ↓
OCCT = Ø30
     ↓
UI = Ø30
```

Il ne doit jamais être possible d'avoir :

```text
UI : Ø30
Model : Ø20
OCCT : Ø20
```

---

# 9. Sélection

La sélection doit également être connectée au système de propriétés.

Exemple :

```text
Sélection Cable dans OCCT
        ↓
Identification du Cable
        ↓
Model
        ↓
CablePropertiesWindow
        ↓
Affichage des propriétés réelles
```

Même principe pour Beam, Column, Bar, Surface, etc.

La fenêtre doit toujours représenter **l'objet réellement sélectionné**.

---

# 10. Undo / Redo

Toutes les modifications importantes doivent passer par le système de commandes existant lorsqu'il existe.

Exemple :

```text
Modifier Cable
      ↓
Command
      ↓
Model
      ↓
OCCT
```

Puis :

```text
Ctrl + Z
      ↓
Undo Command
      ↓
Model restored
      ↓
OCCT updated
      ↓
UI updated
```

Ne pas créer un système Undo spécial uniquement pour les câbles.

---

# 11. Sauvegarde / chargement

Les bibliothèques et les objets doivent rester cohérents après :

```text
Save
Load
```

Exemple :

```text
Cable
 ├── Section ID
 ├── Material ID
 ├── Parameters
 └── Geometry
```

Après chargement :

```text
File
 ↓
Model
 ↓
TsaLib references
 ↓
Cable
 ↓
OCCT
 ↓
UI
```

Vérifier particulièrement les références vers les bibliothèques.

Éviter les pointeurs invalides et les références vers des données qui n'existent plus.

---

# 12. Modification d'une bibliothèque

Analyser le comportement actuel de TSA.

Déterminer si les éléments utilisent :

### Référence dynamique

```text
TsaLib Section
      ↓
Cable
```

ou :

### Snapshot

```text
TsaLib Section
      ↓
copie des propriétés
      ↓
Cable
```

Ne pas changer arbitrairement cette logique.

Documenter le comportement actuel et proposer une amélioration uniquement si nécessaire.

---

# 13. Architecture générique

Si l'architecture actuelle le permet, rechercher la possibilité d'avoir un système générique :

```text
PropertyProvider
LibraryProvider
ElementPropertyAdapter
```

ou une architecture équivalente.

Mais **ne crée pas ces classes automatiquement**.

Avant de créer une nouvelle abstraction, vérifier si une architecture existante peut être réutilisée.

---

# 14. Recherche obligatoire de solutions existantes

Conformément aux directives du projet TSA, avant de créer une nouvelle :

* bibliothèque ;
* extension ;
* composant ;
* widget ;
* système graphique ;
* outil ;
* dépendance ;

rechercher d'abord une solution existante adaptée.

Vérifier :

```text
Source
Maintenance
Licence
Compatibilité
C++20
Qt 6
CMake
Windows
MSVC
MinGW
OCCT
```

Cette règle fait partie des directives de développement TSA.

Si aucune solution existante n'est adaptée, alors seulement envisager une implémentation personnalisée.

---

# 15. Interconnexion des bibliothèques

À terme, je veux pouvoir avoir :

```text
TsaLib
│
├── Sections
├── Materials
├── Profiles
├── Supports
├── Loads
├── Cables
├── Bolts
├── Connections
└── autres bibliothèques
```

et que les différentes parties de TSA puissent utiliser les bibliothèques concernées sans duplication.

Exemple :

```text
Section Library
       ↓
 ┌─────┼──────┐
 ↓     ↓      ↓
Beam  Bar    Cable
```

Mais chaque élément ne voit que les catégories qui lui sont applicables.

---

# 16. Interconnexion avec le calcul

À terme, les données utilisées pour l'affichage doivent également être cohérentes avec les données utilisées pour le calcul.

Exemple :

```text
CableProperties
       ↓
Cable Model
       ↓
Analysis Model
       ↓
Structural Solver
```

Il ne doit pas y avoir :

```text
3D → diamètre 30 mm
Model → diamètre 30 mm
Calculation → diamètre 20 mm
```

Les différentes représentations doivent dériver d'une source cohérente.

---

# 17. Interconnexion avec OCCT

OCCT ne doit pas devenir une deuxième base de données.

Architecture souhaitée :

```text
TSA Model
    ↓
Geometry Representation
    ↓
OCCT
```

OCCT représente le modèle.

Il ne doit pas devenir la source principale des propriétés structurales.

---

# 18. Interconnexion avec l'UI

Les fenêtres Qt doivent refléter l'état réel du modèle.

```text
Model
  ↓
Property System
  ↓
Qt UI
```

et les actions utilisateur :

```text
Qt UI
  ↓
Command
  ↓
Model
  ↓
OCCT
```

Éviter les modifications directes et non contrôlées :

```text
Qt UI → OCCT
```

si cela contourne le modèle et le système de commandes.

---

# 19. Interconnexion avec les diagnostics

Chaque workflow important doit également être traçable :

```text
User Action
     ↓
Command
     ↓
Model
     ↓
Library
     ↓
OCCT
     ↓
Result
```

Le système de diagnostic doit permettre de comprendre où une opération a échoué.

Exemple :

```text
CableCreateStarted
CableLibraryLookup
CableCreated
CableGeometryCreated
OCCTDisplayUpdated
CableCreateCompleted
```

En cas d'erreur :

```text
CableCreateStarted
CableLibraryLookup
CableCreateFailed
```

---

# 20. Attention aux dépendances circulaires

L'interconnexion ne signifie pas que toutes les classes doivent se connaître directement.

Éviter :

```text
Beam → Window → Model → Beam → Window
```

ou :

```text
TsaLib → UI → TsaLib → UI
```

Utiliser les mécanismes appropriés déjà présents :

```text
interfaces
signals / slots
events
commands
services
managers
dependency injection
```

selon l'architecture actuelle.

Objectif :

**forte cohérence fonctionnelle, faible couplage inutile.**

---

# 21. Audit des duplications

Rechercher notamment :

```text
Sections codées en dur
Materials codés en dur
Property lists dupliquées
Conversion de données dupliquée
Logic de validation dupliquée
Logic de sélection dupliquée
Logic de création dupliquée
Logic de mise à jour OCCT dupliquée
```

Pour chaque duplication :

```text
Emplacement
Responsabilité
Source de vérité actuelle
Risque
Solution proposée
```

Ne pas supprimer immédiatement.

D'abord comprendre l'architecture.

---

# 22. Tests d'intégration

Ne pas seulement tester chaque module séparément.

Tester les chaînes complètes.

### Test A — Cable

```text
Tool
 ↓
Create
 ↓
Property Window
 ↓
TsaLib
 ↓
Model
 ↓
OCCT
 ↓
Selection
 ↓
Edit
 ↓
Undo
 ↓
Redo
 ↓
Save
 ↓
Load
```

### Test B — Beam

Même principe.

### Test C — Library

```text
Ajouter section
 ↓
TsaLib
 ↓
Property Window
 ↓
Créer élément
 ↓
Model
 ↓
OCCT
```

### Test D — plusieurs éléments

```text
Beam
Column
Bar
Cable
Surface
```

dans le même modèle.

Vérifier qu'ils ne se contaminent pas entre eux.

---

# 23. Test de cohérence globale

Pour chaque élément, vérifier :

```text
                ┌───────────┐
                │ TsaLib    │
                └─────┬─────┘
                      ↓
                ┌───────────┐
                │ Properties│
                └─────┬─────┘
                      ↓
                ┌───────────┐
                │   Model   │
                └─────┬─────┘
                      ↓
          ┌───────────┴───────────┐
          ↓                       ↓
       OCCT 3D                 Analysis
          ↓                       ↓
       Viewport                Results
```

Toutes les branches doivent utiliser des données cohérentes.

---

# 24. Ce que je ne veux PAS

Ne fais pas :

```text
❌ nouvelle TsaLib parallèle
❌ liste de sections dans chaque fenêtre
❌ liste de matériaux dans chaque fenêtre
❌ système Cable séparé qui ignore le Model
❌ système Cable séparé qui ignore Undo/Redo
❌ OCCT utilisé comme base de données
❌ calcul utilisant des données différentes du Model
❌ duplication de commandes
❌ duplication de validation
❌ duplication des bibliothèques
❌ refactorisation massive sans nécessité
```

---

# 25. Ce que je veux

Je veux :

```text
                    TsaLib
                      │
                      ▼
               Property System
                      │
                      ▼
                   Model
                      │
          ┌───────────┼───────────┐
          ↓           ↓           ↓
       Commands   Selection    Analysis
          │           │           │
          └───────────┼───────────┘
                      ↓
                  Undo / Redo
                      │
                      ↓
                 Save / Load
                      │
                      ↓
                    OCCT
                      │
                      ↓
                   Viewport
```

avec les fenêtres Qt connectées au système approprié.

---

# 26. Rapport d'audit obligatoire

Avant toute modification, produire :

### A. Architecture actuelle

```text
TsaLib :
...

Property System :
...

Model :
...

Commands :
...

OCCT :
...

Analysis :
...
```

### B. Connexions existantes

Identifier ce qui est déjà correctement interconnecté.

### C. Ruptures d'interconnexion

Identifier les systèmes actuellement isolés.

### D. Duplications

Identifier les données ou fonctionnalités dupliquées.

### E. Risques

Identifier :

* incohérences ;
* références invalides ;
* dépendances circulaires ;
* problèmes d'initialisation ;
* problèmes de synchronisation ;
* données différentes entre UI/Model/OCCT/Analysis.

### F. Architecture proposée

Présenter les modifications minimales nécessaires.

### G. Recherche externe

Pour toute nouvelle bibliothèque ou composant envisagé :

```text
Besoin
Solution existante
Source
Compatibilité TSA
Avantages
Limites
Solution personnalisée : Oui / Non
```

Ce format correspond aux directives de développement TSA.

---

# 27. RÈGLE FINALE

**Ne commence pas par coder.**

Commence par comprendre comment TSA fonctionne actuellement.

L'objectif n'est pas simplement de faire fonctionner `CablePropertiesWindow`.

L'objectif est de construire une architecture où :

**Bibliothèques + UI + Modèle + Commandes + Sélection + Undo/Redo + Sauvegarde + OCCT + Calculs + Diagnostics**

fonctionnent comme **un seul système cohérent et interconnecté**, sans duplication inutile et avec une source de vérité clairement définie.

Après l'audit :

**STOP et attends mon approbation.**
