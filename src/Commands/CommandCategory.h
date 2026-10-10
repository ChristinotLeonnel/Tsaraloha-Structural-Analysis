#pragma once

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
    case CommandCategory::File:          return "Gestion des projets";
    case CommandCategory::Edit:          return "Édition";
    case CommandCategory::Create:        return "Modélisation";
    case CommandCategory::Modify:        return "Modification et transformations";
    case CommandCategory::Selection:     return "Sélection et isolation";
    case CommandCategory::Properties:    return "Propriétés";
    case CommandCategory::Libraries:     return "Matériaux et sections";
    case CommandCategory::Structure:     return "Grilles et niveaux";
    case CommandCategory::Loads:         return "Charges";
    case CommandCategory::LoadCases:     return "Cas de charge et combinaisons";
    case CommandCategory::Model:         return "Modèle";
    case CommandCategory::Analysis:      return "Analyse structurelle";
    case CommandCategory::Results:       return "Résultats";
    case CommandCategory::Design:        return "Dimensionnement";
    case CommandCategory::View:          return "Navigation 2D et 3D";
    case CommandCategory::Display:       return "Affichage";
    case CommandCategory::Tools:         return "Outils généraux";
    case CommandCategory::Documentation: return "Rapports";
    case CommandCategory::Workspace:     return "Fenêtres et panneaux";
    case CommandCategory::Settings:      return "Paramètres";
    case CommandCategory::Snap:          return "Grille, accrochage et plans de travail";
    case CommandCategory::Mesh:          return "Maillage";
    case CommandCategory::Bim:           return "BIM et échanges de données";
    case CommandCategory::Annotation:    return "Cotation et mesure";
    case CommandCategory::Help:          return "Aide";
    }
    return "Général";
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
