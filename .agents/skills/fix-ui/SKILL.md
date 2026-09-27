---
name: fix-ui
description: Diagnostiquer et corriger un bug d'interface (Ribbon, Dock, Properties, ModelTree, Dialogs, Widgets) en remontant jusqu'à la cause racine plutôt qu'au symptôme visible.
---

# fix-ui

Objectif : corriger un problème d'UI en remontant toute la chaîne réelle plutôt qu'en
patchant seulement le widget où le symptôme est visible.

## Chaîne de diagnostic

```text
Widget
→ Signal
→ Slot / Callback
→ Command
→ Model
→ View (rafraîchissement)
```

1. **Widget** — reproduire le bug, identifier précisément le widget concerné
   (`src/UI/Ribbon`, `Dock`, `Properties/PropertyPanel`, `ModelTree/ModelTreeWidget`,
   `Dialogs`, `Widgets`).
2. **Signal** — vérifier quel signal Qt est émis (ou aurait dû l'être) au moment de
   l'action utilisateur.
3. **Slot / Callback** — vérifier le slot connecté, et s'il s'agit bien d'un callback
   `IModelObserver` dans le cas d'un rafraîchissement depuis le modèle.
4. **Command** — si l'action modifie le modèle, vérifier qu'elle passe par une `ICommand`
   exécutée via `CommandManager::executeCommand`, et que cette commande fait ce qu'elle
   prétend faire (`execute()`/`undo()` cohérents).
5. **Model** — vérifier l'état réel du modèle après l'action (pas seulement l'affichage) :
   le bug est-il dans le modèle lui-même ou seulement dans son reflet UI ?
6. **View** — enfin, vérifier que le widget se rafraîchit correctement depuis l'état du
   modèle (pas depuis un cache local désynchronisé).

## Ne pas corriger seulement le symptôme

- Si un widget affiche une valeur incorrecte, ne pas se contenter de corriger l'affichage :
  déterminer si la valeur stockée dans le modèle est elle-même incorrecte, auquel cas la
  vraie cause est en amont (Command ou Model).
- Utiliser le skill `verify-sync` si le doute porte sur *où* la divergence apparaît
  réellement.

## Après correction

- Vérifier qu'aucun autre widget consommant la même donnée n'a le même bug latent (ex. si
  `PropertyPanel` et `ModelTreeWidget` affichent tous deux une propriété d'un `Beam`).
- Ajouter un test si le bug touche une logique non triviale au-delà du simple rendu.
