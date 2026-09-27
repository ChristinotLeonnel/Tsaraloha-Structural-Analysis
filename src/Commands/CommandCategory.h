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
    Analysis,       // Génération maillage EF, calcul statique linéaire, modal, non-linéaire
    Results,        // Visualisation des résultats (déplacements, efforts internes, contraintes)
    Design,         // Dimensionnement et vérifications normatives Eurocodes
    View,           // Vues standard, zoom, pan, orbite 3D, section cut
    Display,        // Visibilité des entités (nœuds, profilés 3D, charges, appuis)
    Tools,          // Outils de mesure, calculatrices, vérificateurs géométriques
    Documentation,  // Notes de calcul, génération de rapports
    Workspace,      // Disposition des docks, environnement de travail
    Settings        // Préférences, unités métriques, thèmes
};

inline std::string categoryToString(CommandCategory cat)
{
    switch (cat)
    {
    case CommandCategory::File:          return "Fichier";
    case CommandCategory::Edit:          return "Édition";
    case CommandCategory::Create:        return "Création / Dessin";
    case CommandCategory::Modify:        return "Modification CAO";
    case CommandCategory::Selection:     return "Sélection";
    case CommandCategory::Properties:    return "Propriétés";
    case CommandCategory::Libraries:     return "Bibliothèques";
    case CommandCategory::Structure:     return "Structure";
    case CommandCategory::Loads:         return "Charges";
    case CommandCategory::LoadCases:     return "Cas de Charges";
    case CommandCategory::Model:         return "Modèle";
    case CommandCategory::Analysis:      return "Calculs / Analyse";
    case CommandCategory::Results:       return "Résultats";
    case CommandCategory::Design:        return "Dimensionnement";
    case CommandCategory::View:          return "Vue";
    case CommandCategory::Display:       return "Affichage";
    case CommandCategory::Tools:         return "Outils";
    case CommandCategory::Documentation: return "Documentation";
    case CommandCategory::Workspace:     return "Espace de travail";
    case CommandCategory::Settings:      return "Paramètres";
    default:                             return "Général";
    }
}

} // namespace TSA::Commands
