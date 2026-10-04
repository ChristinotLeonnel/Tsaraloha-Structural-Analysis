# TSA Thumbnail Provider — miniatures .tsa dans l'Explorateur Windows

## Résultat

Explorateur › Affichage › Grandes / Très grandes icônes : chaque fichier `.tsa` affiche le dernier
état visuel du modèle, tel qu'il était dans le viewport TSA au moment de l'enregistrement.

## Architecture

```text
TSA (enregistrement)                                  Explorateur Windows
  OccView::captureViewImage(640×480)  ── viewport réel, sans cube ni trièdre
        │
  TSAFileWriter ── .tsa 1.2 : [en-tête 272 o][payload compressé][bloc PRVW 32 o][PNG]
        │                                                         ▲
  SHChangeNotify(SHCNE_UPDATEITEM) ── invalide la miniature       │ lit l'en-tête + le PNG
                                                     TSAThumbnailProvider.dll
                                                     (IThumbnailProvider + IInitializeWithStream)
```

- **Un seul moteur de rendu** : l'image est produite par le viewport OCCT de TSA. L'extension ne
  calcule ni ne dessine rien ; elle lit le PNG et le met à l'échelle (WIC, intégré à Windows).
  `TSAPreviewGenerator` (projection QPainter) n'intervient qu'en repli, si aucun viewport n'est
  disponible lors de l'enregistrement (enregistrement programmatique).
- **Bloc d'aperçu** (`src/IO/TSAPreviewBlock.h`, format 1.2) : non compressé, après le payload
  (`header.fileSize` = fin du payload). Le chunk `THMB` reste écrit dans le payload.
- **Extension** (`src/ShellExtension/`) : C++/Win32, runtime C statique, aucune dépendance Qt /
  OpenCASCADE / solveur. Dépendances : DLL système uniquement (ole32, shell32, advapi32, kernel32,
  gdi32 ; WIC via COM). Fonctionne sur tout poste Windows 10/11 x64.
- **Robustesse** : fichier non TSA, tronqué, corrompu, chiffré, d'une version majeure future ou
  sans bloc d'aperçu (fichiers ≤ 1.1) → `E_FAIL` : l'Explorateur affiche l'icône TSA. Tailles et
  dimensions bornées (8 Mo, 4096 px). Aucune exception ne sort de la DLL. L'extension s'exécute
  dans le processus d'isolation des miniatures de Windows (pas de `DisableProcessIsolation`).

## Enregistrement Windows

| Mode | Commande | Droits | Clés |
| :--- | :--- | :--- | :--- |
| Utilisateur (par défaut) | automatique à chaque lancement de TSA, ou `TSA.exe --register-associations`, ou `regsvr32 TSAThumbnailProvider.dll` | aucun | `HKCU\Software\Classes` |
| Tous les utilisateurs (installeur) | `regsvr32 /i:machine /n TSAThumbnailProvider.dll` | administrateur | `HKLM\Software\Classes` |
| Retrait utilisateur | `TSA.exe --unregister-associations` ou `regsvr32 /u TSAThumbnailProvider.dll` | aucun | |
| Retrait machine | `regsvr32 /u /i:machine /n TSAThumbnailProvider.dll` | administrateur | |

Clés écrites : `CLSID\{5BA6698A-ED79-442D-92EE-31DA2704C07D}\InprocServer32` (chemin de la DLL,
`ThreadingModel=Apartment`) et `.tsa\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}` (+ même clé
sous le ProgID `TSA.Project`). Le registre persiste après redémarrage ; TSA réécrit les clés à
chaque lancement, ce qui suit un déplacement de l'installation. La DLL doit rester à côté de
`TSA.exe` (cible CMake `TSAThumbnailProvider`, copiée dans le même dossier).

## Cache et invalidation

L'Explorateur conserve les miniatures dans `%LocalAppData%\Microsoft\Windows\Explorer\thumbcache_*.db`,
indexées par fichier et date de modification. À chaque enregistrement, le fichier change et TSA
envoie `SHCNE_UPDATEITEM` : la miniature est redemandée. Après désinstallation, les miniatures déjà
en cache peuvent rester visibles tant que les fichiers ne changent pas (comportement Windows) :
Nettoyage de disque › « Miniatures » les supprime.

## Performances (mesurées, poste de développement)

| Cas | Temps d'extraction |
| :--- | :--- |
| Petit modèle (3 barres), 256 px | ≈ 2,3 ms |
| Grand modèle (20 000 barres, fichier 284 Ko), 256 px | ≈ 2,6 ms |
| Via le Shell (`IShellItemImageFactory`), 256 px / 1024 px | ≈ 10 ms / ≈ 40 ms |

Le temps ne dépend pas de la taille du modèle : seuls l'en-tête et le PNG (≈ 40–60 Ko) sont lus.

## Compatibilité

- Fichiers ≤ 1.1 : toujours lisibles par TSA ; dans l'Explorateur, icône TSA jusqu'au prochain
  enregistrement (qui ajoute le bloc d'aperçu).
- **Une version de TSA antérieure au format 1.2 ne peut pas ouvrir un fichier 1.2** : elle lit le
  bloc d'aperçu comme du payload et signale un CRC invalide. Les versions ≥ 1.2 lisent tous les formats.

## Tests

- `TSA_TestSuite --suite=thumbnail` (tests 127–129) : format 1.2, fichier ancien, fichiers
  invalides/tronqués/vides, grand modèle, tailles 16 → 1024 px, absence de fuite COM, vue A puis B.
- Vérification manuelle par le Shell : voir « Dépannage ».

## Dépannage

1. Les clés existent ?
   `reg query "HKCU\Software\Classes\.tsa\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}"`
2. La DLL est présente à l'emplacement de `InprocServer32` ?
3. Le fichier a été enregistré par TSA ≥ 1.2 ? (sinon : l'ouvrir et l'enregistrer une fois)
4. Miniature obsolète : vider le cache (Nettoyage de disque › Miniatures) ou redémarrer
   l'Explorateur.
5. Fichier protégé par mot de passe : volontairement aucune miniature (aucun aperçu en clair n'est écrit).
