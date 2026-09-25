# Directives de Développement TSA - Recherche Préalable de Solutions & Dépendances Externes

Pour toute nouvelle fonctionnalité, extension, bibliothèque, outil, composant visuel (icônes, widgets, thèmes) ou module dans le projet TSA, appliquer systématiquement la règle suivante :

---

## 1. Règle Principale : Recherche Préalable Obligatoire sur Internet

Avant de créer ou de coder nous-mêmes une extension, une bibliothèque ou un composant :
- **Toujours rechercher sur Internet s'il existe déjà une solution adaptée, maintenue, fiable et reconnue**.
- **Ne jamais créer ou réinventer une solution si une bibliothèque ou un composant existant répond proprement au besoin**.

---

## 2. Protocole de Recherche & Évaluation

Pour chaque besoin identifié :
1. **Identifier précisément le besoin technique ou visuel**.
2. **Rechercher les solutions existantes** (via `search_web`, documentation officielle, dépôts GitHub).
3. **Privilégier les sources officielles et reconnues** :
   - Dépôts GitHub officiels et maintenus
   - Documentation officielle Qt
   - Écosystème Microsoft / Windows SDK / MSVC
   - CMake packages & modules
   - MinGW-w64
   - OpenCASCADE (OCCT)
   - VTK
   - Projets open source établis et activement maintenus
4. **Vérifier les critères de compatibilité stricts avec TSA** :
   - Compatibilité Windows 10/11 x64
   - Compatibilité standard C++20
   - Compatibilité toolchains (MSVC / MinGW)
   - Compatibilité Qt 6
   - Intégration CMake native
   - Licence compatible (MIT, Apache 2.0, BSD, LGPL, etc.)
   - Maintenance active et date des dernières mises à jour
5. **Si une solution existante convient** : l'utiliser et l'intégrer au lieu de la recoder.
6. **Si plusieurs solutions existent** : les comparer techniquement avant sélection.
7. **Si aucune solution existante n'est adaptée** : créer une solution personnalisée.

---

## 3. Format d'Évaluation Obligatoire

Pour toute proposition ou étude de nouvelle fonctionnalité ou composant, présenter obligatoirement l'analyse sous ce format :

```text
Besoin :
...

Solution existante trouvée :
...

Source :
...

Compatibilité TSA :
...

Avantages :
...

Limites :
...

Solution personnalisée nécessaire :
Oui / Non
```

---

## 4. Conditions pour Développer une Solution Personnalisée

Une solution personnalisée n'est développée que si :
- Aucune solution existante ne répond au besoin technique.
- Les solutions existantes sont incompatibles avec l'ABI ou l'architecture de TSA.
- La licence est restrictive ou incompatible.
- Le projet tiers est abandonné ou obsolète.
- L'intégration de la dépendance est inutilement lourde/complexe comparée au besoin réel.
- **OU** une implémentation sur-mesure est démontrée comme étant nettement plus légère, plus rapide, plus stable, plus moderne, plus esthétique ou mieux intégrée à l'architecture TSA (comparaison comparative obligatoire préalable).

---

## 5. Application aux Composants Visuels et Graphiques

Cette règle s'applique identiquement aux :
- Systèmes d'icônes (Font Awesome, Material Symbols, Fluent UI, svg-icons)
- Widgets et composants d'interface Qt
- Panneaux, docks et rubans
- Thèmes et feuilles de style (QSS)
- Moteurs de calcul et algorithmes
