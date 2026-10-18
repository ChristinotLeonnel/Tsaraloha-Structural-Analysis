#pragma once

// Synonymes de la recherche globale (docs/UI_SEARCH.md) : termes du métier en français, anglais et malgache qui ne
// sont pas le libellé de la commande (ses libellés traduits sont ajoutés par l'index à partir des traductions). Les
// termes malgaches suivent le glossaire docs/I18N_GLOSSARY.md.

#include <QString>
#include <QStringList>

namespace TSA::Commands
{

/// Synonymes d'une commande du catalogue (vide si aucun).
QStringList commandSynonyms(const QString& commandId);

} // namespace TSA::Commands
