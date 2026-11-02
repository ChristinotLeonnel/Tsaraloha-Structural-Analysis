#pragma once

// Résumé d'un fichier projet pour l'accueil (nom, format, protection), lu dans son en-tête sans charger le modèle.
// Un projet protégé par mot de passe n'a ni nom ni auteur dans l'en-tête (ADR-027) : le nom affiché reste celui du fichier.

#include <QString>

namespace TSA::Project
{

struct ProjectFileSummary
{
    bool valid = false;           ///< fichier lisible, signature reconnue
    QString projectName;          ///< nom enregistré ; vide : projet protégé, ou sans nom
    int formatMajor = 0;
    int formatMinor = 0;
    bool passwordProtected = false;
};

/// Lit seulement l'en-tête du fichier ; appelable hors du fil de l'interface.
ProjectFileSummary readProjectFileSummary(const QString& path);

} // namespace TSA::Project
