#pragma once

// Moteur de templates des documents (notes de calcul, rapports) — docs/TEMPLATES.md.
//
// Sous-ensemble « logic-less » de Mustache appliqué à des données JSON (QJsonValue) :
//   {{nom}}            valeur échappée (HTML : & < > " ')
//   {{{nom}}} {{&nom}} valeur brute (contenu HTML déjà produit par l'application)
//   {{#nom}}…{{/nom}}  section : liste → répétée par élément ; objet → contexte ; vrai → affichée
//   {{^nom}}…{{/nom}}  section inverse : affichée si la valeur est absente, fausse, vide, 0 ou liste vide
//   {{> partiel}}      inclusion d'un fragment du même paquet
//   {{! commentaire}}  ignoré
//   a.b.c, {{.}}       noms pointés, élément courant
// Une balise de section, de commentaire ou de partiel seule sur sa ligne supprime la ligne (règle
// « standalone » de Mustache). Non pris en charge (refusés explicitement) : changement de délimiteurs,
// lambdas. Le moteur n'exécute aucun code, n'accède à aucun fichier ni réseau : il ne fait que
// substituer des données. Les données ne sont jamais inventées : une variable absente rend une chaîne
// vide ET est signalée (missingVariables) ; les templates testent les données facultatives par section.

#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace TSA::Templates
{

struct RenderOptions
{
    int maxPartialDepth = 32;                       ///< garde contre les inclusions récursives
    qsizetype maxOutputChars = 64 * 1024 * 1024;    ///< garde contre une sortie démesurée
};

struct RenderResult
{
    QString output;
    QStringList errors;             ///< syntaxe, partiel introuvable, limites : rendu invalide
    QStringList missingVariables;   ///< variables affichées mais absentes des données (sans doublon)
    QStringList warnings;           ///< remarques non bloquantes (option refusée…)
    bool ok() const { return errors.isEmpty(); }
};

class TemplateEngine
{
public:
    /// Fournit le texte d'un partiel ; std::nullopt s'il n'existe pas.
    using PartialResolver = std::function<std::optional<QString>(const QString& name)>;

    /// Erreurs de syntaxe (sections non fermées ou mal imbriquées, balises non prises en charge).
    static QStringList check(const QString& templateText);
    /// Noms de variables et de sections utilisés (sans doublon, ordre d'apparition).
    static QStringList referencedNames(const QString& templateText);
    /// Partiels inclus (sans doublon).
    static QStringList referencedPartials(const QString& templateText);

    static RenderResult render(const QString& templateText, const QJsonValue& data, const PartialResolver& partials = {},
                               const RenderOptions& options = {});

    /// Échappement HTML appliqué par {{nom}}.
    static QString escapeHtml(const QString& text);
};

} // namespace TSA::Templates
