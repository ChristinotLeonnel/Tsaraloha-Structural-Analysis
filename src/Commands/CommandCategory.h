#pragma once

#include <QtGlobal>   // QT_TRANSLATE_NOOP : noms en français, traduits à l'affichage (contexte « CommandCategory »)
#include <string>

namespace TSA::Commands
{

/**
 * @brief Classification professionnelle des commandes TSA inspirée d'AutoCAD et de Robot Structural Analysis.
 */
enum class CommandCategory
{
    File,           // Gestion des projets, fichiers .tsa, import/export
    Edit,           // Undo, Redo, Cut, Copy, Paste, Delete
    Create,         // Création d'éléments structuraux (Nœuds, Poutres, Poteaux, Câbles, Dalles, Voiles, etc.)
    Modify,         // Transformations géométriques CAO (Move, Rotate, Copy, Mirror, Scale, Split)
    Selection,      // Sélection par type, par propriété, fenêtre, effacer
    Properties,     // Gestion et inspection des propriétés des éléments
    Libraries,      // Catalogues et bibliothèques TSALib (Sections, Matériaux, Câbles)
    Structure,      // Grilles d'axes 3D, gestionnaire d'étages/niveaux, presets
    Loads,          // Définition des charges (ponctuelles, réparties, surfaciques)
    LoadCases,      // Cas de charges, combinaisons ELU/ELS
    Model,          // Opérations globales sur le modèle
    Analysis,       // Génération maillage EF, calcul statique linéaire et non linéaire
    Results,        // Visualisation des résultats (déplacements, efforts internes, contraintes)
    Design,         // Dimensionnement et vérifications normatives Eurocodes
    View,           // Vues standard, zoom, pan, orbite 3D, section cut
    Display,        // Visibilité des entités (nœuds, profilés 3D, charges, appuis)
    Tools,          // Outils de mesure, calculatrices, vérificateurs géométriques
    Documentation,  // Notes de calcul, génération de rapports
    Workspace,      // Disposition des docks, environnement de travail
    Settings,       // Préférences, unités métriques, thèmes
    Snap,           // Grille, accrochage et plans de travail
    Mesh,           // Maillage éléments finis
    Bim,            // BIM et échanges de données (IFC)
    Annotation,     // Cotation et mesure
    Help            // Aide, liste et personnalisation des raccourcis
};

inline std::string categoryToString(CommandCategory cat)
{
    switch (cat)
    {
    case CommandCategory::File:          return QT_TRANSLATE_NOOP("CommandCategory", "Gestion des projets");
    case CommandCategory::Edit:          return QT_TRANSLATE_NOOP("CommandCategory", "Édition");
    case CommandCategory::Create:        return QT_TRANSLATE_NOOP("CommandCategory", "Modélisation");
    case CommandCategory::Modify:        return QT_TRANSLATE_NOOP("CommandCategory", "Modification et transformations");
    case CommandCategory::Selection:     return QT_TRANSLATE_NOOP("CommandCategory", "Sélection et isolation");
    case CommandCategory::Properties:    return QT_TRANSLATE_NOOP("CommandCategory", "Propriétés");
    case CommandCategory::Libraries:     return QT_TRANSLATE_NOOP("CommandCategory", "Matériaux et sections");
    case CommandCategory::Structure:     return QT_TRANSLATE_NOOP("CommandCategory", "Grilles et niveaux");
    case CommandCategory::Loads:         return QT_TRANSLATE_NOOP("CommandCategory", "Charges");
    case CommandCategory::LoadCases:     return QT_TRANSLATE_NOOP("CommandCategory", "Cas de charge et combinaisons");
    case CommandCategory::Model:         return QT_TRANSLATE_NOOP("CommandCategory", "Modèle");
    case CommandCategory::Analysis:      return QT_TRANSLATE_NOOP("CommandCategory", "Analyse structurelle");
    case CommandCategory::Results:       return QT_TRANSLATE_NOOP("CommandCategory", "Résultats");
    case CommandCategory::Design:        return QT_TRANSLATE_NOOP("CommandCategory", "Dimensionnement");
    case CommandCategory::View:          return QT_TRANSLATE_NOOP("CommandCategory", "Navigation 2D et 3D");
    case CommandCategory::Display:       return QT_TRANSLATE_NOOP("CommandCategory", "Affichage");
    case CommandCategory::Tools:         return QT_TRANSLATE_NOOP("CommandCategory", "Outils généraux");
    case CommandCategory::Documentation: return QT_TRANSLATE_NOOP("CommandCategory", "Rapports");
    case CommandCategory::Workspace:     return QT_TRANSLATE_NOOP("CommandCategory", "Fenêtres et panneaux");
    case CommandCategory::Settings:      return QT_TRANSLATE_NOOP("CommandCategory", "Paramètres");
    case CommandCategory::Snap:          return QT_TRANSLATE_NOOP("CommandCategory", "Grille, accrochage et plans de travail");
    case CommandCategory::Mesh:          return QT_TRANSLATE_NOOP("CommandCategory", "Maillage");
    case CommandCategory::Bim:           return QT_TRANSLATE_NOOP("CommandCategory", "BIM et échanges de données");
    case CommandCategory::Annotation:    return QT_TRANSLATE_NOOP("CommandCategory", "Cotation et mesure");
    case CommandCategory::Help:          return QT_TRANSLATE_NOOP("CommandCategory", "Aide");
    }
    return QT_TRANSLATE_NOOP("CommandCategory", "Général");
}

/// Ordre des catégories (fichier shortcut.txt, éditeur des raccourcis).
inline const CommandCategory* categoryOrder(std::size_t* count)
{
    static const CommandCategory order[] = {
        CommandCategory::File,      CommandCategory::Edit,       CommandCategory::Selection, CommandCategory::Create,
        CommandCategory::Modify,    CommandCategory::Annotation, CommandCategory::Snap,      CommandCategory::Structure,
        CommandCategory::View,      CommandCategory::Display,    CommandCategory::Properties, CommandCategory::Libraries,
        CommandCategory::Mesh,      CommandCategory::Loads,      CommandCategory::LoadCases, CommandCategory::Analysis,
        CommandCategory::Results,   CommandCategory::Documentation, CommandCategory::Bim,    CommandCategory::Model,
        CommandCategory::Design,    CommandCategory::Workspace,  CommandCategory::Tools,     CommandCategory::Settings,
        CommandCategory::Help };
    *count = sizeof(order) / sizeof(order[0]);
    return order;
}

} // namespace TSA::Commands
