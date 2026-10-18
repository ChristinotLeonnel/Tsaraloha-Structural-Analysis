#pragma once

// Index de la recherche globale (docs/UI_SEARCH.md) : commandes, panneaux, paramètres et commandes de la console,
// retrouvés par leur nom dans la langue affichée, par leurs noms dans les autres langues (anglais, français, malgache)
// et par des synonymes. Aucune exécution ici : l'index rend des identifiants, que l'interface relie aux actions
// existantes (ShortcutManager, panneaux, console).
//
// Comparaison : sans casse ni accents (« modele » trouve « Modèle »), ligatures et apostrophes unifiées ; ordre de
// pertinence : libellé exact, début du libellé, début d'un mot, sous-chaîne, puis faute de frappe tolérée (une lettre
// à partir de 4, deux à partir de 7). Les mots courts (« de », « the », « ny »…) sont facultatifs.

#include <QList>
#include <QString>
#include <QStringList>

namespace TSA::Commands
{

enum class SearchKind
{
    Command,         ///< action de l'interface (identifiant du catalogue)
    Panel,           ///< fenêtre ancrée à afficher
    Setting,         ///< réglage (langue, thème, raccourcis…)
    ConsoleCommand   ///< commande du registre d'automatisation (paramètres saisis dans la console)
};

struct SearchEntry
{
    QString id;              ///< identifiant stable : « cmd.file.save », « panel:… », « console:model.create_node »
    SearchKind kind = SearchKind::Command;
    QString title;           ///< libellé dans la langue affichée
    QString description;
    QString location;        ///< emplacement dans le ruban (« Modèle › Éléments filaires »)
    QString shortcut;        ///< raccourci actif, texte natif
    QString iconPath;
    QStringList keywords;    ///< noms dans les autres langues, synonymes
    int order = 0;           ///< départage à pertinence égale (ordre du ruban)
};

struct SearchHit
{
    int entry = -1;          ///< indice dans entries()
    double score = 0.0;
};

class CommandSearchIndex
{
public:
    /// Remplace l'entrée de même identifiant (aucun doublon).
    void add(const SearchEntry& entry);
    void clear();
    const QList<SearchEntry>& entries() const { return m_entries; }
    int size() const { return static_cast<int>(m_entries.size()); }

    /// Résultats triés par pertinence (au plus limit) ; requête vide : aucun résultat.
    QList<SearchHit> search(const QString& query, int limit = 12) const;

    /// Texte comparable : minuscules, sans accents, œ → oe, ponctuation → espace, espaces réduits.
    static QString normalize(const QString& text);
    /// Distance de Damerau-Levenshtein (transpositions adjacentes) bornée : maxDistance + 1 si elle est dépassée.
    static int boundedDistance(const QString& a, const QString& b, int maxDistance);
    /// Mot facultatif de la requête (article, préposition en anglais, français ou malgache).
    static bool isStopWord(const QString& normalizedWord);

private:
    struct Field
    {
        QString text;
        QStringList words;
        double weight = 1.0;
    };
    struct Prepared
    {
        QList<Field> fields;   ///< libellé, noms et synonymes, identifiant, description
    };
    static Field makeField(const QString& raw, double weight);
    double score(const Prepared& p, const QString& query, const QStringList& tokens) const;

    QList<SearchEntry> m_entries;
    QList<Prepared> m_prepared;
};

} // namespace TSA::Commands
