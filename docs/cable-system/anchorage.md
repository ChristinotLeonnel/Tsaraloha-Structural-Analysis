# SYSTÈMES D'ANCRAGE, CULOTS & APPUIS

Dans les structures tendues et ponts à câbles, les dispositifs d'ancrage sont des organes critiques assurant le transfert des efforts de traction colossaux vers les massifs de fondation, les pylônes ou les poutres principales. Le module **Cable & Tension System** de **TSA** modélise ces dispositifs via la classe [CableAnchor](file:///E:/Book/Dev/TSA/src/Model/Cable/CableAnchor.h#L18-L80) et l'énumération [AnchorType](file:///E:/Book/Dev/TSA/src/Model/Cable/CableTypes.h#L26-L36).

---

## 1. Typologie des Ancrages (`AnchorType`)

| Type d'Ancrage | Nom Français | Norme & Système | Caractéristiques Principales |
| :--- | :--- | :--- | :--- |
| `GenericSocket` | Culot Générique | Paramétrable | Modèle universel cylindro-conique pour études préliminaires. |
| `CylindricalSocket` | Culot Cylindrique | EN 13411-4 | Manchon cylindrique pour sertissage ou clavetage direct. |
| `ConicalSocket` | Culot Conique | EN 13411-4 | Culot tronconique standard pour injection de résine ou métal coulé (zinc/bismuth). |
| `ThreadedNut` | Écrou Fileté & Platine | EN 10138-4 / DIN | Écrou d'ancrage pour barres haute résistance avec plaque d'assise rigide. |
| `SwagedSocket` | Manchon Embouti / Sertissage | EN 13411-8 | Raccord métallique déformé plastiquement à froid sur le câble. |
| `SpelterSocket` | Culot Culotté Résine/Zinc | EN 13411-4 | Culot ouvert ou fermé coulé à chaud (alliage de zinc) ou à froid (résine époxy pure). |

---

## 2. Propriétés Mécaniques & Dimensionnelles

Chaque ancrage d'extrémité (début `startAnchor` ou fin `endAnchor`) est caractérisé par :

1. **Dimensions Géométriques** :
   - Diamètre extérieur $D_{socket}$ ($\text{m}$)
   - Longueur totale $L_{socket}$ ($\text{m}$)
   - Largeur de platine d'appui $W_{plate}$ ($\text{m}$)
   - Épaisseur de platine $T_{plate}$ ($\text{m}$)
2. **Capacité Résistante** :
   - Capacité limite de calcul $F_{Rd,anchor}$ ($\text{kN}$)
   - Rendement d'ancrage $\eta_a = \frac{F_{Rd,anchor}}{F_{uk,cable}} \ge 0.95$ (exigé par l'EN 1993-1-11 §5.2).
3. **Rentrée d'Ancrage (*Slip*)** :
   - Glissement $\Delta L_{slip}$ ($\text{m}$) pris en compte automatiquement dans le calcul des pertes de précontrainte.

---

## 3. Modélisation Géométrique 3D OpenCASCADE (B-Rep)

Dans le générateur [CableGeometry3D](file:///E:/Book/Dev/TSA/src/Geometry/CableGeometry3D.h#L18-L45) :

1. **Repère Local d'Orientation** :
   L'axe de l'ancrage est aligné rigoureusement avec le vecteur tangent unitaire $\vec{t}$ du câble au point d'attache :
   $$\vec{t}_{start} = \frac{P_1 - P_0}{\|P_1 - P_0\|}, \quad \vec{t}_{end} = \frac{P_N - P_{N-1}}{\|P_N - P_{N-1}\|}$$
2. **Primitives Volumiques** :
   - Réalisation de culots tronconiques via `BRepPrimAPI_MakeCone` orientés le long de $\vec{t}$.
   - Réalisation de manchons cylindriques via `BRepPrimAPI_MakeCylinder`.
   - Modélisation de platines d'appui planes orthogonales via `BRepPrimAPI_MakeBox` ou prismes surfaciques extrudés.
3. **Fusion Topologique B-Rep** :
   Les solides d'ancrage peuvent être affichés de manière solidaire ou dissociée pour faciliter la sélection et l'inspection géométrique dans le viewport 3D de TSA.
