#pragma once

// Emplacements utilisés à l'exécution par TSA et TSALab (docs/DEPLOYMENT.md). Point unique de résolution :
// aucun module ne doit supposer que le répertoire courant est celui de l'exécutable, ni écrire dans le
// dossier d'installation (Program Files n'est pas inscriptible).
//
//   Livré avec l'application (lecture seule) : <app>/            exécutable, DLL, plugins Qt
//                                              <app>/resources/   ressources OCCT, données volumineuses
//                                              <app>/engines/     moteurs de calcul (OpenSees)
//                                              <app>/modules/     modules livrés (module.json)
//                                              <app>/plugins/     plugins binaires (DLL)
//                                              <app>/Extensions/  bibliothèques TSALib
//   Intégré à l'exécutable (qrc)            : templates intégrés, configurations par défaut
//   Données utilisateur (modifiables)        : QStandardPaths::AppConfigLocation (raccourcis, réglages),
//                                              AppLocalDataLocation (templates, modules, extensions,
//                                              journaux, bibliothèques utilisateur)
//
// Sur un poste de développement, les dossiers livrés absents sont recherchés dans les sources
// (TSA_SOURCE_DIR) : uniquement en repli, jamais avant les emplacements livrés.

#include <QString>
#include <QStringList>

namespace TSA::Core
{

class AppPaths
{
public:
    /// Dossier de l'exécutable (jamais le répertoire courant).
    static QString applicationDir();
    /// Dossier des sources de développement s'il existe sur ce poste (vide sinon, ex. paquet installé).
    static QString developmentSourceDir();

    // --- Livré avec l'application ---------------------------------------------------------------
    /// Premier dossier existant parmi <app>/<relative> puis, en développement, <sources>/<devRelative>.
    static QString shippedDir(const QString& relative, const QString& devRelative = QString());
    static QString occtResourcesDir();      ///< resources/occt (ou SDK OCCT en développement)
    static QString enginesDir();            ///< engines
    static QString openSeesDir();           ///< engines/OpenSees (ou thirdparty/OpenSees)
    static QString shippedModulesDir();     ///< modules
    static QString shippedExtensionsDir();  ///< Extensions

    // --- Données utilisateur ----------------------------------------------------------------------
    static QString userConfigDir();         ///< AppConfigLocation (créé au besoin)
    static QString userDataDir();           ///< AppLocalDataLocation (créé au besoin)
    static QString userTemplatesDir();      ///< <données>/templates
    static QString userModulesDir();        ///< <données>/modules
    static QString userExtensionsDir();     ///< <données>/Extensions
    /// Journaux : <app>/../logs ou <app>/logs en développement s'ils sont inscriptibles, sinon <données>/logs.
    static QString logsDir();

    /// Le dossier existe (ou peut être créé) et accepte l'écriture d'un fichier.
    static bool isWritableDir(const QString& dir);
    /// Sauvegarde horodatée d'un fichier utilisateur avant migration : <fichier>.bak-<étiquette>[-n].
    static QString backupFile(const QString& file, const QString& label, QString* error = nullptr);
};

} // namespace TSA::Core
