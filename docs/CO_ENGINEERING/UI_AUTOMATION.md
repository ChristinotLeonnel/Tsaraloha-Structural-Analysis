# Architecture d'Automatisation de l'Interface Qt Réelle (TSA)
**Date :** 19 octobre 2026  
**Statut :** Spécification Technique d'Automatisation Graphique & IPC  
**Projet :** Tsaraloha Structural Analysis (TSA)  
**Cible :** Windows 10/11 x64, Qt 6.11, OpenCASCADE 8.0.1, OpenSees

---

## 1. Évaluation Comparative des Outils d'Automatisation UI

Pour automatiser une application de bureau native C++/Qt 6 sous Windows sans introduire de dépendances propriétaires ou coûteuses :

| Solution | Gratuit / Ouvert | Compatibilité Qt 6 / OCCT | Niveau d'intrusion dans le code | Recommandation TSA |
| :--- | :---: | :---: | :---: | :--- |
| **Squish for Qt** | Non (Licence commerciale très chère) | Excellente | Nécessite un hook d'injection binaire | **Rejeté** (inaccessible aux utilisateurs et non pérenne financièrement). |
| **Playwright / Selenium** | Oui | Nulle (Réservé au Web / Chromium) | Inadapté aux widgets natifs C++ | **Rejeté** pour le cœur Qt desktop. |
| **Microsoft UI Automation (UIA)** | Oui (Inclus dans Windows) | Bonne sur les contrôles standard | Faible (dépend de l'arbre d'accessibilité Qt) | **Retenu en couche complémentaire** via `comtypes` / PowerShell. |
| **Win32 API (`pywin32` / `SendInput`)** | Oui (Standard Windows) | Universelle (clavier, souris, fenêtres) | Nulle (pilotage OS externe) | **Retenu pour les entrées réelles** (focus, frappes, clics réels). |
| **Canal IPC `AutomationServer` (Named Pipe)** | Oui (Natif dans TSA) | Totale (conçu pour TSA et TSALab) | Intégré dans `src/Automation/` | **Retenu comme colonne vertébrale déterministe** (`tsaraloha-tsa`). |
| **Capture Visuelle (Pillow / OpenCV)** | Oui (Python standard) | Universelle (rendu OCCT 3D) | Nulle (lecture du framebuffer) | **Retenu pour la collecte de preuves** et l'archivage horodaté. |

---

## 2. Architecture d'Automatisation Retenue (Triptyque Hybride)

La stratégie retenue pour l'Agent 3 (Testeur UI Autonome) combine **trois couches synergiques** :

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   AGENT 3 : HARNAIS DE TEST UI AUTONOME                │
└────────────────────────────────────────────────────────────────────────┘
                                    │
    ┌───────────────────────────────┼───────────────────────────────┐
    ▼                               ▼                               ▼
 COUCHE 1 : CONTRÔLE OS         COUCHE 2 : IPC DÉTERMINISTE     COUCHE 3 : PREUVES VISUELLES
 (Win32 API / pywin32)          (Named Pipe tsaraloha-tsa)      (Pillow / OpenCV)
 ──────────────────────         ──────────────────────────      ────────────────────────────
 • Démarrage / Arrêt du process • Requêtes JSON synchrones      • Capture écran intégrale
 • Détection HWND de la fenêtre • Création nœuds, barres, chgs  • Découpage région d'intérêt
 • Raccourcis clavier réels     • Déclenchement calcul OpenSees • Détection d'artefacts visuels
 • Déplacement / redimension    • Vérification état & Undo/Redo • Sauvegarde PNG horodatée
```

---

## 3. Protocole du Canal IPC (`\\.\pipe\tsaraloha-tsa`)

L'application `TSA.exe` embarque nativement le serveur `TSA::Automation::AutomationServer`. Dès le lancement de la fenêtre principale, un tube nommé local Windows est ouvert sous le nom `\\.\pipe\tsaraloha-tsa`.

### Caractéristiques de sécurité :
- Réservé exclusivement à la session du compte utilisateur Windows actif (`UserAccessOption`).
- Aucun port réseau ouvert (immunité contre les écoutes distantes).
- Exécution synchrone dans le thread UI de Qt (garantie que l'état graphique et le modèle sont synchronisés).

### Format des échanges (JSON Lines) :
Chaque requête est une ligne JSON terminée par `\n` :
```json
{"id": 1, "method": "execute", "params": {"command": "model.create_node", "arguments": {"position": [0, 0, 3]}}}
```
Réponse synchrone de TSA :
```json
{"id": 1, "result": {"id": 2, "name": "N2", "position": [0, 0, 3]}}
```

### Commandes principales disponibles pour les tests :
- `model.create_node` : création d'un nœud structurel avec coordonnées $[X, Y, Z]$.
- `model.create_beam` / `model.create_column` : création d'éléments linéaires avec profilé et matériau.
- `model.set_support` : application d'un encastrement (`fixed`) ou d'une rotule (`pinned`).
- `loads.create_case` : création de cas de charges ($G, Q, W$).
- `loads.add_uniform` / `loads.add_nodal` : application de charges réparties ou ponctuelles.
- `analysis.run` : exécution du solveur OpenSees asynchrone.
- `results.summary` : extraction des déplacements et moments maximaux.
- `undo` / `redo` : annulation et rétablissement transactionnels.

---

## 4. Stratégie de Capture et Preuves Visuelles

Pour chaque test exécuté par l'Agent 3 :
1. Une capture de référence est réalisée à l'ouverture de l'application (`tsa_startup.png`).
2. Une capture est effectuée après la modélisation de la structure (`tsa_modeled.png`).
3. Une capture est effectuée après la résolution OpenSees (`tsa_results.png`).
4. Toutes les captures sont archivées dans le dossier dédié :
   `tests/results/screenshots/`.
5. Si une boîte de dialogue d'erreur inattendue apparaît, la capture est automatiquement étiquetée `BUG_<id>_<timestamp>.png` et indexée dans `BUG_REPORTS.md`.
