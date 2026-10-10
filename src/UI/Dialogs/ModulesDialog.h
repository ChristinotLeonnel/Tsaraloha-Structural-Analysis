#pragma once

// Gestion des modules (docs/SDK.md) : liste des modules découverts (origine, version, état, messages),
// approbation des modules installés par l'utilisateur, désactivation, rechargement, dossier utilisateur.
// L'interface ne fait qu'appeler le registre (src/Modules) ; aucune règle de validation n'est ici.

#include "../../Modules/ModuleRegistry.h"

#include <QDialog>

class QTableWidget;
class QPlainTextEdit;
class QPushButton;

namespace TSA::UI
{

class ModulesDialog : public QDialog
{
    Q_OBJECT

public:
    ModulesDialog(TSA::Modules::ModuleRegistry& registry, TSA::Modules::ModuleHostServices services, QWidget* parent = nullptr);

signals:
    /// Les modules actifs ont changé (convertisseurs, templates) : l'hôte met à jour ses menus.
    void modulesChanged();

private:
    void populate();
    void updateDetails();
    QString selectedId() const;
    void reload();

    TSA::Modules::ModuleRegistry& m_registry;
    TSA::Modules::ModuleHostServices m_services;
    QTableWidget* m_table = nullptr;
    QPlainTextEdit* m_details = nullptr;
    QPushButton* m_approve = nullptr;
    QPushButton* m_toggle = nullptr;
};

} // namespace TSA::UI
