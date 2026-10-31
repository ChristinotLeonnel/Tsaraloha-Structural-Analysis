#include "CommandSynonyms.h"

#include <QHash>

namespace TSA::Commands
{

QStringList commandSynonyms(const QString& commandId)
{
    // Termes séparés par « ; » : français ; anglais ; malagasy (non traduits : ce sont des clés de recherche).
    static const QHash<QString, QString> table = {
        // Projet
        { "cmd.file.new", "créer;nouveau fichier;new file;create;vaovao;mamorona" },
        { "cmd.file.open", "charger;ouvrir fichier;load;open file;sokafy;ampidiro rakitra" },
        { "cmd.file.save", "sauvegarder;sauver;store;tehirizo;tahiry" },
        { "cmd.file.save_as", "copie du projet;dupliquer le fichier;save copy;export copy;tehirizo amin'ny anarana hafa" },
        { "cmd.file.close", "fermer;close file;hidio" },
        { "cmd.file.exit", "sortir;fermer TSA;exit;quit;hivoaka;ialana" },
        // Édition
        { "cmd.edit.undo", "retour arrière;défaire;undo;back;foanana;averina any aloha" },
        { "cmd.edit.redo", "refaire;redo;averina" },
        { "cmd.edit.delete", "effacer;retirer;enlever;erase;remove;fafao;esory" },
        { "cmd.edit.copy", "dupliquer;copy;adikao" },
        { "cmd.edit.paste", "insérer;paste;apetaho" },
        { "cmd.select.mode", "curseur;pointer;flèche;cursor;pick;fifantenana;fidio" },
        { "cmd.select.all", "tout;everything;select everything;fidio daholo" },
        // Modélisation
        { "cmd.create.node", "nœud;point;sommet;jonction;node;joint;vertex;fihaonana;teboka" },
        { "cmd.create.beam", "beam;poutre;solive;linteau;IPE;HEA;HEB;profilé;girder;joist;lintel;andry mitsivalana;vatan-kazo" },
        { "cmd.create.column", "column;poteau;pilier;colonne;pile;pillar;post;stanchion;andry;tsanganana" },
        { "cmd.create.bar", "barre;élément filaire;membrure;member;rod;frame element;tsorakazo" },
        { "cmd.create.cable", "câble;hauban;tirant;cable;stay;tie;tariby" },
        { "cmd.create.slab", "dalle;plancher;plaque;slab;floor;plate;takelaka;gorodona" },
        { "cmd.create.wall", "voile;mur;paroi;wall;shear wall;rindrina" },
        { "cmd.create.truss", "treillis;ferme;Warren;Pratt;Howe;truss;rafitra makarakara" },
        { "cmd.create.foundation", "semelle;fondation;footing;foundation;fototra" },
        { "cmd.support.fixed", "encastrement;appui fixe;fixed support;clamp;fanohanana raikitra" },
        { "cmd.support.pinned", "rotule;articulation;appui articulé;pin;hinge;fanohanana mihodina" },
        { "cmd.support.roller", "appui simple;rouleau;glissant;roller;sliding;fanohanana mihodikodina" },
        // Modification
        { "cmd.modify.move", "déplacer;translater;bouger;move;translate;afindrao;fifindrana" },
        { "cmd.modify.copy_3d", "copier;dupliquer;copy;duplicate;adikao" },
        { "cmd.modify.rotate", "tourner;pivoter;rotation;rotate;turn;ahodino;fihodinana" },
        { "cmd.modify.mirror", "miroir;symétrie;mirror;symmetry;reflect;taratra;fitaratra" },
        { "cmd.modify.split_bars", "couper;diviser;split;divide;zarazarao" },
        { "cmd.modify.merge_nodes", "fusionner;souder;merge;weld;atambaro" },
        // Grille et plans
        { "cmd.display.grid", "grille;quadrillage;grid;makarakara" },
        { "cmd.struct.new_grid", "axes;files;trame;grid lines;axis;makarakara;tsipika fototra" },
        { "cmd.struct.grid_dialog", "axes;trame;grid lines;axis;makarakara" },
        { "cmd.struct.levels", "étages;niveaux;storeys;floors;levels;rihana" },
        { "cmd.coord.workplane_xy", "plan horizontal;plan de travail;work plane;sketch plane;planina fiasana" },
        { "cmd.coord.workplane_xz", "plan vertical;façade;work plane;elevation;planina fiasana" },
        { "cmd.coord.workplane_yz", "plan vertical;pignon;work plane;planina fiasana" },
        { "cmd.coord.workplane_custom", "plan incliné;trois points;3 points;inclined plane;planina mitongilana" },
        { "cmd.snap.grid", "accrochage;magnétisme;aimantation;snap;fipetahana" },
        { "cmd.snap.object_snap", "accrochage;OSNAP;extrémité;milieu;snap;endpoint;midpoint;fipetahana" },
        // Charges
        { "cmd.loads.point", "force;charge concentrée;point load;force;enta tokana;hery" },
        { "cmd.loads.distributed", "charge linéaire;répartie;uniform load;UDL;line load;enta miparitaka" },
        { "cmd.loads.trapezoidal", "triangulaire;variable;trapezoidal;triangular;enta miova" },
        { "cmd.loads.surface", "pression;charge surfacique;area load;pressure;enta mivelatra;tsindry" },
        { "cmd.loads.self_weight", "poids propre;gravité;self weight;dead load;gravity;lanjany;lanja" },
        { "cmd.loadcases.manager", "combinaisons;ELU;ELS;cas de charge;load combinations;ULS;SLS;fitambaran'enta;tranga enta" },
        // Maillage, calcul, résultats
        { "cmd.analysis.mesh", "maillage;mailler;éléments finis;mesh;FEM;Gmsh;harato;singa voafetra" },
        { "cmd.mesh.settings", "taille des mailles;finesse;mesh size;harato" },
        { "cmd.mesh.quality", "qualité;distorsion;aspect;quality;harato" },
        { "cmd.analysis.solve", "calculer;lancer le calcul;résoudre;OpenSees;analyse;run;solve;compute;kajy;atombohy ny kajy" },
        { "cmd.analysis.config", "solveur;paramètres de calcul;linéaire;modal;solver;settings;fikirana kajy" },
        { "cmd.results.deformed", "déformée;déformation;flèche;deformed shape;deflection;endrika miova" },
        { "cmd.results.reactions", "réactions d'appui;support reactions;hery mamaly" },
        { "cmd.results.diagram_mz", "moment fléchissant;bending moment;momenta fiforetana" },
        { "cmd.results.diagram_my", "moment fléchissant;bending moment;momenta fiforetana" },
        { "cmd.results.diagram_mx", "torsion;torque;fiolonana" },
        { "cmd.results.diagram_vz", "effort tranchant;cisaillement;shear force;hery manapaka" },
        { "cmd.results.diagram_vy", "effort tranchant;cisaillement;shear force;hery manapaka" },
        { "cmd.results.diagram_n", "effort normal;traction;compression;axial force;hery mahitsy" },
        { "cmd.results.displacements", "déplacements;flèches;displacements;fifindrana" },
        { "cmd.results.stresses", "contraintes;von Mises;stresses;tsindry anatiny" },
        { "cmd.report.ndc", "note de calcul;rapport;PDF;calculation report;tatitra momba ny kajy" },
        // BIM, affichage, outils
        { "cmd.bim.import_ifc", "IFC;BIM;Revit;Tekla;importer;import;ampidiro" },
        { "cmd.bim.export_ifc", "IFC;BIM;exporter;export;avoahy" },
        { "cmd.view.fit_all", "cadrer;tout voir;zoom étendu;zoom extents;fit;jereo daholo" },
        { "cmd.view.3d", "perspective;axonométrie;3D view;fijery 3D" },
        { "cmd.view.top", "plan;dessus;top view;fijery avy ambony" },
        { "cmd.tools.measure", "mesurer;distance;règle;measure;ruler;refeso" },
        { "cmd.settings.theme", "sombre;clair;couleurs;dark;light;colors;maizina;mazava" },
        { "cmd.settings.language", "langue;anglais;français;malgache;language;English;French;Malagasy;fiteny;teny" },
        { "cmd.help.shortcuts", "raccourcis;touches;clavier;keyboard;hotkeys;lalana fohy" },
        { "cmd.help.full", "aide;documentation;manuel;help;manual;fanampiana" },
    };
    const auto it = table.constFind(commandId);
    return it == table.constEnd() ? QStringList() : it.value().split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

} // namespace TSA::Commands
