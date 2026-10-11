# Essais de l'interface de TSA (UI Automation)

Scripts PowerShell (Windows PowerShell 5.1) qui pilotent l'application compilée par UI Automation :
vrais clics (bascule, invocation), sélection dans les listes (liste dépliée, flèches, Entrée), valeurs
des compteurs, touches (F5, Ctrl+F5, Ctrl+S), redimensionnement, fermeture et réouverture, captures de la
fenêtre. Chaque étape relit l'état de l'interface (case, étiquette du facteur) avant de conclure.

1. Projet d'essai : `TSA_TEST_DUMP_DIR=<dossier> TSA_TestSuite.exe --suite=torsion` écrit
   `<dossier>/torsion_ui.tsa` : console en torsion, barre inclinée sous couple réparti, cas FLEXION ;
   réglages d'analyse OpenSees 3D, cas TORSION.
2. Scénario :
   `powershell -ExecutionPolicy Bypass -File tools/ui_tests/torsion_scenario.ps1 -Project <dossier>/torsion_ui.tsa -OutDir <sorties>`
   Résultat : lignes `OK` / `ÉCHEC` et un bilan, captures `NN_*.png`. Le scénario enregistre le projet
   (réglages du cas FLEXION) : régénérer le projet avant une nouvelle exécution.
3. Contrôle visuel : `torsion_visual.ps1 -Project … -OutDir …` (fenêtre maximisée, panneau de qualité du
   maillage fermé, cadrage sur la déformée, captures avec et sans rotation des sections).

Les scripts doivent être enregistrés en UTF-8 avec BOM (sinon PowerShell 5 lit les accents en ANSI). Le
poste doit avoir une session interactive ouverte (UI Automation et les touches simulées agissent sur le
bureau actif). La fenêtre de TSA doit rester au premier plan pendant l'essai.

4. Boîte « Charge sur barre » : `torsion_dialog.ps1 -Project <dossier>/torsion_dialog.tsa -OutDir …` (projet écrit
   par la même commande que torsion_ui.tsa). Moment réparti saisi (cas T, local x, 2 kNm/m), refus de l'axe
   « gravité », treillis signalé au calcul, enregistrement, réouverture. Contrôle du fichier enregistré :
   `TSA_UI_CHECK_FILE=<dossier>/torsion_dialog.tsa TSA_TestSuite.exe --suite=torsion` (charges relues, θ comparé
   à m·L²/(2GJ)).
5. Taille de la vue 3D selon les panneaux : `viewport_layout.ps1 -Project … [-Width 1220 -Height 765]`.

Les boutons qui ouvrent une boîte modale sont cliqués à la souris (`Click-Mouse`) : l'Invoke d'UI Automation
ne rend pas la main pendant la boucle modale de Qt.
