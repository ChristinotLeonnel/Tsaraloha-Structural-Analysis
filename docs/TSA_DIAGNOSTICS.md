# TSA — Système Intégré de Diagnostic, Logging, Crash Reporting & Télémétrie

Ce document détaille l'architecture, le fonctionnement et l'utilisation du système de diagnostic et de télémétrie natif intégré dans **TSA (Tsaraloha Structural Analysis)**.

---

## 1. Vue d'Ensemble & Objectifs

Le système de diagnostic de TSA est conçu pour fonctionner de manière autonome, continue et sans dépendance tierce lourde. Il permet :
1. **La journalisation structurée** de tous les événements de modélisation, d'analyse, d'E/S et de commandes utilisateur.
2. **Le post-mortem complet en cas de crash** grâce à un intercepteur d'exceptions Windows SEH (`SetUnhandledExceptionFilter`) générant un minidump mémoire (`.dmp`) et un dump des 100 derniers événements survenus avant l'incident.
3. **Une mémoire circulaire (Ring Buffer)** conservant en permanence les 100 derniers événements en mémoire vive.
4. **La rotation automatique des sessions** (conservation des 10 dernières sessions dans `logs/sessions/`).
5. **L'exportation manuelle ou sur demande d'un rapport de diagnostic consolidé** (`TSA_Diagnostic_Report_*.txt`) incluant les spécifications matérielles, l'état du modèle et l'historique récent.
6. **L'intégration directe à l'interface graphique** via le dock `LogConsoleDock`, avec filtrage par sévérité/module et bouton d'exportation dédié.

---

## 2. Architecture des Composants

L'ensemble des composants réside dans le namespace `TSA::Diagnostics` sous `src/Diagnostics/` :

```
src/Diagnostics/
├── LogLevel.h           # Énumération des niveaux de sévérité (Trace, Debug, Info, Warning, Error, Critical)
├── LogEntry.h           # Structure standardisée de l'événement horodaté
├── RingBuffer.h         # Tampon circulaire thread-safe paramétrable (N=100)
├── Logger.h / .cpp      # Singleton central de journalisation, gestion de session et rotation
├── CrashHandler.h / .cpp# Gestionnaire de crash Windows SEH, minidump DbgHelp & StackWalk64
└── DiagnosticReport.h / .cpp # Générateur de rapport de diagnostic complet
```

### 2.1 Schéma Fonctionnel

```
[ Événements TSA ] ---> TSA_LOG_*(...) 
                             │
                             ▼
                  [ TSA::Diagnostics::Logger ]
                   │            │           │
                   ▼            ▼           ▼
        [ RingBuffer (100) ]  [ Session Log ] [ UI Console ]
                   │
                   ▼ (En cas de crash SEH)
        [ TSA::Diagnostics::CrashHandler ]
          ├── tsa_crash.log (Dernière commande + 100 événements)
          └── tsa_crash_*.dmp (MiniDump Windows DbgHelp)
```

---

## 3. Niveaux de Sévérité & Macros de Journalisation

### 3.1 Niveaux de Sévérité (`LogLevel`)

| Niveau | Chaîne | Description |
|---|---|---|
| `LogLevel::Trace` | `TRACE` | Diagnostics fins (détails de calcul géométrique, étapes de maillage). Actif uniquement en mode développeur. |
| `LogLevel::Debug` | `DEBUG` | Événements techniques détaillés (reconstruction de grille, mise à jour des scènes). |
| `LogLevel::Info` | `INFO` | Actions de modélisation standard (création poutre, chargement fichier, calcul FEA). |
| `LogLevel::Warning` | `WARN` | Situations anormales non bloquantes (géométrie dégénérée évitée, tolérance dépassée). |
| `LogLevel::Error` | `ERROR` | Échec d'une opération (échec commande, échec d'E/S, singularité matrice). |
| `LogLevel::Critical` | `CRIT` | Anomalie critique pouvant compromettre l'intégrité de la session. |

### 3.2 Macros d'Appel Recommandées

```cpp
#include "Diagnostics/Logger.h"

// Exemples d'appels :
TSA_LOG_TRACE("Geometry", "MeshDiscretize", "Discrétisation segment 12");
TSA_LOG_DEBUG("Grid", "CartesianGridRebuildStarted", "Recalcul grille bâtiment");
TSA_LOG_INFO("Model", "NodeCreated", "Création du nœud 14 (X=2.0, Y=0.0, Z=3.0)");
TSA_LOG_WARNING("Viewer", "HighlightFallback", "Entité introuvable pour surbrillance");
TSA_LOG_ERROR("IO", "FileOpenFailed", "Fichier introuvable ou verrouillé");
TSA_LOG_CRITICAL("Memory", "AllocationExhausted", "Mémoire insuffisante pour solveur");
```

---

## 4. Structure de l'Événement (`LogEntry`)

Chaque événement enregistré comporte les champs structurés suivants :
- `sequenceId` : Compteur séquentiel global incrémental (monotone).
- `timestamp` : Horodatage précis à la milliseconde (`YYYY-MM-DD HH:MM:SS.mmm`).
- `level` : Niveau de gravité `LogLevel`.
- `module` : Module d'origine (`Model`, `Grid`, `Viewer`, `Command`, `IO`, `FEA`, `UI`, `App`).
- `eventName` : Identifiant programmatique unique de l'événement (CamelCase).
- `message` : Message textuel explicatif.
- `file`, `line`, `function` : Origine exacte dans le code source C++ (via `__FILE__`, `__LINE__`, `__func__`).

---

## 5. Ring Buffer des 100 Derniers Événements

Le tampon circulaire `TSA::Diagnostics::RingBuffer<100, LogEntry>` :
- Alloue une capacité fixe de 100 entrées.
- Fonctionne en **FIFO** : lorsque 100 entrées sont atteintes, la plus ancienne est écrasée sans réallocation mémoire.
- Est protégé par un mutex standard pour une utilisation concurrente sans corruption mémoire.
- Permet l'extraction instantanée d'un instantané chronologique (`snapshot()`) à tout moment.

---

## 6. Gestionnaire de Crash Windows (`CrashHandler`)

### 6.1 Interception SEH & Terminate
Au démarrage de l'application (`Application.cpp`), le gestionnaire installe :
- `SetUnhandledExceptionFilter(&TSA_UnhandledExceptionFilter)` pour intercepter toutes les violations d'accès (`0xC0000005`), divisions par zéro, etc.
- `std::set_terminate(...)` pour intercepter les exceptions C++ non capturées.

### 6.2 Données Sauvegardées en Cas de Crash
Lors d'un crash, `CrashHandler` :
1. Crée un fichier de dump mémoire `logs/tsa_crash_YYYYMMDD_HHMMSS.dmp` via l'API Windows `MiniDumpWriteDump` (`MiniDumpWithIndirectlyReferencedMemory`).
2. Effectue un déroulement de pile d'appels (`StackWalk64`) pour obtenir l'adresse et les modules de l'instruction fautive.
3. Écrit en urgence le fichier texte `logs/tsa_crash.log` contenant :
   - Le code d'exception Windows et l'adresse IP fautive.
   - La dernière commande exécutée par l'utilisateur (`lastCommand()`).
   - L'horodatage exact du crash.
   - Les 100 derniers événements chronologiques du Ring Buffer.

---

## 7. Organisation des Fichiers de Logs

Tous les journaux sont stockés dans le dossier `logs/` de TSA :

```
logs/
├── tsa_latest.log                      # Fichier de la session courante ou la plus récente
├── tsa_crash.log                       # Dernier rapport d'urgence post-mortem
├── tsa_crash_*.dmp                     # Minidump binaire analysable dans Visual Studio / WinDbg
├── TSA_Diagnostic_Report_*.txt         # Rapports consolidés exportés manuellement
└── sessions/                           # Historique des 10 dernières sessions
    ├── session_20260927_040000_1234.log
    ├── session_20260927_041530_5678.log
    └── ...
```

---

## 8. Interface Utilisateur & Exportation

### 8.1 Console Dock (`LogConsoleDock`)
- Situé dans la zone inférieure de la fenêtre principale.
- Affichage en temps réel avec coloration par gravité (Bleu=Trace, Gris=Debug, Blanc=Info, Jaune=Warn, Rouge=Error).
- Filtres combinés : par sévérité minimale et par module.
- Bouton **"Exporter Rapport..."** pour générer un diagnostic instantané.

### 8.2 Commandes Console Directes
Dans la zone de saisie de la console de commandes, taper l'une des commandes suivantes déclenche l'exportation du rapport :
- `DIAG`
- `REPORT`
- `DIAGNOSTIC`

### 8.3 Menu d'Aide
Accessible via la barre de menus : `Aide > Exporter Rapport de Diagnostic...`.
