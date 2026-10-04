#pragma once

// Base de connaissances locale (RAG) : documentation TSA (docs/*.md) et documents ajoutés par
// l'utilisateur (<AppData>/ai/knowledge : .md, .txt). Recherche lexicale BM25, sans dépendance
// ni service externe. Chaque extrait garde sa source pour être cité. Si aucune source pertinente
// n'est trouvée, l'assistant doit le dire au lieu de prétendre connaître une norme.

#include <QString>
#include <QStringList>
#include <unordered_map>
#include <vector>

namespace TSA::AI
{

struct KnowledgeChunk
{
    QString source;   // chemin relatif lisible, ex. « docs/ANALYSIS_OPENSEES.md »
    QString heading;  // titre de section le plus proche
    QString text;
};

struct KnowledgeHit
{
    const KnowledgeChunk* chunk = nullptr;
    double score = 0.0;
};

class EngineeringKnowledgeBase
{
public:
    /// Indexe les fichiers .md/.txt des dossiers donnés (récursif). Renvoie le nombre d'extraits.
    int indexDirectories(const QStringList& directories);
    void addDocument(const QString& source, const QString& content);
    void clear();

    std::vector<KnowledgeHit> search(const QString& query, int k = 4, double minScore = 0.5) const;
    int chunkCount() const { return static_cast<int>(m_chunks.size()); }
    QStringList sources() const;

    /// Dossiers par défaut : docs/ à côté de l'exécutable ou du dépôt, et dossier utilisateur.
    static QStringList defaultDirectories();
    static QStringList tokenize(const QString& text);

private:
    void rebuildStatistics();

    std::vector<KnowledgeChunk> m_chunks;
    std::vector<std::unordered_map<QString, int>> m_termFreq;
    std::vector<int> m_lengths;
    std::unordered_map<QString, int> m_docFreq;
    double m_avgLength = 0.0;
};

} // namespace TSA::AI
