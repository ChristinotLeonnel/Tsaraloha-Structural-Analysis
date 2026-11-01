---
name: essais-sans-effet-de-bord
description: "Lire un outil avant de le lancer, et ne pas ouvrir de fenêtres sur le poste de l'utilisateur pendant une tâche de fond"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 84fdaea9-ba6e-422a-80ce-1f048ba5db29
  modified: 2026-10-31T09:40:18.489Z
---

Lire le code d'un exécutable avant de le lancer, même s'il ressemble à un test. Lancer les essais sans fenêtre visible : parent `CREATE_NO_WINDOW`, entrée fermée, erreurs système muettes.

**Why:** le 2026-10-31, `fem3d_opensees_benchmarks.exe`, lancé sans lecture préalable, a ouvert des fenêtres OpenSees sur le poste, que l'utilisateur a signalées, et a supprimé des fichiers suivis dans `3DDesplacement`, restaurés ensuite par Git.

**How to apply:** vérifier ce qu'un outil écrit et où, surtout hors du dépôt courant. Sous Git Bash, `std::system` et `CreateProcess` sans drapeau ouvrent une console. `TSA.exe`, `TSA_TestSuite` et `TSALab_TestSuite` ouvrent de vraies fenêtres : prévenir avant. Relatif à [[audit-2026-10-31]].
