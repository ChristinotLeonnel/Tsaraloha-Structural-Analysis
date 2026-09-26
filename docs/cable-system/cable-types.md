# TYPOLOGIE DES CÂBLES ET ÉLÉMENTS TENDUS

Le module Cable & Tension System de **TSA** prend en charge neuf (9) typologies distinctes de câbles et éléments tendus, modélisées via l'énumération [CableType](file:///E:/Book/Dev/TSA/src/Model/Cable/CableTypes.h#L12-L24). Chaque type est adapté à des applications structurales spécifiques avec des propriétés géométriques, mécaniques et normatives dédiées.

---

## 1. Classification & Énumération `CableType`

| Valeur Énumérée | Nom Français | Norme Principale | Module d'Élasticité Typique $E$ | Description & Usage Principal |
| :--- | :--- | :--- | :--- | :--- |
| `Generic` | Câble Générique | Personnalisé / ISO | Paramétrable (ex: $150 - 210\text{ GPa}$) | Câble polyvalent de contreventement, haubans légers ou tirants divers. |
| `Strand` | Toron de Précontrainte | EN 10138-3 / ASTM A416 | $195\text{ GPa}$ (EN) / $196.5\text{ GPa}$ (ASTM) | Torons 7 fils (classe standard ou haute résistance Y1860S7, Grade 270) pour béton précontraint ou haubans multi-torons. |
| `Wire` | Fil Tréfilé | EN 10138-2 | $205\text{ GPa}$ | Fils d'acier à haute résistance pour câbles parallèles ou faisceaux de suspension. |
| `Bar` | Barre Haute Résistance | EN 10138-4 | $205\text{ GPa}$ | Barres lisses ou nervurées à filetage continu (ex: Y1030 $\varnothing 26$ à $75\text{ mm}$, Macalloy / SAS / DYWIDAG). |
| `StayCable` | Hauban de Pont | EN 1993-1-11 / FIB | $195 - 205\text{ GPa}$ | Câbles porteurs rectilignes à haute résistance sous gaine PEHD pour ponts à haubans et grandes toitures. |
| `SuspensionCable` | Câble Porteur Suspendu | EN 1993-1-11 | $150 - 170\text{ GPa}$ (torons clos) | Câble principal à profil caténaire ou parabolique reprenant le tablier par des suspentes. |
| `Hanger` | Suspente | EN 1993-1-11 | $160 - 205\text{ GPa}$ | Éléments verticaux ou inclinés reliant le tablier d'un pont au câble porteur ou à un arc. |
| `ExternalPrestressing`| Précontrainte Extérieure | EN 1992-1-1 / EN 10138 | $195\text{ GPa}$ | Torons ou barres placés hors du béton avec déviateurs pour ponts à poutres-caissons et renforcements. |
| `GroundAnchor` | Tirant d'Ancrage | EN 1537 / EN 10138 | $195 - 205\text{ GPa}$ | Tirants provisoires ou permanents forés et scellés dans le sol ou la roche pour soutènement et radiers. |

---

## 2. Caractéristiques par Type d'Élément

### 2.1 Câbles Génériques (`Generic`)
- **Modélisation** : Profil droit par défaut avec prise en compte optionnelle d'une courbure initiale.
- **Section** : Cercle plein équivalent ou section métallique nette $A_{metal}$.
- **Comportement** : Traction seule (*tension-only*) avec formulation d'Ernst pour la flexibilité géométrique sous faible tension.

### 2.2 Torons de Précontrainte (`Strand`)
- **Norme** : EN 10138-3 (Y1770S7, Y1860S7) et ASTM A416 / AASHTO M203 (Grade 250, Grade 270).
- **Géométrie** : Torons 7 fils compactés ou standard, diamètres nominaux $12.5\text{ mm}$, $12.9\text{ mm}$, $15.2\text{ mm}$, $15.7\text{ mm}$.
- **Applications** : Post-tension, pré-tension en usine, câbles de ponts en faisceau sous gaine.

### 2.3 Barres Haute Résistance (`Bar`)
- **Norme** : EN 10138-4 (classe Y1030).
- **Propriétés** : Rigides en flexion comparativement aux torons, insensibles au mou géométrique sur courtes portées ($E_{eq} \approx E$).
- **Systèmes d'ancrage** : Écrous d'ancrage filetés sphériques avec platines d'appui normalisées.

### 2.4 Haubans (`StayCable`)
- **Normes de référence** : EN 1993-1-11, recommandations fib (fédération internationale du béton).
- **Configurations géométriques** :
  - **Éventail pur (Fan)** : Tous les haubans convergent au sommet du pylône.
  - **Harpe (Harp)** : Haubans parallèles régulièrement espacés le long du pylône et du tablier.
  - **Semi-éventail (Semi-Fan)** : Convergence distribuée sur la partie haute du pylône (compromis optimal mécanique / encombrement d'ancrage).
- **Prise en compte du fléchissement** : Module sécant d'Ernst indispensable lors de la modélisation à l'échelle globale.

### 2.5 Câbles Porteurs Suspendus (`SuspensionCable`)
- **Géométrie** : Courbe funiculaire funiculaire exacte sous poids propre (caténaire) ou sous charge uniforme horizontale (parabole).
- **Rapports de flèche usuels** : Flèche au centre $f$ comprise typiquement entre $L/12$ et $L/8$.
- **Câbles de retenue (Backstays)** : Câbles latéraux reliant le sommet des pylônes aux massifs d'ancrage gravitaires.

### 2.6 Suspentes (`Hanger`)
- **Raccordement** : Nœud supérieur sur le câble porteur (calculé analytiquement sur la caténaire), nœud inférieur sur les poutres de rive ou les traverses du tablier.
- **Orientation** :
  - **Verticale** : Système standard isostatique en traction.
  - **Inclinée / Triangulée (Réseau de Nielsen-Lohse)** : Amortissement accru et rigidité en torsion supérieure du tablier.

### 2.7 Précontrainte Extérieure (`ExternalPrestressing`)
- **Particularité cinématique** : Câbles droits entre déviateurs et entretoises d'ancrage.
- **Pertes de tension** : Pertes par frottement localisées aux déviateurs courbes, sans perte par ondulation continue ($k \approx 0$).

### 2.8 Tirants d'Ancrage (`GroundAnchor`)
- **Composition** : Longueur libre élastique (gaine déliante) et longueur scellée (bulbe de scellement au coulis de ciment).
- **Contrôle en service** : Précontrainte initiale avec compensation du tassement et de la rentrée d'ancrage.

---

## 3. Implémentation Logicielle dans TSA

Dans l'architecture de TSA :
1. La classe [Cable](file:///E:/Book/Dev/TSA/src/Model/Cable/Cable.h) porte le type `CableType` ainsi que sa définition paramétrique [CableDefinition](file:///E:/Book/Dev/TSA/src/Model/Cable/CableDefinition.h).
2. Les classes dérivées spécialisées [StayCable](file:///E:/Book/Dev/TSA/src/Model/Cable/StayCable.h) et [SuspensionBridge](file:///E:/Book/Dev/TSA/src/Model/Cable/SuspensionSystem.h) ajoutent des méthodes spécifiques pour la génération automatisée d'ensembles complexes.
3. Le modèle [Model](file:///E:/Book/Dev/TSA/src/Model/Model.h) indexe et gère la persistance de tous les éléments quel que soit leur type via la méthode `addCable()`.
