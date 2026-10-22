# TSA — Contexte & Directives pour Gemini

> Ce fichier configure le contexte spécifique de Gemini pour le projet **TSA (Tsaraloha Structural Analysis)**.
> Le document de référence général est [**`AGENTS.md`**](file:///d:/Tsaraloha%20Platforme/TSA/AGENTS.md).
> Les règles modulaires par domaine sont dans [**`.agents/rules/`**](file:///d:/Tsaraloha%20Platforme/TSA/.agents/rules).
> Les procédures opérationnelles sont dans [**`.agents/skills/`**](file:///d:/Tsaraloha%20Platforme/TSA/.agents/skills).

---

## 1. Références Techniques Vivantes
Avant d'intervenir sur un sous-système, consulter directement sa spécification dans [`docs/`](file:///d:/Tsaraloha%20Platforme/TSA/docs) :
- Architecture globale : [`docs/ARCHITECTURE.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/ARCHITECTURE.md)
- Modèle de données : [`docs/MODEL.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/MODEL.md)
- Interface & Ribbon : [`docs/UI.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/UI.md)
- Noyau géométrique OCCT : [`docs/OCCT.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/OCCT.md)
- Coordonnées & Niveaux : [`docs/COORDINATES.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/COORDINATES.md)
- Profilés & Matériaux : [`docs/SECTIONS.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/SECTIONS.md) et [`docs/MATERIALS.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/MATERIALS.md)
- Moteurs de calcul & FEA : [`docs/ANALYSIS_ENGINES.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/ANALYSIS_ENGINES.md) et [`docs/OPENSEES_CALCULATION_METHODS.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/OPENSEES_CALCULATION_METHODS.md)
- BIM & IFC : [`docs/BIM_ARCHITECTURE.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/BIM_ARCHITECTURE.md) et [`docs/IFC_MAPPING.md`](file:///d:/Tsaraloha%20Platforme/TSA/docs/IFC_MAPPING.md)

*En cas de divergence entre la documentation et le code source, **le code réel fait foi** ; la documentation doit être corrigée en conséquence.*

---

## 2. Démarche Efficace & Proportionnée pour Gemini

```text
COMPRENDRE LA DEMANDE → INSPECTION CIBLÉE DU SOUS-SYSTÈME → IMPLÉMENTATION → VALIDATION CIBLÉE
```

- **Inspection ciblée** : Ne jamais relire l'intégralité du dépôt pour une tâche locale. Inspecter uniquement les fichiers concernés et leurs dépendances immédiates.
- **Réutilisation préalable** :
  - `SEARCH EXISTING CODE` avant de créer une fonction.
  - `CHECK WHETHER AN EQUIVALENT CLASS ALREADY EXISTS` avant de concevoir une classe.
  - `CHECK WHETHER AN EXISTING SYSTEM CAN BE EXTENDED` avant d'ajouter une infrastructure.
- **Hiérarchie absolue des priorités** :
  ```text
  CORRECTNESS → ARCHITECTURE → MAINTAINABILITY → TESTABILITY → PERFORMANCE
  ```
- **Préservation absolue** : Ne jamais exécuter d'opération Git destructrice (`reset --hard`, `clean -fd`), ne jamais supprimer de travail local sans demande explicite.
