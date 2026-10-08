#pragma once

// Application Qt de l'écosystème Tsaraloha (TSA, TSALab) : initialisation commune — ADR-024.
//
// Journaux et rapport de plantage, AppUserModelID Windows, traduction française des textes Qt,
// identité (nom, organisation, version, icône du produit compilé), style Fusion et thème sombre,
// environnement OpenCASCADE. Chaque application dérive de cette classe et n'ajoute que sa fenêtre.

#include <QApplication>

namespace TSA::UI
{

class EcosystemApplication : public QApplication
{
    Q_OBJECT

public:
    EcosystemApplication(int& argc, char** argv);
    ~EcosystemApplication() override;

    /// Fichiers projet passés en ligne de commande (format natif ou importé du produit).
    QStringList projectFilesFromArguments() const;

    /// Association Windows de l'extension native du produit (enregistrée à chaque lancement) et
    /// options --register-associations / --unregister-associations.
    /// @return faux si l'application doit se terminer (option d'association traitée).
    bool registerPlatformIntegration();
};

} // namespace TSA::UI
