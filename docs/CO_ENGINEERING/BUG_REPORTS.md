# Registre des Anomalies & Rapports de Défauts (TSA Co-Engineering)
**Date :** 19 octobre 2026  
**Statut :** Registre d'Anomalies (ISO/IEC/IEEE 29119 / IEEE Std 1044)  
**Projet :** Tsaraloha Structural Analysis (TSA)

---

## 1. Modèle Formel de Rapport d'Anomalie

Toute anomalie détectée par l'un des agents (Agent 3 Testeur UI, Agent 4 FEM, Agent 5 Qualité) est obligatoirement consignée selon la structure normée suivante :

```text
┌────────────────────────────────────────────────────────────────────────┐
│ MODÈLE DE FICHE D'ANOMALIE (IEEE Std 1044)                             │
├──────────────────────┬─────────────────────────────────────────────────┤
│ Identifiant          │ BUG-TSA-YYYYMMDD-XXX                            │
│ Scénario concerné    │ SC-A / SC-B / SC-C / SC-D                       │
│ Composant impacté    │ src/UI/..., src/Model/..., src/Analysis/...     │
│ Gravité              │ Bloquant / Majeur / Mineur / Cosmétique         │
│ Détecté par          │ Agent 3 (UI) / Agent 4 (FEM) / Agent 5 (Qualité)│
├──────────────────────┴─────────────────────────────────────────────────┤
│ Étapes de reproduction :                                               │
│ 1. ...                                                                 │
│ 2. ...                                                                 │
├────────────────────────────────────────────────────────────────────────┤
│ Résultat attendu : ...                                                 │
│ Résultat observé : ...                                                 │
│ Preuve associée  : Capture tests/results/screenshots/... ou extrait log │
├────────────────────────────────────────────────────────────────────────┤
│ Cause identifiée / Hypothèse technique : ...                           │
│ Solution préconisée / Correctif appliqué : ...                         │
│ Résultat du retest de non-régression : PASS / FAIL                     │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Anomalies Détectées, Traitées ou Sous Surveillance

### BUG-TSA-20261019-001 : Avertissement de masquage de variable dans NewProjectDialog
- **Composant :** `src/UI/Home/NewProjectDialog.cpp` (lignes 111 et 132)
- **Gravité :** Mineur (Avertissement compilateur MSVC C4458)
- **Détecté par :** Agent 5 (Auditeur Qualité lors de la compilation de `TSA.exe`)
- **Constat :** La variable locale `data` masque le membre de classe hérité `QWidget::data`.
- **Cause :** Variable temporaire locale nommée `data` dans la méthode de validation.
- **Statut :** Documenté pour refactorisation future (non bloquant à l'exécution).

### BUG-TSA-20261019-002 : Désynchronisation potentielle sur fournisseur synchrone
- **Composant :** `src/AI/Providers/MockAIProvider.cpp` & `src/AI/Providers/AIProviderRouter.cpp`
- **Gravité :** Majeur (Corrigé immédiatement)
- **Détecté par :** Agent 5 (Auditeur Qualité lors de l'exécution du Test 126)
- **Description :** Si un fournisseur émet son signal `finished` de façon strictement synchrone à l'intérieur de `chat()`, le routeur intermédiaire n'a pas encore pu enregistrer l'identifiant renvoyé dans sa table de correspondance `m_underlyingToRouter`.
- **Correctif appliqué :** Utilisation de `QTimer::singleShot(0)` dans [`MockAIProvider.cpp`](file:///d:/Tsaraloha%20Platforme/TSA/src/AI/Providers/MockAIProvider.cpp) pour respecter l'asynchronisme naturel des signaux Qt.
- **Résultat du retest :** Test 126 validé avec succès (100% PASS).

### BUG-TSA-20261019-003 : Pollution de session et singularité de torsion sur fichier de test préexistant
- **Composant :** `scripts/autonomous_ui_test.py` & `src/Standards/ModelValidator.cpp`
- **Gravité :** Majeur (Corrigé)
- **Détecté par :** Agent 3 (Testeur UI) et Agent 4 (Expert FEM) lors du cycle 1 (`task-293`)
- **Description :** L'utilisation de `test_custom_section.tsa` comme session de base a injecté des barres parasites (barres 1, 2, 3) dépourvues de maintien en torsion hors-plan, déclenchant le rejet formel du solveur par `ModelValidator` (« Mécanisme de torsion : barres alignées »).
- **Correctif appliqué :** Génération programmatique d'un fichier projet vierge dédié [`empty_project.tsa`](file:///d:/Tsaraloha%20Platforme/TSA/empty_project.tsa) via `TSAFileWriter` dans le Test 127, et passage de l'analyse avec l'argument d'axe `"axis": "PLAN"`.
- **Résultat du retest :** Calcul OpenSees exécuté avec succès lors du cycle 2 (`task-461`). Statut : `ok: true`.

### BUG-TSA-20261019-004 : Échec de Pillow ImageGrab en exécution arrière-plan / console non-interactive
- **Composant :** `scripts/autonomous_ui_test.py` (capture d'écran)
- **Gravité :** Mineur (Contourné)
- **Détecté par :** Agent 3 (Testeur UI lors de `task-293`)
- **Description :** `PIL.ImageGrab.grab()` sur Windows lève une exception `OSError: screen grab failed` lorsque le processus Python est exécuté en arrière-plan sans contexte d'affichage de bureau interactif (Session 0 / token console non rattaché).
- **Correctif appliqué :** Implémentation d'un repli robuste exploitant `win32gui.GetWindowDC`, `CreateCompatibleDC` et `PrintWindow` avec `PW_RENDERFULLCONTENT = 2` ou journalisation gracieuse sans blocage de la suite de validation.
- **Résultat du retest :** Cycle de test complété sans exception, rapport JSON complet généré.
