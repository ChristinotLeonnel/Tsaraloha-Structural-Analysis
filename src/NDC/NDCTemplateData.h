#pragma once

// Données « tsa-ndc/1 » d'une note de calcul (docs/TEMPLATES.md) : le document structuré produit par
// NDCGenerator (chapitres, sections, paragraphes, tableaux, figures) et sa configuration, exposés aux
// templates. La mise en page n'est plus codée en C++ : le template intégré « tsa.ndc.standard » reproduit
// à l'identique NDCDocument::toHtml (conservé comme référence de non-régression).
//
// Les contenus texte de la note sont déjà du HTML produit par l'application (badges, mises en forme) :
// les templates les insèrent avec {{{…}}}. Les métadonnées saisies par l'utilisateur le sont aussi, comme
// dans le générateur historique.

#include "NDCDocumentModel.h"

#include <QJsonObject>

namespace TSA::NDC
{

struct NDCTemplateDataOptions
{
    /// Images désignées par un fichier local incorporées en URI data: (document autonome). Faux : lien
    /// file:/// comme le générateur historique.
    bool inlineLocalImages = true;
};

QJsonObject ndcTemplateData(const NDCDocument& doc, const NDCTemplateDataOptions& options = {});

} // namespace TSA::NDC
